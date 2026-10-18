#include "battle_command.h"
#include "bi_utils.h"
#include "core.h"
#include "image/img_render.h"
#include "sea/ships_list.h"
#include "shared/battle_interface/msg_control.h"

BICommandList::BICommandList(entid_t eid, ATTRIBUTES *pA, VDX9RENDER *rs)
{
    m_idHostObj = eid;
    m_pARoot = pA;
    m_pRS = rs;

    m_pImgRender = new BIImageRender(rs);
    Assert(m_pImgRender);

    m_nStartUsedCommandIndex = 0;
    m_nSelectedCommandIndex = 0;
    m_nIconShowMaxQuantity = 5;

    m_NoteFontID = -1;
    m_NotePos.x = m_NotePos.y = 0;
    m_NoteOffset.x = m_NoteOffset.y = 0;

    m_bUpArrow = m_bDownArrow = m_bLeftArrow = m_bRightArrow = false;

    m_bActive = false;

    Init();
}

BICommandList::~BICommandList()
{
    Release();
}

void BICommandList::Draw()
{
    if (m_aCooldownUpdate.size() > 0)
    {
        const auto fDT = core.GetDeltaTime() * .001f;
        for (int32_t n = 0; n < m_aCooldownUpdate.size(); n++)
        {
            m_aCooldownUpdate[n].fTime -= fDT;
            if (m_aCooldownUpdate[n].fTime < 0)
            {
                m_aCooldownUpdate[n].fTime = m_aCooldownUpdate[n].fUpdateTime;
                auto *pDat = core.Event("neGetCooldownFactor", "s",
                                        m_aUsedCommand[m_aCooldownUpdate[n].nIconNum].sCommandName.c_str());
                if (pDat)
                    m_aUsedCommand[m_aCooldownUpdate[n].nIconNum].fCooldownFactor = pDat->GetFloat();
                UpdateShowIcon();
            }
        }
    }
    if (m_pImgRender)
        m_pImgRender->Render();

    if (!m_NoteText.empty())
        m_pRS->ExtPrint(m_NoteFontID, m_NoteFontColor, 0, PR_ALIGN_CENTER, true, m_NoteFontScale, 0, 0, m_NotePos.x,
                        m_NotePos.y, "%s", m_NoteText.c_str());
}

void BICommandList::Update(int32_t nTopLine, int32_t nCharacterIndex, int32_t nCommandMode)
{

    if (m_nCurrentCommandMode != nCommandMode)
    {
        m_nStartUsedCommandIndex = 0;
        m_nSelectedCommandIndex = 0;
    }

    int32_t nOldSelIndex = 0;
    if (nTopLine == m_LeftTopPoint.y && nCharacterIndex == m_nCurrentCommandCharacterIndex &&
        m_nCurrentCommandMode == nCommandMode)
        nOldSelIndex = m_nSelectedCommandIndex;

    m_LeftTopPoint.y = nTopLine;
    if (nCharacterIndex != m_nCurrentCommandCharacterIndex)
        m_sCurrentCommandName = "";
    m_nCurrentCommandCharacterIndex = nCharacterIndex;
    m_nCurrentCommandMode = nCommandMode;

    m_aUsedCommand.clear();
    m_aCooldownUpdate.clear();

    m_NoteText.clear();

    FillIcons();

    if (nOldSelIndex > 0 && nOldSelIndex < m_aUsedCommand.size())
        m_nSelectedCommandIndex = nOldSelIndex;

    UpdateShowIcon();
}

// Pirates of the Caribbean: the selected pictures are to the right of the nCols x nRows grid of square pictures
// (the flags texture has no selected pictures)
bool BICommandList::HasPotcSelectedPictures(const char *pcTextureName, uint32_t nCols, uint32_t nRows) const
{
    if (core.GetTargetEngineVersion() > storm::ENGINE_VERSION::PIRATES_OF_THE_CARIBBEAN)
        return false;
    auto result = true;
    const auto id = m_pRS->TextureCreate(pcTextureName);
    auto *texture = id >= 0 ? m_pRS->GetBaseTexture(id) : nullptr;
    D3DSURFACE_DESC desc;
    if (texture && texture->GetType() == D3DRTYPE_TEXTURE &&
        SUCCEEDED(static_cast<IDirect3DTexture9 *>(texture)->GetLevelDesc(0, &desc)))
        result = desc.Width * nRows >= 2 * desc.Height * nCols;
    if (id >= 0)
        m_pRS->TextureRelease(id);
    return result;
}

