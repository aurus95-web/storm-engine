#pragma once

#include "bi_defines.h"
#include <string>
#include <vector>

class BIImageRender;

// nCommandType
#define BISCT_Command 1
#define BISCT_Ability 2
#define BISCT_Ship 3
#define BISCT_Fort 4
#define BISCT_Land 5
#define BISCT_Charge 6
#define BISCT_UserIcon 7
#define BISCT_Cancel 8

class BICommandList
{
  public:
    BICommandList(BICommandList &&) = delete;
    BICommandList(const BICommandList &) = delete;
    BICommandList(entid_t eid, ATTRIBUTES *pA, VDX9RENDER *rs);
    virtual ~BICommandList();

    void Draw();
    void Update(int32_t nTopLine, int32_t nCharacterIndex, int32_t nCommandMode);
    virtual void FillIcons() = 0;

    size_t AddTexture(const char *pcTextureName, uint32_t nCols, uint32_t nRows);
    void SetLeft(int32_t nLeft)
    {
        m_LeftTopPoint.x = nLeft;
    }
    // Pirates of the Caribbean command textures: the grid of normal pictures and the same grid of selected
    // pictures to the right of it, commands without texNum use the default texture
    void SetPotcPictures(int32_t nDefaultTexture)
    {
        m_nPotcDefaultTexture = nDefaultTexture;
    }

    // commands
    int32_t ExecuteConfirm();
    int32_t ExecuteLeft();
    int32_t ExecuteRight();
    int32_t ExecuteCancel();

    void SetActive(bool bActive);
    // touch screens: the list is always open without the commands mode of the scripts (ComState),
    // the commands are tapped
    void SetTouchMode(bool bTouch)
    {
        m_bTouchMode = bTouch;
    }
    bool IsTouchMode() const
    {
        return m_bTouchMode;
    }
    // the command at the point of the screen, kTouchLeftArrow/kTouchRightArrow, or -1
    static constexpr int32_t kTouchLeftArrow = -2;
    static constexpr int32_t kTouchRightArrow = -3;
    int32_t GetCommandAt(int32_t x, int32_t y) const;
    // the shown icons and arrows are not the stick under them
    void AddTouchPointerAreas() const;
    void SelectCommand(int32_t n);

    bool GetActive() const
    {
        return m_bActive;
    }

    void SetUpDown(bool bUp, bool bDown);

    virtual void Init();

    int32_t AddToIconList(int32_t nTextureNum, int32_t nNormPictureNum, int32_t nSelPictureNum,
                          int32_t nCooldownPictureNum, int32_t nCharacterIndex, const char *pcCommandName,
                          int32_t nTargetIndex, const char *pcLocName, const char *pcNoteName);
    void AddAdditiveToIconList(int32_t nTextureNum, int32_t nPictureNum, float fDist, float fWidth, float fHeight);
    // the state bars of a ship above the last added icon: the sails, the crew and the hull (from 0 to 1)
    void SetStateToIcon(float fSails, float fCrew, float fHull);
    // the height of the state bars above the last added icon (0 without them)
    float GetIconStateHeight() const;

  protected:
    entid_t m_idHostObj;
    ATTRIBUTES *m_pARoot;
    VDX9RENDER *m_pRS;

    BIImageRender *m_pImgRender;

    struct TextureDescr
    {
        std::string sFileName;
        uint32_t nCols;
        uint32_t nRows;
    };

    std::vector<TextureDescr> m_aTexture;
    bool HasPotcSelectedPictures(const char *pcTextureName, uint32_t nCols, uint32_t nRows) const;

    struct UsedCommand
    {
        int32_t nCharIndex;
        std::string sCommandName;
        int32_t nTargetIndex;
        std::string sLocName;
        std::string sNote;

        int32_t nTextureIndex;
        int32_t nSelPictureIndex;
        int32_t nNormPictureIndex;
        int32_t nCooldownPictureIndex;

        float fCooldownFactor;

        struct AdditiveIcon
        {
            int32_t nTex;
            int32_t nPic;
            float fDelta;
            FPOINT fpSize;
        };

        std::vector<AdditiveIcon> aAddPicList;
        float fState[3] = {-1.f, -1.f, -1.f}; // the sails, the crew, the hull; none if negative
    };
    std::string m_sStateTexture; // the texture of the state bars of the ships (ShipStateTexture)
    float GetStateBarHeight() const;
    void StateBarsAdd(const RECT &rIcon, const float (&fState)[3]);

    bool m_bActive;
    bool m_bTouchMode = false;
    std::vector<std::pair<int32_t, RECT>> m_aTouchRects; // the shown icons and arrows

    std::vector<UsedCommand> m_aUsedCommand;
    int32_t m_nStartUsedCommandIndex;
    int32_t m_nSelectedCommandIndex;
    int32_t m_nIconShowMaxQuantity;
    int32_t m_nPotcDefaultTexture = -1;

    IPOINT m_pntActiveIconOffset;
    IPOINT m_pntActiveIconSize;
    std::string m_sActiveIconTexture;
    FRECT m_frActiveIconUV1;
    FRECT m_frActiveIconUV2;
    std::string m_sActiveIconNote;

    bool m_bUpArrow;
    bool m_bDownArrow;
    std::string m_sUpDownArrowTexture;
    FRECT m_frUpArrowUV;
    FRECT m_frDownArrowUV;
    IPOINT m_pntUpDownArrowSize;
    IPOINT m_pntUpArrowOffset;
    IPOINT m_pntDownArrowOffset;

    bool m_bLeftArrow;
    bool m_bRightArrow;
    std::string m_sLeftRightArrowTexture;
    FRECT m_frLeftArrowUV;
    FRECT m_frRightArrowUV;
    IPOINT m_pntLeftRightArrowSize;
    IPOINT m_pntLeftArrowOffset;
    IPOINT m_pntRightArrowOffset;

    std::string m_sCurrentCommandName;
    int32_t m_nCurrentCommandCharacterIndex;
    int32_t m_nCurrentCommandMode;

    IPOINT m_LeftTopPoint;
    IPOINT m_IconSize;
    int32_t m_nIconSpace;

    int32_t m_NoteFontID;
    uint32_t m_NoteFontColor;
    float m_NoteFontScale;
    IPOINT m_NotePos;
    IPOINT m_NoteOffset;
    std::string m_NoteText;

    struct CoolDownUpdateData
    {
        int32_t nIconNum;
        float fTime;
        float fUpdateTime;
    };

    std::vector<CoolDownUpdateData> m_aCooldownUpdate;

    void Release();

    int32_t IconAdd(int32_t nPictureNum, int32_t nTextureNum, RECT &rpos);
    int32_t ClockIconAdd(int32_t nForePictureNum, int32_t nBackPictureNum, int32_t nTextureNum, RECT &rpos,
                         float fFactor);
    void AdditiveIconAdd(float fX, float fY, std::vector<UsedCommand::AdditiveIcon> &aList);
    FRECT &GetPictureUV(int32_t nTextureNum, int32_t nPictureNum, FRECT &uv);
    RECT &GetCurrentPos(int32_t num, RECT &rpos) const;
    RECT &GetAddingPos(int32_t num, RECT &rpos);

    void UpdateShowIcon();
    void SetNote(const char *pcNote, int32_t nX, int32_t nY);

    ATTRIBUTES *GetCurrentCommandAttribute() const;
};
