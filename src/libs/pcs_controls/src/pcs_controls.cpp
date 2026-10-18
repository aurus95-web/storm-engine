#include "pcs_controls.h"

#include "core.h"
#include "string_compare.hpp"
#include "storm/port_options.hpp"

#include "file_service.h"

#include <input.hpp>
#include <SDL2/SDL_timer.h>
#ifdef __APPLE__
#include "dx9render.h"
#endif

CREATE_SERVICE(PCS_CONTROLS)

using namespace storm;

PCS_CONTROLS::PCS_CONTROLS()
{
    m_bLockAll = false;
    m_bIsOffDebugKeys = false;

    fMouseSensivityX = 1.0f;
    fMouseSensivityY = 1.0f;

    nFrameCounter = 0;

    nSystemControlsNum = 0;

    nControlsNum = 0;

    nLastControlTime = 0;

    nMouseWheel = 0;
    memset(&ControlsTab[0], 0, sizeof(ControlsTab));

    auto pIni = fio->OpenIniFile(core.EngineIniFileName());
    if (pIni)
    {
        m_bIsOffDebugKeys = pIni->GetInt("controls", "ondebugkeys", 0) == 0;
#ifdef __APPLE__
        // macOS and iPadOS deliver the mouse and trackpad motion in points: twice slower than on Windows
        constexpr float defaultMouseMoveScale = 2.0f;
#else
        constexpr float defaultMouseMoveScale = 1.0f;
#endif
        fMouseMoveScale = pIni->GetFloat("controls", "mouse_move_scale", defaultMouseMoveScale);
        m_bTouchButtons = pIni->GetInt("controls", "touch_buttons", 0) != 0;
    }

    input_ = Input::Create();
    inputHandlerID_ = input_->Subscribe([this](const InputEvent &evt) { HandleEvent(evt); });

    // RECT r;
    // GetWindowRect(core.GetAppHWND(),&r);
    // ClipCursor(&r);
}

PCS_CONTROLS::~PCS_CONTROLS()
{
    input_->Unsubscribe(inputHandlerID_);
    Release();
    // ClipCursor(0);
}

bool PCS_CONTROLS::Init()
{
    return true;
}

void PCS_CONTROLS::AppState(bool state)
{
    updateCursor_ = state;
    if (state)
    {
        // RECT r;
        // GetWindowRect(core.GetAppHWND(),&r);
        // ClipCursor(&r);
    }
    else
    {
        // ClipCursor(0);
    }
}

void PCS_CONTROLS::Release()
{
    int32_t n;
    for (n = 0; n < nControlsNum; n++)
    {
        delete pUserControls[n].name;
    }

    nControlsNum = 0;

    nSystemControlsNum = 0;
}

int32_t PCS_CONTROLS::GetSystemControlsNum()
{
    return nSystemControlsNum;
}

bool PCS_CONTROLS::GetSystemControlDesc(int32_t code, SYSTEM_CONTROL_DESC &_control_desc_struct)
{
    if (code >= CONTROL_ELEMENTS_NUM)
    {
        _control_desc_struct.pControlName = "invalid control";
        return false;
    }

    switch (code)
    {
    case CE_MOUSE_X_AXIS:
        _control_desc_struct.ControlType = SYSTEM_CONTROL_DESC::CT_AXIS;
        _control_desc_struct.pControlName = "Mouse X axis";
        break;
    case CE_MOUSE_Y_AXIS:
        _control_desc_struct.ControlType = SYSTEM_CONTROL_DESC::CT_AXIS;
        _control_desc_struct.pControlName = "Mouse Y axis";
        break;
    case VK_LBUTTON:
        _control_desc_struct.ControlType = SYSTEM_CONTROL_DESC::CT_BUTTON;
        _control_desc_struct.pControlName = "Mouse left button";
        break;
    case VK_RBUTTON:
        _control_desc_struct.ControlType = SYSTEM_CONTROL_DESC::CT_BUTTON;
        _control_desc_struct.pControlName = "Mouse right button";
        break;
    case VK_MBUTTON:
        _control_desc_struct.ControlType = SYSTEM_CONTROL_DESC::CT_BUTTON;
        _control_desc_struct.pControlName = "Mouse middle button";
        break;
    case VK_XBUTTON1:
        _control_desc_struct.ControlType = SYSTEM_CONTROL_DESC::CT_BUTTON;
        _control_desc_struct.pControlName = "Mouse special button 1";
        break;
    case VK_XBUTTON2:
        _control_desc_struct.ControlType = SYSTEM_CONTROL_DESC::CT_BUTTON;
        _control_desc_struct.pControlName = "Mouse special button 2";
        break;
    default:
        _control_desc_struct.ControlType = SYSTEM_CONTROL_DESC::CT_BUTTON;
        _control_desc_struct.pControlName = "Keyboard button";
        break;
    }
    return true;
}

int32_t PCS_CONTROLS::CreateControl(const char *control_name)
{
    int32_t n;
    if (control_name == nullptr)
        return INVALID_CONTROL_CODE;
    for (n = 0; n < nControlsNum; n++)
    {
        if (storm::iEquals(control_name, pUserControls[n].name))
            return n;
    }
    n = nControlsNum;
    nControlsNum++;
    pUserControls.resize(nControlsNum);
    const auto len = strlen(control_name) + 1;
    pUserControls[n].name = new char[len];
    memcpy(pUserControls[n].name, control_name, len);
    pUserControls[n].system_code = UNASSIGNED_CONTROL;
    pUserControls[n].flags = 0;
    pUserControls[n].state = CST_INACTIVE;
    pUserControls[n].nframe = 0;
    pUserControls[n].bLocked = false;
    pUserControls[n].control_type = UCT_Standart;

    return n;
}

int32_t PCS_CONTROLS::GetControlsNum()
{
    return nControlsNum;
}

bool PCS_CONTROLS::GetControlDesc(int32_t code, USER_CONTROL &_user_desc_struct)
{
    if (code < 0 || code >= nControlsNum)
        return false;

    _user_desc_struct.name = pUserControls[code].name;
    _user_desc_struct.system_code = pUserControls[code].system_code;

    return true;
}

int32_t PCS_CONTROLS::GetDevicesNum()
{
    return 1;
}

bool PCS_CONTROLS::GetDeviceDesc(int32_t code, DEVICE_DESC &_device_desc)
{
    _device_desc.name = "Keyboard and Mouse";
    return true;
}

int32_t PCS_CONTROLS::AddControlTreeNode(int32_t nParent, const char *pcBaseControl, const char *pcOutControl, float fTimeOut)
{
    const auto ntree = m_ControlTree.AddControlChild(nParent, pcBaseControl, pcOutControl, fTimeOut);
    if (ntree >= 0 && pcOutControl)
    {
        const auto nc = CreateControl((char *)pcOutControl);
        if (nc >= 0)
        {
            pUserControls[nc].control_type = UCT_ControlTree;
            pUserControls[nc].state = CST_INACTIVE;
        }
    }
    return ntree;
}

void PCS_CONTROLS::MapControl(int32_t control_code, int32_t system_control_code)
{
    if (control_code < 0 || control_code >= nControlsNum)
    {
        pUserControls[control_code].system_code = UNASSIGNED_CONTROL;
        return;
    }
    pUserControls[control_code].system_code = system_control_code;
}

bool PCS_CONTROLS::GetControlState(const char *control_name, CONTROL_STATE &_state_struct)
{
    /*
      int32_t n;
      _state_struct.state = CST_INACTIVE;
      _state_struct.lValue = 0;
      _state_struct.fValue = 0.0f;

      if(control_name == 0) return false;
      for(n=0;n<nControlsNum;n++)
      {
        if(storm::iEquals(control_name,pUserControls[n].name)) return GetControlState(n,_state_struct);
      }
      return false;
      //*/

    int32_t n;
    const auto bControlFound = false;

    _state_struct.state = CST_INACTIVE;
    _state_struct.lValue = 0;
    _state_struct.fValue = 0.0f;

    if (m_bLockAll)
        return true;
    if (control_name == nullptr)
        return false;
    for (n = 0; n < nControlsNum; n++)
    {
        if (storm::iEquals(control_name, pUserControls[n].name))
        {
            if (pUserControls[n].bLocked)
            {
                _state_struct.state = CST_INACTIVE;
                _state_struct.lValue = 0;
                _state_struct.fValue = 0.0f;
                return true;
            }

            /*if(GetControlState(n,local_state_struct))
            {
              bControlFound = true;

              if(_state_struct.state == CST_INACTIVE)    // if control inactive just copy what we have on new control
              {
                _state_struct = local_state_struct;
              }
              else
              {
                if(_state_struct.state == CST_ACTIVE)    // if control already active, check for activated state
                {
                  if(local_state_struct.state == CST_ACTIVATED) _state_struct = local_state_struct;
                }
                else return true;
              }


            }  */
            return GetControlState(n, _state_struct);
        }
    }
    return bControlFound;
}