size_t BICommandList::AddTexture(const char *pcTextureName, uint32_t nCols, uint32_t nRows)
{
    if (HasPotcSelectedPictures(pcTextureName, nCols, nRows))
        nCols *= 2;
    m_aTexture.push_back(TextureDescr{pcTextureName, nCols, nRows});
    return m_aTexture.size() - 1;
}

int32_t BICommandList::ExecuteConfirm()
{
    if (m_nSelectedCommandIndex >= m_aUsedCommand.size())
        return 0; // error!

    int32_t endCode = 0;
    int32_t nTargIndex = 0;
    std::string sLocName;

    if (m_aUsedCommand[m_nSelectedCommandIndex].nCooldownPictureIndex >= 0 &&
        m_aUsedCommand[m_nSelectedCommandIndex].fCooldownFactor < 1.f)
        return -1;

    if (!m_aUsedCommand[m_nSelectedCommandIndex].sCommandName.empty())
    {
        m_sCurrentCommandName = m_aUsedCommand[m_nSelectedCommandIndex].sCommandName;
        auto *pVD = core.Event("BI_CommandEndChecking", "s", m_sCurrentCommandName.c_str());
        if (pVD != nullptr)
            pVD->Get(endCode);
    }
    else
    {
        sLocName = m_aUsedCommand[m_nSelectedCommandIndex].sLocName;
        nTargIndex = m_aUsedCommand[m_nSelectedCommandIndex].nTargetIndex;
        if (sLocName.empty() && m_aUsedCommand[m_nSelectedCommandIndex].nCharIndex >= 0)
            nTargIndex = m_aUsedCommand[m_nSelectedCommandIndex].nCharIndex;
    }
    core.Event("evntBattleCommandSound", "s", "activate"); // boal 22.08.06
    switch (endCode)
    {
    case -1:
    case 0:
        core.Event("BI_LaunchCommand", "lsls", m_nCurrentCommandCharacterIndex, m_sCurrentCommandName.c_str(),
                   nTargIndex, sLocName.c_str());
        m_sCurrentCommandName = "";
        break;
    default:
        Update(m_LeftTopPoint.y, m_nCurrentCommandCharacterIndex, endCode);
    }
    return endCode;
}

int32_t BICommandList::ExecuteLeft()
{
    if (m_nSelectedCommandIndex > 0)
    {
        m_nSelectedCommandIndex--;
        if (m_nSelectedCommandIndex < m_nStartUsedCommandIndex)
        {
            m_nStartUsedCommandIndex = m_nSelectedCommandIndex;
        }
        core.Event("evntBattleCommandSound", "s", "left"); // boal 22.08.06
        UpdateShowIcon();
    }
    return 0;
}

int32_t BICommandList::ExecuteRight()
{
    if (m_nSelectedCommandIndex < static_cast<int32_t>(m_aUsedCommand.size()) - 1)
    {
        m_nSelectedCommandIndex++;
        if (m_nSelectedCommandIndex >= m_nStartUsedCommandIndex + m_nIconShowMaxQuantity)
        {
            m_nStartUsedCommandIndex = m_nSelectedCommandIndex - m_nIconShowMaxQuantity + 1;
        }
        core.Event("evntBattleCommandSound", "s", "right"); // boal 22.08.06
        UpdateShowIcon();
    }
    return 0;
}

int32_t BICommandList::ExecuteCancel()
{
    m_nSelectedCommandIndex = 0;
    m_nStartUsedCommandIndex = 0;
    if (m_sCurrentCommandName.empty())
        return 0;
    m_sCurrentCommandName = "";
    if (m_nCurrentCommandMode == BI_COMMODE_COMMAND_SELECT)
        return 0;
    return BI_COMMODE_COMMAND_SELECT;
}

void BICommandList::SetActive(bool bActive)
{
    if (m_pARoot)
    {
        m_pARoot->SetAttributeUseDword("ComState", (bActive && !m_bTouchMode ? 1 : 0));
    }

    if (m_bActive == bActive)
        return;
    m_bActive = bActive;
    UpdateShowIcon();
}

