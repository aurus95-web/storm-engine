#include "fmod.hpp"
#include "fmod_errors.h"

#include <miniaudio.h>

#include <cstdio>
#include <deque>
#include <string>
#include <vector>

// FMOD is left-handed (as D3D: +z forward), miniaudio right-handed (-z forward): z is mirrored

namespace FMOD
{
namespace
{
// the calls on a null handle are errors, as in FMOD (the compiler would drop a check of this against null)
template <class T> T *Handle(T *object)
{
    volatile auto value = reinterpret_cast<uintptr_t>(object);
    return reinterpret_cast<T *>(value);
}

// a stopped or ended channel is reused after a few updates: the sound service frees its slot first
constexpr int kReuseUpdates = 2;
} // namespace

struct Sound::Impl
{
    System::Impl *system = nullptr;
    std::string path;
    FMOD_MODE mode = FMOD_DEFAULT;
    bool stream = false;
    ma_sound base{}; // the decoded sound the channels copy (not a stream)
    bool baseInitialized = false;
};

struct Channel::Impl
{
    System::Impl *system = nullptr;
    ma_sound sound{};
    bool initialized = false;
    bool stopping = false; // fading out before its release (without a click)
    bool paused = false;
    Sound *current = nullptr;
    FMOD_MODE mode = FMOD_DEFAULT;
    float volume = 1.f;
    float minDistance = 1.f, maxDistance = 10000.f;
    FMOD_VECTOR position{};
    int priority = 128;
    // the first point of a fade (addFadePoint), the second one starts it
    bool fadeStarted = false;
    unsigned long long fadeClock = 0;
    float fadeVolume = 1.f;

    uint32_t SampleRate()
    {
        ma_uint32 rate = 0;
        ma_sound_get_data_format(&sound, nullptr, nullptr, &rate, nullptr, 0);
        return rate ? rate : 44100;
    }
};

struct System::Impl
{
    ma_engine engine{};
    bool engineInitialized = false;
    std::deque<Channel> channels;
    std::deque<Channel::Impl> channelImpls;
    std::vector<Channel *> freeChannels;
    std::vector<std::pair<Channel *, int>> stoppedChannels; // and the updates before the reuse
    ChannelGroup master;
    FMOD_VECTOR listenerPos{}, listenerForward{0.f, 0.f, 1.f}, listenerUp{0.f, 1.f, 0.f};

