#include <cstdio>
#include <cstdlib>
#include <thread>

#include <SDL2/SDL.h>
#ifdef STORM_USE_MIMALLOC
#include <mimalloc-new-delete.h>
#include <mimalloc.h>
#endif
#include <spdlog/spdlog.h>
#include <CLI/CLI.hpp>

#ifdef __APPLE__
#include <TargetConditionals.h>
#include <csignal>
#include <ctime>
#include <dlfcn.h>
#include <execinfo.h>
#include <fcntl.h>
#include <mach-o/dyld.h>
#include <unistd.h>
#if TARGET_OS_IPHONE
#include <fstream>
#include <sys/clonefile.h>
#endif
#endif

#include "core_private.h"
#include "fs.h"
#include "lifecycle_diagnostics_service.hpp"
#include "logging.hpp"
#include "os_window.hpp"
#include "steam_api.hpp"
#include "storm/engine_settings.hpp"
#include "storm/port_options.hpp"
#include "string_compare.hpp"
#include "v_sound_service.h"
#include "watermark.hpp"

namespace
{

CorePrivate *core_private;

constexpr char defaultLoggerName[] = "system";
constexpr const char *DEFAULT_WINDOW_TITLE = "Storm Engine";
bool isRunning = false;
bool bActive = true;
bool bSoundInBackground = false;

storm::diag::LifecycleDiagnosticsService lifecycleDiagnostics;

void RunFrame()
{
    if (!core_private->Run())
    {
        isRunning = false;
    }

    lifecycleDiagnostics.notifyAfterRun();
}

#ifdef _WIN32
void RunFrameWithOverflowCheck()
{
    __try
    {
        RunFrame();
    }
    __except ([](unsigned code, struct _EXCEPTION_POINTERS *ep) {
        return code == EXCEPTION_STACK_OVERFLOW;
    }(GetExceptionCode(), GetExceptionInformation()))
    {
        _resetstkoflw();
        throw std::runtime_error("Stack overflow");
    }
}
#else
#define RunFrameWithOverflowCheck RunFrame
#endif

#ifdef STORM_USE_MIMALLOC
void mimalloc_fun(const char *msg, void *arg)
{
    static std::filesystem::path mimalloc_log_path;
    if (mimalloc_log_path.empty())
    {
        mimalloc_log_path =storm::GetEngineSettings().GetEnginePath(storm::EngineSettingsPathType::Logs) / "mimalloc.log";
        std::error_code ec;
        remove(mimalloc_log_path, ec);
    }

    FILE *mimalloc_log =
#ifdef _MSC_VER
        _wfopen(mimalloc_log_path.c_str(), L"a+b");
#else
        fopen(mimalloc_log_path.c_str(), "a+b");
#endif
    if (mimalloc_log != nullptr)
    {
        fputs(msg, mimalloc_log);
        fclose(mimalloc_log);
    }
}
#endif

#ifdef __APPLE__
// Crashes: the call stacks of the engine (crash.log next to the game data, symbolize with atos and the load address)
// and of the scripts (script_stack.log); iOS keeps few crash reports
void CrashSignalHandler(int sig, siginfo_t *info, void *context)
{
    // once: the crash information may abort, that must end the process and not come back here
    static volatile sig_atomic_t handled = 0;
    signal(sig, SIG_DFL);
    if (handled)
    {
        raise(sig);
        return;
    }
    handled = 1;
    if (const int fd = open("crash.log", O_WRONLY | O_CREAT | O_APPEND, 0644); fd >= 0)
    {
        // the address the engine is loaded at (for atos -l), from this function
        Dl_info dl{};
        dladdr(reinterpret_cast<const void *>(&CrashSignalHandler), &dl);
        dprintf(fd, "\n=== signal %d at %ld, load address %p, fault address %p\n", sig, static_cast<long>(time(nullptr)),
                dl.dli_fbase, info ? info->si_addr : nullptr);
#if defined(__arm64__)
        // the crashed instruction and the registers (the call stack below starts at the caller of the crashed function)
        if (context)
        {
            const auto &ss = static_cast<ucontext_t *>(context)->uc_mcontext->__ss;
            dprintf(fd, "pc %p lr %p fp %p sp %p\n", reinterpret_cast<void *>(ss.__pc),
                    reinterpret_cast<void *>(ss.__lr), reinterpret_cast<void *>(ss.__fp),
                    reinterpret_cast<void *>(ss.__sp));
            for (int i = 0; i < 29; i++)
                dprintf(fd, "x%d %p%s", i, reinterpret_cast<void *>(ss.__x[i]), i % 4 == 3 ? "\n" : " ");
            dprintf(fd, "\n");
        }
#endif
        void *frames[128];
        backtrace_symbols_fd(frames, backtrace(frames, 128), fd);
        close(fd);
    }
    if (core_private)
    {
        core_private->collectCrashInfo();
        spdlog::apply_all([](const std::shared_ptr<spdlog::logger> &logger) { logger->flush(); });
    }
    raise(sig);
}

#if TARGET_OS_IPHONE
// An IPA with the game (tools/ios/package-adhoc.sh --data) carries it in GameData of the app bundle, which is read-only:
// it is put into the Documents folder (the working directory) once per version of the data. The files are APFS clones,
// which take no time and no space until changed. The settings of the player (engine.ini, options) and the saved games
// are kept.
void InstallBundledGameData(const std::filesystem::path &bundle)
{
    namespace fs = std::filesystem;
    const auto data = bundle / "GameData";
    std::error_code ec;
    if (!fs::is_directory(data, ec))
        return;
    const auto readVersion = [](const fs::path &path) {
        std::ifstream file(path);
        std::string version;
        std::getline(file, version);
        return version;
    };
    const auto version = readVersion(data / ".version");
    if (version.empty() || version == readVersion(".gamedata_version"))
        return;
    SDL_Log("Installing the game data %s", version.c_str());
    for (auto it = fs::recursive_directory_iterator(data, ec); !ec && it != fs::recursive_directory_iterator();
         it.increment(ec))
    {
        const auto relative = fs::relative(it->path(), data, ec);
        if (it->is_directory(ec))
        {
            fs::create_directories(relative, ec);
            continue;
        }
        const auto name = relative.string();
        if (name == ".version" || (name == "engine.ini" || name == "options") && fs::exists(relative, ec))
            continue;
        fs::remove(relative, ec);
        if (clonefile(it->path().c_str(), relative.c_str(), 0) != 0)
            fs::copy_file(it->path(), relative, fs::copy_options::overwrite_existing, ec);
    }
    std::ofstream(".gamedata_version") << version << "\n";
}
#endif

// DXVK and SDL load Vulkan (MoltenVK) at runtime, point SDL to the copy that is bundled with the engine
// and, on iOS, run from the app's Documents folder which contains the game data
void SetupApplePlatform()
{
    if (char *base_path = SDL_GetBasePath())
    {
#if TARGET_OS_IPHONE
        const std::string moltenvk = std::string(base_path) + "Frameworks/MoltenVK.framework/MoltenVK";
#else
        const std::string moltenvk = std::string(base_path) + "libMoltenVK.dylib";
#endif
        // read by SDL_Vulkan_LoadLibrary(nullptr), which DXVK calls
        setenv("SDL_VULKAN_LIBRARY", moltenvk.c_str(), 0);
        // the window of the engine is SDL2: DXVK would pick SDL3 if it was built with it too (SDL3 headers found)
        setenv("DXVK_WSI_DRIVER", "SDL2", 0);
        SDL_free(base_path);
    }

#if TARGET_OS_IPHONE
    if (const char *home = std::getenv("HOME"))
    {
        const std::string documents = std::string(home) + "/Documents";
        if (chdir(documents.c_str()) != 0)
        {
            SDL_Log("Unable to switch to the Documents folder");
        }
        // DXVK, MoltenVK and Metal validation report errors to stderr, which iOS discards;
        // the previous log is kept, as the crash happens before the game is started again
        std::rename("stderr.log", "stderr.prev.log");
        if (freopen("stderr.log", "w", stderr))
        {
            setvbuf(stderr, nullptr, _IONBF, 0);
        }

        // engine resources (techniques, shaders) are shipped in the app bundle, but resources are
        // looked up in the working directory next to the game data, so they are synchronized there
        if (char *bundle_path = SDL_GetBasePath())
        {
            InstallBundledGameData(bundle_path);
            // the game folder is usually named RESOURCE, reuse it whatever the case is
            std::filesystem::path resource_dir = "resource";
            std::error_code ec;
            for (const auto &entry : std::filesystem::directory_iterator(".", ec))
            {
                if (entry.is_directory() && storm::iEquals(entry.path().filename().string(), "resource"))
                {
                    resource_dir = entry.path().filename();
                    break;
                }
            }
            std::filesystem::copy(std::filesystem::path(bundle_path) / "resource", resource_dir,
                                  std::filesystem::copy_options::recursive |
                                      std::filesystem::copy_options::update_existing,
                                  ec);
            if (ec)
            {
                SDL_Log("Unable to copy engine resources: %s", ec.message().c_str());
            }
            SDL_free(bundle_path);
        }
        // game files are looked up case-insensitively through a scan of the working directory
        fio->ScanResourcePaths();
    }
    SDL_SetHint(SDL_HINT_IOS_HIDE_HOME_INDICATOR, "2");
#endif
}
#endif

} // namespace

