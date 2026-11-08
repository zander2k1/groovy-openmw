#ifndef GAME_SOUND_GROOVYAUDIO_H
#define GAME_SOUND_GROOVYAUDIO_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include "alc.h"

namespace MWSound
{
    /// Groovy MiSTer audio: lets the sound go to the MiSTer together with the picture.
    ///
    /// With Groovy audio on, OpenAL renders into a loopback device instead of the sound card. Every frame,
    /// GroovyOutput (on the draw thread) asks for the sound for the time since the previous frame, sends it
    /// to the MiSTer right after the picture, and the same sound is also played on the PC (optional).
    namespace GroovyAudio
    {
        constexpr int sSampleRate = 48000; // what the Groovy core plays; stereo, 16-bit

        /// Opens an OpenAL loopback device, or returns nullptr if OpenAL can't do 48 kHz stereo 16-bit loopback.
        ALCdevice* openLoopbackDevice();

        /// Context attributes the loopback device needs (format and rate), to add before the terminating 0.
        void addContextAttributes(std::vector<ALCint>& attributes);

        /// Makes the device available for rendering, or removes it (nullptr) before it's closed.
        void setDevice(ALCdevice* device, bool playOnPc);

        /// True while a loopback device is in use.
        bool active();

        /// Renders the sound for the time since the previous call (at most 100 ms) into out, as interleaved
        /// stereo 16-bit samples, and plays it on the PC if enabled. Returns the number of stereo frames.
        std::size_t render(std::vector<std::int16_t>& out);
    }
}

#endif