bool PCS_CONTROLS::GetControlState(int32_t control_code, CONTROL_STATE &_state_struct)
{
    uint32_t system_code;
    int32_t lRes;

    if (control_code >= nControlsNum)
    {
        _state_struct.state = CST_INACTIVE;
        _state_struct.lValue = 0;
        _state_struct.fValue = 0.0f;
        return false;
    }

    if (pUserControls[control_code].bLocked)
    {
        _state_struct.state = CST_INACTIVE;
        _state_struct.lValue = 0;
        _state_struct.fValue = 0.0f;
        return true;
    }

    // processing of controls which is a tree of controls
    if (pUserControls[control_code].control_type == UCT_ControlTree)
    {
        if (pUserControls[control_code].nframe != nFrameCounter)
        {
            pUserControls[control_code].nframe = nFrameCounter;
            pUserControls[control_code].state = m_ControlTree.CheckControlActivated(pUserControls[control_code].name);
        }
        _state_struct.state = pUserControls[control_code].state;
        if (_state_struct.state == CST_ACTIVATED || _state_struct.state == CST_ACTIVE)
        {
            _state_struct.fValue = 1.f;
            _state_struct.lValue = 1;
        }
        else
        {
            _state_struct.fValue = 0.f;
            _state_struct.lValue = 0;
        }
        return true;
    }

    system_code = pUserControls[control_code].system_code;

    if (system_code >= CONTROL_ELEMENTS_NUM)
    {
        _state_struct.state = CST_INACTIVE;
        _state_struct.lValue = 0;
        _state_struct.fValue = 0.0f;
        return false;
    }

    if (ControlsTab[system_code].update_frame == nFrameCounter)
    {
        _state_struct = ControlsTab[system_code].state;
    }
    else
    {
        ControlsTab[system_code].update_frame = nFrameCounter;

        if (system_code == CE_MOUSE_X_AXIS || system_code == CE_MOUSE_Y_AXIS)
        {
            _state_struct = ControlsTab[system_code].state;
        }
        else if (system_code == CE_MOUSE_WHEEL_UP || system_code == CE_MOUSE_WHEEL_DOWN)
        {
            _state_struct = ControlsTab[system_code].state;
        }
        else
        {
            if (IsKeyPressed(system_code))
            {
                ControlsTab[system_code].state.lValue = 1;
                ControlsTab[system_code].state.fValue = static_cast<float>(ControlsTab[system_code].state.lValue);

                if (ControlsTab[system_code].state.state == CST_INACTIVE ||
                    ControlsTab[system_code].state.state == CST_INACTIVATED)
                {
                    ControlsTab[system_code].state.state = CST_ACTIVATED;
                }
                else
                    ControlsTab[system_code].state.state = CST_ACTIVE;
            }
            else
            {
                ControlsTab[system_code].state.lValue = 0;
                ControlsTab[system_code].state.fValue = 0;

                if (ControlsTab[system_code].state.state == CST_ACTIVE ||
                    ControlsTab[system_code].state.state == CST_ACTIVATED)
                {
                    ControlsTab[system_code].state.state = CST_INACTIVATED;
                }
                else
                    ControlsTab[system_code].state.state = CST_INACTIVE;
            }
        }
    }

    _state_struct.state = ControlsTab[system_code].state.state;

    if (pUserControls[control_code].flags & INVERSE_CONTROL)
    {
        if (pUserControls[control_code].flags & USE_AXIS_AS_BUTTON)
        {
            _state_struct.lValue = 1 - _state_struct.lValue;
            _state_struct.fValue = 1.f - _state_struct.fValue;
        }
        else
        {
            _state_struct.lValue = -_state_struct.lValue;
            _state_struct.fValue = -_state_struct.fValue;
        }
        switch (_state_struct.state)
        {
        case CST_ACTIVE:
            _state_struct.state = CST_INACTIVE;
            break;
        case CST_INACTIVE:
            _state_struct.state = CST_ACTIVE;
            break;
        case CST_ACTIVATED:
            _state_struct.state = CST_INACTIVATED;
            break;
        case CST_INACTIVATED:
            _state_struct.state = CST_ACTIVATED;
            break;
        }
    }

    if ((pUserControls[control_code].flags & USE_AXIS_AS_BUTTON) && pUserControls[control_code].state != FORCE_DWORD)
    {
        if (_state_struct.lValue > 0)
        {
            if (pUserControls[control_code].state == CST_INACTIVE ||
                pUserControls[control_code].state == CST_INACTIVATED)
                _state_struct.state = CST_ACTIVATED;
            else
                _state_struct.state = CST_ACTIVE;

            _state_struct.lValue = 1;
            _state_struct.fValue = 1.0f;
        }
        else
        {
            if (pUserControls[control_code].state == CST_ACTIVE || pUserControls[control_code].state == CST_INACTIVE)
                _state_struct.state = CST_INACTIVATED;
            else
                _state_struct.state = CST_INACTIVE;
            _state_struct.lValue = 0;
            _state_struct.fValue = 0.0f;
        }
    }
    if (_state_struct.state != CST_INACTIVE)
        if (!(system_code == CE_MOUSE_X_AXIS || system_code == CE_MOUSE_Y_AXIS))
            nLastControlTime = 0;
    return true;
}