void HandleWindowEvent(const storm::OSWindow::Event &event)
{
    if (event == storm::OSWindow::Closed)
    {
        isRunning = false;
        if (core_private->initialized())
        {
            core_private->Event("DestroyWindow");
        }
    }
    else if (event == storm::OSWindow::FocusGained)
    {
        bActive = true;
        if (core_private->initialized())
        {
            core_private->AppState(bActive);
            if (const auto soundService = core.GetServiceX<VSoundService>();
                soundService && !bSoundInBackground)
            {
                soundService->SetActiveWithFade(true);
            }
        }
    }
    else if (event == storm::OSWindow::FocusLost)
    {
        bActive = false;
        if (core_private->initialized())
        {
            core_private->AppState(bActive);
            if (const auto soundService = core.GetServiceX<VSoundService>();
                soundService && !bSoundInBackground)
            {
                soundService->SetActiveWithFade(false);
            }
        }
    }
}

int main(int argc, char *argv[])
{
    CLI::App app("Storm Engine");

    bool enable_editor = false;
    app.add_flag("--editor", enable_editor, "Enable in-game editor");

    try
    {
        app.parse(argc, argv);
    }
    catch (const CLI::ParseError &e)
    {
        return app.exit(e);
    }

#ifdef __APPLE__
    SetupApplePlatform();
    for (const int sig : {SIGSEGV, SIGBUS, SIGABRT, SIGILL, SIGFPE})
    {
        struct sigaction action = {};
        action.sa_sigaction = CrashSignalHandler;
        action.sa_flags = SA_SIGINFO;
        sigaction(sig, &action, nullptr);
    }
#endif

    // Prevent multiple instances
#ifdef _WIN32 // CreateEventA
    if (!CreateEventA(nullptr, false, false, "Global\\FBBD2286-A9F1-4303-B60C-743C3D7AA7BE") ||
        GetLastError() == ERROR_ALREADY_EXISTS)
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Another instance is already running!", nullptr);
        return EXIT_SUCCESS;
    }
