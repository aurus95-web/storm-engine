#pragma once

#include <functional>
#include <SDL2/SDL_scancode.h>
#include <variant>
#include <string>
#include <memory>
#include <vector>

namespace storm
{
// TODO: Make own type for keycodes and use it everywhere (reuse SDL scancodes?)
//! Keyboard key, using virtual key codes (VK_* from WinUser.h) for now
using KeyboardKey = uint32_t;

//! Mouse relative position
struct MousePos
{
    int x = 0;
    int y = 0;
};

//! Mouse keys
enum class MouseKey
{
    Left,
    Right,
    Middle,
    Special1,
    Special2
};

//! Controller axes
enum class ControllerAxis
{
    LeftX = 0,
    //!< Left stick horizontal axis
    LeftY,
    //!< Left stick vertical axis
    RightX,
    //!< Right stick horizontal axis
    RightY,
    //!< Right stick vertical axis
    TriggerLeft,
    //!< Left trigger axis
    TriggerRight //!< Right trigger axis
};

//! Controller buttons
enum class ControllerButton
{
    A,
    B,
    X,
    Y,
    Back,
    Guide,
    Start,
    LeftStick,
    RightStick,
    LeftBumper,
    RightBumper,
    Up,
    Down,
    Left,
    Right
};

//! Current state of controller axis
struct ControllerAxisState
{
    ControllerAxis axis; //!< Axis
    int value;           //!< Current value, from -32768 to 32767 (0 to 32767 for triggers)
};

//! Input event
struct InputEvent
{
    enum Type
    {
        Unknown,
        //!< Invalid event

        KeyboardKeyDown,
        //!< Keyboard key down, data contains KeyboardKey
        KeyboardKeyUp,
        //!< Keyboard key up, data contains KeyboardKey
        KeyboardText,
        //!< Some text was entered, data contains std::string with text in utf-8 encoding

        MouseMove,
        //!< Mouse moved, data contains MousePos with relative mouse movement
        MouseKeyDown,
        //!< Mouse key down, data contains MouseKey
        MouseKeyUp,
        //!< Mouse key up, data contains MouseKey
        MouseWheel,
        //!< Mouse wheel, data contains MousePos with relative wheel movement

        ControllerAxis,
        //!< Controller axis value changed, data contains ControllerAxisState
        ControllerButtonDown,
        //!< Controller button pressed, data contains ControllerButton
        ControllerButtonUp //!< Controller button released, data contains ControllerButton
    };

    //! Event type
    Type type;
    //! Data associated with event
    std::variant<std::string, KeyboardKey, MousePos, MouseKey, ControllerAxisState, ControllerButton> data;
};

//! On-screen button of a touch screen (the window is from 0 to 1): the key is pressed while a finger is on it,
//! a toggle is switched by a touch
struct TouchButton
{
    float left, top, right, bottom;
    KeyboardKey key;
    bool toggle = false;
    //! a stick: key while it is pushed up, backKey down, farKey up to the edge; its horizontal position is MoveStick();
    //! the button is the area where it appears under the finger
    bool stick = false;
    KeyboardKey backKey = 0;
    KeyboardKey farKey = 0;
    KeyboardKey leftKey = 0;  //!< while the stick is pushed to the left
    KeyboardKey rightKey = 0; //!< to the right
    //! an area of the pointer touches (the commands drawn by the game) over a stick: no key, the stick is not there
    bool pointer = false;
};

//! Abstract input manager that handles all input events
class Input
{
  public:
    using EventHandler = std::function<void(const InputEvent &)>;

    virtual ~Input(){};

    //! Subscribe for events
    //! \param handler event callback
    //! \return subscription id, which should be passed to Unsubscribe()
    virtual int Subscribe(const EventHandler &handler) = 0;

    //! Unsubscribe from events
    //! \param id handler returned by Subscribe()
    virtual void Unsubscribe(int id) = 0;