void PCS_CONTROLS::Update(uint32_t DeltaTime)
{
    if (auto *editor = core.GetEditor(); editor != nullptr && editor->IsFocused()) {
        // Skip control processing
        nMouseDx = 0;
        nMouseDy = 0;
        return;
    }

#ifdef _WIN32
    static int nMouseXPrev, nMouseYPrev;
    if (updateCursor_)
    {
        POINT point;
        GetCursorPos(&point);

        nMouseDx = point.x - nMouseXPrev;
        nMouseDy = point.y - nMouseYPrev;

        RECT r;
        GetWindowRect(static_cast<HWND>(core.GetWindow()->OSHandle()), &r);
        nMouseXPrev = r.left + (r.right - r.left) / 2;
        nMouseYPrev = r.top + (r.bottom - r.top) / 2;
        SetCursorPos(nMouseXPrev, nMouseYPrev);
    }
    else
    {
        nMouseDx = 0;
        nMouseDy = 0;
    }
#endif

    m_ControlTree.Process();
    m_KeyBuffer.Reset();

    nFrameCounter++;
#ifdef __APPLE__
    // touch screens: the camera turns while a finger is moved away from where it touched the screen,
    // the faster the farther up to kMaxDistance (in the window heights, with a dead zone)
    if (float startX, startY, x, y; input_->TouchStick(startX, startY, x, y))
    {
        static constexpr float kDeadZone = 0.02f;
        static constexpr float kMaxDistance = 0.085f; // the radius of the drawn stick
        static constexpr float kSpeed = 22600.f;     // the mouse motion per second at the distance of 1
        // the sizes of the sticks and the speed of the camera of the options of the game
        const auto maxDistance = kMaxDistance * storm::GetPortOptionScale("stick_scale");
        const auto maxSpeed = kSpeed * kMaxDistance * storm::GetPortOptionScale("camera_speed");
        const auto speed = [&](float offset) {
            const auto distance = std::clamp(std::abs(offset) - kDeadZone, 0.f, maxDistance);
            return std::copysign(distance / maxDistance * maxSpeed, offset);
        };
        const auto *window = core.GetWindow();
        const auto aspect = window && window->Height() > 0 ? static_cast<float>(window->Width()) / window->Height() : 1.f;
        // the ship cameras turn slower than the camera of the character
        const auto kCamera = m_bTouchAtSea ? 2.f : 1.f;
        fTouchStickRestX += speed((x - startX) * aspect) * kCamera * DeltaTime * 0.001f;
        fTouchStickRestY += speed(y - startY) * 0.5f * kCamera * DeltaTime * 0.001f;
        const auto dx = static_cast<int32_t>(fTouchStickRestX);
        const auto dy = static_cast<int32_t>(fTouchStickRestY);
        fTouchStickRestX -= static_cast<float>(dx);
        fTouchStickRestY -= static_cast<float>(dy);
        nMouseDx += dx;
        nMouseDy += dy;
    }
    // the right stick of the gamepad turns the camera as the touch camera stick at its edge
    if (m_bGamepadActive)
    {
        static constexpr float kDeadZone = 0.15f;
        static constexpr float kSpeed = 22600.f * 0.085f; // the mouse motion per second at the edge
        const auto maxSpeed = kSpeed * storm::GetPortOptionScale("pad_camera_speed");
        const auto speed = [&](ControllerAxis a) {
            const auto value = std::clamp(input_->ControllerAxisValue(a) / 32767.f, -1.f, 1.f);
            return std::copysign(std::max(std::abs(value) - kDeadZone, 0.f) / (1.f - kDeadZone) * maxSpeed, value);
        };
        const auto kCamera = m_bTouchAtSea ? 2.f : 1.f;
        fGamepadRestX += speed(ControllerAxis::RightX) * kCamera * DeltaTime * 0.001f;
        fGamepadRestY += speed(ControllerAxis::RightY) * 0.5f * kCamera * DeltaTime * 0.001f;
        // the left stick turns the character (the walking stick)
        if (m_fGamepadStickX != 0.f)
        {
            static constexpr float kTurnSpeed = 1500.f;
            fGamepadRestX += m_fGamepadStickX * kTurnSpeed * storm::GetPortOptionScale("pad_turn_speed") *
                             DeltaTime * 0.001f;
        }
        const auto dx = static_cast<int32_t>(fGamepadRestX);
        const auto dy = static_cast<int32_t>(fGamepadRestY);
        fGamepadRestX -= static_cast<float>(dx);
        fGamepadRestY -= static_cast<float>(dy);
        nMouseDx += dx;
        nMouseDy += dy;
    }
    // the walking stick turns the character to the sides
    if (float x, y; m_bStickTurns && input_->MoveStick(x, y))
    {
        static constexpr float kDeadZone = 0.3f;
        static constexpr float kTurnSpeed = 1500.f; // the mouse motion per second at the edge
        const auto turn = std::max(std::abs(x) - kDeadZone, 0.f) / (1.f - kDeadZone);
        fTouchStickRestX += std::copysign(turn * kTurnSpeed, x) * DeltaTime * 0.001f;
        const auto dx = static_cast<int32_t>(fTouchStickRestX);
        fTouchStickRestX -= static_cast<float>(dx);
        nMouseDx += dx;
    }
#endif
    uint32_t system_code = CE_MOUSE_X_AXIS;
    ControlsTab[system_code].state.lValue = nMouseDx;
    ControlsTab[system_code].state.fValue =
        static_cast<float>(ControlsTab[system_code].state.lValue) * fMouseSensivityX;
    ControlsTab[system_code].update_frame = nFrameCounter;

    system_code = CE_MOUSE_Y_AXIS;
    ControlsTab[system_code].state.lValue = nMouseDy;
    ControlsTab[system_code].state.fValue =
        static_cast<float>(ControlsTab[system_code].state.lValue) * fMouseSensivityY;
    ControlsTab[system_code].update_frame = nFrameCounter;

    nMouseDx = 0;
    nMouseDy = 0;

    system_code = CE_MOUSE_WHEEL_UP;
    if (nMouseWheel > 0)
    {
        ControlsTab[system_code].state.lValue = nMouseWheel / 10;
        ControlsTab[system_code].state.fValue = nMouseWheel * .1f;
        if (ControlsTab[system_code].state.state == CST_INACTIVE ||
            ControlsTab[system_code].state.state == CST_INACTIVATED)
            ControlsTab[system_code].state.state = CST_ACTIVATED;
        else
            ControlsTab[system_code].state.state = CST_ACTIVE;
    }
    else
    {
        ControlsTab[system_code].state.lValue = 0;
        ControlsTab[system_code].state.fValue = 0.f;
        if (ControlsTab[system_code].state.state == CST_ACTIVE || ControlsTab[system_code].state.state == CST_ACTIVATED)
            ControlsTab[system_code].state.state = CST_INACTIVATED;
        else
            ControlsTab[system_code].state.state = CST_INACTIVE;
    }
    ControlsTab[system_code].update_frame = nFrameCounter;

    system_code = CE_MOUSE_WHEEL_DOWN;
    if (nMouseWheel < 0)
    {
        ControlsTab[system_code].state.lValue = -nMouseWheel / 10;
        ControlsTab[system_code].state.fValue = -nMouseWheel * .1f;
        if (ControlsTab[system_code].state.state == CST_INACTIVE ||
            ControlsTab[system_code].state.state == CST_INACTIVATED)
            ControlsTab[system_code].state.state = CST_ACTIVATED;
        else
            ControlsTab[system_code].state.state = CST_ACTIVE;
    }
    else
    {
        ControlsTab[system_code].state.lValue = 0;
        ControlsTab[system_code].state.fValue = 0.f;
        if (ControlsTab[system_code].state.state == CST_ACTIVE || ControlsTab[system_code].state.state == CST_ACTIVATED)
            ControlsTab[system_code].state.state = CST_INACTIVATED;
        else
            ControlsTab[system_code].state.state = CST_INACTIVE;
    }
    ControlsTab[system_code].update_frame = nFrameCounter;

    nMouseWheel = 0;

    nLastControlTime += DeltaTime;

#ifdef __APPLE__
    if (const auto window = core.GetWindow())
    {
        float left, top, right, bottom;
        window->GetContentArea(left, top, right, bottom);
        input_->SetTouchArea(left, top, right, bottom);
    }
    const auto taps = input_->TouchTaps(m_fTouchTapX, m_fTouchTapY);
    m_bTouchTap = taps != m_nTouchTaps;
    m_nTouchTaps = taps;
    UpdateTouchButtons();
#endif
}

