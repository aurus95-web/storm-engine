#include "apple_video.h"

#import <AVFoundation/AVFoundation.h>
#import <QuartzCore/QuartzCore.h>

struct AppleVideo::Impl
{
    AVPlayer *player = nil;
    AVPlayerItem *item = nil;
    AVPlayerItemVideoOutput *output = nil;
    bool loop = false;
};

AppleVideo::AppleVideo() : impl_(std::make_unique<Impl>())
{
}

AppleVideo::~AppleVideo()
{
    [impl_->player pause];
    if (impl_->output)
        [impl_->item removeOutput:impl_->output];
}

bool AppleVideo::Open(const char *path)
{
    NSString *file = [NSString stringWithUTF8String:path];
    if (!file || ![[NSFileManager defaultManager] fileExistsAtPath:file])
        return false;
    impl_->item = [AVPlayerItem playerItemWithURL:[NSURL fileURLWithPath:file]];
    NSDictionary *attributes = @{(id)kCVPixelBufferPixelFormatTypeKey : @(kCVPixelFormatType_32BGRA)};
    impl_->output = [[AVPlayerItemVideoOutput alloc] initWithPixelBufferAttributes:attributes];
    [impl_->item addOutput:impl_->output];
    impl_->player = [AVPlayer playerWithPlayerItem:impl_->item];
    impl_->player.actionAtItemEnd = AVPlayerActionAtItemEndPause;
    [impl_->player play];
    return true;
}

void AppleVideo::SetLoop(bool loop)
{
    impl_->loop = loop;
}

bool AppleVideo::IsFinished()
{
    AVPlayerItem *item = impl_->item;
    if (!item || item.status == AVPlayerItemStatusFailed)
        return true;
    const CMTime duration = item.duration;
    if (item.status != AVPlayerItemStatusReadyToPlay || !CMTIME_IS_NUMERIC(duration) ||
        CMTimeCompare(item.currentTime, duration) < 0)
        return false;
    if (!impl_->loop)
        return true;
    [item seekToTime:kCMTimeZero completionHandler:nil];
    [impl_->player play];
    return false;
}

bool AppleVideo::GetFrame(
    const std::function<void(const uint8_t *pixels, int32_t pitch, int32_t width, int32_t height)> &consume)
{
    AVPlayerItemVideoOutput *output = impl_->output;
    if (!output)
        return false;
    const CMTime time = [output itemTimeForHostTime:CACurrentMediaTime()];
    if (![output hasNewPixelBufferForItemTime:time])
        return false;
    CVPixelBufferRef buffer = [output copyPixelBufferForItemTime:time itemTimeForDisplay:nullptr];
    if (!buffer)
        return false;
    CVPixelBufferLockBaseAddress(buffer, kCVPixelBufferLock_ReadOnly);
    consume(static_cast<const uint8_t *>(CVPixelBufferGetBaseAddress(buffer)),
            static_cast<int32_t>(CVPixelBufferGetBytesPerRow(buffer)), static_cast<int32_t>(CVPixelBufferGetWidth(buffer)),
            static_cast<int32_t>(CVPixelBufferGetHeight(buffer)));
    CVPixelBufferUnlockBaseAddress(buffer, kCVPixelBufferLock_ReadOnly);
    CVPixelBufferRelease(buffer);
    return true;
}
