#pragma once

#include <entity.h>

#include "camera.hpp"

namespace storm {

class Scene : public Entity {
public:
    bool Init() override;

    void ProcessStage(Stage stage, uint32_t delta) override;

    Scene &SetActiveCamera(Camera* camera);

    Camera *GetActiveCamera() const;

    uint64_t ProcessMessage(MESSAGE &msg) override;

    static Scene& GetDefaultScene();

  private:
    entid_t activeCamera_ = invalid_entity;
    entid_t previousCamera_ = invalid_entity; // active again if the active one is deleted
};

} // namespace storm