void BICommandList::SetUpDown(bool bUp, bool bDown)
{
    if (bUp == m_bUpArrow && bDown == m_bDownArrow)
        return;
    m_bUpArrow = bUp;
    m_bDownArrow = bDown;
    UpdateShowIcon();
}

void BICommandList::Init()
{
    Assert(m_pImgRender);
    ATTRIBUTES *pAList, *pATextures;

    m_LeftTopPoint.x = 120;
    m_LeftTopPoint.y = 100;
    m_IconSize.x = 64;
    m_IconSize.y = 64;
    m_nIconSpace = 8;

    FONT_RELEASE(m_pRS, m_NoteFontID);
    m_NoteFontColor = ARGB(255, 255, 255, 255);
    m_NoteFontScale = 1.f;
    m_NoteOffset.x = m_NoteOffset.y = 0;

    // default arrow data value
    m_sUpDownArrowTexture = "";
    m_pntUpDownArrowSize.x = m_pntUpDownArrowSize.y = 32;
    FULLRECT(m_frUpArrowUV);
    FULLRECT(m_frDownArrowUV);
    m_pntUpArrowOffset.x = 32;
    m_pntUpArrowOffset.y = -34;
    m_pntDownArrowOffset.x = 32;
    m_pntDownArrowOffset.y = 66;

    m_sLeftRightArrowTexture = "";
    m_pntLeftRightArrowSize.x = m_pntLeftRightArrowSize.y = 0;
    FULLRECT(m_frLeftArrowUV);
    FULLRECT(m_frRightArrowUV);
    m_pntLeftArrowOffset.x = 0;
    m_pntLeftArrowOffset.y = 0;
    m_pntRightArrowOffset.x = 0;
    m_pntRightArrowOffset.y = 0;

    // default data for active icon
    m_sActiveIconTexture = "";
    m_pntActiveIconOffset.x = -33;
    m_pntActiveIconOffset.y = 0;
    m_pntActiveIconSize.x = m_pntActiveIconSize.y = 64;
    FULLRECT(m_frActiveIconUV1);
    FULLRECT(m_frActiveIconUV2);
    m_sActiveIconNote = "";

    pAList = nullptr;
    if (m_pARoot)
        pAList = m_pARoot->GetAttributeClass("CommandList");
    if (pAList)
    {
        // get icon parameters
        m_nIconSpace = pAList->GetAttributeAsDword("CommandIconSpace", 0);
        m_LeftTopPoint.x = pAList->GetAttributeAsDword("CommandIconLeft", 100);
        m_IconSize.x = pAList->GetAttributeAsDword("CommandIconWidth", m_IconSize.x);
        m_IconSize.y = pAList->GetAttributeAsDword("CommandIconHeight", m_IconSize.y);
        if (const char *texture = pAList->GetAttribute("ShipStateTexture"))
            m_sStateTexture = texture;

        const char *attr = nullptr;

        // get note font parameters
        if (pAList->GetAttribute("CommandNoteFont"))
            m_NoteFontID = m_pRS->LoadFont(to_string(pAList->GetAttribute("CommandNoteFont")));
        m_NoteFontColor = pAList->GetAttributeAsDword("CommandNoteColor", m_NoteFontColor);
        m_NoteFontScale = pAList->GetAttributeAsFloat("CommandNoteScale", m_NoteFontScale);
        if (pAList->GetAttribute("CommandNoteOffset"))
            sscanf(pAList->GetAttribute("CommandNoteOffset"), "%d,%d", &m_NoteOffset.x, &m_NoteOffset.y);

        // Setting values for arrows (up / down)
        attr = pAList->GetAttribute("UDArrow_Texture");
        if (attr)
            m_sUpDownArrowTexture = attr;
        BIUtils::ReadRectFromAttr(pAList, "UDArrow_UV_Up", m_frUpArrowUV, m_frUpArrowUV);
        BIUtils::ReadRectFromAttr(pAList, "UDArrow_UV_Down", m_frDownArrowUV, m_frDownArrowUV);
        BIUtils::ReadPosFromAttr(pAList, "UDArrow_Size", m_pntUpDownArrowSize.x, m_pntUpDownArrowSize.y,
                                 m_pntUpDownArrowSize.x, m_pntUpDownArrowSize.y);
        BIUtils::ReadPosFromAttr(pAList, "UDArrow_Offset_Up", m_pntUpArrowOffset.x, m_pntUpArrowOffset.y,
                                 m_pntUpArrowOffset.x, m_pntUpArrowOffset.y);
        BIUtils::ReadPosFromAttr(pAList, "UDArrow_Offset_Down", m_pntDownArrowOffset.x, m_pntDownArrowOffset.y,
                                 m_pntDownArrowOffset.x, m_pntDownArrowOffset.y);

        attr = pAList->GetAttribute("LRArrow_Texture");
        if (attr)
            m_sLeftRightArrowTexture = attr;
        BIUtils::ReadRectFromAttr(pAList, "LRArrow_UV_Left", m_frLeftArrowUV, m_frLeftArrowUV);
        BIUtils::ReadRectFromAttr(pAList, "LRArrow_UV_Right", m_frRightArrowUV, m_frRightArrowUV);
        BIUtils::ReadPosFromAttr(pAList, "LRArrow_Size", m_pntLeftRightArrowSize.x, m_pntLeftRightArrowSize.y,
                                 m_pntLeftRightArrowSize.x, m_pntLeftRightArrowSize.y);
        BIUtils::ReadPosFromAttr(pAList, "LRArrow_Offset_Left", m_pntLeftArrowOffset.x, m_pntLeftArrowOffset.y,
                                 m_pntLeftArrowOffset.x, m_pntLeftArrowOffset.y);
        BIUtils::ReadPosFromAttr(pAList, "LRArrow_Offset_Right", m_pntRightArrowOffset.x, m_pntRightArrowOffset.y,
                                 m_pntRightArrowOffset.x, m_pntRightArrowOffset.y);

        // set values for the menu activity icon
        attr = pAList->GetAttribute("ActiveIcon_Texture");
        if (attr)
            m_sActiveIconTexture = attr;
        BIUtils::ReadPosFromAttr(pAList, "ActiveIcon_Offset", m_pntActiveIconOffset.x, m_pntActiveIconOffset.y,
                                 m_pntActiveIconOffset.x, m_pntActiveIconOffset.y);
        BIUtils::ReadPosFromAttr(pAList, "ActiveIcon_Size", m_pntActiveIconSize.x, m_pntActiveIconSize.y,
                                 m_pntActiveIconSize.x, m_pntActiveIconSize.y);
        BIUtils::ReadRectFromAttr(pAList, "ActiveIcon_UV1", m_frActiveIconUV1, m_frActiveIconUV1);
        BIUtils::ReadRectFromAttr(pAList, "ActiveIcon_UV2", m_frActiveIconUV2, m_frActiveIconUV2);
        attr = pAList->GetAttribute("ActiveIcon_Note");
        if (attr)
            m_sActiveIconNote = attr;
    }

    pAList = nullptr;
    if (m_pARoot)
        pAList = m_pARoot->GetAttributeClass("CommandTextures");

    pATextures = nullptr;
    if (pAList)
        pATextures = pAList->GetAttributeClass("list");
    if (pATextures)
    {
        size_t q = pATextures->GetAttributesNum();
        for (int n = 0; n < q; n++)
        {
            auto *pA = pATextures->GetAttributeClass(n);
            if (pA)
            {
                TextureDescr td = {pA->GetAttribute("name") ? pA->GetAttribute("name") : std::string(),
                                   pA->GetAttributeAsDword("xsize", 1), pA->GetAttributeAsDword("ysize", 1)};

                if (td.nCols < 1)
                    td.nCols = 1;
                if (td.nRows < 1)
                    td.nRows = 1;
                if (HasPotcSelectedPictures(td.sFileName.c_str(), td.nCols, td.nRows))
                    td.nCols *= 2;

                m_pImgRender->CreateMaterial(td.sFileName.c_str());

                m_aTexture.push_back(td);
            }
        }
        if (core.GetTargetEngineVersion() <= storm::ENGINE_VERSION::PIRATES_OF_THE_CARIBBEAN)
            m_nPotcDefaultTexture = static_cast<int32_t>(pAList->GetAttributeAsDword("CommandTexNum", 0));
    }
    m_nIconShowMaxQuantity = 5;
}

