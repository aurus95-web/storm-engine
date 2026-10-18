#include "battle_land.h"
#include "battle_man_sign.h"
#include "../message_icons.h"

#include <algorithm>
#include "core.h"
#include "message.h"
#include "string_compare.hpp"
#include "shared/battle_interface/msg_control.h"

BATTLE_LAND_INTERFACE::BATTLE_LAND_INTERFACE() : m_bShowCommandos(false)
{
    m_pRS = nullptr;
    m_pManSign = nullptr;
}

BATTLE_LAND_INTERFACE::~BATTLE_LAND_INTERFACE()
{
    Release();
}

bool BATTLE_LAND_INTERFACE::Init()
{
    m_pRS = core.GetServiceX<VDX9RENDER>();
    if (!m_pRS)
    {
        throw std::runtime_error("Can`t create render service");
    }

    SetShowParameters();
    return true;
}

void BATTLE_LAND_INTERFACE::Execute(uint32_t delta_time) const
{
    CONTROL_STATE cs;

    if (m_bShowCommandos && m_pManSign && core.Controls->IsTouchScreen() && !core.Controls->IsGamepadActive() &&
        core.GetTargetEngineVersion() <= storm::ENGINE_VERSION::PIRATES_OF_THE_CARIBBEAN)
    {
        m_pManSign->TouchControl();
    }
    else if (m_bShowCommandos && m_pManSign)
    {
        m_pManSign->LeaveTouchMode();
        if (!m_pManSign->IsActive())
        {
            core.Controls->GetControlState(BI_COMMANDS_ACTIVATE_LAND, cs);
            if (cs.state == CST_ACTIVATED)
            {
                m_pManSign->SetActive(true);
            }
        }
        else
        {
            m_pManSign->MakeControl();
        }
    }
}

void BATTLE_LAND_INTERFACE::Realize(uint32_t delta_time)
{
    if (m_bShowCommandos)
    {
        m_pRS->MakePostProcess();
        if (m_pManSign)
            m_pManSign->Draw();
        if (m_pMessageIcons)
        {
            // the scripts send the icons of the officers (MSG_BATTLE_LAND_SET_MSGICONS)
            if ((m_nMessageIconsRequestTime += delta_time) > 1000)
            {
                m_nMessageIconsRequestTime = 0;
                core.Event("blieGetMsgIconRoot");
            }
            m_pMessageIcons->Update(delta_time);
            m_pMessageIcons->Draw();
        }

        m_Images.Draw();
        BIUtils::PrintTextInfoArray(m_TextInfo);
    }
}

uint64_t BATTLE_LAND_INTERFACE::ProcessMessage(MESSAGE &message)
{
    switch (message.Long())
    {
    case MSG_BATTLE_LAND_START:
        AddPotcCommandTextures();
        if (m_pManSign)
            m_pManSign->SetUpdate();
        break;

    case MSG_BATTLE_LAND_END:
        EndShow();
        break;

    case BI_IN_SET_COMMAND_MODE:
        if (m_bShowCommandos && m_pManSign)
        {
            auto comMode = message.Long();
            auto startTextureNumber = message.Long();
            auto startPictureNumber = message.Long();
            auto characterNum = message.Long();
            // Pirates of the Caribbean scripts close the menu (mode 0) or refresh it (mode -1)
            if (core.GetTargetEngineVersion() <= storm::ENGINE_VERSION::PIRATES_OF_THE_CARIBBEAN)
            {
                if (comMode == 0)
                    m_pManSign->SetActive(false);
                else if (comMode == -1)
                    m_pManSign->SetUpdate();
            }
        }
        break;

    case MSG_BATTLE_LAND_SET_SHOW:
        m_bShowCommandos = (message.Long() != 0);
        break;

    case MSG_BATTLE_LAND_SET_MSGICONS:
        EnableMessageIcons(message.ScriptVariablePointer());
        break;

    case BI_MSG_ADD_NEWTEXTURE: {
        const std::string &param = message.String();
        const int hQ = message.Long();
        const int vQ = message.Long();
        if (m_pManSign)
            return m_pManSign->AddTexture(param.c_str(), hQ, vQ);
        return -1;
    }
    break;

    case MSG_BATTLE_LAND_MAKE_COMMAND: {
        const std::string &param = message.String();
        if (storm::iEquals(param, "cancel"))
        {
            if (m_pManSign)
                m_pManSign->ExecuteCommand(BI_MSG_COMMAND_DEACTIVATE);
        }
    }
    break;

    case BI_MSG_CARE_COMMANDLIST_UPDATE:
        if (m_pManSign)
            m_pManSign->SetUpdate();
        break;
    }
    return 0;
}

