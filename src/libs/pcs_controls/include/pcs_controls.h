#pragma once

#include "control_tree.h"
#include "key_buffer.h"

#include <shared/controls.h>

#include <array>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace storm
{
struct InputEvent;
class Input;
struct TouchButton;
} // namespace storm

struct SYSTEM_CONTROL_ELEMENT
{
    uint32_t update_frame;
    CONTROL_STATE state;
};

#define CONTROL_ELEMENTS_NUM 260

class PCS_CONTROLS : public CONTROLS
{
    bool m_bLockAll;
    bool updateCursor_ = true;

    float fMouseSensivityX;
    // engine.ini [controls] mouse_move_scale: multiplier of the mouse motion (the camera uses the raw motion)
    float fMouseMoveScale = 1.0f;
    float fMouseMoveRestX = 0.0f;
    float fMouseMoveRestY = 0.0f;
    float fMouseSensivityY;

    int32_t nSystemControlsNum;
    std::vector<USER_CONTROL> pUserControls;
    int32_t nControlsNum;
    uint32_t nFrameCounter;
    int32_t nLastControlTime;

    int32_t nMouseWheel;
    int32_t nMouseDx, nMouseDy;

    SYSTEM_CONTROL_ELEMENT ControlsTab[CONTROL_ELEMENTS_NUM];

    ControlKeyBuffer m_KeyBuffer;

    ControlTree m_ControlTree;

    std::shared_ptr<storm::Input> input_;
    int inputHandlerID_ = 0;

    // touch screens (Apple): engine.ini [controls] touch_buttons = 1 shows the on-screen buttons without a touch screen
    // too; the taps and the places of the icons of the game the taps are for
    bool m_bTouchButtons = false;
    bool m_bTouchTap = false; // a tap in this frame
    std::vector<std::array<float, 4>> m_aTouchPointerAreas; // added in the last frame: left, top, right, bottom
    float m_fTouchTapX = 0.f, m_fTouchTapY = 0.f;
    bool m_bGamepadActive = false; // used after the last touch: the touch buttons are hidden

#ifdef __APPLE__
    // touch screens: the on-screen buttons of the keys the current controls group needs
    int32_t m_nTouchFont = -1;
    float fTouchStickRestX = 0.f, fTouchStickRestY = 0.f;
    bool m_bTouchAtSea = false;
    uint32_t m_nTouchTaps = 0; // the taps counted by the input
    bool m_bStickTurns = false; // the walking stick turns the character, else its keys turn
    // the gamepad: the keys of the controls that its buttons and its left stick press in the current group of the
    // controls (storm::GetGamepadControl), as the touch buttons do
    std::vector<int32_t> m_aGamepadKeys;
    uint32_t m_nGamepadTime = 0;   // when it was used last (SDL_GetTicks)
    std::string m_sGamepadGroup;   // the group of the controls of the last frame
    uint32_t m_nGamepadBlocked = 0; // the buttons held when the group changed (bits): nothing until released
    uint32_t m_nGamepadDown = 0;    // the buttons pressed in the last frame (bits, without the stick)
    float m_fGamepadStickX = 0.f;  // the left stick from -1 to 1
    float fGamepadRestX = 0.f, fGamepadRestY = 0.f;
    std::vector<std::string> m_aTouchLabels;
    // the buttons drawn over the menu of the touch settings (storm::IsTouchPreview), the input does not have them
    std::vector<storm::TouchButton> m_aTouchPreview;
    bool m_bTouchPreview = false;
    // the touch settings: the button moved by the finger (its name in the layout) and its center from the finger
    std::string m_sTouchDragButton;
    bool m_bTouchDragChecked = false;
    float m_fTouchDragX = 0.f, m_fTouchDragY = 0.f;
    float m_fTouchDragMoved = 0.f;   // how far the finger moved it
    uint32_t m_nTouchEscapeTime = 0; // the Esc of the touch settings is pressed until this time (SDL_GetTicks)
    // the pictures of the game on the buttons: a grid of cols x rows (and the selected ones in the right half)
    struct TouchIcon
    {
        std::string texture;
        int32_t cols = 1, rows = 1, index = -1;
        bool selectedHalf = true;
        bool selected = false;
    };
    std::vector<TouchIcon> m_aTouchIcons;
    std::map<std::string, int32_t> m_TouchTextures;
    int32_t m_nTouchCharge = -1; // the current charge at sea
    void UpdateTouchButtons();
    void DrawOverlay() override;
#endif

  public:
    PCS_CONTROLS();
    ~PCS_CONTROLS() override;
    bool Init() override;

    void Update(uint32_t DeltaTime) override;
    int32_t GetSystemControlsNum() override;
    bool GetSystemControlDesc(int32_t code, SYSTEM_CONTROL_DESC &_control_desc_struct) override;

    void Release();
    void AddSystemControl(SYSTEM_CONTROL_DESC &_control_desc_struct);

    int32_t CreateControl(const char *control_name) override;
    int32_t GetControlsNum() override;
    bool GetControlDesc(int32_t code, USER_CONTROL &_user_desc_struct) override;
    bool SetControlFlags(int32_t code, uint32_t flags) override;

    int32_t GetDevicesNum() override;
    bool GetDeviceDesc(int32_t code, DEVICE_DESC &_device_desc) override;

    int32_t AddControlTreeNode(int32_t nParent, const char *pcBaseControl, const char *pcOutControl, float fTimeOut) override;

    void MapControl(int32_t control_code, int32_t system_control_code) override;
    bool GetControlState(int32_t control_code, CONTROL_STATE &_state_struct) override;
    bool GetControlState(const char *control_name, CONTROL_STATE &_state_struct) override;
    bool SetControlState(const char *control_name, CONTROL_STATE &_state_struct) override;
    bool SetControlState(int32_t control_code, CONTROL_STATE &_state_struct) override;
    void AppState(bool state) override;
    int32_t LastControlTime() override;
    void SetControlTreshold(int32_t control_code, float thval) override;
    void LockControl(const char *control_name, bool mode) override;

    void SetMouseSensivityX(float) override;
    void SetMouseSensivityY(float) override;
    bool GetPointerPosition(float &x, float &y) override;
    bool IsTouchScreen() override;
    bool IsGamepadActive() override
    {
        return m_bGamepadActive;
    }
    bool GetTouchTap(float &x, float &y) override;
    void AddTouchPointerArea(float left, float top, float right, float bottom) override;

    short GetAsyncKeyState(int vk) override;
    short GetKeyState(int vk) override;
    short GetDebugAsyncKeyState(int vk) override;
    short GetDebugKeyState(int vk) override;
    bool m_bIsOffDebugKeys;

    bool IsKeyPressed(int vk) override;

    void HandleEvent(const storm::InputEvent &evt);

    // Get the keystroke buffer per frame (taking into account the language)
    int32_t GetKeyBufferLength() override;
    const KeyDescr *GetKeyBuffer() override;
    void ClearKeyBuffer() override;
};
