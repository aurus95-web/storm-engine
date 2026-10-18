#pragma once

#include <string>

namespace storm
{
// The settings of the touch controls changed in the options of the game (the port to the iPad): the [touch] section of
// engine.ini, in percent of the defaults ("button_scale", "stick_scale", "camera_speed", "zoom_speed")
int GetPortOption(const char *name, int defaultValue = 100);
void SetPortOption(const char *name, int value);

// the touch controls are drawn over the menu of their settings (they do not work)
void SetTouchPreview(bool preview);
bool IsTouchPreview();
// the buttons of the group are drawn over the menu and moved by the finger ("": not)
void SetTouchLayoutGroup(const std::string &group);
const std::string &GetTouchLayoutGroup();

// A menu of the game is open (the "MainInterface" group of the controls): fewer frames are drawn
void SetMenuActive(bool active);
bool IsMenuActive();

// The places of the touch buttons moved by the player: the centers of the buttons (from 0 to 1 of the screen) in a
// group of the controls, the [touch_layout] section of engine.ini ("<group> = <button>:<x>,<y>;...")
bool GetTouchButtonPosition(const std::string &group, const std::string &button, float &x, float &y);
// saved in engine.ini if save (else while the button is moved)
void SetTouchButtonPosition(const std::string &group, const std::string &button, float x, float y, bool save);
void ResetTouchLayout(const std::string &group);

// The buttons of the gamepad (the names of PlayStation: "cross", "circle", "square", "triangle", "share", "ps",
// "options", "l3", "r3", "l1", "r1", "up", "down", "left", "right", "l2", "r2"): the control of the game that each
// one presses in a group of the controls of the scripts ("PrimaryLand", "Sailing3Pers"...), the [gamepad_<group>]
// sections of engine.ini or the defaults
constexpr const char *kGamepadButtons[] = {"cross", "circle", "square", "triangle", "share", "ps",
                                           "options", "l3", "r3", "l1", "r1", "up",
                                           "down", "left", "right", "l2", "r2"};
constexpr int kGamepadButtonsNum = sizeof(kGamepadButtons) / sizeof(kGamepadButtons[0]);
std::string GetGamepadControl(const std::string &group, const char *button);
std::string GetDefaultGamepadControl(const std::string &group, const char *button);
void SetGamepadControl(const std::string &group, const char *button, const std::string &control);
// The options of the game wait for a button of the gamepad for the control of the group: the controls (pcs_controls)
// give it the next pressed button (instead of pressing anything) and mark the capture done
void StartGamepadCapture(const std::string &group, const std::string &control);
void StopGamepadCapture();
bool GetGamepadCapture(std::string &group, std::string &control);
void SetGamepadCaptureDone();
// a button was given since the last call
bool TakeGamepadCaptureDone();

// the factor of the setting in percent: 1 by default
inline float GetPortOptionScale(const char *name)
{
    return GetPortOption(name) * 0.01f;
}
} // namespace storm