#ifdef __APPLE__
void PCS_CONTROLS::UpdateTouchButtons()
{
    if (!m_bTouchButtons && !input_->IsTouchScreen())
        return;

    struct Button
    {
        int col, row; // from the bottom right corner (col -1 is the left edge); row -1 is the top row in the middle
        const char *control; // the key is the one of this control (the players reassign them), or key
        const char *label;
        bool toggle;
        storm::KeyboardKey key;
        bool stick = false; // the walking stick: control forward, backControl back, farControl to run
        const char *backControl = nullptr;
        const char *farControl = nullptr;
        const char *leftControl = nullptr; // or the turns (without them the stick turns the character)
        const char *rightControl = nullptr;
        storm::KeyboardKey backKey = 0, farKey = 0, leftKey = 0, rightKey = 0;
        TouchIcon icon; // the picture of the game (row -2: small buttons under the top row)
        const char *id = nullptr; // the name in the layout moved by the player (the control or the label)
    };
    std::vector<Button> buttons;
    // the current controls group of the scripts (GetCurControlGroup in controls.c)
    std::string group;
    if (auto *pVar = static_cast<VDATA *>(core.GetScriptVariable("curKeyGroupName")))
    {
        const char *value = nullptr;
        if (pVar->Get(value) && value)
            group = value;
    }
    // the menus of the game: the finger drags the sliders, no camera stick
    input_->SetTouchDrag(group.empty() || storm::iEquals(group, "MainInterface"));
    storm::SetMenuActive(storm::iEquals(group, "MainInterface"));
    // the buttons of the land over the menu of the touch settings: their sizes
    const auto padGroup = group;
    // or the buttons of the group the player moves
    const auto layoutGroup = storm::GetTouchLayoutGroup();
    m_bTouchPreview = storm::IsTouchPreview() || !layoutGroup.empty();
    if (m_bTouchPreview)
        group = layoutGroup.empty() ? "PrimaryLand" : layoutGroup;
    if (storm::iEquals(group, "BattleInterfaceControls"))
    {
        buttons = {{2, 0, "BICommandsLeft", "<"},
                   {0, 0, "BICommandsRight", ">"},
                   {1, 0, "BICommandsConfirm", "OK"},
                   {1, 1, "BICommandsCancel", "Отмена"}};
    }
    else if (storm::iEquals(group, "FightModeControls"))
    {
        // the scripts do not open the commands in the fight mode
        buttons = {{0, 0, "ChrAttack", "Удар"},          {1, 0, "ChrBlock", "Блок"},
                   {0, 1, "ChrFire", "Выстрел"},         {1, 1, "ChrRecoil", "Отскок"},
                   {0, 2, "ChrFightMode", "Убрать"},     {1, 2, "hk_Cure", "Лечение"},
                   {-1, 2, "ChrForward", "", false, 0, true, "ChrBackward", "ChrRun"}};
    }
    else if (storm::iEquals(group, "PrimaryLand"))
    {
        // the fast command does the action of the icon: talk, pick an item, open a box or a door
        // the stick runs when pushed to the edge
        buttons = {{0, 0, "BIFastCommand", "Выполнить"}, {1, 0, "ChrFightMode", "Оружие"},
                   {1, 1, "hk_Cure", "Лечение"},      {0, 1, "TimeScale", "Время"},
                   {-1, 2, "ChrForward", "", false, 0, true, "ChrBackward", "ChrRun"}};
    }
    else if (storm::iEquals(group, "Sailing3Pers") || storm::iEquals(group, "Sailing1Pers"))
    {
        // the sails and the turns are the stick at the left, the buttons above the camera stick
        buttons = {{-1, 2, "Ship_SailUp", "", false, 0, true, "Ship_SailDown", nullptr, "Ship_TurnLeft",
                    "Ship_TurnRight"},
                   {0, 2, "Ship_Fire", "Залп"},
                   {0, 3, "BIFastCommand", "Выполнить"}, // moor, board, the map
                   {1, 3, "Sea_CameraSwitch", "Камера"},
                   {0, 4, "TimeScale", "Время"}}; // the time acceleration on and off
        // the charges: balls, grapes, knippels, bombs (the pictures of cicons_charge.tga), the current one is selected
        m_nTouchCharge = -1;
        if (auto *pVD = core.Event("GetCurrentCharge"); pVD && !pVD->IsArray())
        {
            // the pictures of small_cannons.tga: knippels, bombs, balls, grapes
            int32_t charge = -1;
            pVD->Get(charge);
            static const int32_t toIcon[] = {2, 3, 0, 1};
            if (charge >= 0 && charge < 4)
                m_nTouchCharge = toIcon[charge];
        }
        for (int32_t charge = 0; charge < 4; charge++)
        {
            static const char *controls[] = {"hk_charge1", "hk_charge2", "hk_charge3", "hk_charge4"};
            Button b{charge, -2, controls[charge], ""};
            b.icon = {"battle_interface\\cicons_charge.tga", 1, 4, charge, true, charge == m_nTouchCharge};
            buttons.push_back(b);
        }
        if (storm::iEquals(group, "Sailing3Pers"))
        {
            // the camera zoom (the mouse buttons)
            buttons.push_back({0, 5, "ShipCamera_Forward", "Ближе"});
            buttons.push_back({1, 5, "ShipCamera_Backward", "Дальше"});
        }
        if (storm::iEquals(group, "Sailing1Pers"))
        {
            // on the deck the stick walks forward and back, the sails are buttons
            buttons[0].control = "DeckCamera_Forward";
            buttons[0].backControl = "DeckCamera_Backward";
            buttons.push_back({1, 4, "TelescopeIn", "Труба"});
            buttons.push_back({0, 5, "Ship_SailUp", "Паруса +"});
            buttons.push_back({1, 5, "Ship_SailDown", "Паруса -"});
        }
    }
    else if (storm::iEquals(group, "DialogControls"))
    {
        // above the dialog window
        buttons = {{-1, 4, "DlgUp", "Вверх"}, {-1, 3, "DlgDown", "Вниз"}, {0, 3, "DlgAction", "Выбрать"}};
    }
    else if (storm::iEquals(group, "WorldMapControls"))
    {
        // the ship stick at the left, the buttons above the camera stick
        buttons = {{-1, 2, "WMapShipSailUp", "", false, 0, true, "WMapShipSailDown", nullptr, "WMapShipTurnLeft",
                    "WMapShipTurnRight"},
                   {0, 2, "WMapCancel", "В море"},
                   {0, 3, "WMapCameraSwitch", "Обзор"},
                   {0, 4, "WMapForward", "Ближе"},
                   {1, 4, "WMapBackward", "Дальше"}};
        // the encounter can be skipped (the skip icon of the map)
        const auto *pMap = core.Entity_GetAttributePointer(core.GetEntityId("WorldMap"));
        if (pMap && pMap->GetAttributeAsDword("encounter_skip", 0) != 0)
            buttons.push_back({1, 2, "WMapSkipEncounter", "Пропуск"});
    }
    const auto inGame = !buttons.empty() && !storm::iEquals(group, "DialogControls");
    // the real group, not the one shown over the menu of the touch settings (the taps press the menu there)
    m_bTouchAtSea = storm::iEquals(padGroup, "Sailing3Pers") || storm::iEquals(padGroup, "Sailing1Pers");
    // the left mouse button zooms the camera at sea and on the map (walks on the deck): the touches do not press it
    // there (the commands of the ships take the taps, GetTouchTap)
    input_->SetTouchPress(!m_bTouchAtSea && !storm::iEquals(padGroup, "WorldMapControls"));
    // on land the buttons are above the place of the camera stick in the bottom right corner
    if (storm::iEquals(group, "PrimaryLand") || storm::iEquals(group, "FightModeControls"))
    {
        for (auto &button : buttons)
        {
            if (button.col >= 0)
                button.row += 2;
        }
    }
    buttons.push_back({0, -1, nullptr, "Esc", false, VK_ESCAPE});
    if (inGame)
    {
        buttons.push_back({1, -1, "MainMenu", "Меню", false, VK_F1});
        buttons.push_back({2, -1, "Interface", "Герой", false, VK_F2});
    }
    for (auto &button : buttons)
        button.id = button.control ? button.control : button.label;
    // the picture of the time acceleration and the one of the fast command (the others are labels)
    for (auto &button : buttons)
    {
        if (button.control && storm::iEquals(button.control, "TimeScale"))
            button.icon = {"battle_interface\\TimeSpeed.tga", 1, 1, 0, false};
        // a cannon of the ship interface
        if (button.control && storm::iEquals(button.control, "Ship_Fire"))
            button.icon = {"interfaces\\Cannons.tga", 8, 1, 0, false};
        static const std::pair<const char *, const char *> kDrawnIcons[] = {
            {"ShipCamera_Forward", "zoom_in"},   {"WMapForward", "zoom_in"},  {"ShipCamera_Backward", "zoom_out"},
            {"WMapBackward", "zoom_out"},        {"Sea_CameraSwitch", "eye"}, {"WMapCameraSwitch", "eye"},
            {"WMapSkipEncounter", "skip"},       {"DlgUp", "up"},             {"DlgDown", "down"},
            {"DlgAction", "check"},              {"Ship_SailUp", "sails_up"}, {"Ship_SailDown", "sails_down"}};
        for (const auto &[control, name] : kDrawnIcons)
        {
            if (button.control && storm::iEquals(button.control, control))
                button.icon = {std::string("interfaces\\touch_") + name + ".tga", 1, 1, 0, false};
        }
        // the small buttons of the top row: the pictures without captions
        static const std::pair<const char *, const char *> kTopIcons[] = {
            {"Esc", "back"}, {"Меню", "menu"}, {"Герой", "hero"}};
        for (const auto &[label, name] : kTopIcons)
        {
            if (button.row == -1 && button.label && storm::iEquals(button.label, label))
            {
                button.icon = {std::string("interfaces\\touch_") + name + ".tga", 1, 1, 0, false};
                button.label = "";
            }
        }
        // the spyglass of the items
        if (button.control && storm::iEquals(button.control, "TelescopeIn"))
            button.icon = {"interfaces\\items6.tga", 4, 4, 15, false};
        // the pictures of the abilities (perks) and of the potion
        // to draw the weapon (the swordsman), to sheathe it (the pictures drawn by tools/touch-icons/touch_icons.py)
        if (button.control && storm::iEquals(button.control, "ChrFightMode"))
            button.icon = storm::iEquals(button.label, "Убрать")
                              ? TouchIcon{"interfaces\\touch_scabbard.tga", 1, 1, 0, false}
                              : TouchIcon{"interfaces\\perks\\128\\SwordplayProfessional.tga", 1, 1, 0, false};
        if (button.control && storm::iEquals(button.control, "ChrAttack"))
            button.icon = {"interfaces\\perks\\128\\CriticalHit.tga", 1, 1, 0, false};
        if (button.control && storm::iEquals(button.control, "ChrFire"))
            button.icon = {"interfaces\\perks\\128\\GunMan.tga", 1, 1, 0, false};
        // the boots of the move command of the land commands
        if (button.control && storm::iEquals(button.control, "ChrRecoil"))
            button.icon = {"battle_interface\\LandCommands.tga", 4, 4, 5, false};
        if (button.control && storm::iEquals(button.control, "ChrBlock"))
            button.icon = {"interfaces\\perks\\128\\BasicDefense.tga", 1, 1, 0, false};
        if (button.control && storm::iEquals(button.control, "hk_Cure"))
            button.icon = {"interfaces\\items1.tga", 4, 4, 6, false};
        // the cannon on the background of the potion of the cure (tools/touch-icons/game_icons.py), when it is made;
        // the other pictures of the battle interface are replaced as textures (DX9RENDER::TextureCreate)
        if (button.control && storm::iEquals(button.control, "Ship_Fire"))
        {
            static const auto broadside =
                fio->_FileOrDirectoryExists("resource\\textures\\interfaces\\touch_pic_broadside.tga");
            if (broadside)
                button.icon = {"interfaces\\touch_pic_broadside.tga", 1, 1, 0, false};
        }
        // the icon of the map where the ship goes to sea: the encounter, the storm, the island or the open sea
        if (button.control && storm::iEquals(button.control, "WMapCancel"))
        {
            const auto *pMap = core.Entity_GetAttributePointer(core.GetEntityId("WorldMap"));
            button.icon = {"WorldMap\\Interfaces\\BigIcons.tga", 8, 1,
                           static_cast<int32_t>(pMap ? pMap->GetAttributeAsDword("sea_icon", 4) : 4), false};
        }
        if (button.control && storm::iEquals(button.control, "BIFastCommand"))
        {
            // the action of the icon in the top left corner (ILogAndActions)
            const auto *pLog = core.Entity_GetAttributePointer(core.GetEntityId("ILogAndActions"));
            const char *action = pLog ? pLog->GetAttribute("CurrentAction") : nullptr;
            const auto *pActions = pLog ? pLog->GetAttributeClass("ActiveActions") : nullptr;
            const auto *pAction = pActions && action && action[0] ? pActions->GetAttributeClass(action) : nullptr;
            const char *texture = pActions ? pActions->GetAttribute("TextureName") : nullptr;
            const auto icon = pAction ? static_cast<int32_t>(pAction->GetAttributeAsDword("IconNum", 0)) : -1;
            if (texture && icon >= 0)
            {
                button.icon = {texture, static_cast<int32_t>(pActions->GetAttributeAsDword("horzQ", 1)),
                               static_cast<int32_t>(pActions->GetAttributeAsDword("vertQ", 1)), icon, false};
            }
            else
            {
                // nothing to do: the closed lock of the land actions; on land out of the fight the button is the action
                // key then (the space of the computers: the reload of the boarding after the fight, if a box has replaced
                // its icon; the space fires a broadside at sea and attacks in the fight)
                button.icon = {"battle_interface\\land_fast_commands.tga", 8, 1, 0, false};
                if (storm::iEquals(group, "PrimaryLand"))
                    button.control = "ChrAction";
            }
        }
    }
    // the keys of the controls: the buttons of the keyboard and of the mouse, not the axes
    const auto controlKey = [this](const char *control, storm::KeyboardKey key) {
        for (int32_t n = 0; control && n < nControlsNum; n++)
        {
            const auto code = pUserControls[n].system_code;
            if (storm::iEquals(control, pUserControls[n].name) && code > 0 && code < CE_MOUSE_X_AXIS)
                return static_cast<storm::KeyboardKey>(code);
        }
        return key;
    };
    for (auto &button : buttons)
    {
        button.key = controlKey(button.control, button.key);
        button.backKey = controlKey(button.backControl, button.backKey);
        button.farKey = controlKey(button.farControl, button.farKey);
        button.leftKey = controlKey(button.leftControl, button.leftKey);
        button.rightKey = controlKey(button.rightControl, button.rightKey);
    }

    // the gamepad: its buttons press the controls of the group, its left stick is the stick of the touch buttons (the
    // arrows without it: the menus)
    {
        m_aGamepadKeys.clear();
        const auto axis = [this](ControllerAxis a) {
            const auto value = input_->ControllerAxisValue(a) / 32767.f;
            return std::abs(value) < 0.25f ? 0.f : std::clamp(value, -1.f, 1.f);
        };
        const auto x = axis(ControllerAxis::LeftX), y = axis(ControllerAxis::LeftY);
        bool pressed[storm::kGamepadButtonsNum]{};
        for (int32_t n = 0; n <= static_cast<int32_t>(ControllerButton::Right); n++)
            pressed[n] = input_->ControllerButtonState(static_cast<ControllerButton>(n));
        pressed[15] = input_->ControllerAxisValue(ControllerAxis::TriggerLeft) > 8000;
        pressed[16] = input_->ControllerAxisValue(ControllerAxis::TriggerRight) > 8000;
        // the options of the game wait for a button: the newly pressed one (not the Xbox button, iPadOS takes it) is
        // given to their control, which no other button of the group keeps; the held buttons press nothing
        uint32_t down = 0;
        for (int32_t n = 0; n < storm::kGamepadButtonsNum; n++)
            down |= pressed[n] ? 1u << n : 0u;
        std::string captureGroup, captureControl;
        const bool capture = storm::GetGamepadCapture(captureGroup, captureControl);
        if (capture)
        {
            const auto fresh = down & ~m_nGamepadDown;
            for (int32_t n = 0; n < storm::kGamepadButtonsNum; n++)
            {
                const auto *name = storm::kGamepadButtons[n];
                if (!(fresh & (1u << n)) || std::string_view(name) == "ps")
                    continue;
                for (const auto *other : storm::kGamepadButtons)
                {
                    if (other != name && storm::GetGamepadControl(captureGroup, other) == captureControl)
                        storm::SetGamepadControl(captureGroup, other, "");
                }
                storm::SetGamepadControl(captureGroup, name, captureControl);
                storm::StopGamepadCapture();
                storm::SetGamepadCaptureDone();
                break;
            }
            // the stick moves nothing either
            m_nGamepadBlocked = ~0u;
        }
        m_nGamepadDown = down;
        const auto stick = std::find_if(buttons.begin(), buttons.end(), [](const Button &b) { return b.stick; });
        m_fGamepadStickX = 0.f;
        if (stick != buttons.end())
        {
            const std::pair<storm::KeyboardKey, bool> stickKeys[] = {
                {stick->key, y < -0.3f},       {stick->backKey, y > 0.3f},    {stick->farKey, y < -0.85f},
                {stick->leftKey, x < -0.3f},   {stick->rightKey, x > 0.3f}};
            for (const auto &[key, on] : stickKeys)
            {
                if (on && key != 0)
                    m_aGamepadKeys.push_back(key);
            }
            if (stick->leftKey == 0 && stick->rightKey == 0)
                m_fGamepadStickX = x;
        }
        else
        {
            pressed[11] = pressed[11] || y < -0.5f;
            pressed[12] = pressed[12] || y > 0.5f;
            pressed[13] = pressed[13] || x < -0.5f;
            pressed[14] = pressed[14] || x > 0.5f;
        }
        auto used = x != 0.f || y != 0.f || axis(ControllerAxis::RightX) != 0.f || axis(ControllerAxis::RightY) != 0.f;
        // the button that ends the dialog would start it again on land: the held buttons wait for the release when the
        // group changes
        if (padGroup != m_sGamepadGroup)
        {
            m_sGamepadGroup = padGroup;
            m_nGamepadBlocked = 0;
            for (int32_t n = 0; n < storm::kGamepadButtonsNum; n++)
                m_nGamepadBlocked |= pressed[n] ? 1u << n : 0u;
        }
        for (int32_t n = 0; n < storm::kGamepadButtonsNum; n++)
        {
            if (!pressed[n])
                m_nGamepadBlocked &= ~(1u << n);
            if (!pressed[n] || (m_nGamepadBlocked & (1u << n)))
                continue;
            used = true;
            const auto control = storm::GetGamepadControl(padGroup, storm::kGamepadButtons[n]);
            if (const auto key = control.empty() ? 0 : controlKey(control.c_str(), 0); key != 0)
                m_aGamepadKeys.push_back(key);
        }
        if (used)
            m_nGamepadTime = SDL_GetTicks();
        // the touch buttons are hidden while the gamepad is used, a touch shows them
        m_bGamepadActive = m_nGamepadTime != 0 && m_nGamepadTime > input_->LastTouchTime();
    }
    std::erase_if(buttons, [](const Button &button) { return button.key == 0; });

    // the layout: two columns at the right edge, the top row in the middle
    std::vector<storm::TouchButton> touchButtons;
    // square buttons: the sizes in the window height
    const auto *window = core.GetWindow();
    const auto aspect = window && window->Height() > 0 ? static_cast<float>(window->Width()) / window->Height() : 1.f;
    // the size of the buttons and the sticks of the options of the game
    const auto scale = storm::GetPortOptionScale("button_scale");
    input_->SetTouchStickRadius(0.085f * storm::GetPortOptionScale("stick_scale"));
    std::vector<std::string> ids;
    for (const auto &button : buttons)
    {
        storm::TouchButton b{};
        if (button.row == -2)
        {
            // small squares in a row under the top row
            const auto size = 0.075f * scale / aspect;
            b.left = 0.5f + (button.col - 1.5f) * (size + 0.008f) - size * 0.5f;
            b.right = b.left + size;
            b.top = 0.02f + 0.07f * scale;
            b.bottom = b.top + 0.075f * scale;
        }
        else if (button.row < 0)
        {
            const auto size = 0.07f * scale / aspect;
            b.left = 0.5f + (button.col - 1.f) * (size + 0.01f) - size * 0.5f;
            b.right = b.left + size;
            b.top = 0.01f;
            b.bottom = 0.01f + 0.07f * scale;
        }
        else if (button.stick)
        {
            // the walking stick appears where the left half of the screen is touched, but on the buttons and the
            // commands of the game (AddTouchPointerArea)
            b.left = 0.f;
            b.right = 0.5f;
            b.top = 0.f;
            b.bottom = 1.f;
        }
        else
        {
            // col -1 and less are at the left edge
            const auto size = 0.1f * scale / aspect;
            b.right = button.col < 0 ? 0.015f + size + (-button.col - 1) * (size + 0.012f)
                                     : 0.985f - button.col * (size + 0.012f);
            b.left = b.right - size;
            b.bottom = 0.975f - button.row * (0.1f * scale + 0.015f);
            b.top = b.bottom - 0.1f * scale;
        }
        // where the player moved it (its center), on the screen
        if (float x, y; !button.stick && storm::GetTouchButtonPosition(group, button.id, x, y))
        {
            const auto w = b.right - b.left, h = b.bottom - b.top;
            b.left = std::clamp(x - w * 0.5f, 0.f, 1.f - w);
            b.top = std::clamp(y - h * 0.5f, 0.f, 1.f - h);
            b.right = b.left + w;
            b.bottom = b.top + h;
        }
        ids.emplace_back(button.id);
        b.key = button.key;
        b.toggle = button.toggle;
        b.stick = button.stick;
        b.backKey = button.backKey;
        b.farKey = button.farKey;
        b.leftKey = button.leftKey;
        b.rightKey = button.rightKey;
        if (b.stick)
            m_bStickTurns = b.leftKey == 0 && b.rightKey == 0;
        touchButtons.push_back(b);
    }
    // the commands of the game over the stick (after the buttons: no labels to draw)
    for (const auto &[left, top, right, bottom] : m_aTouchPointerAreas)
    {
        storm::TouchButton area{};
        area.left = left;
        area.top = top;
        area.right = right;
        area.bottom = bottom;
        area.pointer = true;
        touchButtons.push_back(area);
    }
    m_aTouchPointerAreas.clear();
    // the touch settings: the finger moves a button of the group, the place is saved when it leaves the screen; a tap on
    // the Esc button (drawn over the menu) is Esc
    if (m_bTouchPreview)
    {
        if (float startX, startY, x, y; input_->TouchPoint(startX, startY, x, y))
        {
            if (!m_bTouchDragChecked)
            {
                m_bTouchDragChecked = true;
                m_fTouchDragMoved = 0.f;
                for (size_t i = 0; i < ids.size(); i++)
                {
                    const auto &b = touchButtons[i];
                    if (!b.stick && (!layoutGroup.empty() || ids[i] == "Esc") && startX >= b.left &&
                        startX <= b.right && startY >= b.top && startY <= b.bottom)
                    {
                        m_sTouchDragButton = ids[i];
                        m_fTouchDragX = (b.left + b.right) * 0.5f - startX;
                        m_fTouchDragY = (b.top + b.bottom) * 0.5f - startY;
                        // not a click on the menu under it
                        input_->CancelTouch();
                        break;
                    }
                }
            }
            m_fTouchDragMoved = std::max(m_fTouchDragMoved, std::hypot(x - startX, y - startY));
            // moved: the buttons of the layout (a tap does not move them)
            if (!m_sTouchDragButton.empty() && !layoutGroup.empty() && m_fTouchDragMoved > 0.01f)
                storm::SetTouchButtonPosition(group, m_sTouchDragButton, std::clamp(x + m_fTouchDragX, 0.f, 1.f),
                                              std::clamp(y + m_fTouchDragY, 0.f, 1.f), false);
        }
        else
        {
            if (m_sTouchDragButton == "Esc" && m_fTouchDragMoved <= 0.01f)
                m_nTouchEscapeTime = SDL_GetTicks() + 100;
            else if (float x, y; !m_sTouchDragButton.empty() && !layoutGroup.empty() &&
                                 storm::GetTouchButtonPosition(group, m_sTouchDragButton, x, y))
                storm::SetTouchButtonPosition(group, m_sTouchDragButton, x, y, true);
            m_sTouchDragButton.clear();
            m_bTouchDragChecked = false;
        }
    }
    if (m_bTouchPreview)
    {
        m_aTouchPreview = touchButtons;
        touchButtons.clear();
    }
    input_->SetTouchButtons(touchButtons);
    m_aTouchLabels.clear();
    m_aTouchIcons.clear();
    for (const auto &button : buttons)
    {
        m_aTouchLabels.emplace_back(button.label);
        m_aTouchIcons.push_back(button.icon);
        if (button.toggle && input_->TouchButtonState(button.key))
            m_aTouchIcons.back().selected = true;
    }
}

