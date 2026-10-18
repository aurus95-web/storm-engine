#include <stdio.h>

#include "i_battle.h"

#include "core.h"
#include "vma.hpp"

#include "ship_pointer.h"
#include "battle_ship_sign.h"
#include "island_descr.h"
#include "math_inlines.h"

#include "../land/i_boarding_status.h"
#include "../land/battle_land.h"

#include "../active_perk_shower.h"
#include "../log_and_action.h"
#include "../item_entity/item_entity.h"
#include "../world_map_interface/interface.h"
#include "../spyglass/spyglass.h"
#include "../timer/timer.h"

#include "shared/battle_interface/msg_control.h"

#include "../interface_manager/interface_manager.h"

#include "ship_info_images.h"
#include "string_compare.hpp"

CREATE_CLASS(BATTLE_INTERFACE)

CREATE_CLASS(ILogAndActions)

CREATE_CLASS(IBoardingStatus)

CREATE_CLASS(BATTLE_LAND_INTERFACE)

CREATE_CLASS(ISPYGLASS)

CREATE_CLASS(TELESCOPE)

CREATE_CLASS(SHIPPOINTER)

CREATE_CLASS(ActivePerkShower)

CREATE_CLASS(BITimer)

CREATE_CLASS(ItemEntity)

CREATE_CLASS(WM_INTERFACE)

CREATE_CLASS(BI_InterfaceManager)

SHIP_DESCRIBE_LIST g_ShipList;
ISLAND_DESCRIBER g_IslandDescr;

#define MINIMAP_ZOOM_IN "MiniMapZoomIn"
#define MINIMAP_ZOOM_OUT "MiniMapZoomOut"

BATTLE_INTERFACE::BATTLE_INTERFACE()
{
    g_IslandDescr.ReleaseAll();
    g_ShipList.ReleaseAll();
    // m_pMessageIcons = null;
    m_pShipIcon = nullptr;
    rs = nullptr;
    m_bShowCommandMenu = true;
    m_bShowBattleNavigator = true;
    m_bYesShowAll = false;
    m_bVisible = true;
    m_fBlinkSpeed = 0.f;
    m_fCurBlinkTime = 0.f;
    m_bNeedIslandSet = true;
    m_bMyShipView = false;
    m_bShowBattleBorder = false;
    m_pShipInfoImages = nullptr;
}

BATTLE_INTERFACE::~BATTLE_INTERFACE()
{
    // STORM_DELETE(m_pMessageIcons);
    STORM_DELETE(m_pShipIcon);
    STORM_DELETE(m_pShipInfoImages);
}

bool BATTLE_INTERFACE::Init()
{
    BIUtils::idBattleInterface = GetId();

    if ((rs = core.GetServiceX<VDX9RENDER>()) == nullptr)
    {
        throw std::runtime_error("Can`t create render service");
    }

    LoadIniFile();

    return true;
}