int32_t BICommandList::AddToIconList(int32_t nTextureNum, int32_t nNormPictureNum, int32_t nSelPictureNum,
                                     int32_t nCooldownPictureNum, int32_t nCharacterIndex, const char *pcCommandName,
                                     int32_t nTargetIndex, const char *pcLocName, const char *pcNoteName)
{
    int32_t n;
    // filtering out already used objects
    for (n = 0; n < m_aUsedCommand.size(); n++)
    {
        if (nCharacterIndex != -1 && m_aUsedCommand[n].nCharIndex == nCharacterIndex)
            return 0;
        if (pcCommandName && m_aUsedCommand[n].sCommandName == pcCommandName)
            return 0;
        if (pcLocName && m_aUsedCommand[n].sLocName == pcLocName)
            return 0;
        if (nTargetIndex != -1 && m_aUsedCommand[n].nTargetIndex == nTargetIndex)
            return 0;
    }

    if (m_nPotcDefaultTexture >= 0)
    {
        if (nTextureNum < 0)
            nTextureNum = m_nPotcDefaultTexture;
        if (nTextureNum < m_aTexture.size() && nNormPictureNum >= 0)
        {
            const int32_t nCols = std::max(1, static_cast<int32_t>(m_aTexture[nTextureNum].nCols) / 2);
            nNormPictureNum = nNormPictureNum / nCols * nCols * 2 + nNormPictureNum % nCols;
            nSelPictureNum = nNormPictureNum + nCols;
        }
    }

    UsedCommand uc;
    uc.fCooldownFactor = 1.f;
    uc.nCharIndex = nCharacterIndex;
    uc.nCooldownPictureIndex = nCooldownPictureNum;
    uc.nNormPictureIndex = nNormPictureNum;
    uc.nSelPictureIndex = nSelPictureNum;
    uc.nTargetIndex = nTargetIndex;
    uc.nTextureIndex = nTextureNum;
    if (pcCommandName)
        uc.sCommandName = pcCommandName;
    if (pcLocName)
        uc.sLocName = pcLocName;
    if (pcNoteName)
        uc.sNote = pcNoteName;
    m_aUsedCommand.push_back(uc);

    if (nCooldownPictureNum >= 0)
    {
        auto *pDat = core.Event("neGetCooldownFactor", "s", pcCommandName);
        if (pDat)
            m_aUsedCommand[n].fCooldownFactor = pDat->GetFloat();
        CoolDownUpdateData data;
        data.fUpdateTime = data.fTime = 1.f;
        data.nIconNum = n;
        m_aCooldownUpdate.push_back(data);
    }
    return 1;
}