    void Stop(Channel &channel);
};

void System::Impl::Stop(Channel &channel)
{
    auto &impl = *channel.impl_;
    if (!impl.initialized)
        return;
    // a sound cut in the middle of its wave clicks: it fades out for some milliseconds, released when reused
    ma_sound_stop_with_fade_in_milliseconds(&impl.sound, 10);
    impl.stopping = true;
    impl.initialized = false;
    impl.current = nullptr;
    stoppedChannels.emplace_back(&channel, kReuseUpdates);
}

FMOD_RESULT System_Create(System **system)
{
    *system = new System;
    (*system)->impl_ = new System::Impl;
    return FMOD_OK;
}

FMOD_RESULT Memory_GetStats(int *currentalloced, int *maxalloced, bool)
{
    if (currentalloced)
        *currentalloced = 0;
    if (maxalloced)
        *maxalloced = 0;
    return FMOD_OK;
}

//--------------------------------------------------------------------------------------------------
// System

FMOD_RESULT System::getVersion(unsigned int *version)
{
    *version = FMOD_VERSION;
    return FMOD_OK;
}

FMOD_RESULT System::setSoftwareChannels(int)
{
    return FMOD_OK;
}

FMOD_RESULT System::setOutput(FMOD_OUTPUTTYPE)
{
    return FMOD_OK;
}

FMOD_RESULT System::init(int, FMOD_INITFLAGS, void *)
{
    auto config = ma_engine_config_init();
    config.listenerCount = 1;
    if (ma_engine_init(&config, &impl_->engine) != MA_SUCCESS)
        return FMOD_ERR_INTERNAL;
    impl_->engineInitialized = true;
    return FMOD_OK;
}

FMOD_RESULT System::set3DSettings(float, float, float)
{
    return FMOD_OK;
}

FMOD_RESULT System::close()
{
    if (!impl_->engineInitialized)
        return FMOD_OK;
    for (auto &channel : impl_->channels)
    {
        impl_->Stop(channel);
        if (channel.impl_->stopping)
            ma_sound_uninit(&channel.impl_->sound);
        channel.impl_->stopping = false;
    }
    ma_engine_uninit(&impl_->engine);
    impl_->engineInitialized = false;
    return FMOD_OK;
}

FMOD_RESULT System::release()
{
    close();
    delete impl_;
    delete this;
    return FMOD_OK;
}

FMOD_RESULT System::update()
{
    // the ended sounds stop (their channels are reused later)
    for (auto &channel : impl_->channels)
    {
        auto &impl = *channel.impl_;
        if (impl.initialized && !impl.paused && !ma_sound_is_looping(&impl.sound) && ma_sound_at_end(&impl.sound))
            impl_->Stop(channel);
    }
    auto &stopped = impl_->stoppedChannels;
    for (auto it = stopped.begin(); it != stopped.end();)
    {
        if (--it->second > 0)
        {
            ++it;
            continue;
        }
        auto &impl = *it->first->impl_;
        if (impl.stopping)
        {
            ma_sound_uninit(&impl.sound);
            impl.stopping = false;
        }
        impl_->freeChannels.push_back(it->first);
        it = stopped.erase(it);
    }
    return FMOD_OK;
}

FMOD_RESULT System::createSound(const char *name_or_data, FMOD_MODE mode, FMOD_CREATESOUNDEXINFO *, Sound **sound)
{
    *sound = nullptr;
    auto *impl = new Sound::Impl;
    impl->system = impl_;
    impl->path = name_or_data;
    impl->mode = mode;
    // decoded once (the resource manager of miniaudio), the channels share the data
    if (ma_sound_init_from_file(&impl_->engine, name_or_data, MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_NO_SPATIALIZATION,
                                nullptr, nullptr, &impl->base) != MA_SUCCESS)
    {
        delete impl;
        return FMOD_ERR_FILE_NOTFOUND;
    }
    impl->baseInitialized = true;
    *sound = new Sound;
    (*sound)->impl_ = impl;
    return FMOD_OK;
}

FMOD_RESULT System::createStream(const char *name_or_data, FMOD_MODE mode, FMOD_CREATESOUNDEXINFO *, Sound **sound)
{
    *sound = nullptr;
    // the file is opened by each channel
    ma_decoder decoder;
    if (ma_decoder_init_file(name_or_data, nullptr, &decoder) != MA_SUCCESS)
        return FMOD_ERR_FILE_NOTFOUND;
    ma_decoder_uninit(&decoder);
    auto *impl = new Sound::Impl;
    impl->system = impl_;
    impl->path = name_or_data;
    impl->mode = mode;
    impl->stream = true;
    *sound = new Sound;
    (*sound)->impl_ = impl;
    return FMOD_OK;
}

FMOD_RESULT System::playSound(Sound *sound, ChannelGroup *, bool paused, Channel **channel)
{
    *channel = nullptr;
    sound = Handle(sound);
    if (!sound)
        return FMOD_ERR_INVALID_HANDLE;
    Channel *result;
    if (!impl_->freeChannels.empty())
    {
        result = impl_->freeChannels.back();
        impl_->freeChannels.pop_back();
    }
    else
    {
        result = &impl_->channels.emplace_back();
        result->impl_ = &impl_->channelImpls.emplace_back();
        result->impl_->system = impl_;
    }
    auto &impl = *result->impl_;
    const auto &source = *sound->impl_;
    const ma_uint32 flags = source.mode & FMOD_3D ? 0 : MA_SOUND_FLAG_NO_SPATIALIZATION;
    const auto status =
        source.stream ? ma_sound_init_from_file(&impl_->engine, source.path.c_str(), flags | MA_SOUND_FLAG_STREAM,
                                                nullptr, nullptr, &impl.sound)
                      : ma_sound_init_copy(&impl_->engine, &sound->impl_->base, flags, nullptr, &impl.sound);
    if (status != MA_SUCCESS)
    {
        impl_->freeChannels.push_back(result);
        return FMOD_ERR_FORMAT;
    }
    impl.initialized = true;
    impl.paused = paused;
    impl.current = sound;
    impl.mode = source.mode;
    impl.volume = 1.f;
    impl.minDistance = 1.f;
    impl.maxDistance = 10000.f;
    impl.position = {};
    impl.fadeStarted = false;
    ma_sound_set_looping(&impl.sound, (source.mode & FMOD_LOOP_NORMAL) != 0);
    if (source.mode & FMOD_3D)
    {
        // full volume up to the min distance, none at the max one (FMOD_3D_LINEARROLLOFF)
        ma_sound_set_attenuation_model(&impl.sound, ma_attenuation_model_linear);
        ma_sound_set_min_distance(&impl.sound, impl.minDistance);
        ma_sound_set_max_distance(&impl.sound, impl.maxDistance);
        ma_sound_set_doppler_factor(&impl.sound, 0.f);
    }
    if (!paused)
        ma_sound_start(&impl.sound);
    *channel = result;
    return FMOD_OK;
}

FMOD_RESULT System::set3DListenerAttributes(int, const FMOD_VECTOR *pos, const FMOD_VECTOR *, const FMOD_VECTOR *forward,
                                            const FMOD_VECTOR *up)
{
    if (pos)
    {
        impl_->listenerPos = *pos;
        ma_engine_listener_set_position(&impl_->engine, 0, pos->x, pos->y, -pos->z);
    }
    if (forward)
    {
        impl_->listenerForward = *forward;
        ma_engine_listener_set_direction(&impl_->engine, 0, forward->x, forward->y, -forward->z);
    }
    if (up)
    {
        impl_->listenerUp = *up;
        ma_engine_listener_set_world_up(&impl_->engine, 0, up->x, up->y, -up->z);
    }
    return FMOD_OK;
}

FMOD_RESULT System::get3DListenerAttributes(int, FMOD_VECTOR *pos, FMOD_VECTOR *vel, FMOD_VECTOR *forward,
                                            FMOD_VECTOR *up)
{
    if (pos)
        *pos = impl_->listenerPos;
    if (vel)
        *vel = {};
    if (forward)
        *forward = impl_->listenerForward;
    if (up)
        *up = impl_->listenerUp;
    return FMOD_OK;
}

FMOD_RESULT System::getCPUUsage(FMOD_CPU_USAGE *usage)
{
    *usage = {};
    return FMOD_OK;
}

FMOD_RESULT System::getMasterChannelGroup(ChannelGroup **channelgroup)
{
    *channelgroup = &impl_->master;
    return FMOD_OK;
}

FMOD_RESULT System::getSoftwareFormat(int *samplerate, int *speakermode, int *numrawspeakers)
{
    if (samplerate)
        *samplerate = static_cast<int>(ma_engine_get_sample_rate(&impl_->engine));
    if (speakermode)
        *speakermode = 0;
    if (numrawspeakers)
        *numrawspeakers = 2;
    return FMOD_OK;
}

FMOD_RESULT System::mixerSuspend()
{
    ma_engine_stop(&impl_->engine);
    return FMOD_OK;
}

FMOD_RESULT System::mixerResume()
{
    ma_engine_start(&impl_->engine);
    return FMOD_OK;
}

//--------------------------------------------------------------------------------------------------
// Sound

FMOD_RESULT Sound::release()
{
    auto *self = Handle(this);
    if (!self)
        return FMOD_ERR_INVALID_HANDLE;
    // its channels stop, as in FMOD
    auto &system = *impl_->system;
    for (auto &channel : system.channels)
    {
        if (channel.impl_->initialized && channel.impl_->current == self)
            system.Stop(channel);
    }
    if (impl_->baseInitialized)
        ma_sound_uninit(&impl_->base);
    delete impl_;
    delete self;
    return FMOD_OK;
}

FMOD_RESULT Sound::getLength(unsigned int *length, FMOD_TIMEUNIT lengthtype)
{
    auto *self = Handle(this);
    if (!self)
        return FMOD_ERR_INVALID_HANDLE;
    ma_uint64 frames = 0;
    ma_uint32 rate = 44100;
    if (impl_->baseInitialized)
    {
        ma_sound_get_length_in_pcm_frames(&impl_->base, &frames);
        ma_sound_get_data_format(&impl_->base, nullptr, nullptr, &rate, nullptr, 0);
    }
    else
    {
        ma_decoder decoder;
        if (ma_decoder_init_file(impl_->path.c_str(), nullptr, &decoder) != MA_SUCCESS)
            return FMOD_ERR_FILE_NOTFOUND;
        ma_decoder_get_length_in_pcm_frames(&decoder, &frames);
        rate = decoder.outputSampleRate;
        ma_decoder_uninit(&decoder);
    }
    *length = static_cast<unsigned int>(lengthtype == FMOD_TIMEUNIT_MS ? frames * 1000 / (rate ? rate : 44100) : frames);
    return FMOD_OK;
}

//--------------------------------------------------------------------------------------------------
// Channel

#define CHANNEL_IMPL                                                                                                   \
    auto *self = Handle(this);                                                                                         \
    if (!self || !self->impl_->initialized)                                                                            \
        return FMOD_ERR_INVALID_HANDLE;                                                                                \
    auto &impl = *self->impl_

FMOD_RESULT Channel::stop()
{
    auto *self = Handle(this);
    if (!self)
        return FMOD_ERR_INVALID_HANDLE;
    self->impl_->system->Stop(*self);
    return FMOD_OK;
}

FMOD_RESULT Channel::setPaused(bool paused)
{
    CHANNEL_IMPL;
    impl.paused = paused;
    if (paused)
        ma_sound_stop(&impl.sound);
    else
        ma_sound_start(&impl.sound);
    return FMOD_OK;
}

FMOD_RESULT Channel::getPaused(bool *paused)
{
    CHANNEL_IMPL;
    *paused = impl.paused;
    return FMOD_OK;
}

FMOD_RESULT Channel::isPlaying(bool *isplaying)
{
    // a paused channel plays, as in FMOD
    auto *self = Handle(this);
    *isplaying = self && self->impl_->initialized &&
                 (ma_sound_is_looping(&self->impl_->sound) || !ma_sound_at_end(&self->impl_->sound));
    return self ? FMOD_OK : FMOD_ERR_INVALID_HANDLE;
}

FMOD_RESULT Channel::isVirtual(bool *isvirtual)
{
    CHANNEL_IMPL;
    *isvirtual = false;
    return FMOD_OK;
}

FMOD_RESULT Channel::setVolume(float volume)
{
    CHANNEL_IMPL;
    impl.volume = volume;
    ma_sound_set_volume(&impl.sound, volume);
    return FMOD_OK;
}

FMOD_RESULT Channel::getVolume(float *volume)
{
    CHANNEL_IMPL;
    *volume = impl.volume;
    return FMOD_OK;
}

FMOD_RESULT Channel::getAudibility(float *audibility)
{
    CHANNEL_IMPL;
    *audibility = impl.volume;
    return FMOD_OK;
}

FMOD_RESULT Channel::setPitch(float pitch)
{
    CHANNEL_IMPL;
    ma_sound_set_pitch(&impl.sound, pitch);
    return FMOD_OK;
}

FMOD_RESULT Channel::setPosition(unsigned int position, FMOD_TIMEUNIT postype)
{
    CHANNEL_IMPL;
    const auto frame = postype == FMOD_TIMEUNIT_MS ? static_cast<ma_uint64>(position) * impl.SampleRate() / 1000
                                                   : static_cast<ma_uint64>(position);
    ma_sound_seek_to_pcm_frame(&impl.sound, frame);
    return FMOD_OK;
}

FMOD_RESULT Channel::getPosition(unsigned int *position, FMOD_TIMEUNIT postype)
{
    CHANNEL_IMPL;
    ma_uint64 frame = 0;
    ma_sound_get_cursor_in_pcm_frames(&impl.sound, &frame);
    *position = static_cast<unsigned int>(postype == FMOD_TIMEUNIT_MS ? frame * 1000 / impl.SampleRate() : frame);
    return FMOD_OK;
}

FMOD_RESULT Channel::setPriority(int priority)
{
    CHANNEL_IMPL;
    impl.priority = priority;
    return FMOD_OK;
}

FMOD_RESULT Channel::getPriority(int *priority)
{
    CHANNEL_IMPL;
    *priority = impl.priority;
    return FMOD_OK;
}

FMOD_RESULT Channel::setMode(FMOD_MODE mode)
{
    CHANNEL_IMPL;
    if (mode & FMOD_LOOP_NORMAL)
        impl.mode = (impl.mode & ~FMOD_LOOP_OFF) | FMOD_LOOP_NORMAL;
    if (mode & FMOD_LOOP_OFF)
        impl.mode = (impl.mode & ~FMOD_LOOP_NORMAL) | FMOD_LOOP_OFF;
    ma_sound_set_looping(&impl.sound, (impl.mode & FMOD_LOOP_NORMAL) != 0);
    return FMOD_OK;
}

FMOD_RESULT Channel::getMode(FMOD_MODE *mode)
{
    CHANNEL_IMPL;
    *mode = impl.mode;
    return FMOD_OK;
}

FMOD_RESULT Channel::setLoopPoints(unsigned int loopstart, FMOD_TIMEUNIT loopstarttype, unsigned int loopend,
                                   FMOD_TIMEUNIT loopendtype)
{
    CHANNEL_IMPL;
    const auto toFrame = [&](unsigned int value, FMOD_TIMEUNIT type) {
        return type == FMOD_TIMEUNIT_MS ? static_cast<ma_uint64>(value) * impl.SampleRate() / 1000
                                        : static_cast<ma_uint64>(value);
    };
    ma_data_source_set_loop_point_in_pcm_frames(ma_sound_get_data_source(&impl.sound), toFrame(loopstart, loopstarttype),
                                                toFrame(loopend, loopendtype));
    return FMOD_OK;
}

FMOD_RESULT Channel::setLoopCount(int)
{
    // miniaudio loops until stopped
    return FMOD_OK;
}

FMOD_RESULT Channel::set3DAttributes(const FMOD_VECTOR *pos, const FMOD_VECTOR *)
{
    CHANNEL_IMPL;
    if (pos)
    {
        impl.position = *pos;
        ma_sound_set_position(&impl.sound, pos->x, pos->y, -pos->z);
    }
    return FMOD_OK;
}

FMOD_RESULT Channel::get3DAttributes(FMOD_VECTOR *pos, FMOD_VECTOR *vel)
{
    CHANNEL_IMPL;
    if (pos)
        *pos = impl.position;
    if (vel)
        *vel = {};
    return FMOD_OK;
}

FMOD_RESULT Channel::set3DMinMaxDistance(float mindistance, float maxdistance)
{
    CHANNEL_IMPL;
    impl.minDistance = mindistance;
    impl.maxDistance = maxdistance;
    ma_sound_set_min_distance(&impl.sound, mindistance);
    ma_sound_set_max_distance(&impl.sound, maxdistance);
    return FMOD_OK;
}

FMOD_RESULT Channel::get3DMinMaxDistance(float *mindistance, float *maxdistance)
{
    CHANNEL_IMPL;
    *mindistance = impl.minDistance;
    *maxdistance = impl.maxDistance;
    return FMOD_OK;
}

FMOD_RESULT Channel::addFadePoint(unsigned long long dspclock, float volume)
{
    // the fades of SetActiveWithFade: two points, from the volume of the first to the one of the second
    CHANNEL_IMPL;
    if (!impl.fadeStarted)
    {
        impl.fadeStarted = true;
        impl.fadeClock = dspclock;
        impl.fadeVolume = volume;
        return FMOD_OK;
    }
    impl.fadeStarted = false;
    const auto frames = dspclock > impl.fadeClock ? dspclock - impl.fadeClock : 0;
    ma_sound_set_fade_in_pcm_frames(&impl.sound, impl.fadeVolume, volume, frames);
    return FMOD_OK;
}

FMOD_RESULT Channel::getCurrentSound(Sound **sound)
{
    CHANNEL_IMPL;
    *sound = impl.current;
    return FMOD_OK;
}

//--------------------------------------------------------------------------------------------------
// ChannelGroup

FMOD_RESULT ChannelGroup::getDSPClock(unsigned long long *dspclock, unsigned long long *parentclock)
{
    // the clock of the engine (the only group is the master one)
    const auto *system = reinterpret_cast<const System::Impl *>(reinterpret_cast<const char *>(this) -
                                                                offsetof(System::Impl, master));
    const auto clock = ma_engine_get_time_in_pcm_frames(&system->engine);
    if (dspclock)
        *dspclock = clock;
    if (parentclock)
        *parentclock = clock;
    return FMOD_OK;
}

FMOD_RESULT ChannelGroup::setDelay(unsigned long long, unsigned long long, bool)
{
    return FMOD_OK;
}
} // namespace FMOD

const char *FMOD_ErrorString(FMOD_RESULT result)
{
    switch (result)
    {
    case FMOD_OK:
        return "No errors.";
    case FMOD_ERR_FILE_NOTFOUND:
        return "File not found.";
    case FMOD_ERR_FORMAT:
        return "Unsupported file or audio format.";
    case FMOD_ERR_INVALID_HANDLE:
        return "An invalid object handle was used.";
    case FMOD_ERR_INVALID_PARAM:
        return "An invalid parameter was passed to this function.";
    default:
        return "An error occurred inside the sound library.";
    }
}