void PCS_CONTROLS::DrawOverlay()
{
    // played with the gamepad: no touch buttons (but over the menu of their settings)
    if (m_bGamepadActive && !m_bTouchPreview)
        return;
    const auto &touchButtons = m_bTouchPreview ? m_aTouchPreview : input_->GetTouchButtons();
    if (touchButtons.empty())
        return;
    float stickStartX, stickStartY, stickX, stickY;
    const auto isStick = input_->TouchStick(stickStartX, stickStartY, stickX, stickY);
    auto *rs = static_cast<VDX9RENDER *>(core.GetService("dx9render"));
    if (!rs)
        return;
    if (m_nTouchFont < 0)
        m_nTouchFont = rs->LoadFont("interface_normal");
    D3DVIEWPORT9 vp;
    rs->GetViewport(&vp);
    struct Vertex
    {
        float x, y, z, rhw;
        uint32_t color;
    };
    // circles in pixels
    const auto drawCircle = [&](float cx, float cy, float rx, float ry, uint32_t circleColor) {
        constexpr int32_t kSegments = 32;
        Vertex fan[kSegments + 2];
        fan[0] = {cx, cy, 0.f, 1.f, circleColor};
        for (int32_t n = 0; n <= kSegments; n++)
        {
            const auto angle = n * 2.f * 3.14159265f / kSegments;
            fan[n + 1] = {cx + rx * std::cos(angle), cy + ry * std::sin(angle), 0.f, 1.f, circleColor};
        }
        rs->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, D3DFVF_XYZRHW | D3DFVF_DIFFUSE, kSegments, fan, sizeof(Vertex),
                            "battle_only_color");
    };
    const auto drawRing = [&](float cx, float cy, float rx, float ry, float width, uint32_t ringColor) {
        constexpr int32_t kSegments = 48;
        Vertex strip[(kSegments + 1) * 2];
        for (int32_t n = 0; n <= kSegments; n++)
        {
            const auto angle = n * 2.f * 3.14159265f / kSegments;
            const auto c = std::cos(angle), sn = std::sin(angle);
            strip[n * 2] = {cx + rx * c, cy + ry * sn, 0.f, 1.f, ringColor};
            strip[n * 2 + 1] = {cx + (rx - width) * c, cy + (ry - width) * sn, 0.f, 1.f, ringColor};
        }
        rs->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, D3DFVF_XYZRHW | D3DFVF_DIFFUSE, kSegments * 2, strip, sizeof(Vertex),
                            "battle_only_color");
    };
    // a picture (with its transparency) in the rectangle
    const auto drawPicture = [&](const char *texture, float l, float t, float r, float b) {
        auto it = m_TouchTextures.find(texture);
        if (it == m_TouchTextures.end())
            it = m_TouchTextures.emplace(texture, rs->TextureCreate(texture)).first;
        if (it->second < 0)
            return false;
        struct TexturedVertex
        {
            float x, y, z, rhw;
            uint32_t color;
            float u, v;
        };
        const TexturedVertex v[4] = {{l, t, 0.f, 1.f, 0xFFFFFFFF, 0.f, 0.f},
                                     {r, t, 0.f, 1.f, 0xFFFFFFFF, 1.f, 0.f},
                                     {l, b, 0.f, 1.f, 0xFFFFFFFF, 0.f, 1.f},
                                     {r, b, 0.f, 1.f, 0xFFFFFFFF, 1.f, 1.f}};
        rs->TextureSet(0, it->second);
        // smoothly smaller and larger than the picture (its smaller copies): the filters the renderer sets for the
        // whole game (DX9RENDER), the game draws with them after the overlay
        rs->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
        rs->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
        rs->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
        rs->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1, 2, v,
                            sizeof(TexturedVertex), "battle_colorRectangle");
        return true;
    };
    // the sticks: a brass ring around a leather inset and a brass knob where the finger is
    // (tools/touch-icons/touch_icons.py), or circles in the colours of the buttons without the pictures
    const auto drawStick = [&](float cx, float cy, float rx, float ry, float kx, float ky) {
        const auto kr = 0.42f; // the knob in the radius of the stick
        if (drawPicture("interfaces\\touch_stick_base.tga", cx - rx, cy - ry, cx + rx, cy + ry) &&
            drawPicture("interfaces\\touch_stick_knob.tga", kx - rx * kr, ky - ry * kr, kx + rx * kr, ky + ry * kr))
            return;
        drawCircle(cx, cy, rx, ry, 0x602E3050);
        drawRing(cx, cy, rx, ry, 2.f, 0xFFFFBA4A);
        drawCircle(kx, ky, rx * 0.38f, ry * 0.38f, 0xE02E3050);
        drawRing(kx, ky, rx * 0.38f, ry * 0.38f, 2.f, 0xFFFFBA4A);
    };
    for (size_t i = 0; i < touchButtons.size() && i < m_aTouchLabels.size(); i++)
    {
        const auto &b = touchButtons[i];
        float stickCenterX, stickCenterY, stickRadiusX, stickRadiusY;
        if (b.stick && !input_->MoveStickArea(stickCenterX, stickCenterY, stickRadiusX, stickRadiusY))
            continue;
        const auto pressed = !b.stick && input_->TouchButtonState(b.key);
        const auto color = pressed ? 0xA0C0A060 : 0x60202020;
        // the stick is drawn where it appeared
        const auto left = (b.stick ? stickCenterX - stickRadiusX : b.left) * vp.Width;
        const auto right = (b.stick ? stickCenterX + stickRadiusX : b.right) * vp.Width;
        const auto top = (b.stick ? stickCenterY - stickRadiusY : b.top) * vp.Height;
        const auto bottom = (b.stick ? stickCenterY + stickRadiusY : b.bottom) * vp.Height;
        if (b.stick)
        {
            // the round stick and its knob where the finger is
            float x = 0.f, y = 0.f;
            input_->MoveStick(x, y);
            const auto rx = (right - left) * 0.5f, ry = (bottom - top) * 0.5f;
            const auto cx = (left + right) * 0.5f, cy = (top + bottom) * 0.5f;
            drawStick(cx, cy, rx, ry, cx + x * rx, cy + y * ry);
            continue;
        }
        const auto quad = [&](float l, float t, float r, float bt, uint32_t c) {
            const Vertex v[4] = {{l, t, 0.f, 1.f, c}, {r, t, 0.f, 1.f, c}, {l, bt, 0.f, 1.f, c}, {r, bt, 0.f, 1.f, c}};
            rs->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, D3DFVF_XYZRHW | D3DFVF_DIFFUSE, 2, v, sizeof(Vertex),
                                "battle_only_color");
        };
        const auto label = [&](float scale, float y) {
            if (m_nTouchFont < 0 || m_aTouchLabels[i].empty())
                return;
            // the lines (\n) centred around y, the long ones are made smaller to fit
            std::vector<std::string> lines;
            for (size_t begin = 0;;)
            {
                const auto end = m_aTouchLabels[i].find('\n', begin);
                lines.push_back(m_aTouchLabels[i].substr(begin, end - begin));
                if (end == std::string::npos)
                    break;
                begin = end + 1;
            }
            auto fit = scale;
            for (const auto &line : lines)
            {
                const auto width = static_cast<float>(rs->StringWidth(line.c_str(), m_nTouchFont, 1.f));
                if (width > 0.f)
                    fit = std::min(fit, (right - left - 6.f) / width);
            }
            const auto lineHeight = rs->CharHeight(m_nTouchFont) * fit;
            auto lineY = y - lineHeight * lines.size() * 0.5f;
            for (const auto &line : lines)
            {
                rs->ExtPrint(m_nTouchFont, 0xFFFFFFFF, 0, PR_ALIGN_CENTER, false, fit, 0, 0,
                             static_cast<int32_t>((left + right) / 2), static_cast<int32_t>(lineY), "%s",
                             line.c_str());
                lineY += lineHeight;
            }
        };
        // the square in the colours of the command pictures: dark blue, orange when pressed
        const auto background = [&] { quad(left, top, right, bottom, pressed ? 0xFF6A4A20 : 0xFF2E3050); };
        // the same thin frame on all the buttons (the frames of the pictures are cut off) in the golden colour of the
        // selected commands, light when pressed
        const auto frame = [&] {
            const auto c = pressed ? 0xFFFFF0C0 : 0xFFFFBA4A;
            // in whole pixels: the same width on all the sides
            constexpr auto w = 2.f;
            const auto l = std::round(left), t = std::round(top), r = std::round(right), b = std::round(bottom);
            quad(l, t, r, t + w, c);
            quad(l, b - w, r, b, c);
            quad(l, t + w, l + w, b - w, c);
            quad(r - w, t + w, r, b - w, c);
        };
        const auto &icon = i < m_aTouchIcons.size() ? m_aTouchIcons[i] : TouchIcon{};
        if (!icon.texture.empty() && icon.index >= 0)
        {
            // the pictures without the selected half may be transparent
            const auto pl = left, pt = top, pr = right, pb = bottom;
            if (!icon.selectedHalf)
                background();
            // the picture of the game, its selected one (the right half of the texture) when pressed
            auto it = m_TouchTextures.find(icon.texture);
            if (it == m_TouchTextures.end())
                it = m_TouchTextures.emplace(icon.texture, rs->TextureCreate(icon.texture.c_str())).first;
            struct TexturedVertex
            {
                float x, y, z, rhw;
                uint32_t color;
                float u, v;
            };
            const auto columns = static_cast<float>(icon.cols * (icon.selectedHalf ? 2 : 1));
            const auto du = 1.f / columns, dv = 1.f / icon.rows;
            auto u = (icon.index % icon.cols) * du;
            if (icon.selectedHalf && (icon.selected || pressed))
                u += 0.5f;
            const auto tv = (icon.index / icon.cols) * dv;
            // without the frame of the picture
            constexpr auto kCut = 0.05f;
            const auto u0 = u + du * kCut, u1 = u + du * (1.f - kCut);
            const auto v0 = tv + dv * kCut, v1 = tv + dv * (1.f - kCut);
            const uint32_t iconColor = 0xFFFFFFFF;
            const TexturedVertex tex[4] = {{pl, pt, 0.f, 1.f, iconColor, u0, v0},
                                           {pr, pt, 0.f, 1.f, iconColor, u1, v0},
                                           {pl, pb, 0.f, 1.f, iconColor, u0, v1},
                                           {pr, pb, 0.f, 1.f, iconColor, u1, v1}};
            rs->TextureSet(0, it->second);
            rs->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1, 2, tex,
                                sizeof(TexturedVertex), "battle_colorRectangle");
            // a caption on a dark band at the bottom
            if (!m_aTouchLabels[i].empty())
            {
                quad(left, bottom - (bottom - top) * 0.3f, right, bottom, 0xA0000000);
                label(0.7f, bottom - (bottom - top) * 0.15f);
            }
            frame();
            continue;
        }
        background();
        frame();
        label(0.75f, (top + bottom) * 0.5f);
    }
    // the stick: where the finger touched the screen and where it is
    if (isStick)
    {
        // as the walking stick, the knob within the circle
        const auto r = 0.085f * storm::GetPortOptionScale("stick_scale") * vp.Height;
        const auto cx = stickStartX * vp.Width, cy = stickStartY * vp.Height;
        auto dx = (stickX - stickStartX) * vp.Width, dy = (stickY - stickStartY) * vp.Height;
        if (const auto length = std::hypot(dx, dy); length > r)
        {
            dx *= r / length;
            dy *= r / length;
        }
        drawStick(cx, cy, r, r, cx + dx, cy + dy);
    }
    // the menu of the touch settings: the stick at the left edge (the walking and the camera ones are alike), its knob
    // moved a little
    if (m_bTouchPreview)
    {
        const auto r = 0.085f * storm::GetPortOptionScale("stick_scale") * vp.Height;
        const auto cx = 0.03f * vp.Width + r, cy = 0.65f * vp.Height;
        drawStick(cx, cy, r, r, cx + r * 0.35f, cy - r * 0.35f);
    }
}
#endif