void BICommandList::AddAdditiveToIconList(int32_t nTextureNum, int32_t nPictureNum, float fDist, float fWidth,
                                          float fHeight)
{
    if (m_aUsedCommand.empty())
        return;

    UsedCommand::AdditiveIcon icon;
    icon.fDelta = fDist;
    icon.fpSize.x = fWidth;
    icon.fpSize.y = fHeight;
    icon.nPic = nPictureNum;
    icon.nTex = nTextureNum;

    m_aUsedCommand[m_aUsedCommand.size() - 1].aAddPicList.push_back(icon);
}

void BICommandList::SetStateToIcon(float fSails, float fCrew, float fHull)
{
    if (m_aUsedCommand.empty() || m_sStateTexture.empty())
        return;
    auto &state = m_aUsedCommand.back().fState;
    state[0] = std::clamp(fSails, 0.f, 1.f);
    state[1] = std::clamp(fCrew, 0.f, 1.f);
    state[2] = std::clamp(fHull, 0.f, 1.f);
}

float BICommandList::GetStateBarHeight() const
{
    // as thin as the bars above the icons of the own ships
    return std::round(static_cast<float>(m_IconSize.y) * (m_bTouchMode ? .15f : .25f));
}

float BICommandList::GetIconStateHeight() const
{
    return !m_aUsedCommand.empty() && m_aUsedCommand.back().fState[0] >= 0.f ? 3.f * GetStateBarHeight() : 0.f;
}

void BICommandList::StateBarsAdd(const RECT &rIcon, const float (&fState)[3])
{
    // from the top: the sails (white), the crew (blue), the hull (red) of indicators.tga on the dark tracks
    static const float kRows[3] = {0.f, .167f, .333f};
    const auto height = static_cast<int32_t>(GetStateBarHeight());
    for (int32_t i = 0; i < 3; i++)
    {
        RECT r = rIcon;
        r.top = rIcon.top - (3 - i) * height;
        r.bottom = r.top + height;
        FRECT uv = {0.f, .5f, 1.f, .667f};
        m_pImgRender->CreateImage(BIType_square, m_sStateTexture.c_str(), 0xFF808080, uv, r);
        r.right = r.left + static_cast<int32_t>((rIcon.right - rIcon.left) * fState[i]);
        if (r.right <= r.left)
            continue;
        uv = {0.f, kRows[i], fState[i], kRows[i] + .167f};
        m_pImgRender->CreateImage(BIType_square, m_sStateTexture.c_str(), 0xFF808080, uv, r);
    }
}

