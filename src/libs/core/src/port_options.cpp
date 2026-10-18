#include "storm/port_options.hpp"

#include "core.h"
#include "file_service.h"

#include <algorithm>
#include <cstdio>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>

namespace storm
{
namespace
{
constexpr auto kSection = "touch";

std::mutex &OptionsMutex()
{
    static std::mutex mutex;
    return mutex;
}

// read once: the touch controls ask for them every frame
std::map<std::string, int, std::less<>> &Options()
{
    static std::map<std::string, int, std::less<>> options;
    return options;
}
} // namespace

namespace
{
bool touchPreview = false;
}

void SetTouchPreview(bool preview)
{
    touchPreview = preview;
}

bool IsTouchPreview()
{
    return touchPreview;
}

namespace
{
std::string touchLayoutGroup;

using TouchLayout = std::map<std::string, std::pair<float, float>, std::less<>>;

std::map<std::string, TouchLayout, std::less<>> &TouchLayouts()
{
    static std::map<std::string, TouchLayout, std::less<>> layouts;
    return layouts;
}

constexpr auto kLayoutSection = "touch_layout";

// read once
TouchLayout &GetTouchLayout(const std::string &group)
{
    auto &layouts = TouchLayouts();
    if (const auto it = layouts.find(group); it != layouts.end())
        return it->second;
    auto &layout = layouts[group];
    char buffer[2048]{};
    if (auto ini = fio->OpenIniFile(core.EngineIniFileName());
        ini && ini->ReadString(kLayoutSection, group.c_str(), buffer, sizeof(buffer), ""))
    {
        std::string_view value = buffer;
        while (!value.empty())
        {
            const auto end = std::min(value.find(';'), value.size());
            const std::string item(value.substr(0, end));
            value.remove_prefix(std::min(end + 1, value.size()));
            const auto colon = item.find(':');
            float x, y;
            if (colon != std::string::npos && sscanf(item.c_str() + colon + 1, "%f,%f", &x, &y) == 2)
                layout[item.substr(0, colon)] = {x, y};
        }
    }
    return layout;
}

void SaveTouchLayout(const std::string &group)
{
    std::string value;
    for (const auto &[button, position] : GetTouchLayout(group))
    {
        char item[160];
        snprintf(item, sizeof(item), "%s:%.4f,%.4f;", button.c_str(), position.first, position.second);
        value += item;
    }
    if (auto ini = fio->OpenIniFile(core.EngineIniFileName()))
    {
        ini->WriteString(kLayoutSection, group.c_str(), value.empty() ? "-" : value.c_str());
        ini->Flush();
    }
}
} // namespace

namespace
{
bool menuActive = false;
} // namespace

void SetMenuActive(bool active)
{
    menuActive = active;
}

bool IsMenuActive()
{
    return menuActive;
}

void SetTouchLayoutGroup(const std::string &group)
{
    std::lock_guard lock(OptionsMutex());
    touchLayoutGroup = group;
}

const std::string &GetTouchLayoutGroup()
{
    return touchLayoutGroup;
}

bool GetTouchButtonPosition(const std::string &group, const std::string &button, float &x, float &y)
{
    std::lock_guard lock(OptionsMutex());
    const auto &layout = GetTouchLayout(group);
    const auto it = layout.find(button);
    if (it == layout.end())
        return false;
    x = it->second.first;
    y = it->second.second;
    return true;
}

void SetTouchButtonPosition(const std::string &group, const std::string &button, float x, float y, bool save)
{
    std::lock_guard lock(OptionsMutex());
    GetTouchLayout(group)[button] = {x, y};
    if (save)
        SaveTouchLayout(group);
}

void ResetTouchLayout(const std::string &group)
{
    std::lock_guard lock(OptionsMutex());
    GetTouchLayout(group).clear();
    SaveTouchLayout(group);
}

int GetPortOption(const char *name, int defaultValue)
{
    std::lock_guard lock(OptionsMutex());
    auto &options = Options();
    if (const auto it = options.find(name); it != options.end())
        return it->second;
    auto value = defaultValue;
    if (auto ini = fio->OpenIniFile(core.EngineIniFileName()))
        value = ini->GetInt(kSection, name, defaultValue);
    options.emplace(name, value);
    return value;
}

namespace
{
// the layout of the Xbox version of the game (init_xbox.c) with the hot keys of the computers
const char *DefaultGamepadControl(const std::string_view group, const std::string_view button)
{
    struct Binding
    {
        const char *group, *button, *control;
    };
    static constexpr Binding bindings[] = {
        // the menus
        {"MainInterface", "cross", "IAction"},
        {"MainInterface", "circle", "ICancel"},
        {"MainInterface", "square", "IContextHelp"},
        {"MainInterface", "share", "IAllCancel"},
        {"MainInterface", "options", "IStartButton"},
        {"MainInterface", "l1", "ILeftShift"},
        {"MainInterface", "r1", "IRightShift"},
        {"MainInterface", "up", "IUp"},
        {"MainInterface", "down", "IDown"},
        {"MainInterface", "left", "ILeft"},
        {"MainInterface", "right", "IRight"},
        // walking
        {"PrimaryLand", "cross", "BIFastCommand"},
        {"PrimaryLand", "circle", "ChrAction"},
        {"PrimaryLand", "square", "ChrFightMode"},
        {"PrimaryLand", "triangle", "LICommandsActivate"},
        {"PrimaryLand", "l1", "hk_Cure"},
        {"PrimaryLand", "r1", "TimeScale"},
        {"PrimaryLand", "l2", "ChrCamCameraSwitch"},
        {"PrimaryLand", "options", "MainMenu"},
        {"PrimaryLand", "share", "Interface"},
        // the fight
        {"FightModeControls", "cross", "ChrAttack"},
        {"FightModeControls", "circle", "ChrRecoil"},
        {"FightModeControls", "square", "ChrFightMode"},
        {"FightModeControls", "triangle", "LICommandsActivate"},
        {"FightModeControls", "l1", "hk_Cure"},
        {"FightModeControls", "l2", "ChrBlock"},
        {"FightModeControls", "r2", "ChrFire"},
        {"FightModeControls", "options", "MainMenu"},
        {"FightModeControls", "share", "Interface"},
        // the ship
        {"Sailing3Pers", "cross", "Ship_Fire"},
        {"Sailing3Pers", "circle", "BIFastCommand"},
        {"Sailing3Pers", "square", "Sea_CameraSwitch"},
        {"Sailing3Pers", "triangle", "BICommandsActivate"},
        {"Sailing3Pers", "l1", "TimeScale"},
        {"Sailing3Pers", "l2", "ShipCamera_Backward"},
        {"Sailing3Pers", "r2", "ShipCamera_Forward"},
        {"Sailing3Pers", "up", "hk_charge1"},
        {"Sailing3Pers", "right", "hk_charge2"},
        {"Sailing3Pers", "down", "hk_charge3"},
        {"Sailing3Pers", "left", "hk_charge4"},
        {"Sailing3Pers", "options", "MainMenu"},
        {"Sailing3Pers", "share", "Interface"},
        // the deck
        {"Sailing1Pers", "cross", "Ship_Fire"},
        {"Sailing1Pers", "circle", "BIFastCommand"},
        {"Sailing1Pers", "square", "Sea_CameraSwitch"},
        {"Sailing1Pers", "triangle", "BICommandsActivate"},
        {"Sailing1Pers", "l1", "TimeScale"},
        {"Sailing1Pers", "l2", "TelescopeOut"},
        {"Sailing1Pers", "r2", "TelescopeIn"},
        {"Sailing1Pers", "up", "Ship_SailUp"},
        {"Sailing1Pers", "down", "Ship_SailDown"},
        {"Sailing1Pers", "left", "Ship_TurnLeft"},
        {"Sailing1Pers", "right", "Ship_TurnRight"},
        {"Sailing1Pers", "options", "MainMenu"},
        {"Sailing1Pers", "share", "Interface"},
        // the map of the world
        {"WorldMapControls", "cross", "WMapCancel"},
        {"WorldMapControls", "circle", "WMapSkipEncounter"},
        {"WorldMapControls", "square", "WMapCameraSwitch"},
        {"WorldMapControls", "l2", "WMapBackward"},
        {"WorldMapControls", "r2", "WMapForward"},
        {"WorldMapControls", "options", "MainMenu"},
        {"WorldMapControls", "share", "Interface"},
        // the commands of the battle interface
        {"BattleInterfaceControls", "cross", "BICommandsConfirm"},
        {"BattleInterfaceControls", "circle", "BICommandsCancel"},
        {"BattleInterfaceControls", "triangle", "BICommandsCancel"},
        {"BattleInterfaceControls", "left", "BICommandsLeft"},
        {"BattleInterfaceControls", "right", "BICommandsRight"},
        {"BattleInterfaceControls", "l1", "BICommandsLeft"},
        {"BattleInterfaceControls", "r1", "BICommandsRight"},
        // the dialogs
        {"DialogControls", "cross", "DlgAction"},
        {"DialogControls", "circle", "DlgCancel"},
        {"DialogControls", "up", "DlgUp"},
        {"DialogControls", "down", "DlgDown"},
        {"DialogControls", "left", "DlgScrollUp"},
        {"DialogControls", "right", "DlgScrollDown"},
    };
    for (const auto &binding : bindings)
    {
        if (group == binding.group && button == binding.button)
            return binding.control;
    }
    return "";
}

std::map<std::string, std::string, std::less<>> &GamepadControls()
{
    static std::map<std::string, std::string, std::less<>> controls;
    return controls;
}
} // namespace

std::string GetGamepadControl(const std::string &group, const char *button)
{
    std::lock_guard lock(OptionsMutex());
    const auto key = group + "." + button;
    auto &controls = GamepadControls();
    if (const auto it = controls.find(key); it != controls.end())
        return it->second;
    std::string control = DefaultGamepadControl(group, button);
    if (auto ini = fio->OpenIniFile(core.EngineIniFileName()))
    {
        char buffer[128]{};
        if (ini->ReadString(("gamepad_" + group).c_str(), button, buffer, sizeof(buffer), ""))
            control = std::string_view(buffer) == "-" ? "" : buffer;
    }
    controls.emplace(key, control);
    return control;
}

std::string GetDefaultGamepadControl(const std::string &group, const char *button)
{
    return DefaultGamepadControl(group, button);
}

void SetGamepadControl(const std::string &group, const char *button, const std::string &control)
{
    std::lock_guard lock(OptionsMutex());
    GamepadControls()[group + "." + button] = control;
    if (auto ini = fio->OpenIniFile(core.EngineIniFileName()))
    {
        // "-" is no control (an empty value would be the default)
        ini->WriteString(("gamepad_" + group).c_str(), button, control.empty() ? "-" : control.c_str());
        ini->Flush();
    }
}

namespace
{
std::string captureGroup, captureControl;
bool captureDone = false;
} // namespace

void StartGamepadCapture(const std::string &group, const std::string &control)
{
    std::lock_guard lock(OptionsMutex());
    captureGroup = group;
    captureControl = control;
}

void StopGamepadCapture()
{
    std::lock_guard lock(OptionsMutex());
    captureGroup.clear();
    captureControl.clear();
}

bool GetGamepadCapture(std::string &group, std::string &control)
{
    std::lock_guard lock(OptionsMutex());
    group = captureGroup;
    control = captureControl;
    return !captureControl.empty();
}

void SetGamepadCaptureDone()
{
    std::lock_guard lock(OptionsMutex());
    captureDone = true;
}

bool TakeGamepadCaptureDone()
{
    std::lock_guard lock(OptionsMutex());
    return std::exchange(captureDone, false);
}

void SetPortOption(const char *name, int value)
{
    std::lock_guard lock(OptionsMutex());
    Options()[name] = value;
    if (auto ini = fio->OpenIniFile(core.EngineIniFileName()))
    {
        ini->WriteString(kSection, name, std::to_string(value).c_str());
        ini->Flush();
    }
}
} // namespace storm