void BATTLE_INTERFACE::Execute(uint32_t delta_time)
{
    m_fCurBlinkTime += delta_time * m_fBlinkSpeed;
    if (m_fCurBlinkTime > PIm2)
        m_fCurBlinkTime -= PIm2;
    BIUtils::g_dwBlinkColor = static_cast<uint32_t>(255 * fabsf(sinf(m_fCurBlinkTime))) & 0xFF;
    CheckSeaState();
    if (m_bYesShowAll && m_bVisible)
    {
        CONTROL_STATE cs;

        if (m_bShowBattleNavigator)
        {
            core.Controls->GetControlState(MINIMAP_ZOOM_IN, cs);
            if (cs.state == CST_ACTIVE)
                BattleNavigator.DecrementScale();
            core.Controls->GetControlState(MINIMAP_ZOOM_OUT, cs);
            if (cs.state == CST_ACTIVE)
                BattleNavigator.IncrementScale();
        }

        if (m_bShowCommandMenu && m_pShipIcon && core.Controls->IsTouchScreen() &&
            !core.Controls->IsGamepadActive() &&
            core.GetTargetEngineVersion() <= storm::ENGINE_VERSION::PIRATES_OF_THE_CARIBBEAN)
        {
            m_pShipIcon->TouchControl();
        }
        else if (m_bShowCommandMenu && m_pShipIcon)
        {
            m_pShipIcon->LeaveTouchMode();
            if (!m_pShipIcon->IsActive())
            {
                core.Controls->GetControlState(BI_COMMANDS_ACTIVATE_SEA, cs);
                if (cs.state == CST_ACTIVATED)
                {
                    m_pShipIcon->SetActive(true);
                }
            }
            else
            {
                m_pShipIcon->MakeControl();
            }
        }
        if (m_bShowBattleNavigator)
            BattleNavigator.Update();

        /*if(m_pMessageIcons)
        {
          if( !m_pShipIcon || !m_pShipIcon->IsActive() )
          {
            if(!m_bMyShipView) core.Event("blieGetMsgIconRoot");
            m_bMyShipView = true;
          }
          else
          {
            if(m_bMyShipView) m_pMessageIcons->SetShowMsg(false);
            m_bMyShipView = false;
          }
        }*/
    }
    // if(m_pMessageIcons) m_pMessageIcons->Update(delta_time);
}

void BATTLE_INTERFACE::Realize(uint32_t delta_time)
{
    if (core.Controls->GetDebugAsyncKeyState('K') < 0)
        return;
    if (m_bNeedIslandSet)
    {
        core.Trace("Island Set");
        BattleNavigator.SetIsland();
        m_bNeedIslandSet = false;
    }

    if (m_bYesShowAll)
    {
        if (m_bVisible)
        {
            rs->MakePostProcess();

            if (m_bShowBattleNavigator && m_pShipInfoImages)
                m_pShipInfoImages->Draw();

            if (m_bShowBattleNavigator)
                m_ImagesInfo.Draw();

            if (m_bShowCommandMenu)
                m_pShipIcon->Draw();
            if (m_bShowBattleNavigator)
                BattleNavigator.Draw();
            // if( m_pMessageIcons ) m_pMessageIcons->Draw();

            if (m_bShowBattleNavigator)
            {
                BIUtils::PrintTextInfoArray(m_TextArray);
                m_LinesInfo.Draw();
                if (m_bShowBattleBorder)
                    m_BattleBorder.Draw();
            }
        }
    }
    else
    {
        m_bYesShowAll = true;
    }
}

void BATTLE_INTERFACE::LoadIniFile()
{
    m_fBlinkSpeed = .003f;
    if (AttributesPointer != nullptr)
        m_fBlinkSpeed = AttributesPointer->GetAttributeAsFloat("blindSpeed", m_fBlinkSpeed);
    m_fCurBlinkTime = 0;

    BattleNavigator.Init(rs, this);

    ATTRIBUTES *pA = nullptr;
    if (AttributesPointer != nullptr)
    {
        m_bShowCommandMenu = AttributesPointer->GetAttributeAsDword("ShowCommands", 1) != 0;
        m_bShowBattleNavigator = AttributesPointer->GetAttributeAsDword("ShowNavigator", 1) != 0;
        pA = AttributesPointer->FindAClass(AttributesPointer, "MessageIcons");

        BIUtils::FillTextInfoArray(rs, AttributesPointer->GetAttributeClass("TextInfo"), m_TextArray);
        m_LinesInfo.Init(rs, AttributesPointer->GetAttributeClass("LineInfo"));

        m_BattleBorder.Init(rs, AttributesPointer->GetAttributeClass("battleborder"));

        m_ImagesInfo.Init(rs, AttributesPointer->GetAttributeClass("imageslist"));
    }

    /*STORM_DELETE( m_pMessageIcons );
    m_pMessageIcons = new MESSAGE_ICONS;
    if(m_pMessageIcons==NULL) {
      throw std::runtime_error("allocate memory error");
    }
    if(m_pMessageIcons)    m_pMessageIcons->InitData(GetId(),rs,pA);*/

    STORM_DELETE(m_pShipIcon);
    m_pShipIcon = new BIShipIcon(GetId(), rs);
    Assert(m_pShipIcon);
    // Pirates of the Caribbean: converted every time, the saved games keep the attributes of older versions
    ATTRIBUTES *pShipIconA = nullptr;
    if (core.GetTargetEngineVersion() <= storm::ENGINE_VERSION::PIRATES_OF_THE_CARIBBEAN)
        pShipIconA = ConvertPotcParameters();
    if (!pShipIconA && AttributesPointer)
        pShipIconA = AttributesPointer->GetAttributeClass("ShipIcon");
    m_pShipIcon->Init(AttributesPointer, pShipIconA);

    m_pShipInfoImages =
        new ShipInfoImages(rs, AttributesPointer ? AttributesPointer->GetAttributeClass("ShipInfoImages") : nullptr);
    if (m_pShipInfoImages)
    {
        m_pShipInfoImages->SetVisible(
            AttributesPointer ? (AttributesPointer->GetAttributeAsDword("ShifInfoVisible", 0) != 0) : false);
    }
}