void BICommandList::Release()
{
    STORM_DELETE(m_pImgRender);
    FONT_RELEASE(m_pRS, m_NoteFontID);
}

int32_t BICommandList::IconAdd(int32_t nPictureNum, int32_t nTextureNum, RECT &rpos)
{
    if (nTextureNum < 0 || nTextureNum >= m_aTexture.size() || nPictureNum < 0 ||
        nPictureNum >= m_aTexture[nTextureNum].nCols * m_aTexture[nTextureNum].nRows)
        return 0;

    FRECT uv;
    m_pImgRender->CreateImage(BIType_square, m_aTexture[nTextureNum].sFileName.c_str(), 0xFF808080,
                              GetPictureUV(nTextureNum, nPictureNum, uv), rpos);
    return 1;
}

int32_t BICommandList::ClockIconAdd(int32_t nForePictureNum, int32_t nBackPictureNum, int32_t nTextureNum, RECT &rpos,
                                    float fFactor)
{
    if (nTextureNum < 0 || nTextureNum >= m_aTexture.size() || nForePictureNum < 0 ||
        nForePictureNum >= m_aTexture[nTextureNum].nCols * m_aTexture[nTextureNum].nRows)
        return 0;

    FRECT uv;
    m_pImgRender->CreateImage(BIType_square, m_aTexture[nTextureNum].sFileName.c_str(), 0xFF808080,
                              GetPictureUV(nTextureNum, nBackPictureNum, uv), rpos);
    auto *pImg = m_pImgRender->CreateImage(BIType_clocksquare, m_aTexture[nTextureNum].sFileName.c_str(), 0xFF808080,
                                           GetPictureUV(nTextureNum, nForePictureNum, uv), rpos);
    if (pImg)
        pImg->CutClock(0.f, 1.f, fFactor);
    return 1;
}

void BICommandList::AdditiveIconAdd(float fX, float fY, std::vector<UsedCommand::AdditiveIcon> &aList)
{
    if (aList.size() <= 0)
        return;
    RECT rCur;
    for (int32_t n = 0; n < aList.size(); n++)
    {
        rCur.top = static_cast<int32_t>(fY + aList[n].fDelta);
        rCur.bottom = rCur.top + static_cast<int32_t>(aList[n].fpSize.y);
        rCur.left = static_cast<int32_t>(fX - .5f * aList[n].fpSize.x);
        rCur.right = rCur.left + static_cast<int32_t>(aList[n].fpSize.x);
        if (IconAdd(aList[n].nPic, aList[n].nTex, rCur) > 0)
        {
            fY = static_cast<float>(rCur.bottom);
        }
    }
}

FRECT &BICommandList::GetPictureUV(int32_t nTextureNum, int32_t nPictureNum, FRECT &uv)
{
    if (nTextureNum < 0 || nTextureNum >= m_aTexture.size() || nPictureNum < 0 ||
        nPictureNum >= m_aTexture[nTextureNum].nCols * m_aTexture[nTextureNum].nRows)
    {
        uv.left = uv.top = 0.f;
        uv.right = uv.bottom = 1.f;
    }
    else
    {
        const auto fDU = 1.f / m_aTexture[nTextureNum].nCols;
        const auto fDV = 1.f / m_aTexture[nTextureNum].nRows;
        const auto ny = nPictureNum / m_aTexture[nTextureNum].nCols;
        const auto nx = nPictureNum - ny * m_aTexture[nTextureNum].nCols;
        uv.left = nx * fDU;
        uv.right = uv.left + fDU;
        uv.top = ny * fDV;
        uv.bottom = uv.top + fDV;
    }
    return uv;
}

RECT &BICommandList::GetCurrentPos(int32_t num, RECT &rpos) const
{
    rpos.left = m_LeftTopPoint.x + num * (m_IconSize.x + m_nIconSpace);
    rpos.right = rpos.left + m_IconSize.x;
    rpos.top = m_LeftTopPoint.y;
    rpos.bottom = rpos.top + m_IconSize.y;
    return rpos;
}

