#pragma once

#include <input.hpp>
#include <SDL2/SDL.h>
#include <array>
#include <map>

#include "platform/platform.hpp"

namespace storm
{
class SDLInput : public Input
{
  public:
    SDLInput();
    ~SDLInput() override;

    int Subscribe(const EventHandler &handler) override;
    void Unsubscribe(int id) override;

    bool KeyboardModState(const KeyboardKey &key) const override;
    bool KeyboardKeyState(const KeyboardKey &key) const override;
    bool KeyboardSDLKeyState(const SDL_Scancode &key) const override;
    bool MouseKeyState(const MouseKey &key) const override;

    uint32_t GetWheelFactor() const override;

    bool ControllerButtonState(const ControllerButton &button) const override;
    int ControllerAxisValue(const ControllerAxis &axis) const override;
    bool PointerPosition(float &x, float &y) const override;
    bool TouchStick(float &startX, float &startY, float &x, float &y) const override;
    bool MoveStick(float &x, float &y) const override;
    bool MoveStickArea(float &centerX, float &centerY, float &radiusX, float &radiusY) const override;
    bool IsTouchScreen() const override;
    void SetTouchPress(bool press) override;
    void SetTouchArea(float left, float top, float right, float bottom) override;
    void SetTouchStickRadius(float radius) override;
    void SetTouchDrag(bool drag) override;
    uint32_t LastTouchTime() const override;
    bool TouchPoint(float &startX, float &startY, float &x, float &y) const override;
    void CancelTouch() override;
    uint32_t TouchTaps(float &x, float &y) const override;
    void SetTouchButtons(const std::vector<TouchButton> &buttons) override;
    const std::vector<TouchButton> &GetTouchButtons() const override;
    bool TouchButtonState(const KeyboardKey &key) const override;

  private:
    static int SDLCALL SDLEventHandler(void *userdata, SDL_Event *evt);
    void ProcessEvent(const SDL_Event &event);
    void OpenController();
#ifdef __APPLE__
    // touch screen: a tap is a click where the finger is, a long touch is a right click, dragging moves the mouse,
    // pinching turns the wheel
    void ProcessTouch(const SDL_Event &event, InputEvent &out);
    struct Touch
    {
        SDL_FingerID finger = 0;
        float x = 0.f, y = 0.f;
        float startX = 0.f, startY = 0.f;
        uint32_t downTime = 0;
        bool active = false;
        bool dragging = false;
        bool swiped = false;    // moved before it was held: the mouse moves, its button is not pressed
        bool cancelled = false; // a second finger: no click
    } touch_;
    SDL_FingerID pinchFinger_ = 0;
    float pinchX_ = 0.f, pinchY_ = 0.f;
    float pinchDistance_ = -1.f;
    float pinchRest_ = 0.f;
    float pinchMoved_ = 0.f;
    uint32_t pinchDownTime_ = 0;
    bool isPointerTouch_ = false;
    uint32_t clickReleaseTime_ = 0;
    uint32_t rightClickReleaseTime_ = 0;
    std::vector<TouchButton> buttons_;
    std::map<SDL_FingerID, KeyboardKey> buttonFingers_; // the fingers on the buttons
    std::vector<KeyboardKey> toggledKeys_;
    // the finger on the stick button
    bool isMoveStick_ = false;
    SDL_FingerID moveFinger_ = 0;
    TouchButton moveStick_{};
    float moveX_ = 0.f, moveY_ = 0.f;
    float moveCenterX_ = 0.f, moveCenterY_ = 0.f;
    float moveRadiusX_ = 0.f, moveRadiusY_ = 0.f;
    float heightToWidth_ = 1.f; // of the window
    bool touchPress_ = true;
    float moveStickRadius_ = 0.085f; // in the window heights
    bool touchDrag_ = false;
    uint32_t lastTouchTime_ = 0;         // a moved finger drags with the left button (the menus)
    std::array<float, 4> touchArea_{0.f, 0.f, 1.f, 1.f}; // left, top, right, bottom in the window
    uint32_t taps_ = 0;
    float tapX_ = 0.f, tapY_ = 0.f;
    void StartMoveStick(SDL_FingerID finger, float x, float y, const TouchButton &button);
    void UpdateMoveStick(float x, float y);
#endif

private:
    std::map<int, EventHandler> handlers_;
    const uint8_t *keyStates_;
    std::unique_ptr<SDL_GameController, std::function<void(SDL_GameController *)>> controller_;
    SDL_JoystickID joyID_;
};
} // namespace storm
