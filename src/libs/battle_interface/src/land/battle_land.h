#pragma once

#include "bi_utils.h"
#include "bi_defines.h"

class BIManSign;
class MESSAGE_ICONS;

class BATTLE_LAND_INTERFACE : public Entity
{
    // metods
  public:
    BATTLE_LAND_INTERFACE();
    ~BATTLE_LAND_INTERFACE() override;
    bool Init() override;
    void Execute(uint32_t delta_time) const;
    void Realize(uint32_t delta_time);
    uint64_t ProcessMessage(MESSAGE &message) override;

    void ProcessStage(Stage stage, uint32_t delta) override
    {
        switch (stage)
        {
        case Stage::execute:
            Execute(delta);
            break;
        case Stage::realize:
            Realize(delta);
            break;
            /*case Stage::lost_render:
              LostRender(delta); break;
            case Stage::restore_render:
              RestoreRender(delta); break;*/
        }
    }

  protected:
    void SetShowParameters();
    ATTRIBUTES *ConvertPotcParameters();
    void AddPotcCommandTextures();
    void SetParameters();
    void UpdateCommandos() const;
    void UpdateAlarm();
    void EndShow();
    void Release();
    void EnableMessageIcons(VDATA *pvdat);
    void DisableMessageIcons();
    void SetTextData();

    // data
  protected:
    VDX9RENDER *m_pRS;
    bool m_bShowCommandos;

    BIManSign *m_pManSign;
    // the icons above the portraits (the level up...): Pirates of the Caribbean
    MESSAGE_ICONS *m_pMessageIcons = nullptr;
    uint32_t m_nMessageIconsRequestTime = 0;
    int32_t m_aMessageIconLeft[4]{};
    ATTRIBUTES *m_aMessageIconAttr[4]{};
    int32_t m_nMessageIconBottom = -1, m_nMessageIconWidth = 0;
    // Pirates of the Caribbean: the command textures are set after the entity is created
    bool m_bPotcCommandTexturesAdded = false;

    std::vector<BITextInfo> m_TextInfo;

    BIImagesInfo m_Images;
};
