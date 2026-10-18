#include "sdl_input.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_system.h>
#include <SDL2/SDL_video.h>
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>

namespace storm
{
namespace
{
// TODO: Use scancodes everywhere instead of Windows virtual key
static const std::map<unsigned int, unsigned int> VK_TO_SDL_MAP{
    {VK_CANCEL, SDL_SCANCODE_CANCEL},
    {VK_BACK, SDL_SCANCODE_BACKSPACE},
    {VK_TAB, SDL_SCANCODE_TAB},
    {VK_CLEAR, SDL_SCANCODE_CLEAR},
    {VK_RETURN, SDL_SCANCODE_RETURN},
    {VK_SHIFT, SDL_SCANCODE_LSHIFT},
    {VK_CONTROL, SDL_SCANCODE_LCTRL},
    {VK_MENU, SDL_SCANCODE_LALT},
    {VK_PAUSE, SDL_SCANCODE_PAUSE},
    {VK_CAPITAL, SDL_SCANCODE_CAPSLOCK},
    {VK_ESCAPE, SDL_SCANCODE_ESCAPE},
    {VK_SPACE, SDL_SCANCODE_SPACE},
    {VK_PRIOR, SDL_SCANCODE_PAGEUP},
    {VK_NEXT, SDL_SCANCODE_PAGEDOWN},
    {VK_END, SDL_SCANCODE_END},
    {VK_HOME, SDL_SCANCODE_HOME},
    {VK_LEFT, SDL_SCANCODE_LEFT},
    {VK_UP, SDL_SCANCODE_UP},
    {VK_RIGHT, SDL_SCANCODE_RIGHT},
    {VK_DOWN, SDL_SCANCODE_DOWN},
    {VK_SELECT, SDL_SCANCODE_SELECT},
    {VK_EXECUTE, SDL_SCANCODE_EXECUTE},
    {VK_SNAPSHOT, SDL_SCANCODE_PRINTSCREEN},
    {VK_INSERT, SDL_SCANCODE_INSERT},
    {VK_DELETE, SDL_SCANCODE_DELETE},
    {VK_HELP, SDL_SCANCODE_HELP},
    {'0', SDL_SCANCODE_0},
    {'1', SDL_SCANCODE_1},
    {'2', SDL_SCANCODE_2},
    {'3', SDL_SCANCODE_3},
    {'4', SDL_SCANCODE_4},
    {'5', SDL_SCANCODE_5},
    {'6', SDL_SCANCODE_6},
    {'7', SDL_SCANCODE_7},
    {'8', SDL_SCANCODE_8},
    {'9', SDL_SCANCODE_9},
    {'A', SDL_SCANCODE_A},
    {'B', SDL_SCANCODE_B},
    {'C', SDL_SCANCODE_C},
    {'D', SDL_SCANCODE_D},
    {'E', SDL_SCANCODE_E},
    {'F', SDL_SCANCODE_F},
    {'G', SDL_SCANCODE_G},
    {'H', SDL_SCANCODE_H},
    {'I', SDL_SCANCODE_I},
    {'J', SDL_SCANCODE_J},
    {'K', SDL_SCANCODE_K},
    {'L', SDL_SCANCODE_L},
    {'M', SDL_SCANCODE_M},
    {'N', SDL_SCANCODE_N},
    {'O', SDL_SCANCODE_O},
    {'P', SDL_SCANCODE_P},
    {'Q', SDL_SCANCODE_Q},
    {'R', SDL_SCANCODE_R},
    {'S', SDL_SCANCODE_S},
    {'T', SDL_SCANCODE_T},
    {'U', SDL_SCANCODE_U},
    {'V', SDL_SCANCODE_V},
    {'W', SDL_SCANCODE_W},
    {'X', SDL_SCANCODE_X},
    {'Y', SDL_SCANCODE_Y},
    {'Z', SDL_SCANCODE_Z},
    {VK_LWIN, SDL_SCANCODE_LGUI},
    {VK_RWIN, SDL_SCANCODE_RGUI},
    {VK_APPS, SDL_SCANCODE_MENU},
    {VK_SLEEP, SDL_SCANCODE_SLEEP},
    {VK_NUMPAD0, SDL_SCANCODE_KP_0},
    {VK_NUMPAD1, SDL_SCANCODE_KP_1},
    {VK_NUMPAD2, SDL_SCANCODE_KP_2},
    {VK_NUMPAD3, SDL_SCANCODE_KP_3},
    {VK_NUMPAD4, SDL_SCANCODE_KP_4},
    {VK_NUMPAD5, SDL_SCANCODE_KP_5},
    {VK_NUMPAD6, SDL_SCANCODE_KP_6},
    {VK_NUMPAD7, SDL_SCANCODE_KP_7},
    {VK_NUMPAD8, SDL_SCANCODE_KP_8},
    {VK_NUMPAD9, SDL_SCANCODE_KP_9},
    {VK_MULTIPLY, SDL_SCANCODE_KP_MULTIPLY},
    {VK_ADD, SDL_SCANCODE_KP_PLUS},
    {VK_SUBTRACT, SDL_SCANCODE_KP_MINUS},
    {VK_DECIMAL, SDL_SCANCODE_KP_COMMA},
    {VK_DIVIDE, SDL_SCANCODE_KP_DIVIDE},
    {VK_F1, SDL_SCANCODE_F1},
    {VK_F2, SDL_SCANCODE_F2},
    {VK_F3, SDL_SCANCODE_F3},
    {VK_F4, SDL_SCANCODE_F4},
    {VK_F5, SDL_SCANCODE_F5},
    {VK_F6, SDL_SCANCODE_F6},
    {VK_F7, SDL_SCANCODE_F7},
    {VK_F8, SDL_SCANCODE_F8},
    {VK_F9, SDL_SCANCODE_F9},
    {VK_F10, SDL_SCANCODE_F10},
    {VK_F11, SDL_SCANCODE_F11},
    {VK_F12, SDL_SCANCODE_F12},
    {VK_F13, SDL_SCANCODE_F13},
    {VK_F14, SDL_SCANCODE_F14},
    {VK_F15, SDL_SCANCODE_F15},
    {VK_F16, SDL_SCANCODE_F16},
    {VK_F17, SDL_SCANCODE_F17},
    {VK_F18, SDL_SCANCODE_F18},
    {VK_F19, SDL_SCANCODE_F19},
    {VK_F20, SDL_SCANCODE_F20},
    {VK_F21, SDL_SCANCODE_F21},
    {VK_F22, SDL_SCANCODE_F22},
    {VK_F23, SDL_SCANCODE_F23},
    {VK_F24, SDL_SCANCODE_F24},
    {VK_NUMLOCK, SDL_SCANCODE_NUMLOCKCLEAR},
    {VK_SCROLL, SDL_SCANCODE_SCROLLLOCK},
    {VK_LSHIFT, SDL_SCANCODE_LSHIFT},
    {VK_RSHIFT, SDL_SCANCODE_RSHIFT},
    {VK_LCONTROL, SDL_SCANCODE_LCTRL},
    {VK_RCONTROL, SDL_SCANCODE_RCTRL},
    {VK_LMENU, SDL_SCANCODE_MENU},
    {VK_RMENU, SDL_SCANCODE_MENU},
    {VK_OEM_1, SDL_SCANCODE_SEMICOLON},
    {VK_OEM_PLUS, SDL_SCANCODE_EQUALS},
    {VK_OEM_COMMA, SDL_SCANCODE_COMMA},
    {VK_OEM_MINUS, SDL_SCANCODE_MINUS},
    {VK_OEM_PERIOD, SDL_SCANCODE_PERIOD},
    {VK_OEM_2, SDL_SCANCODE_SLASH},
    {VK_OEM_3, SDL_SCANCODE_GRAVE},
    {VK_OEM_4, SDL_SCANCODE_LEFTBRACKET},
    {VK_OEM_5, SDL_SCANCODE_BACKSLASH},
    {VK_OEM_6, SDL_SCANCODE_RIGHTBRACKET},
    {VK_OEM_7, SDL_SCANCODE_APOSTROPHE},
    {VK_VOLUME_MUTE, SDL_SCANCODE_MUTE},
    {VK_VOLUME_DOWN, SDL_SCANCODE_VOLUMEDOWN},
    {VK_VOLUME_UP, SDL_SCANCODE_VOLUMEUP},
};

static const std::map<unsigned int, unsigned int> SDL_TO_VK_MAP{
    {SDL_SCANCODE_CANCEL, VK_CANCEL},
    {SDL_SCANCODE_BACKSPACE, VK_BACK},
    {SDL_SCANCODE_TAB, VK_TAB},
    {SDL_SCANCODE_CLEAR, VK_CLEAR},
    {SDL_SCANCODE_RETURN, VK_RETURN},
    {SDL_SCANCODE_LSHIFT, VK_SHIFT},
    {SDL_SCANCODE_LCTRL, VK_CONTROL},
    {SDL_SCANCODE_LALT, VK_MENU},
    {SDL_SCANCODE_PAUSE, VK_PAUSE},
    {SDL_SCANCODE_CAPSLOCK, VK_CAPITAL},
    {SDL_SCANCODE_ESCAPE, VK_ESCAPE},
    {SDL_SCANCODE_SPACE, VK_SPACE},
    {SDL_SCANCODE_PAGEUP, VK_PRIOR},
    {SDL_SCANCODE_PAGEDOWN, VK_NEXT},
    {SDL_SCANCODE_END, VK_END},
    {SDL_SCANCODE_HOME, VK_HOME},
    {SDL_SCANCODE_LEFT, VK_LEFT},
    {SDL_SCANCODE_UP, VK_UP},
    {SDL_SCANCODE_RIGHT, VK_RIGHT},
    {SDL_SCANCODE_DOWN, VK_DOWN},
    {SDL_SCANCODE_SELECT, VK_SELECT},
    {SDL_SCANCODE_EXECUTE, VK_EXECUTE},
    {SDL_SCANCODE_PRINTSCREEN, VK_SNAPSHOT},
    {SDL_SCANCODE_INSERT, VK_INSERT},
    {SDL_SCANCODE_DELETE, VK_DELETE},
    {SDL_SCANCODE_HELP, VK_HELP},
    {SDL_SCANCODE_0, '0'},
    {SDL_SCANCODE_1, '1'},
    {SDL_SCANCODE_2, '2'},
    {SDL_SCANCODE_3, '3'},
    {SDL_SCANCODE_4, '4'},
    {SDL_SCANCODE_5, '5'},
    {SDL_SCANCODE_6, '6'},
    {SDL_SCANCODE_7, '7'},
    {SDL_SCANCODE_8, '8'},
    {SDL_SCANCODE_9, '9'},
    {SDL_SCANCODE_A, 'A'},
    {SDL_SCANCODE_B, 'B'},
    {SDL_SCANCODE_C, 'C'},
    {SDL_SCANCODE_D, 'D'},
    {SDL_SCANCODE_E, 'E'},
    {SDL_SCANCODE_F, 'F'},
    {SDL_SCANCODE_G, 'G'},
    {SDL_SCANCODE_H, 'H'},
    {SDL_SCANCODE_I, 'I'},
    {SDL_SCANCODE_J, 'J'},
    {SDL_SCANCODE_K, 'K'},
    {SDL_SCANCODE_L, 'L'},
    {SDL_SCANCODE_M, 'M'},
    {SDL_SCANCODE_N, 'N'},
    {SDL_SCANCODE_O, 'O'},
    {SDL_SCANCODE_P, 'P'},
    {SDL_SCANCODE_Q, 'Q'},
    {SDL_SCANCODE_R, 'R'},
    {SDL_SCANCODE_S, 'S'},
    {SDL_SCANCODE_T, 'T'},
    {SDL_SCANCODE_U, 'U'},
    {SDL_SCANCODE_V, 'V'},
    {SDL_SCANCODE_W, 'W'},
    {SDL_SCANCODE_X, 'X'},
    {SDL_SCANCODE_Y, 'Y'},
    {SDL_SCANCODE_Z, 'Z'},
    {SDL_SCANCODE_LGUI, VK_LWIN},
    {SDL_SCANCODE_RGUI, VK_RWIN},
    {SDL_SCANCODE_MENU, VK_APPS},
    {SDL_SCANCODE_SLEEP, VK_SLEEP},
    {SDL_SCANCODE_KP_0, VK_NUMPAD0},
    {SDL_SCANCODE_KP_1, VK_NUMPAD1},
    {SDL_SCANCODE_KP_2, VK_NUMPAD2},
    {SDL_SCANCODE_KP_3, VK_NUMPAD3},
    {SDL_SCANCODE_KP_4, VK_NUMPAD4},
    {SDL_SCANCODE_KP_5, VK_NUMPAD5},
    {SDL_SCANCODE_KP_6, VK_NUMPAD6},
    {SDL_SCANCODE_KP_7, VK_NUMPAD7},
    {SDL_SCANCODE_KP_8, VK_NUMPAD8},
    {SDL_SCANCODE_KP_9, VK_NUMPAD9},
    {SDL_SCANCODE_KP_MULTIPLY, VK_MULTIPLY},
    {SDL_SCANCODE_KP_PLUS, VK_ADD},
    {SDL_SCANCODE_KP_MINUS, VK_SUBTRACT},
    {SDL_SCANCODE_KP_COMMA, VK_DECIMAL},
    {SDL_SCANCODE_KP_DIVIDE, VK_DIVIDE},
    {SDL_SCANCODE_F1, VK_F1},
    {SDL_SCANCODE_F2, VK_F2},
    {SDL_SCANCODE_F3, VK_F3},
    {SDL_SCANCODE_F4, VK_F4},
    {SDL_SCANCODE_F5, VK_F5},
    {SDL_SCANCODE_F6, VK_F6},
    {SDL_SCANCODE_F7, VK_F7},
    {SDL_SCANCODE_F8, VK_F8},
    {SDL_SCANCODE_F9, VK_F9},
    {SDL_SCANCODE_F10, VK_F10},
    {SDL_SCANCODE_F11, VK_F11},
    {SDL_SCANCODE_F12, VK_F12},
    {SDL_SCANCODE_F13, VK_F13},
    {SDL_SCANCODE_F14, VK_F14},
    {SDL_SCANCODE_F15, VK_F15},
    {SDL_SCANCODE_F16, VK_F16},
    {SDL_SCANCODE_F17, VK_F17},
    {SDL_SCANCODE_F18, VK_F18},
    {SDL_SCANCODE_F19, VK_F19},
    {SDL_SCANCODE_F20, VK_F20},
    {SDL_SCANCODE_F21, VK_F21},
    {SDL_SCANCODE_F22, VK_F22},
    {SDL_SCANCODE_F23, VK_F23},
    {SDL_SCANCODE_F24, VK_F24},
    {SDL_SCANCODE_NUMLOCKCLEAR, VK_NUMLOCK},
    {SDL_SCANCODE_SCROLLLOCK, VK_SCROLL},
    {SDL_SCANCODE_LSHIFT, VK_LSHIFT},
    {SDL_SCANCODE_RSHIFT, VK_RSHIFT},
    {SDL_SCANCODE_LCTRL, VK_LCONTROL},
    {SDL_SCANCODE_RCTRL, VK_RCONTROL},
    {SDL_SCANCODE_MENU, VK_LMENU},
    {SDL_SCANCODE_MENU, VK_RMENU},
    {SDL_SCANCODE_SEMICOLON, VK_OEM_1},
    {SDL_SCANCODE_EQUALS, VK_OEM_PLUS},
    {SDL_SCANCODE_COMMA, VK_OEM_COMMA},
    {SDL_SCANCODE_MINUS, VK_OEM_MINUS},
    {SDL_SCANCODE_PERIOD, VK_OEM_PERIOD},
    {SDL_SCANCODE_SLASH, VK_OEM_2},
    {SDL_SCANCODE_GRAVE, VK_OEM_3},
    {SDL_SCANCODE_LEFTBRACKET, VK_OEM_4},
    {SDL_SCANCODE_BACKSLASH, VK_OEM_5},
    {SDL_SCANCODE_RIGHTBRACKET, VK_OEM_6},
    {SDL_SCANCODE_APOSTROPHE, VK_OEM_7},
    {SDL_SCANCODE_MUTE, VK_VOLUME_MUTE},
    {SDL_SCANCODE_VOLUMEDOWN, VK_VOLUME_DOWN},
    {SDL_SCANCODE_VOLUMEUP, VK_VOLUME_UP},
};

#ifdef __APPLE__
// touches: the button is pressed while a finger is held longer than kTouchHoldTime, a tap clicks it for
// kTouchClickTime; the distances are in the window size
constexpr uint32_t kTouchHoldTime = 150;
constexpr uint32_t kTouchTapTime = 300;
constexpr uint32_t kTouchClickTime = 100;
constexpr float kTouchDragDistance = 0.01f;
constexpr float kPinchWheelScale = 20.f;
// the stick: its radius in the window height, the dead zone and the far push in the radius
constexpr float kMoveStickDeadZone = 0.3f;
constexpr float kMoveStickFar = 0.85f;
#endif

inline KeyboardKey sdlToKey(unsigned int code)
{
    auto it = SDL_TO_VK_MAP.find(code);
    if (it == SDL_TO_VK_MAP.end())
        return 0;
    return it->second;
}

inline unsigned int keyToSDL(KeyboardKey key)
{
    auto it = VK_TO_SDL_MAP.find(key);
    if (it == VK_TO_SDL_MAP.end())
        return 0;
    return it->second;
}
} // namespace

SDLInput::SDLInput()
{
    keyStates_ = SDL_GetKeyboardState(nullptr);
#ifdef __APPLE__
    // the touches are processed here (ProcessTouch), SDL would move the mouse by them relatively
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
#endif
#ifndef _WIN32
    // since SDL 2.0.18 breaks WINAPI mouse api
    SDL_SetRelativeMouseMode(SDL_TRUE);
#endif
    SDL_AddEventWatch(&SDLEventHandler, this);
    OpenController();
}

SDLInput::~SDLInput()
{
    SDL_DelEventWatch(&SDLEventHandler, this);
}

int SDLInput::Subscribe(const EventHandler &handler)
{
    int id = 1;
    if (!handlers_.empty())
        id = handlers_.rbegin()->first + 1;
    handlers_[id] = handler;
    return id;
}

void SDLInput::Unsubscribe(int id)
{
    auto it = handlers_.find(id);
    if (it != handlers_.end())
        handlers_.erase(it);
}

void SDLInput::ProcessEvent(const SDL_Event &event)
{
    InputEvent out;
    out.type = InputEvent::Unknown;

#ifdef __APPLE__
    if (event.type == SDL_FINGERDOWN || event.type == SDL_FINGERMOTION || event.type == SDL_FINGERUP)
    {
        ProcessTouch(event, out);
    }
    else
#endif
    // Mouse/keyboard
    if (event.type == SDL_MOUSEMOTION)
    {
#ifdef __APPLE__
        isPointerTouch_ = false;
#endif
        out.type = InputEvent::MouseMove;
        out.data = MousePos{event.motion.xrel, event.motion.yrel};
    }
    else if (event.type == SDL_MOUSEWHEEL)
    {
        out.type = InputEvent::MouseWheel;
        out.data = MousePos{event.wheel.x, event.wheel.y};
    }
    else if (event.type == SDL_KEYDOWN)
    {
        out.type = InputEvent::KeyboardKeyDown;
        out.data = sdlToKey(event.key.keysym.scancode);
    }
    else if (event.type == SDL_KEYUP)
    {
        out.type = InputEvent::KeyboardKeyUp;
        out.data = sdlToKey(event.key.keysym.scancode);
    }
    else if (event.type == SDL_TEXTINPUT)
    {
        out.type = InputEvent::KeyboardText;
        out.data = std::string(event.text.text);
    }
    else if (event.type == SDL_CONTROLLERDEVICEADDED)
    {
        // Try to open controller if it wasn't open already (or is the one disconnected without the removal event:
        // iPadOS while the game was in the background)
        if (!controller_ || !SDL_GameControllerGetAttached(controller_.get()))
            OpenController();
    }
    else if ((event.type == SDL_CONTROLLERDEVICEREMOVED) && (joyID_ == event.cdevice.which))
    {
        // Close controller if it was removed, another one connected takes its place
        controller_ = nullptr;
        joyID_ = -1;
        OpenController();
    }
#ifdef __APPLE__
    else if (event.type == SDL_APP_DIDENTERFOREGROUND && controller_ && !SDL_GameControllerGetAttached(controller_.get()))
    {
        OpenController();
    }
#endif
    else if ((event.type == SDL_CONTROLLERAXISMOTION) && (joyID_ == event.caxis.which))
    {
        ControllerAxisState state;
        state.axis = static_cast<ControllerAxis>(event.caxis.axis);
        state.value = event.caxis.value;

        out.type = InputEvent::ControllerAxis;
        out.data = state;
    }
    else if ((event.type == SDL_CONTROLLERBUTTONDOWN) && (joyID_ == event.cbutton.which))
    {
        out.type = InputEvent::ControllerButtonDown;
        out.data = static_cast<ControllerButton>(event.cbutton.button);
    }
    else if ((event.type == SDL_CONTROLLERBUTTONUP) && (joyID_ == event.cbutton.which))
    {
        out.type = InputEvent::ControllerButtonUp;
        out.data = static_cast<ControllerButton>(event.cbutton.button);
    }

    if (out.type != InputEvent::Unknown)
    {
        for (const auto &handler : handlers_)
            handler.second(out);
    }
}

bool SDLInput::KeyboardModState(const KeyboardKey &key) const
{
    if (key == VK_NUMLOCK)
        return SDL_GetModState() & KMOD_NUM;
    else if (key == VK_CAPITAL)
        return SDL_GetModState() & KMOD_CAPS;
    else if (key == VK_SCROLL)
        return SDL_GetModState() & KMOD_SCROLL;
    else
       return false;
}

bool SDLInput::KeyboardKeyState(const KeyboardKey &key) const
{
    return keyStates_[keyToSDL(key)] != 0;
}

bool SDLInput::KeyboardSDLKeyState(const SDL_Scancode &key) const
{
    return keyStates_[key] != 0;
}

bool SDLInput::ControllerButtonState(const ControllerButton &button) const
{
    if (!controller_)
        return false;

    return SDL_GameControllerGetButton(controller_.get(), static_cast<SDL_GameControllerButton>(button)) == SDL_PRESSED;
}

int SDLInput::ControllerAxisValue(const ControllerAxis &axis) const
{
    if (!controller_)
        return 0;

    return SDL_GameControllerGetAxis(controller_.get(), static_cast<SDL_GameControllerAxis>(axis));
}

bool SDLInput::MouseKeyState(const MouseKey &key) const
{
#ifdef __APPLE__
    const auto now = SDL_GetTicks();
    if (key == MouseKey::Left && touchPress_ &&
        (now < clickReleaseTime_ ||
         touch_.active && !touch_.swiped && !touch_.cancelled && now - touch_.downTime >= kTouchHoldTime))
        return true;
    // the menus: the finger drags the sliders and the scroll bars
    if (key == MouseKey::Left && touchDrag_ && touch_.active && touch_.dragging && !touch_.cancelled)
        return true;
    if (key == MouseKey::Right && now < rightClickReleaseTime_)
        return true;
#endif
    uint32_t btn = SDL_GetMouseState(nullptr, nullptr);
    if (key == MouseKey::Left)
        return btn & SDL_BUTTON_LMASK ? true : false;
    else if (key == MouseKey::Right)
        return btn & SDL_BUTTON_RMASK ? true : false;
    else if (key == MouseKey::Middle)
        return btn & SDL_BUTTON_MMASK ? true : false;
    else if (key == MouseKey::Special1)
        return btn & SDL_BUTTON_X1MASK ? true : false;
    else if (key == MouseKey::Special2)
        return btn & SDL_BUTTON_X2MASK ? true : false;

    return false;
}

uint32_t SDLInput::GetWheelFactor() const
{
    // TODO: use platform specific values
    return WHEEL_DELTA;
}

int SDLInput::SDLEventHandler(void *userdata, SDL_Event *evt)
{
    auto in = static_cast<SDLInput *>(userdata);
    in->ProcessEvent(*evt);
    return 0;
}

bool SDLInput::PointerPosition(float &x, float &y) const
{
#ifdef __APPLE__
    if (isPointerTouch_)
    {
        x = touch_.x;
        y = touch_.y;
        return true;
    }
#endif
    return false;
}

bool SDLInput::TouchStick(float &startX, float &startY, float &x, float &y) const
{
#ifdef __APPLE__
    if (touch_.active && touch_.dragging && pinchDistance_ < 0.f && !touchDrag_)
    {
        startX = touch_.startX;
        startY = touch_.startY;
        x = touch_.x;
        y = touch_.y;
        return true;
    }
#endif
    return false;
}

void SDLInput::SetTouchArea(float left, float top, float right, float bottom)
{
#ifdef __APPLE__
    if (right > left && bottom > top)
        touchArea_ = {left, top, right, bottom};
#endif
}

bool SDLInput::TouchPoint(float &startX, float &startY, float &x, float &y) const
{
#ifdef __APPLE__
    if (touch_.active)
    {
        startX = touch_.startX;
        startY = touch_.startY;
        x = touch_.x;
        y = touch_.y;
        return true;
    }
#endif
    return false;
}

void SDLInput::CancelTouch()
{
#ifdef __APPLE__
    touch_.cancelled = true;
#endif
}

uint32_t SDLInput::LastTouchTime() const
{
#ifdef __APPLE__
    return lastTouchTime_;
#else
    return 0;
#endif
}

void SDLInput::SetTouchDrag(bool drag)
{
#ifdef __APPLE__
    touchDrag_ = drag;
#endif
}

void SDLInput::SetTouchStickRadius(float radius)
{
#ifdef __APPLE__
    if (radius > 0.f)
        moveStickRadius_ = radius;
#endif
}

void SDLInput::SetTouchPress(bool press)
{
#ifdef __APPLE__
    touchPress_ = press;
#endif
}

uint32_t SDLInput::TouchTaps(float &x, float &y) const
{
#ifdef __APPLE__
    x = tapX_;
    y = tapY_;
    return taps_;
#else
    return 0;
#endif
}

bool SDLInput::IsTouchScreen() const
{
#ifdef __APPLE__
    for (int i = 0; i < SDL_GetNumTouchDevices(); i++)
    {
        if (SDL_GetTouchDeviceType(SDL_GetTouchDevice(i)) == SDL_TOUCH_DEVICE_DIRECT)
            return true;
    }
#endif
    return false;
}

void SDLInput::SetTouchButtons(const std::vector<TouchButton> &buttons)
{
#ifdef __APPLE__
    buttons_ = buttons;
    if (isMoveStick_ && std::none_of(buttons_.begin(), buttons_.end(), [](const auto &b) { return b.stick; }))
        isMoveStick_ = false;
    // a finger held since before the stick was there (a loading) becomes the stick
    const auto inPointerArea = [this](float x, float y) {
        return std::any_of(buttons_.begin(), buttons_.end(), [x, y](const auto &b) {
            return b.pointer && x >= b.left && x <= b.right && y >= b.top && y <= b.bottom;
        });
    };
    if (!isMoveStick_ && touch_.active && !touch_.cancelled && pinchDistance_ < 0.f &&
        !inPointerArea(touch_.startX, touch_.startY))
    {
        for (const auto &button : buttons_)
        {
            if (button.stick && touch_.startX >= button.left && touch_.startX <= button.right &&
                touch_.startY >= button.top && touch_.startY <= button.bottom)
            {
                touch_.active = false;
                StartMoveStick(touch_.finger, touch_.x, touch_.y, button);
                break;
            }
        }
    }
#endif
}

bool SDLInput::MoveStickArea(float &centerX, float &centerY, float &radiusX, float &radiusY) const
{
#ifdef __APPLE__
    if (isMoveStick_)
    {
        centerX = moveCenterX_;
        centerY = moveCenterY_;
        radiusX = moveRadiusX_;
        radiusY = moveRadiusY_;
        return true;
    }
#endif
    return false;
}

bool SDLInput::MoveStick(float &x, float &y) const
{
#ifdef __APPLE__
    if (isMoveStick_)
    {
        x = moveX_;
        y = moveY_;
        return true;
    }
#endif
    return false;
}

const std::vector<TouchButton> &SDLInput::GetTouchButtons() const
{
#ifdef __APPLE__
    return buttons_;
#else
    return Input::GetTouchButtons();
#endif
}

bool SDLInput::TouchButtonState(const KeyboardKey &key) const
{
#ifdef __APPLE__
    if (isMoveStick_ && key != 0)
    {
        if (key == moveStick_.key && moveY_ < -kMoveStickDeadZone || key == moveStick_.backKey && moveY_ > kMoveStickDeadZone ||
            key == moveStick_.farKey && moveY_ < -kMoveStickFar ||
            key == moveStick_.leftKey && moveX_ < -kMoveStickDeadZone ||
            key == moveStick_.rightKey && moveX_ > kMoveStickDeadZone)
            return true;
    }
    for (const auto &[finger, fingerKey] : buttonFingers_)
    {
        if (fingerKey == key)
            return true;
    }
    return std::find(toggledKeys_.begin(), toggledKeys_.end(), key) != toggledKeys_.end();
#else
    return false;
#endif
}

#ifdef __APPLE__
void SDLInput::ProcessTouch(const SDL_Event &event, InputEvent &out)
{
    // in the content area of the window (where the frame is drawn)
    auto finger = event.tfinger;
    finger.x = (finger.x - touchArea_[0]) / (touchArea_[2] - touchArea_[0]);
    finger.y = (finger.y - touchArea_[1]) / (touchArea_[3] - touchArea_[1]);
    // the trackpads of Mac are mice
    if (SDL_GetTouchDeviceType(finger.touchId) != SDL_TOUCH_DEVICE_DIRECT)
        return;
    const auto now = SDL_GetTicks();
    int width = 0, height = 0;
    if (auto *window = SDL_GetWindowFromID(finger.windowID))
        SDL_GetWindowSize(window, &width, &height);

    if (width > 0)
        heightToWidth_ = height * (touchArea_[3] - touchArea_[1]) / (width * (touchArea_[2] - touchArea_[0]));

    if (event.type == SDL_FINGERDOWN)
    {
        lastTouchTime_ = now;
        // the on-screen buttons and the areas of the pointer, then the stick under them
        const TouchButton *touched = nullptr;
        for (const auto stick : {false, true})
        {
            for (const auto &button : buttons_)
            {
                if (button.stick == stick && finger.x >= button.left && finger.x <= button.right &&
                    finger.y >= button.top && finger.y <= button.bottom)
                {
                    touched = &button;
                    break;
                }
            }
            if (touched)
                break;
        }
        if (touched && !touched->pointer)
        {
            const auto &button = *touched;
            if (button.stick)
                StartMoveStick(finger.fingerId, finger.x, finger.y, button);
            else if (!button.toggle)
                buttonFingers_[finger.fingerId] = button.key;
            else if (const auto it = std::find(toggledKeys_.begin(), toggledKeys_.end(), button.key);
                     it != toggledKeys_.end())
                toggledKeys_.erase(it);
            else
                toggledKeys_.push_back(button.key);
            return;
        }
        if (!touch_.active)
        {
            touch_ = Touch{finger.fingerId, finger.x, finger.y, finger.x, finger.y, now, true};
            isPointerTouch_ = true;
        }
        else if (pinchDistance_ < 0.f)
        {
            // the second finger: pinch or a right click
            pinchFinger_ = finger.fingerId;
            pinchX_ = finger.x;
            pinchY_ = finger.y;
            pinchDistance_ = std::hypot(touch_.x - pinchX_, touch_.y - pinchY_);
            pinchRest_ = pinchMoved_ = 0.f;
            pinchDownTime_ = now;
            touch_.cancelled = true;
        }
        return;
    }

    if (isMoveStick_ && finger.fingerId == moveFinger_)
    {
        if (event.type == SDL_FINGERUP)
            isMoveStick_ = false;
        else
            UpdateMoveStick(finger.x, finger.y);
        return;
    }
    if (buttonFingers_.count(finger.fingerId) != 0)
    {
        if (event.type == SDL_FINGERUP)
            buttonFingers_.erase(finger.fingerId);
        return;
    }

    if (event.type == SDL_FINGERMOTION)
    {
        if (touch_.active && finger.fingerId == touch_.finger)
        {
            touch_.x = finger.x;
            touch_.y = finger.y;
            if (!touch_.dragging && std::hypot(finger.x - touch_.startX, finger.y - touch_.startY) > kTouchDragDistance)
            {
                touch_.dragging = true;
                touch_.swiped = now - touch_.downTime < kTouchHoldTime;
            }
        }
        else if (pinchDistance_ >= 0.f && finger.fingerId == pinchFinger_)
        {
            pinchX_ = finger.x;
            pinchY_ = finger.y;
        }
        if (pinchDistance_ >= 0.f && touch_.active)
        {
            const auto distance = std::hypot(touch_.x - pinchX_, touch_.y - pinchY_);
            pinchMoved_ += std::abs(distance - pinchDistance_);
            pinchRest_ += (distance - pinchDistance_) * kPinchWheelScale;
            pinchDistance_ = distance;
            const auto wheel = static_cast<int>(pinchRest_);
            if (wheel != 0)
            {
                pinchRest_ -= static_cast<float>(wheel);
                out.type = InputEvent::MouseWheel;
                out.data = MousePos{0, wheel};
            }
        }
        return;
    }

    // SDL_FINGERUP
    if (pinchDistance_ >= 0.f && (finger.fingerId == pinchFinger_ || finger.fingerId == touch_.finger))
    {
        // a short touch of two fingers is a right click
        if (pinchMoved_ < kTouchDragDistance && now - pinchDownTime_ < kTouchTapTime)
            rightClickReleaseTime_ = now + kTouchClickTime;
        pinchDistance_ = -1.f;
    }
    if (touch_.active && finger.fingerId == touch_.finger)
    {
        touch_.active = false;
        // a tap: the button was not pressed while the finger was held
        if (!touch_.dragging && !touch_.cancelled && now - touch_.downTime < kTouchHoldTime)
        {
            clickReleaseTime_ = now + kTouchClickTime;
            taps_++;
            tapX_ = touch_.x;
            tapY_ = touch_.y;
        }
    }
}
#endif

#ifdef __APPLE__
void SDLInput::StartMoveStick(SDL_FingerID finger, float x, float y, const TouchButton &button)
{
    // the stick appears under the finger
    isMoveStick_ = true;
    moveFinger_ = finger;
    moveStick_ = button;
    moveCenterX_ = x;
    moveCenterY_ = y;
    moveRadiusY_ = moveStickRadius_;
    moveRadiusX_ = moveStickRadius_ * heightToWidth_;
    UpdateMoveStick(x, y);
}

void SDLInput::UpdateMoveStick(float x, float y)
{
    // the offset from where the finger touched in the radius, within the circle
    moveX_ = (x - moveCenterX_) / moveRadiusX_;
    moveY_ = (y - moveCenterY_) / moveRadiusY_;
    if (const auto length = std::hypot(moveX_, moveY_); length > 1.f)
    {
        moveX_ /= length;
        moveY_ /= length;
    }
}
#endif

void SDLInput::OpenController()
{
    // the first gamepad (the other joysticks are not: the accelerometer of the iPad)
    int index = 0;
    while (index < SDL_NumJoysticks() && !SDL_IsGameController(index))
        index++;
    controller_ = std::unique_ptr<SDL_GameController, std::function<void(SDL_GameController *)>>(
        index < SDL_NumJoysticks() ? SDL_GameControllerOpen(index) : nullptr, &SDL_GameControllerClose);
    if (controller_)
        fprintf(stderr, "Gamepad: %s\n", SDL_GameControllerName(controller_.get()));

    if (controller_)
        joyID_ = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller_.get()));
    else
        joyID_ = -1;
}

std::shared_ptr<Input> Input::Create()
{
    return std::make_shared<SDLInput>();
}
} // namespace storm