bool PCS_CONTROLS::SetControlFlags(int32_t code, uint32_t _flags)
{
    if (code < 0 || code >= nControlsNum)
        return false;
    pUserControls[code].flags = _flags;
    return true;
}

bool PCS_CONTROLS::SetControlState(const char *control_name, CONTROL_STATE &_state_struct)
{
    int32_t n;
    if (control_name == nullptr)
        return false;
    for (n = 0; n < nControlsNum; n++)
    {
        if (storm::iEquals(control_name, pUserControls[n].name))
            return SetControlState(n, _state_struct);
    }
    return false;
}

bool PCS_CONTROLS::SetControlState(int32_t control_code, CONTROL_STATE &_state_struct)
{
    if (control_code < 0 || control_code >= nControlsNum)
        return false;
    pUserControls[control_code].state = _state_struct.state;
    return true;
}

int32_t PCS_CONTROLS::LastControlTime()
{
    return nLastControlTime;
}

void PCS_CONTROLS::SetControlTreshold(int32_t control_code, float thval)
{
    // ~!~
}

void PCS_CONTROLS::LockControl(const char *control_name, bool mode)
{
    int32_t n;
    if (control_name == nullptr || control_name[0] == 0)
    {
        m_bLockAll = mode;
        return;
    }
    for (n = 0; n < nControlsNum; n++)
    {
        if (storm::iEquals(control_name, pUserControls[n].name))
        {
            pUserControls[n].bLocked = mode;
            pUserControls[n].state = FORCE_DWORD;
            return;
        }
    }
}

