// The implementation of miniaudio with the Vorbis decoder of stb_vorbis (the music of the game is Ogg Vorbis).
// On Apple platforms it is compiled as Objective-C (AVAudioSession of iOS).

#define STB_VORBIS_HEADER_ONLY
#include <extras/stb_vorbis.c>

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#undef STB_VORBIS_HEADER_ONLY
#include <extras/stb_vorbis.c>