#endif
#ifdef STORM_USE_MIMALLOC
    mi_register_output(mimalloc_fun, nullptr);
    mi_option_set(mi_option_show_errors, 1);
    mi_option_set(mi_option_show_stats, 0);
    mi_option_set(mi_option_eager_commit, 1);
    mi_option_set(mi_option_eager_region_commit, 1);
    mi_option_set(mi_option_large_os_pages, 1);
//    mi_option_set(mi_option_page_reset, 0);
//    mi_option_set(mi_option_segment_reset, 0);
    mi_option_set(mi_option_reserve_huge_os_pages, 1);
//    mi_option_set(mi_option_segment_cache, 16);
#ifdef _DEBUG
    mi_option_set(mi_option_verbose, 4);
#endif
#endif // STORM_USE_MIMALLOC

#ifdef __APPLE__
    // the accelerometer of the iPad is not a joystick (the first one, before the gamepads)
    SDL_SetHint(SDL_HINT_ACCELEROMETER_AS_JOYSTICK, "0");
#endif
    SDL_InitSubSystem(SDL_INIT_EVENTS | SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER);

    // Init diagnostics
    const auto lifecycleDiagnosticsGuard =
#ifdef STORM_ENABLE_CRASH_REPORTS
        lifecycleDiagnostics.initialize(true);