void PCS_CONTROLS::SetMouseSensivityX(float _s)
{
    fMouseSensivityX = _s;
}

bool PCS_CONTROLS::IsTouchScreen()
{
#ifdef __APPLE__
    return m_bTouchButtons || input_->IsTouchScreen();
#else
    return false;
#endif
}

void PCS_CONTROLS::AddTouchPointerArea(float left, float top, float right, float bottom)
{
    m_aTouchPointerAreas.push_back({left, top, right, bottom});
}

bool PCS_CONTROLS::GetTouchTap(float &x, float &y)
{
    x = m_fTouchTapX;
    y = m_fTouchTapY;
    return m_bTouchTap;
}

bool PCS_CONTROLS::GetPointerPosition(float &x, float &y)
{
    return input_->PointerPosition(x, y);
}

void PCS_CONTROLS::SetMouseSensivityY(float _s)
{
    fMouseSensivityY = _s;
}

int32_t PCS_CONTROLS::GetKeyBufferLength()
{
    return m_KeyBuffer.GetBufferLength();
}

const KeyDescr *PCS_CONTROLS::GetKeyBuffer()
{
    return m_KeyBuffer.c_str();
}

void PCS_CONTROLS::ClearKeyBuffer()
{
    m_KeyBuffer.Reset();
}

