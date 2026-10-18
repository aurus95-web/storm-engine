#pragma once

#ifdef __APPLE__
#include <cstdint>
#include <functional>
#include <memory>

// Plays a video file (H.264/AAC .mp4: AVFoundation has no WMV decoder) with its sound and gives its frames as BGRA
class AppleVideo
{
  public:
    AppleVideo();
    ~AppleVideo();

    bool Open(const char *path);
    void SetLoop(bool loop);
    bool IsFinished();

    // calls consume with the frame to show now if it is a new one
    bool GetFrame(const std::function<void(const uint8_t *pixels, int32_t pitch, int32_t width, int32_t height)> &consume);

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
#endif