void BICommandList::UpdateShowIcon()
{
    RECT rPos;

    m_pImgRender->ReleaseAllImages();
    m_aTouchRects.clear();

    if (!m_bActive)
    {
        rPos.left = m_LeftTopPoint.x + m_pntActiveIconOffset.x;
        rPos.top = m_LeftTopPoint.y + m_pntActiveIconOffset.y;
        rPos.right = rPos.left + m_pntActiveIconSize.x;
        rPos.bottom = rPos.top + m_pntActiveIconSize.y;
        m_pImgRender->CreateImage(BIType_square, m_sActiveIconTexture.c_str(), 0xFF808080, m_frActiveIconUV1, rPos);
        SetNote(m_sActiveIconNote.c_str(), (rPos.left + rPos.right) / 2, (rPos.top + rPos.bottom) / 2);
        return;
    }
    rPos.left = m_LeftTopPoint.x + m_pntActiveIconOffset.x;
    rPos.top = m_LeftTopPoint.y + m_pntActiveIconOffset.y;
    rPos.right = rPos.left + m_pntActiveIconSize.x;
    rPos.bottom = rPos.top + m_pntActiveIconSize.y;
    m_pImgRender->CreateImage(BIType_square, m_sActiveIconTexture.c_str(), 0xFF808080, m_frActiveIconUV2, rPos);

    m_bLeftArrow = m_nStartUsedCommandIndex > 0;
    m_bRightArrow = m_aUsedCommand.size() - m_nStartUsedCommandIndex > m_nIconShowMaxQuantity;

    int32_t i = 0;
    for (auto n = m_nStartUsedCommandIndex; n < m_aUsedCommand.size() && i < m_nIconShowMaxQuantity; n++)
    {
        GetCurrentPos(i, rPos);
        AdditiveIconAdd(.5f * (rPos.left + rPos.right), static_cast<float>(rPos.bottom), m_aUsedCommand[n].aAddPicList);
        if (m_aUsedCommand[n].fState[0] >= 0.f)
            StateBarsAdd(rPos, m_aUsedCommand[n].fState);
        if (n == m_nSelectedCommandIndex)
        {
            if (m_aUsedCommand[n].nCooldownPictureIndex < 0)
                i += IconAdd(m_aUsedCommand[n].nSelPictureIndex, m_aUsedCommand[n].nTextureIndex, rPos);
            else
                i += ClockIconAdd(m_aUsedCommand[n].nSelPictureIndex, m_aUsedCommand[n].nCooldownPictureIndex,
                                  m_aUsedCommand[n].nTextureIndex, rPos, m_aUsedCommand[n].fCooldownFactor);
            m_aTouchRects.emplace_back(static_cast<int32_t>(n), rPos);
            SetNote(m_aUsedCommand[n].sNote.c_str(), (rPos.left + rPos.right) / 2, (rPos.top + rPos.bottom) / 2);
            auto *const pSD = g_ShipList.FindShip(m_aUsedCommand[n].nCharIndex);
            if (pSD)
            {
                core.Event("evntBISelectShip", "ll", pSD->characterIndex, pSD->relation != BI_RELATION_ENEMY);
            }
            else
                core.Event("evntBISelectShip", "ll", -1, true);
        }
        else
        {
            if (m_aUsedCommand[n].nCooldownPictureIndex < 0)
                i += IconAdd(m_aUsedCommand[n].nNormPictureIndex, m_aUsedCommand[n].nTextureIndex, rPos);
            else
                i += ClockIconAdd(m_aUsedCommand[n].nNormPictureIndex, m_aUsedCommand[n].nCooldownPictureIndex,
                                  m_aUsedCommand[n].nTextureIndex, rPos, m_aUsedCommand[n].fCooldownFactor);
            m_aTouchRects.emplace_back(static_cast<int32_t>(n), rPos);
        }
    }

    // add bottom / top arrows for the command list
    if (m_bUpArrow)
    {
        rPos.left = m_LeftTopPoint.x + m_pntUpArrowOffset.x;
        rPos.top = m_LeftTopPoint.y + m_pntUpArrowOffset.y;
        rPos.right = rPos.left + m_pntUpDownArrowSize.x;
        rPos.bottom = rPos.top + m_pntUpDownArrowSize.y;
        m_pImgRender->CreateImage(BIType_square, m_sUpDownArrowTexture.c_str(), 0xFF808080, m_frUpArrowUV, rPos);
    }
    if (m_bDownArrow)
    {
        rPos.left = m_LeftTopPoint.x + m_pntDownArrowOffset.x;
        rPos.top = m_LeftTopPoint.y + m_pntDownArrowOffset.y;
        rPos.right = rPos.left + m_pntUpDownArrowSize.x;
        rPos.bottom = rPos.top + m_pntUpDownArrowSize.y;
        m_pImgRender->CreateImage(BIType_square, m_sUpDownArrowTexture.c_str(), 0xFF808080, m_frDownArrowUV, rPos);
    }
    if (m_bLeftArrow)
    {
        rPos.left = m_LeftTopPoint.x + m_pntLeftArrowOffset.x;
        rPos.top = m_LeftTopPoint.y + m_pntLeftArrowOffset.y;
        rPos.right = rPos.left + m_pntLeftRightArrowSize.x;
        rPos.bottom = rPos.top + m_pntLeftRightArrowSize.y;
        m_pImgRender->CreateImage(BIType_square, m_sLeftRightArrowTexture.c_str(), 0xFF808080, m_frLeftArrowUV, rPos);
        m_aTouchRects.emplace_back(kTouchLeftArrow, rPos);
    }
    if (m_bRightArrow)
    {
        rPos.left = m_LeftTopPoint.x + m_pntRightArrowOffset.x;
        rPos.top = m_LeftTopPoint.y + m_pntRightArrowOffset.y;
        rPos.right = rPos.left + m_pntLeftRightArrowSize.x;
        rPos.bottom = rPos.top + m_pntLeftRightArrowSize.y;
        m_pImgRender->CreateImage(BIType_square, m_sLeftRightArrowTexture.c_str(), 0xFF808080, m_frRightArrowUV, rPos);
        m_aTouchRects.emplace_back(kTouchRightArrow, rPos);
    }
}