// Pirates of the Caribbean: "CommandShowParam" (ship icons and command menu) is converted to "ShipIcon" and
// "CommandList"; the scripts set it before creating the entity and delete it right after
ATTRIBUTES *BATTLE_INTERFACE::ConvertPotcParameters()
{
    auto *const pShow = AttributesPointer ? AttributesPointer->GetAttributeClass("CommandShowParam") : nullptr;
    if (!pShow)
        return nullptr;
    const auto set = [](ATTRIBUTES &a, const char *name, const std::string &value) {
        a.SetAttribute(name, value.c_str());
    };
    // the values are read with atol: signed, as the scripts write them
    const auto setInt = [](ATTRIBUTES &a, const char *name, uint32_t value) {
        a.SetAttribute(name, std::to_string(static_cast<int32_t>(value)).c_str());
    };
    const auto scriptHeight = static_cast<int32_t>(pShow->GetAttributeAsDword("iconHeight", 64));
    // touch screens: the ships and the commands as large as the on-screen buttons
    int32_t screenHeight;
    const auto k = BIUtils::GetTouchIconScale(scriptHeight, screenHeight);
    const auto sc = [k](int32_t v) { return static_cast<int32_t>(std::lround(v * k)); };
    const auto width = sc(static_cast<int32_t>(pShow->GetAttributeAsDword("iconWidth", 64)));
    const auto height = sc(scriptHeight);
    const auto left = sc(static_cast<int32_t>(pShow->GetAttributeAsDword("leftIconsOffset", 16)));
    auto top = static_cast<int32_t>(pShow->GetAttributeAsDword("downIconsOffset", 520));
    if (screenHeight > 0) // the same gap below them, larger
        top = screenHeight - sc(screenHeight - top - scriptHeight) - height;
    const auto distance = sc(static_cast<int32_t>(pShow->GetAttributeAsDword("iconDistance", 4)));
    const auto stateWidth = pShow->GetAttribute("shipStateWidth") ? sc(pShow->GetAttributeAsDword("shipStateWidth", 64)) : width;
    // touch screens: thinner bars under the larger icons
    const auto stateHeight = static_cast<int32_t>(
        std::lround(sc(static_cast<int32_t>(pShow->GetAttributeAsDword("shipStateHeight", 16))) * (k > 1.f ? 0.6f : 1.f)));

    // replaces the attributes of the older versions kept in the saved games
    if (auto *old = AttributesPointer->GetAttributeClass("ShipIcon"))
        AttributesPointer->DeleteAttributeClassX(old);
    auto &icon = AttributesPointer->CreateAttribute("ShipIcon");
    // the ships in a row, the sails (white), crew (blue) and hull (red) bars of indicators.tga above them
    for (int32_t n = 0; n < 8; n++)
        set(icon, ("iconoffset" + std::to_string(n + 1)).c_str(),
            std::to_string(left + n * (width + distance) + width / 2) + "," + std::to_string(top + height / 2));
    // the ship pictures are in the textures of the commands (most ships in t2, the galleons and some others in their own
    // ones), the selected pictures are to the right of their grids
    if (auto *pList = AttributesPointer->FindAClass(AttributesPointer, "CommandTextures.list"))
    {
        auto &textures = icon.CreateAttribute("ShipTextures");
        for (size_t i = 0; i < pList->GetAttributesNum(); i++)
        {
            auto *pT = pList->GetAttributeClass(i);
            if (!pT || !pT->GetAttribute("name"))
                continue;
            auto &t = textures.CreateAttribute(pT->GetThisName());
            set(t, "name", pT->GetAttribute("name"));
            setInt(t, "xsize", pT->GetAttributeAsDword("xsize", 1) * 2);
            setInt(t, "ysize", pT->GetAttributeAsDword("ysize", 1));
        }
    }
    auto *pTexture = AttributesPointer->FindAClass(AttributesPointer, "CommandTextures.list.t2");
    if (pTexture && pTexture->GetAttribute("name"))
    {
        set(icon, "shiptexturename", pTexture->GetAttribute("name"));
        setInt(icon, "xsize", pTexture->GetAttributeAsDword("xsize", 1) * 2);
        setInt(icon, "ysize", pTexture->GetAttributeAsDword("ysize", 1));
    }
    set(icon, "shipiconsize", std::to_string(width) + "," + std::to_string(height));
    setInt(icon, "shipcolor", ARGB(255, 128, 128, 128));
    if (const char *texture = pShow->GetAttribute("shipStateTexture"))
        set(icon, "shipstatetexturename", texture);
    setInt(icon, "shipstatecolor", ARGB(255, 128, 128, 128));
    const auto barSize = std::to_string(stateWidth) + "," + std::to_string(stateHeight);
    set(icon, "shiphpuv", "0.0,0.333,1.0,0.5");
    // from the top: the sails (white), the crew (blue), the hull (red)
    set(icon, "shiphpoffset", "0," + std::to_string(-height / 2 - stateHeight / 2));
    set(icon, "shiphpiconsize", barSize);
    set(icon, "shipspuv", "0.0,0.0,1.0,0.167");
    set(icon, "shipspoffset", "0," + std::to_string(-height / 2 - stateHeight * 5 / 2));
    set(icon, "shipspiconsize", barSize);
    set(icon, "shipcrewuv", "0.0,0.167,1.0,0.333");
    // the dark empty track under every bar
    set(icon, "shipstatebackuv", "0.0,0.5,1.0,0.667");
    set(icon, "shipcrewoffset", "0," + std::to_string(-height / 2 - stateHeight * 3 / 2));
    set(icon, "shipcrewiconsize", barSize);
    // no back and ship class icons: they are drawn untextured otherwise
    set(icon, "backiconsize", "0,0");
    set(icon, "shipclassiconsize", "0,0");
    // no crew quantity and ship name: the icon prints them always, out of the screen
    set(icon, "sailorfontoffset", "0,-10000");
    set(icon, "shipnamefontoffset", "0,-10000");
    // the command menu is to the right of the ships row
    setInt(icon, "commandlistverticaloffset", static_cast<uint32_t>(-height / 2));
    setInt(icon, "commandlistrightgap", distance + 4);
    setInt(icon, "horizontalbars", 1);

    // replaces the attributes of the older versions kept in the saved games
    if (auto *old = AttributesPointer->GetAttributeClass("CommandList"))
        AttributesPointer->DeleteAttributeClassX(old);
    auto &list = AttributesPointer->CreateAttribute("CommandList");
    setInt(list, "CommandIconLeft", left + width + distance + 4);
    setInt(list, "CommandIconWidth", width);
    setInt(list, "CommandIconHeight", height);
    setInt(list, "CommandIconSpace", distance);
    BIUtils::SetPotcCommandArrows(list, static_cast<int32_t>(pShow->GetAttributeAsDword("maxShowQuantity", 5)), width,
                                  height, distance);
    // the state bars of the ships among the commands (as above the own ships)
    if (const char *texture = pShow->GetAttribute("shipStateTexture"))
        set(list, "ShipStateTexture", texture);
    if (const char *font = pShow->GetAttribute("commandNoteFont"))
        set(list, "CommandNoteFont", font);
    // PotC counts the note offset from the top of the icon, the engine from its centre
    set(list, "CommandNoteOffset",
        std::to_string(sc(static_cast<int32_t>(pShow->GetAttributeAsDword("noteXOffset", 0)))) + "," +
            std::to_string(sc(static_cast<int32_t>(pShow->GetAttributeAsDword("noteYOffset", -30)) - 34) -
                           height / 2)); // and above the nation flags of the ships
    setInt(list, "CommandNoteColor", ARGB(255, 255, 255, 255));

    return &icon;
}