#else
        lifecycleDiagnostics.initialize(false);
#endif
#ifdef __APPLE__
    // exit() (iOS ends a closed app so): the logging thread stops before the static destructors destroy spdlog,
    // which is created first to be destroyed after this
    spdlog::default_logger();
    std::atexit([] { lifecycleDiagnostics.terminate(); });
#endif
    if (!lifecycleDiagnosticsGuard)
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Warning", "Unable to initialize lifecycle service!", nullptr);
    }
    else
    {
        lifecycleDiagnostics.setCrashInfoCollector([]() { core_private->collectCrashInfo(); });
    }

    // Read config
    auto ini = fio->OpenIniFile(fs::ENGINE_INI_FILE_NAME);
    if (!ini)
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Engine config not found.", nullptr);
        return EXIT_FAILURE;
    }

    // Init stash
    create_directories(storm::GetEngineSettings().GetEnginePath(storm::EngineSettingsPathType::SaveData));

    // Init logging
    spdlog::set_default_logger(storm::logging::getOrCreateLogger(defaultLoggerName));
    spdlog::info("Logging system initialized. Running on {}", STORM_BUILD_WATERMARK_STRING);
#ifdef STORM_USE_MIMALLOC
    spdlog::info("mimalloc-redirect status: {}", mi_is_redirected());
#endif

    // Init core
    core_private = static_cast<CorePrivate *>(&core);
    core_private->EnableEditor(enable_editor);
    core_private->Init();

#if defined(__APPLE__) && TARGET_OS_IPHONE
    // the refresh rate of the iPad screens (ProMotion), more frames heat them up for nothing
    uint32_t dwMaxFPS = static_cast<uint32_t>(ini->GetInt(nullptr, "max_fps", 120));
#else
    uint32_t dwMaxFPS = static_cast<uint32_t>(ini->GetInt(nullptr, "max_fps", 0));