int32_t BICommandList::GetCommandAt(int32_t x, int32_t y) const
{
    for (const auto &[n, r] : m_aTouchRects)
    {
        if (x >= r.left && x < r.right && y >= r.top && y < r.bottom)
            return n;
    }
    return -1;
}

void BICommandList::AddTouchPointerAreas() const
{
    D3DVIEWPORT9 vp;
    m_pRS->GetViewport(&vp);
    if (vp.Width == 0 || vp.Height == 0)
        return;
    for (const auto &[n, r] : m_aTouchRects)
    {
        core.Controls->AddTouchPointerArea(static_cast<float>(r.left) / vp.Width, static_cast<float>(r.top) / vp.Height,
                                           static_cast<float>(r.right) / vp.Width,
                                           static_cast<float>(r.bottom) / vp.Height);
    }
}

void BICommandList::SelectCommand(int32_t n)
{
    if (n < 0 || n >= static_cast<int32_t>(m_aUsedCommand.size()) || n == m_nSelectedCommandIndex)
        return;
    m_nSelectedCommandIndex = n;
    UpdateShowIcon();
}

void BICommandList::SetNote(const char *pcNote, int32_t nX, int32_t nY)
{
    m_NoteText = pcNote;
    m_NotePos.x = nX + m_NoteOffset.x;
    m_NotePos.y = nY + m_NoteOffset.y;
}

ATTRIBUTES *BICommandList::GetCurrentCommandAttribute() const
{
    if (m_sCurrentCommandName.empty())
        return nullptr;

    ATTRIBUTES *pAR = nullptr;
    if (m_nCurrentCommandMode & BI_COMMODE_ABILITY_ICONS)
        pAR = m_pARoot->GetAttributeClass("AbilityIcons");
    else
        pAR = m_pARoot->GetAttributeClass("Commands");
    if (!pAR)
        return nullptr;

    const size_t q = pAR->GetAttributesNum();
    for (int32_t n = 0; n < q; n++)
    {
        auto *pA = pAR->GetAttributeClass(n);
        if (!pA)
            continue;
        const char *pcCommName = pA->GetAttribute("event");
        if (m_sCurrentCommandName == pcCommName)
            return pA;
    }
    return nullptr;
}
