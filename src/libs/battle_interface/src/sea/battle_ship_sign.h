#pragma once

#include "bi_defines.h"
#include "ships_list.h"
#include <map>
#include <string>
#include <vector>

#define MAX_SHIP_QUANTITY 8

class BIShipCommandList;

class BIShipIcon
{
  public:
    enum CommandType
    {
        Command_confirm,
        Command_left,
        Command_right,
        Command_up,
        Command_down,
        Command_cancel,

        Command_ForceDword = 0xffffffff
    };

    BIShipIcon(BIShipIcon &&) = delete;
    BIShipIcon(const BIShipIcon &) = delete;
    BIShipIcon(entid_t BIEntityID, VDX9RENDER *pRS);
    ~BIShipIcon();

    void Draw();
    void Init(ATTRIBUTES *pRoot, ATTRIBUTES *pA);

    size_t AddTexture(const char *pcTextureName, int32_t nCols, int32_t nRows) const;

    void Recollect();

    void SetUpdate()
    {
        m_bMakeUpdate = true;
    }

    bool IsActive() const;
    void SetActive(bool bActive);
    void MakeControl();
    void TouchControl();
    // the gamepad: the commands of the computers again (closed until the key opens them)
    void LeaveTouchMode();
    // shows the commands of the own ship of the character (Pirates of the Caribbean: the companion command)
    void SetCommandCharacter(int32_t nCharacterIndex);
    void ExecuteCommand(CommandType command);

  protected:
    void Release();
    int32_t CalculateShipQuantity();
    void UpdateBuffers(int32_t nShipQ);
    void FillIndexBuffer() const;
    void FillVertexBuffer();
    int32_t WriteSquareToVBuff(BI_COLOR_VERTEX *pv, const FRECT &uv, uint32_t color, const BIFPOINT &center,
                            const FPOINT &size);
    int32_t WriteSquareToVBuffWithProgress(BI_COLOR_VERTEX *pv, const FRECT &uv, uint32_t color, const BIFPOINT &center,
                                        const FPOINT &size, float fClampUp, float fClampDown, float fClampLeft,
                                        float fClampRight);
    void UpdateCommandList();

    int32_t GetCurrentCommandTopLine();
    int32_t GetCurrentCommandCharacterIndex();
    int32_t GetCurrentCommandMode() const;

    ATTRIBUTES *GetSailorQuantityAttribute(SHIP_DESCRIBE_LIST::SHIP_DESCR *pSD);
    float GetProgressShipHP(int32_t nShipNum) const;
    float GetProgressShipSP(int32_t nShipNum);
    float GetProgressShipCrew(int32_t nShipNum) const;
    float GetProgressShipClass(int32_t nShipNum);
    void GetShipUVFromPictureIndex(int32_t nPicIndex, FRECT &rUV);
    // the picture of the ship in its texture (m_aShipTextures), the common one otherwise
    int32_t GetShipUV(int32_t nPicIndex, int32_t nTextureNum, FRECT &rUV);

    int32_t GetShipClass(int32_t nCharIdx);

    VDX9RENDER *m_pRS;

    int32_t m_nVBufID;
    int32_t m_nIBufID;

    int32_t m_nMaxSquareQ;

    int32_t m_nBackTextureID;
    int32_t m_nBackSquareQ;
    uint32_t m_dwBackColor;
    FRECT m_rBackUV;
    BIFPOINT m_pntBackOffset;
    FPOINT m_pntBackIconSize;

    int32_t m_nShipStateTextureID;
    int32_t m_nShipStateSquareQ;
    uint32_t m_dwShipStateColor;
    FRECT m_rShipHPUV;
    BIFPOINT m_pntShipHPOffset;
    FPOINT m_pntShipHPIconSize;
    FRECT m_rShipSPUV;
    BIFPOINT m_pntShipSPOffset;
    FPOINT m_pntShipSPIconSize;
    // the crew bar (none if its size is zero)
    FRECT m_rShipCrewUV;
    BIFPOINT m_pntShipCrewOffset;
    FPOINT m_pntShipCrewIconSize;
    // the empty track under every bar, the whole length (none if not set)
    FRECT m_rShipStateBackUV;
    bool m_bShipStateBack = false;

    int32_t m_nShipClassTextureID;
    int32_t m_nShipClassSquareQ;
    uint32_t m_dwShipClassColor;
    FRECT m_rShipClassUV;
    BIFPOINT m_pntShipClassOffset;
    FPOINT m_pntShipClassIconSize;
    std::vector<float> m_aClassProgress;

    int32_t m_nShipTextureID;
    int32_t m_nShipSquareQ;
    uint32_t m_dwShipColor;
    // FRECT m_rShipUV;
    BIFPOINT m_pntShipOffset;
    FPOINT m_pntShipIconSize;
    uint32_t m_dwShipNCols;
    // Pirates of the Caribbean: the ship pictures in several textures, by the texture numbers of the scripts
    struct ShipTexture
    {
        int32_t id = -1;
        uint32_t cols = 1, rows = 1;
    };
    std::map<int32_t, ShipTexture> m_aShipTextures;
    uint32_t m_dwShipNRows;

    struct ShipDescr
    {
        int32_t nCharacterIndex;
        FPOINT pntPos; // center
        ATTRIBUTES *pASailorQuantity;
        ATTRIBUTES *pAShip;
        int32_t nMaxHP;
        int32_t nMaxSP;
        int32_t nMaxCrew;
        int32_t nTexture; // of m_aShipTextures, or -1 for m_nShipTextureID
        FRECT rUV;
        std::string sShipName;
        int32_t nShipClass;
    } m_Ship[MAX_SHIP_QUANTITY];

    int32_t m_nShipQuantity;
    bool m_bCommandCharacterSet = false; // by a command being confirmed
    int32_t m_nCurrentShipIndex;
    int32_t m_nCommandListVerticalOffset;
    // the command menu to the right of the icons row with this gap (Pirates of the Caribbean), -1: not used
    int32_t m_nCommandListRightGap = -1;
    // the state bars decrease to the left (Pirates of the Caribbean), not down
    bool m_bHorizontalBars = false;

    int32_t m_idSailorFont;
    uint32_t m_dwSailorFontColor;
    float m_fSailorFontScale;
    POINT m_SailorFontOffset;

    int32_t m_idShipNameFont;
    uint32_t m_dwShipNameFontColor;
    float m_fShipNameFontScale;
    POINT m_ShipNameFontOffset;

    bool m_bMakeUpdate;

    ATTRIBUTES *m_pARoot;

    BIShipCommandList *m_pCommandList;
    entid_t m_idHostEntity;
    int32_t m_nCommandMode;
};