#endif
    auto bDebugWindow = ini->GetInt(nullptr, "DebugWindow", 0) == 1;
    auto bAcceleration = ini->GetInt(nullptr, "Acceleration", 0) == 1;
    if (ini->GetInt(nullptr, "logs", 1) == 0) // disable logging
    {
        spdlog::set_level(spdlog::level::off);
    }
    int width = ini->GetInt(nullptr, "screen_x", 1024);
    int height = ini->GetInt(nullptr, "screen_y", 768);
    int preferred_display = ini->GetInt(nullptr, "display", 0);
    bool fullscreen = ini->GetInt(nullptr, "full_screen", false);
    bool show_borders = ini->GetInt(nullptr, "window_borders", false);
    bool run_in_background = ini->GetInt(nullptr, "run_in_background", false);
    if (run_in_background) 
    {
        bSoundInBackground = ini->GetInt(nullptr, "sound_in_background", true);
    }
    else 
    {
        bSoundInBackground = false;
    }
    bool bSteam = ini->GetInt(nullptr, "Steam", 1) != 0;
    
    // initialize SteamApi through evaluating its singleton
    try
    {
        steamapi::SteamApi::getInstance(!bSteam);
    }
    catch (const std::exception &e)
    {
        spdlog::critical(e.what());
        return EXIT_FAILURE;
    }

    std::shared_ptr<storm::OSWindow> window =
        storm::OSWindow::Create(width, height, preferred_display, fullscreen, show_borders);

    char title[256]{};
    ini->ReadString(nullptr, "title", title, sizeof(title), DEFAULT_WINDOW_TITLE);
    window->SetTitle(title);

    window->Subscribe(HandleWindowEvent);
    window->Show();
    core_private->SetWindow(window);

    // Init core
    core_private->InitBase();

    // Message loop
    auto dwOldTime = SDL_GetTicks();

    isRunning = true;
    while (isRunning)
    {
        SDL_Event event{};
        while (SDL_PollEvent(&event) )
        {
            if (auto *editor = core.GetEditor(); editor != nullptr) {
                editor->ProcessEvent(event);
            }
        }

        if (bActive || run_in_background)
        {
#ifdef __APPLE__
            // the menus of the game do not need more than 30 frames: less heat
            const auto dwFrameRate = storm::IsMenuActive() && (dwMaxFPS == 0 || dwMaxFPS > 30) ? 30u : dwMaxFPS;
#else
            const auto dwFrameRate = dwMaxFPS;
#endif
            if (dwFrameRate)
            {
                const auto dwMS = 1000u / dwFrameRate;
                const auto dwNewTime = SDL_GetTicks();
                if (dwNewTime - dwOldTime < dwMS)
                {
#ifdef __APPLE__
                    // sleeping until the next frame without loading the processor: the sleep lasts longer than asked,
                    // so most of the time is slept at once and the last 2 milliseconds by one millisecond (a late
                    // frame would wait for the next refresh of the screen)
                    const auto dwRest = dwMS - (dwNewTime - dwOldTime);
                    std::this_thread::sleep_for(std::chrono::milliseconds(dwRest > 3 ? dwRest - 2 : 1));
#endif
                    continue;
                }
                dwOldTime = dwNewTime;
            }

            RunFrameWithOverflowCheck();
#ifdef __APPLE__
            // the frame rate in the log every 10 seconds (and the limit: the menus have 30)
            {
                static uint32_t dwFpsTime = SDL_GetTicks(), dwFrames = 0;
                dwFrames++;
                if (const auto dwNow = SDL_GetTicks(); dwNow - dwFpsTime >= 10000)
                {
                    spdlog::info("FPS {:.1f} (limit {})", dwFrames * 1000.0 / (dwNow - dwFpsTime), dwFrameRate);
                    dwFpsTime = dwNow;
                    dwFrames = 0;
                }
            }
#endif
#ifdef __APPLE__
            // the options of the game change it (XI_NextEngineOption)
            dwMaxFPS = static_cast<uint32_t>(ini->GetInt(nullptr, "max_fps", static_cast<int32_t>(dwMaxFPS)));
#endif
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

#ifdef STORM_USE_MIMALLOC
        if (core.Controls && core.Controls->GetDebugAsyncKeyState(VK_F1) && core.Controls->GetDebugAsyncKeyState(VK_SHIFT))
        {
            mi_stats_print_out(mimalloc_fun, nullptr);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
#endif
    }

    // Release
    core_private->Event("ExitApplication");
    core_private->CleanUp();
    core_private->ReleaseBase();
#ifdef _WIN32 // FIX_LINUX Cursor
    ClipCursor(nullptr);
#elif defined(_DEBUG) && defined(STORM_USE_MIMALLOC)
    mi_option_set(mi_option_verbose, 0); // disable statistics writing in Linux
#endif
    SDL_Quit();

#if defined(__APPLE__) && TARGET_OS_IPHONE
    // iOS keeps the app with a black screen when the main function returns; the app cannot start itself again
    // (XI_RestartGame): the player does it. Without the static destructors: the logging thread still runs and
    // aborts on its destroyed mutex
    spdlog::shutdown();
    fflush(nullptr);
    _exit(EXIT_SUCCESS);
#elif defined(__APPLE__)
    // the restart from the options of the game (XI_RestartGame): the same executable with the same arguments
    if (getenv("STORM_RESTART"))
    {
        unsetenv("STORM_RESTART");
        execv(argv[0], argv);
    }
#endif
    return EXIT_SUCCESS;
}