uint32_t BATTLE_INTERFACE::AttributeChanged(ATTRIBUTES *pAttr)
{
    return 0;
}

uint64_t BATTLE_INTERFACE::ProcessMessage(MESSAGE &message)
{
    switch (message.Long())
    {
    case BI_MSG_SET_ISLAND:
        m_bNeedIslandSet = true;
        g_IslandDescr.SetIsland(message.AttributePointer());
        m_bMyShipView = false;
        break;
    case BI_IN_CREATE_SHIP: // "laallll"
    {
        const auto chIdx = message.Long();
        auto *const pChAttr = message.AttributePointer();
        auto *const pShipAttr = message.AttributePointer();
        const auto bMyShip = (message.Long() != 0L);
        const auto relation = message.Long();
        const uint32_t dwShipColor = message.ParamValid() ? message.Long() : 0;
        const bool bTransferableShip = message.ParamValid() ? (message.Long() != 0) : bMyShip;
        g_ShipList.Add(AttributesPointer ? AttributesPointer->GetAttributeAsDword("MainChrIndex", -1) : -1, chIdx,
                       pChAttr, pShipAttr, bMyShip, bTransferableShip, relation, dwShipColor);
        if (m_pShipIcon)
            m_pShipIcon->SetUpdate();
    }
    break;
    case BI_IN_DELETE_SHIP: // "ll"
        g_ShipList.Release(message.Long());
        if (m_pShipIcon)
            m_pShipIcon->SetUpdate();
        break;
    case BI_IN_DEAD_SHIP: // "l"
        g_ShipList.ShipSink(message.Long());
        if (m_pShipIcon)
            m_pShipIcon->SetUpdate();
        break;
    case BI_IN_SET_COMMAND_MODE: // "lllll"
    {
        auto comMode = message.Long();
        auto startTextureNumber = message.Long();
        auto startPictureNumber = message.Long();
        auto characterNum = message.Long();
        auto commandState = message.Long();
        // Pirates of the Caribbean: the commands of a companion chosen with the companion command
        if (m_pShipIcon && comMode == BI_COMMODE_COMMAND_SELECT && characterNum >= 0 &&
            core.GetTargetEngineVersion() <= storm::ENGINE_VERSION::PIRATES_OF_THE_CARIBBEAN)
            m_pShipIcon->SetCommandCharacter(characterNum);
    }
    break;
    case BI_SET_VISIBLE: // "ll"
        m_bVisible = (0 != message.Long());
        break;
    case BI_SET_ISLAND_LOCATORS: // "la"
        g_IslandDescr.SetIsland(message.AttributePointer());
        break;
    case BI_MSG_NOT_ENOUGH_BALLS_FLAG: // "l"
        BattleNavigator.SetEnoughBalls(message.Long() != 0);
        break;
    case BI_MSG_REFRESH:
        g_ShipList.Refresh();
        g_IslandDescr.Refresh();
        if (m_pShipIcon)
            m_pShipIcon->SetUpdate();
        break;

    case BI_MSG_SET_MSG_ICONS:
        /*if(m_pMessageIcons)
        {
          EnableMessageIcons(message.ScriptVariablePointer());
        }*/
        break;

    case BI_MSG_ADD_NEWTEXTURE: {
        const std::string &param = message.String();
        const int hQ = message.Long();
        const int vQ = message.Long();
        return m_pShipIcon->AddTexture(param.c_str(), hQ, vQ);
    }
    break;

    case MSG_BATTLE_LAND_MAKE_COMMAND: {
        const std::string &param = message.String();
        if (storm::iEquals(param, "cancel"))
        {
            if (m_pShipIcon)
                m_pShipIcon->ExecuteCommand(BIShipIcon::Command_cancel);
        }
    }
    break;
    }
    return 0;
}