short PCS_CONTROLS::GetAsyncKeyState(int vk)
{
    return IsKeyPressed(vk) ? -1 : 0;
}

short PCS_CONTROLS::GetKeyState(int vk)
{
    return GetAsyncKeyState(vk);
}

short PCS_CONTROLS::GetDebugAsyncKeyState(int vk)
{
    // -1 because WinAPI sets msb when key pressed, so old code expects negative value
    if (m_bIsOffDebugKeys)
    {
        return 0;
    }

    return IsKeyPressed(vk) ? -1 : 0;
}

short PCS_CONTROLS::GetDebugKeyState(int vk)
{
    return GetDebugAsyncKeyState(vk);
}

bool PCS_CONTROLS::IsKeyPressed(int vk)
{
    // the on-screen buttons of touch screens
    if (input_->TouchButtonState(static_cast<storm::KeyboardKey>(vk)))
        return true;
#ifdef __APPLE__
    // and the gamepad
    if (std::find(m_aGamepadKeys.begin(), m_aGamepadKeys.end(), vk) != m_aGamepadKeys.end())
        return true;
    // the Esc button of the touch settings
    if (vk == VK_ESCAPE && SDL_GetTicks() < m_nTouchEscapeTime)
        return true;
#endif
    auto pressed = false;
    if (vk == VK_LBUTTON)
        pressed = input_->MouseKeyState(MouseKey::Left);
    else if (vk == VK_RBUTTON)
        pressed = input_->MouseKeyState(MouseKey::Right);
    else if (vk == VK_MBUTTON)
        pressed = input_->MouseKeyState(MouseKey::Middle);
    else if (vk == VK_XBUTTON1)
        pressed = input_->MouseKeyState(MouseKey::Special1);
    else if (vk == VK_XBUTTON2)
        pressed = input_->MouseKeyState(MouseKey::Special2);
    else if (vk == VK_TAB)
    {
        if (!input_->KeyboardKeyState(VK_MENU))
            pressed = input_->KeyboardKeyState(vk);
    }
    else if (vk == VK_SHIFT)
    {
        pressed = input_->KeyboardKeyState(VK_LSHIFT) || input_->KeyboardKeyState(VK_RSHIFT);
    }
    else if (vk == VK_RETURN)
    {
        pressed = input_->KeyboardSDLKeyState(SDL_SCANCODE_RETURN) ||
                  input_->KeyboardSDLKeyState(SDL_SCANCODE_KP_ENTER);
    }
    else if (vk == VK_UP)
    {
        pressed = input_->KeyboardSDLKeyState(SDL_SCANCODE_UP) ||
                  (!input_->KeyboardModState(VK_NUMLOCK) && input_->KeyboardSDLKeyState(SDL_SCANCODE_KP_8));
    }
    else if (vk == VK_DOWN)
    {
        pressed = input_->KeyboardSDLKeyState(SDL_SCANCODE_DOWN) ||
                  (!input_->KeyboardModState(VK_NUMLOCK) && input_->KeyboardSDLKeyState(SDL_SCANCODE_KP_2));
    }
    else if (vk == VK_LEFT)
    {
        pressed = input_->KeyboardSDLKeyState(SDL_SCANCODE_LEFT) ||
                  (!input_->KeyboardModState(VK_NUMLOCK) && input_->KeyboardSDLKeyState(SDL_SCANCODE_KP_4));
    }
    else if (vk == VK_RIGHT)
    {
        pressed = input_->KeyboardSDLKeyState(SDL_SCANCODE_RIGHT) ||
                  (!input_->KeyboardModState(VK_NUMLOCK) && input_->KeyboardSDLKeyState(SDL_SCANCODE_KP_6));
    }
    else if (vk == VK_NUMLOCK)
        pressed = input_->KeyboardModState(vk);
    else
        pressed = input_->KeyboardKeyState(vk);

    return pressed;
}

void PCS_CONTROLS::HandleEvent(const InputEvent &evt)
{
    if (evt.type == InputEvent::KeyboardKeyDown)
    {
        char text[5];
        int size = utf8::CodepointToUtf8(text, (uint32_t)std::get<KeyboardKey>(evt.data));
        m_KeyBuffer.AddKey(text, size, true);
    }
    else if (evt.type == InputEvent::KeyboardText)
    {
        const auto &text = std::get<std::string>(evt.data);
        m_KeyBuffer.AddKey((char *)text.c_str(), text.length(), false);
    }
    else if (evt.type == InputEvent::MouseMove)
    {
        const auto &dxdy = std::get<MousePos>(evt.data);
        fMouseMoveRestX += static_cast<float>(dxdy.x) * fMouseMoveScale;
        fMouseMoveRestY += static_cast<float>(dxdy.y) * fMouseMoveScale;
        const auto dx = static_cast<int32_t>(fMouseMoveRestX);
        const auto dy = static_cast<int32_t>(fMouseMoveRestY);
        fMouseMoveRestX -= static_cast<float>(dx);
        fMouseMoveRestY -= static_cast<float>(dy);
        nMouseDx += dx;
        nMouseDy += dy;
    }
    else if (evt.type == InputEvent::MouseWheel)
    {
        const auto &dxdy = std::get<MousePos>(evt.data);
        nMouseWheel += dxdy.y * input_->GetWheelFactor();
        core.Event("evMouseWeel", "l", static_cast<short>(dxdy.y));
    }
}