    //! Current keyboard key state
    //! \return true if key is Num Lock, Caps Lock or Scroll Lock and key is on
    virtual bool KeyboardModState(const KeyboardKey &key) const = 0;
    //! \return true if key is pressed
    virtual bool KeyboardKeyState(const KeyboardKey &key) const = 0;
    virtual bool KeyboardSDLKeyState(const SDL_Scancode &key) const = 0;

    //! Current mouse key state
    //! \return true if key is pressed
    virtual bool MouseKeyState(const MouseKey &key) const = 0;

    //! Platform-specific multiplier to use for scaling
    //! \return unsigned integer representing specific convenient value
    virtual uint32_t GetWheelFactor() const = 0;

    //! Current controller button state
    //! \return true if button is pressed
    virtual bool ControllerButtonState(const ControllerButton &button) const = 0;

    //! Current controller axis value
    //! \return current axis value, from -32768 to 32767 (0 to 32767 for triggers)
    virtual int ControllerAxisValue(const ControllerAxis &axis) const = 0;

    //! Position of the touch that controls the cursor (touch screens)
    //! \return true and the position in the window, from 0 to 1, while the last pointer input was a touch
    virtual bool PointerPosition(float &x, float &y) const
    {
        return false;
    }

    //! A finger moved away from where it touched the screen works as a stick of a gamepad (the camera)
    //! \return true and the touched and the current positions of the finger (from 0 to 1) while it is moved
    virtual bool TouchStick(float &startX, float &startY, float &x, float &y) const
    {
        return false;
    }

    //! \return true and the position of the finger on the stick, from -1 to 1, while it is touched
    virtual bool MoveStick(float &x, float &y) const
    {
        return false;
    }
    //! \return true and where the stick appeared (from 0 to 1) and its radius while it is touched
    virtual bool MoveStickArea(float &centerX, float &centerY, float &radiusX, float &radiusY) const
    {
        return false;
    }

    //! \return true if the window is on a touch screen
    virtual bool IsTouchScreen() const
    {
        return false;
    }

    //! Whether the touches press the left mouse button (a tap clicks, a held finger walks on land; at sea and on the
    //! map that button zooms the camera)
    virtual void SetTouchPress(bool press)
    {
    }

    //! \return whether a finger (not on a button) touches the screen, where it touched and where it is (from 0 to 1)
    virtual bool TouchPoint(float &startX, float &startY, float &x, float &y) const
    {
        return false;
    }

    //! The finger that touches the screen neither clicks nor drags (it moves a button of the touch settings)
    virtual void CancelTouch()
    {
    }

    //! \return the time (SDL_GetTicks) when a finger last touched the screen, 0 if never
    virtual uint32_t LastTouchTime() const
    {
        return 0;
    }

    //! Whether a moved finger drags with the left mouse button (the menus: the sliders) instead of the camera stick
    virtual void SetTouchDrag(bool drag)
    {
    }

    //! The radius of the walking stick in the window heights
    virtual void SetTouchStickRadius(float radius)
    {
    }

    //! The part of the window (from 0 to 1) where the frame is drawn: the touches are in it
    virtual void SetTouchArea(float left, float top, float right, float bottom)
    {
    }

    //! \return the number of the taps on the screen since the start and the position of the last one (from 0 to 1)
    virtual uint32_t TouchTaps(float &x, float &y) const
    {
        return 0;
    }

    //! On-screen buttons to press by touching them
    virtual void SetTouchButtons(const std::vector<TouchButton> &buttons)
    {
    }
    virtual const std::vector<TouchButton> &GetTouchButtons() const
    {
        static const std::vector<TouchButton> none;
        return none;
    }

    //! \return true if the key is pressed by an on-screen button
    virtual bool TouchButtonState(const KeyboardKey &key) const
    {
        return false;
    }

    static std::shared_ptr<Input> Create();
};
} // namespace storm