void BATTLE_LAND_INTERFACE::SetParameters()
{
    UpdateCommandos();
    UpdateAlarm();
}

void BATTLE_LAND_INTERFACE::Release()
{
    m_pRS = nullptr;
    STORM_DELETE(m_pManSign);
    STORM_DELETE(m_pMessageIcons);

    m_TextInfo.clear();
}

void BATTLE_LAND_INTERFACE::EndShow()
{
}

void BATTLE_LAND_INTERFACE::SetShowParameters()
{
    auto *const pA = AttributesPointer ? AttributesPointer->GetAttributeClass("Parameters") : nullptr;
    m_bShowCommandos = 0 != BIUtils::GetIntFromAttr(pA, "DoShowCommandos", true);

    // Pirates of the Caribbean scripts have no character sign (ManSign), its parameters are converted from
    // theirs; without the attributes it is drawn as an untextured square in the corner of the screen
    ATTRIBUTES *pManSignA = nullptr;
    if (core.GetTargetEngineVersion() <= storm::ENGINE_VERSION::PIRATES_OF_THE_CARIBBEAN)
        pManSignA = ConvertPotcParameters();
    if (!pManSignA && AttributesPointer)
        pManSignA = AttributesPointer->GetAttributeClass("ManSign");
    if (pManSignA || core.GetTargetEngineVersion() > storm::ENGINE_VERSION::PIRATES_OF_THE_CARIBBEAN)
    {
        m_pManSign = new BIManSign(GetId(), m_pRS);
        Assert(m_pManSign);
        m_pManSign->Init(AttributesPointer, pManSignA);
    }

    BIUtils::FillTextInfoArray(m_pRS, AttributesPointer ? AttributesPointer->GetAttributeClass("textinfo") : nullptr,
                               m_TextInfo);

    m_Images.Init(m_pRS, AttributesPointer ? AttributesPointer->GetAttributeClass("imageslist") : nullptr);

    auto *pIconsA = AttributesPointer ? AttributesPointer->GetAttributeClass("MessageIcons") : nullptr;
    if (pIconsA && m_nMessageIconBottom >= 0 &&
        core.GetTargetEngineVersion() <= storm::ENGINE_VERSION::PIRATES_OF_THE_CARIBBEAN)
    {
        STORM_DELETE(m_pMessageIcons);
        std::fill(std::begin(m_aMessageIconAttr), std::end(m_aMessageIconAttr), nullptr);
        m_pMessageIcons = new MESSAGE_ICONS;
        if (m_pMessageIcons->InitData(GetId(), m_pRS, pIconsA))
            // as wide as the portraits, the proportions of the scripts
            m_pMessageIcons->SetLayout(m_nMessageIconBottom, m_nMessageIconWidth,
                                       m_nMessageIconWidth * static_cast<int32_t>(pIconsA->GetAttributeAsDword("IconHeight", 24)) /
                                           std::max(1, static_cast<int32_t>(pIconsA->GetAttributeAsDword("IconWidth", 64))));
        else
            STORM_DELETE(m_pMessageIcons);
        m_nMessageIconsRequestTime = 1000; // at once
    }
}

void BATTLE_LAND_INTERFACE::UpdateCommandos() const
{
    auto *pA = AttributesPointer ? AttributesPointer->GetAttributeClass("data") : nullptr;
    if (pA)
        pA = pA->GetAttributeClass("icons");
    if (!pA)
        return;
}

void BATTLE_LAND_INTERFACE::UpdateAlarm()
{
}

void BATTLE_LAND_INTERFACE::EnableMessageIcons(VDATA *pvdat)
{
    if (!m_pMessageIcons)
        return;
    m_pMessageIcons->SetShowMsg(pvdat != nullptr);
    if (!pvdat)
        return;
    // the icons of the officers above their portraits
    ATTRIBUTES *pAttr[4];
    for (int32_t i = 0; i < 4; i++)
        pAttr[i] = i < static_cast<int32_t>(pvdat->GetElementsNum()) ? pvdat->GetAClass(i) : nullptr;
    // the scripts send them every second: restarting would drop the falling and fading icons
    if (std::equal(pAttr, pAttr + 4, m_aMessageIconAttr))
        return;
    std::copy(pAttr, pAttr + 4, m_aMessageIconAttr);
    m_pMessageIcons->StartData(pAttr, m_aMessageIconLeft);
}

void BATTLE_LAND_INTERFACE::DisableMessageIcons()
{
}

void BATTLE_LAND_INTERFACE::SetTextData()
{
}

