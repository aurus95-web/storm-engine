# Порт Storm Engine на macOS / iPad

Цель: запустить «Корсары 2» — **Beyond New Horizons** (BNH, бесплатная standalone-версия
мода New Horizons для Pirates of the Caribbean, https://cmdrhammie.itch.io/beyond-new-horizons)
на iPad владельца репозитория. Сборка только для себя (sideload через Xcode), не App Store:
движок под GPL-3.0, ассеты игры защищены авторским правом и **никогда не коммитятся** в репозиторий.

Рабочая ветка: `claude/confident-franklin-7779pa`. Основа — форк движка Pirates Ahoy
(https://github.com/PiratesAhoy/storm-engine, `main`), на нём работает BNH.

## Архитектура порта

- Рендер: движок пишет в D3D9. На Apple: **DXVK** (D3D9 → Vulkan) + **MoltenVK** (Vulkan → Metal).
  Тот же стек, что в порте C&C Generals на iPad: https://github.com/ammaarreshi/Generals-Mac-iOS-iPad
  (см. `docs/port/PORTING_PLAYBOOK.md` там — много готовых решений iOS-проблем).
- DXVK: форк `fbraz3/dxvk`, ветка `generalsx-macos-v2.6` (коммит закреплён в `cmake/apple.cmake`),
  с нашим патчем `cmake/dxvk-apple.patch` (поиск MoltenVK/SDL2 в бандле, swapchain в пикселях).
- DXVK загружает SDL2 через `dlopen`, поэтому на Apple SDL2 — **динамическая** библиотека.
- Окно/ввод: SDL2. Звук: FMOD (SDK скачивается вручную с fmod.com, путь в `STORM_FMOD_DIR`;
  без него движок собирается и работает без звука).
- SSE → NEON через `src/libs/math/include/third_party/sse2neon.h` (подключается из `storm_simd.h`).
- Зависимости без Conan: `cmake/dependencies.cmake` (FetchContent из git). `STORM_USE_CONAN=OFF`
  по умолчанию на Apple. Цели называются так же, как в Conan-сборке.
- Sentry и mimalloc на Apple выключены (`STORM_USE_SENTRY`, `STORM_USE_MIMALLOC`).

## Сборка

Инструменты: `brew install cmake meson ninja glslang pkg-config molten-vk`
(или LunarG Vulkan SDK с переменной `VULKAN_SDK` — нужен для iOS: MoltenVK.xcframework).

```bash
# macOS (Apple Silicon)
cmake --preset macos [-DSTORM_FMOD_DIR=~/FMOD/macos]
cmake --build --preset macos-debug          # → build/macos/Debug/engine + dylib рядом

# iOS / iPadOS (Xcode-генератор)
cmake --preset ios -DSTORM_IOS_BUNDLE_ID=com.<you>.storm \
      -DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM=<TEAM_ID> \
      [-DSTORM_MOLTENVK_PATH=<.../ios-arm64/MoltenVK.framework>] [-DSTORM_FMOD_DIR=~/FMOD/ios]
cmake --build --preset ios-debug
xcrun devicectl list devices
xcrun devicectl device install app --device <UUID> build/ios/Debug/engine.app   # путь проверить: find build/ios -name engine.app
xcrun devicectl device process launch --console --device <UUID> <bundle-id>
```

Linux (проверка общего не-Windows кода, так же собирает CI):
```bash
cmake -B build -G Ninja -DSTORM_USE_CONAN=OFF -DSTORM_FETCH_SDL=ON -DCMAKE_BUILD_TYPE=Debug
ninja -C build dependencies && ninja -C build
```

## Запуск игры

- macOS: распаковать BNH, положить собранный `engine` и все `*.dylib` из `build/macos/Debug/`
  в папку игры (рядом с `engine.ini`), запускать из этой папки (`cd <BNH> && ./engine`).
  Поиск файлов игры регистронезависимый (сканирование рабочей папки, `FILE_SERVICE::ScanResourcePaths`).
  Папку `resource/` из `build/macos/Debug/` (техники и шейдеры движка) тоже скопировать в папку игры
  (слить с её `RESOURCE/`).
- iPad: через Finder (iPad → Файлы) или приложение «Файлы» скопировать содержимое папки BNH
  в Documents приложения. На iOS движок делает `chdir` в Documents и сам копирует `resource/`
  из бандла в папку игры (`SetupApplePlatform` в `main.cpp`).
- Логи: `~/Library/Application Support/Akella/Sea Dogs/Logs` (или `stash`/`logs_folder` из `engine.ini`),
  лог DXVK — `*_d3d9.log` рядом с бинарником/в рабочей папке.
- **Осторожно**: `xcrun devicectl device copy to ... --remove-existing-content true` стирает весь
  контейнер приложения (включая данные игры) — не использовать.

## Проверка изменений рендера

Делай скриншоты (`screencapture -x shot.png` на macOS) и смотри на них: ошибки рендера видны только глазами.
Сравнивай с той же сценой на Windows-версии BNH, если владелец может прислать скриншот.

## Состояние и план

1. [x] Слияние с PiratesAhoy, режим без Conan, sse2neon, CMake для macOS/iOS.
   Проверено только в Linux-контейнере: Linux-сборка, все исходники проходят `clang --target=aarch64`,
   движок совместим с заголовками DXVK-форка, патч DXVK применяется и собирается.
   На macOS собирается (`macos-debug`, Apple clang 21, CMake 4, хватает Command Line Tools без Xcode);
   без данных игры движок стартует, грузит DXVK/MoltenVK/SDL2 из своей папки и показывает окно ошибки.
2. [x] Запустить на macOS с данными BNH, получить главное меню — работает (фон, кнопки, текст).
   Можно не копировать `engine` в папку игры: `cd <BNH> && <repo>/build/macos/Debug/engine`
   (dylib ищутся рядом с бинарником). Из `resource/` движка в игру нужны только `*.vso`/`*.pso`:
   остальные файлы у BNH те же (отличаются переводами строк). Картинки `.png/.jpg/.tga` вне Windows
   грузит stb_image (`TextureLoadUsingD3DX`). Язык выбирается в игре (Options → Language), `language.ini` не влияет.
   Осталось: техники `particles_inv` (`texture | complement`) и `wdmseadraw4` (`subtract`) не разбираются,
   звук (FMOD), проверить саму игру (море, локации, карта мира).
3. [x] Шейдеры для не-Windows. На Windows `.fx` компилируются D3DX Effects, на остальных платформах
   `CTechnique` сам разбирает `.fx` и грузит предкомпилированный байткод `<fx>_<функция>.vso/.pso`
   (`GetShaderBinPath`), константы C++ передаёт напрямую в регистры (`#else` ветки рядом с `#ifdef _WIN32 // Effects`).
   Не хватало `worldmap_main.vso` (облака) и `font_ps_font*.pso` (BmFont): добавлены, байткод шрифтов
   генерирует `tools/shader-bins/font_shaders.py`, проверено `tools/shader-bins/test_shaders.cpp`
   через DXVK на lavapipe. При новых HLSL-шейдерах в `.fx` нужно добавлять такие же бинарники.
4. [ ] iPad: подпись, запуск, пути, производительность.
5. [ ] Тач-управление: тап = клик, долгое нажатие = правый клик, щипок = колесо
   (см. раздел Touch input в playbook Generals), ввод текста, разрешение интерфейса.
   Важно: движок включает `SDL_SetRelativeMouseMode` (`src/libs/input/src/sdl_input.cpp`) и сам ведёт
   курсор по приращениям мыши — синтезированный из касания клик придётся на текущую позицию курсора,
   а не под палец. Тач-слой должен сначала ставить курсор в точку касания (абсолютные координаты).
   Уже работают без доработок: геймпад (SDL GameController, MFi/Xbox/PS на iPad) и мышь/трекпад iPadOS.

Известные риски: D3D9 на MoltenVK (нет geometry shaders — в форке fbraz3 уже обработано),
видео-ролики (`aviplayer`) на не-Windows отключены, Linux-путь DXVK (`cmake/linux.cmake`)
использует старый dxvk-native.

## Правила

- Код пишется в стиле окружающего кода, правки минимальные и по делу.
- Платформенный код — под `#ifdef __APPLE__` / `TARGET_OS_IPHONE`, Windows-сборка не должна ломаться.
- Перед пушем: Linux-сборка без Conan (или CI Linux) должна проходить.
