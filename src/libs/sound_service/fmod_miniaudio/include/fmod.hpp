#pragma once

// The part of the FMOD Core API used by the sound service (sound_service.cpp), played by miniaudio
// (https://miniaud.io, public domain): the engine has sound without the proprietary FMOD SDK
// (STORM_SOUND_MINIAUDIO). The handles behave as the FMOD ones: a stopped or ended channel is reused,
// the calls on a null handle return FMOD_ERR_INVALID_HANDLE.

#include <cstdint>

typedef enum FMOD_RESULT
{
    FMOD_OK,
    FMOD_ERR_FILE_NOTFOUND,
    FMOD_ERR_FORMAT,
    FMOD_ERR_INVALID_HANDLE,
    FMOD_ERR_INVALID_PARAM,
    FMOD_ERR_INTERNAL,
} FMOD_RESULT;

typedef unsigned int FMOD_MODE;
typedef unsigned int FMOD_TIMEUNIT;
typedef unsigned int FMOD_INITFLAGS;

#define FMOD_VERSION 0x00020235

#define FMOD_DEFAULT 0x00000000
#define FMOD_LOOP_OFF 0x00000001
#define FMOD_LOOP_NORMAL 0x00000002
#define FMOD_2D 0x00000008
#define FMOD_3D 0x00000010
#define FMOD_CREATESAMPLE 0x00000100
#define FMOD_3D_LINEARROLLOFF 0x00200000

#define FMOD_TIMEUNIT_MS 0x00000001
#define FMOD_TIMEUNIT_PCM 0x00000002

#define FMOD_INIT_NORMAL 0x00000000

typedef enum FMOD_OUTPUTTYPE
{
    FMOD_OUTPUTTYPE_AUTODETECT,
} FMOD_OUTPUTTYPE;

struct FMOD_VECTOR
{
    float x, y, z;
};

struct FMOD_CPU_USAGE
{
    float dsp, stream, geometry, update, convolution1, convolution2;
};

struct FMOD_CREATESOUNDEXINFO;

namespace FMOD
{
class System;
class Sound;
class Channel;
class ChannelGroup;

FMOD_RESULT System_Create(System **system);
FMOD_RESULT Memory_GetStats(int *currentalloced, int *maxalloced, bool blocking = true);

class Sound
{
  public:
    FMOD_RESULT release();
    FMOD_RESULT getLength(unsigned int *length, FMOD_TIMEUNIT lengthtype);

    struct Impl;
    Impl *impl_ = nullptr;
};

class Channel
{
  public:
    FMOD_RESULT stop();
    FMOD_RESULT setPaused(bool paused);
    FMOD_RESULT getPaused(bool *paused);
    FMOD_RESULT isPlaying(bool *isplaying);
    FMOD_RESULT isVirtual(bool *isvirtual);
    FMOD_RESULT setVolume(float volume);
    FMOD_RESULT getVolume(float *volume);
    FMOD_RESULT getAudibility(float *audibility);
    FMOD_RESULT setPitch(float pitch);
    FMOD_RESULT setPosition(unsigned int position, FMOD_TIMEUNIT postype);
    FMOD_RESULT getPosition(unsigned int *position, FMOD_TIMEUNIT postype);
    FMOD_RESULT setPriority(int priority);
    FMOD_RESULT getPriority(int *priority);
    FMOD_RESULT setMode(FMOD_MODE mode);
    FMOD_RESULT getMode(FMOD_MODE *mode);
    FMOD_RESULT setLoopPoints(unsigned int loopstart, FMOD_TIMEUNIT loopstarttype, unsigned int loopend,
                              FMOD_TIMEUNIT loopendtype);
    FMOD_RESULT setLoopCount(int loopcount);
    FMOD_RESULT set3DAttributes(const FMOD_VECTOR *pos, const FMOD_VECTOR *vel);
    FMOD_RESULT get3DAttributes(FMOD_VECTOR *pos, FMOD_VECTOR *vel);
    FMOD_RESULT set3DMinMaxDistance(float mindistance, float maxdistance);
    FMOD_RESULT get3DMinMaxDistance(float *mindistance, float *maxdistance);
    FMOD_RESULT addFadePoint(unsigned long long dspclock, float volume);
    FMOD_RESULT getCurrentSound(Sound **sound);

    struct Impl;
    Impl *impl_ = nullptr;
};

class ChannelGroup
{
  public:
    FMOD_RESULT getDSPClock(unsigned long long *dspclock, unsigned long long *parentclock);
    FMOD_RESULT setDelay(unsigned long long dspclock_start, unsigned long long dspclock_end, bool stopchannels = true);
};

class System
{
  public:
    FMOD_RESULT getVersion(unsigned int *version);
    FMOD_RESULT setSoftwareChannels(int numsoftwarechannels);
    FMOD_RESULT setOutput(FMOD_OUTPUTTYPE output);
    FMOD_RESULT init(int maxchannels, FMOD_INITFLAGS flags, void *extradriverdata);
    FMOD_RESULT set3DSettings(float dopplerscale, float distancefactor, float rolloffscale);
    FMOD_RESULT close();
    FMOD_RESULT release();
    FMOD_RESULT update();
    FMOD_RESULT createSound(const char *name_or_data, FMOD_MODE mode, FMOD_CREATESOUNDEXINFO *exinfo, Sound **sound);
    FMOD_RESULT createStream(const char *name_or_data, FMOD_MODE mode, FMOD_CREATESOUNDEXINFO *exinfo, Sound **sound);
    FMOD_RESULT playSound(Sound *sound, ChannelGroup *channelgroup, bool paused, Channel **channel);
    FMOD_RESULT set3DListenerAttributes(int listener, const FMOD_VECTOR *pos, const FMOD_VECTOR *vel,
                                        const FMOD_VECTOR *forward, const FMOD_VECTOR *up);
    FMOD_RESULT get3DListenerAttributes(int listener, FMOD_VECTOR *pos, FMOD_VECTOR *vel, FMOD_VECTOR *forward,
                                        FMOD_VECTOR *up);
    FMOD_RESULT getCPUUsage(FMOD_CPU_USAGE *usage);
    FMOD_RESULT getMasterChannelGroup(ChannelGroup **channelgroup);
    FMOD_RESULT getSoftwareFormat(int *samplerate, int *speakermode, int *numrawspeakers);
    FMOD_RESULT mixerSuspend();
    FMOD_RESULT mixerResume();

    struct Impl;
    Impl *impl_ = nullptr;
};
} // namespace FMOD