// Pirates of the Caribbean: "Parameters" (officer portraits) and "CommandShowParam" (command menu) are
// converted to "ManSign" and "CommandList"; the scripts set them before creating the entity and delete
// all the attributes right after it
ATTRIBUTES *BATTLE_LAND_INTERFACE::ConvertPotcParameters()
{
    auto *const pParam = AttributesPointer ? AttributesPointer->GetAttributeClass("Parameters") : nullptr;
    if (!pParam)
        return nullptr;
    auto *const pShow = AttributesPointer->GetAttributeClass("CommandShowParam");

    const auto scriptHeight = static_cast<int32_t>(pParam->GetAttributeAsDword("heightCom", 64));
    // touch screens: the portraits and the commands as large as the on-screen buttons
    int32_t screenHeight;
    const auto k = BIUtils::GetTouchIconScale(scriptHeight, screenHeight);
    const auto sc = [k](int32_t v) { return static_cast<int32_t>(std::lround(v * k)); };
    const auto width = sc(static_cast<int32_t>(pParam->GetAttributeAsDword("widthCom", 64)));
    const auto height = sc(scriptHeight);
    const auto left = sc(static_cast<int32_t>(pParam->GetAttributeAsDword("leftPosCom", 16)));
    auto top = static_cast<int32_t>(pParam->GetAttributeAsDword("topPosCom", 520));
    if (screenHeight > 0) // the same gap below them, larger
        top = screenHeight - sc(screenHeight - top - scriptHeight) - height;
    const auto distance = sc(static_cast<int32_t>(pParam->GetAttributeAsDword("distCom", 8)));
    const auto healthWidth = pParam->GetAttribute("widthHealth") ? sc(pParam->GetAttributeAsDword("widthHealth", 64)) : width;
    // touch screens: thinner bars under the larger icons, as above the ships
    const auto healthHeight = static_cast<int32_t>(
        std::lround(sc(static_cast<int32_t>(pParam->GetAttributeAsDword("heightHealth", 16))) * (k > 1.f ? 0.6f : 1.f)));
    const auto healthDistance = sc(static_cast<int32_t>(pParam->GetAttributeAsDword("distHealth", 4)));

    // replaces the attributes of the older versions kept in the saved games
    if (auto *old = AttributesPointer->GetAttributeClass("ManSign"))
        AttributesPointer->DeleteAttributeClassX(old);
    auto &sign = AttributesPointer->CreateAttribute("ManSign");
    const auto set = [](ATTRIBUTES &a, const char *name, const std::string &value) {
        a.SetAttribute(name, value.c_str());
    };
    // the values are read with atol: signed, as the scripts write them
    const auto setInt = [](ATTRIBUTES &a, const char *name, uint32_t value) {
        a.SetAttribute(name, std::to_string(static_cast<int32_t>(value)).c_str());
    };
    // portraits in a row, the health (red) and pistol charge (blue) bars of officerstate.tga above them
    for (int32_t n = 0; n < 4; n++)
        set(sign, ("iconoffset" + std::to_string(n + 1)).c_str(),
            std::to_string(left + n * (width + distance) + width / 2) + "," + std::to_string(top + height / 2));
    set(sign, "manfaceiconsize", std::to_string(width) + "," + std::to_string(height));
    setInt(sign, "manfacecolor", ARGB(255, 128, 128, 128));
    if (const char *texture = pParam->GetAttribute("stateTexture"))
        set(sign, "manstatetexturename", texture);
    setInt(sign, "manstatecolor", ARGB(255, 128, 128, 128));
    const auto barSize = std::to_string(healthWidth) + "," + std::to_string(healthHeight);
    // the health bar is right above the portrait, the pistol charge bar above it
    set(sign, "manhpuv", "0.0,0.25,1.0,0.5");
    set(sign, "manhpoffset", "0," + std::to_string(-height / 2 - healthDistance - healthHeight / 2));
    set(sign, "manhpiconsize", barSize);
    set(sign, "manenegryuv", "0.0,0.0,1.0,0.25");
    set(sign, "manenegryoffset", "0," + std::to_string(-height / 2 - healthDistance - healthHeight * 3 / 2));
    set(sign, "manenergyiconsize", barSize);
    // the dark empty track under every bar
    set(sign, "manstatebackuv", "0.0,0.5,1.0,0.75");
    // and the pistol bullets (loaded and empty) in a row above the charge bar
    const auto bulletSize = sc(static_cast<int32_t>(pParam->GetAttributeAsDword("GunShootHeight", 16)));
    const auto bulletSpace = sc(static_cast<int32_t>(pParam->GetAttributeAsDword("GunShootSpace", 2)));
    if (const char *texture = pParam->GetAttribute("gunChargeTexture"))
        set(sign, "gunchargetexturename", texture);
    setInt(sign, "gunchargecolor", ARGB(255, 128, 128, 128));
    setInt(sign, "gunchargebullets", 8);
    setInt(sign, "gunchargebulletspace", bulletSpace);
    set(sign, "gunchargeoffset", "0," + std::to_string(-height / 2 - healthDistance - healthHeight * 2 - bulletSpace -
                                                       bulletSize / 2));
    set(sign, "gunchargeiconsize", std::to_string(bulletSize) + "," + std::to_string(bulletSize));
    // the message icons (the level up...) above the portraits, the bars and the bullets
    for (int32_t n = 0; n < 4; n++)
        m_aMessageIconLeft[n] = left + n * (width + distance);
    m_nMessageIconWidth = width;
    m_nMessageIconBottom = top - healthDistance - healthHeight * 2 - bulletSpace - bulletSize - healthDistance;
    // no back and alarm icons: they are drawn untextured otherwise
    set(sign, "backiconsize", "0,0");
    set(sign, "alarmiconsize", "0,0");
    // in danger the portrait of the hero blinks red, slowly lighting up and fading
    setInt(sign, "alarmfacecolor", ARGB(255, 210, 80, 80)); // modulate2x: gray 128 is the face as is
    set(sign, "alarmuptime", "0.8");
    set(sign, "alarmdowntime", "0.8");
    // the command menu is to the right of the portraits row
    setInt(sign, "commandlistverticaloffset", static_cast<uint32_t>(-height / 2));
    setInt(sign, "commandlistrightgap", distance + 4);
    setInt(sign, "horizontalbars", 1);

    // replaces the attributes of the older versions kept in the saved games
    if (auto *old = AttributesPointer->GetAttributeClass("CommandList"))
        AttributesPointer->DeleteAttributeClassX(old);
    auto &list = AttributesPointer->CreateAttribute("CommandList");
    if (pShow)
    {
        setInt(list, "CommandIconLeft", left + width + distance);
        setInt(list, "CommandIconWidth", pShow->GetAttribute("iconWidth") ? sc(pShow->GetAttributeAsDword("iconWidth", 64)) : width);
        setInt(list, "CommandIconHeight", pShow->GetAttribute("iconHeight") ? sc(pShow->GetAttributeAsDword("iconHeight", 64)) : height);
        setInt(list, "CommandIconSpace", sc(static_cast<int32_t>(pShow->GetAttributeAsDword("iconDistance", 4))));
        BIUtils::SetPotcCommandArrows(list, static_cast<int32_t>(pShow->GetAttributeAsDword("maxShowQuantity", 5)),
                                      static_cast<int32_t>(list.GetAttributeAsDword("CommandIconWidth", width)),
                                      static_cast<int32_t>(list.GetAttributeAsDword("CommandIconHeight", height)),
                                      static_cast<int32_t>(list.GetAttributeAsDword("CommandIconSpace", 4)));
        if (const char *font = pShow->GetAttribute("commandNoteFont"))
            set(list, "CommandNoteFont", font);
        // PotC counts the note offset from the top of the icon, the engine from its centre
        set(list, "CommandNoteOffset",
            std::to_string(sc(static_cast<int32_t>(pShow->GetAttributeAsDword("noteXOffset", 0)))) + "," +
                std::to_string(sc(static_cast<int32_t>(pShow->GetAttributeAsDword("noteYOffset", -30)) -
                                  static_cast<int32_t>(pShow->GetAttributeAsDword("iconHeight", scriptHeight)) / 2)));
    }
    setInt(list, "CommandNoteColor", ARGB(255, 255, 255, 255));

    return &sign;
}

void BATTLE_LAND_INTERFACE::AddPotcCommandTextures()
{
    if (m_bPotcCommandTexturesAdded || !m_pManSign ||
        core.GetTargetEngineVersion() > storm::ENGINE_VERSION::PIRATES_OF_THE_CARIBBEAN)
        return;
    auto *const pTextures = AttributesPointer ? AttributesPointer->GetAttributeClass("CommandTextures") : nullptr;
    auto *const pA = pTextures ? pTextures->GetAttributeClass("list") : nullptr;
    if (!pA)
        return;
    // in the order of texNum: t0, t1...
    for (size_t n = 0; n < pA->GetAttributesNum(); n++)
    {
        auto *const pT = pA->GetAttributeClass(n);
        if (pT && pT->GetAttribute("name"))
            m_pManSign->AddTexture(pT->GetAttribute("name"), pT->GetAttributeAsDword("xsize", 1),
                                   pT->GetAttributeAsDword("ysize", 1));
    }
    m_pManSign->SetPotcPictures(static_cast<int32_t>(pTextures->GetAttributeAsDword("CommandTexNum", 0)));
    m_bPotcCommandTexturesAdded = true;
}