void BATTLE_INTERFACE::CheckSeaState()
{
    auto *main_sd = g_ShipList.GetMainCharacterShip();
    if (main_sd == nullptr)
        return;

    int32_t nReloadTargetIndex = -1;
    auto sqrRadius = core.Entity_GetAttributeAsFloat(BIUtils::idBattleInterface, "boardRadius", 0.f);
    sqrRadius *= sqrRadius;
    auto minReloadRadius = sqrRadius;
    auto sqrFreeDistance = core.Entity_GetAttributeAsFloat(BIUtils::idBattleInterface, "freeDistance", 500.f);
    sqrFreeDistance *= sqrFreeDistance;

    auto bSailTo = false, bLandTroops = false, bMap = true;
    auto bAttack = false, bDefend = false, bReload = false;

    for (auto *ps = g_ShipList.GetShipRoot(); ps != nullptr; ps = ps->next)
    {
        if (ps == main_sd)
            continue;
        bSailTo = true;
        if (ps->isTransferableShip)
        {
            float curRad;
            if ((curRad = ~(ps->pShip->GetPos() - main_sd->pShip->GetPos())) < sqrRadius)
            {
                bReload = true;
                if (curRad < minReloadRadius)
                {
                    minReloadRadius = curRad;
                    nReloadTargetIndex = ps->characterIndex;
                }
            }
        }
        if (ps->isMyShip)
            bDefend = true;
        else
        {
            if (ps->relation == BI_RELATION_ENEMY)
            {
                if (!ps->isDead)
                {
                    bAttack = true;
                    float fdist;
                    if ((fdist = ~(ps->pShip->GetPos() - main_sd->pShip->GetPos())) < sqrFreeDistance)
                        bMap = false;
                }
            }
        }
    }

    if (g_IslandDescr.GetFirstLocator() != nullptr)
        bSailTo = true;

    if (g_IslandDescr.GetFirstEnemyFort() != nullptr)
        bAttack = true;

    core.Event(BI_EVENT_SET_SEA_STATE, "lllllll", bSailTo, bLandTroops, bMap, bAttack, bDefend, bReload,
               nReloadTargetIndex);
    m_bShowBattleBorder = !bMap;
}

void BATTLE_INTERFACE::EnableMessageIcons(VDATA *pvdat)
{
    /*if(!m_pMessageIcons) return;
    m_pMessageIcons->SetShowMsg(false);
    if(!pvdat) return;
    m_pMessageIcons->SetShowMsg(true);

    ATTRIBUTES * pAttr[4];
    int32_t pLeft[4];

    int nCommandos = 0;

    for(int i=0; i<4; i++)
    {
      pAttr[i] = null;
      if(i<nCommandos)    pAttr[i] = pvdat->GetAClass(i);
    }

    m_pMessageIcons->StartData( pAttr, pLeft );*/
}
