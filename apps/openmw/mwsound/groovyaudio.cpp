#include "groovyaudio.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <mutex>

#include <SDL.h>

#include <components/debug/debuglog.hpp>

#include "al.h"
#include "alext.h"

namespace MWSound::GroovyAudio
{
    namespace
    {
        std::mutex sMutex;
        ALCdevice* sDevice = nullptr;
        LPALCRENDERSAMPLESSOFT sRenderSamples = nullptr;

        SDL_AudioDeviceID sPcDevice = 0;

        bool sStarted = false;
        double sPending = 0.0;
        std::chrono::steady_clock::time_point sLastRender{};

        void openPcDevice()
        {
            if (SDL_WasInit(SDL_INIT_AUDIO) == 0 && SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
            {
                Log(Debug::Warning) << "Groovy MiSTer: can't play sound on the PC: " << SDL_GetError();
                return;
            }
            SDL_AudioSpec want{};
            want.freq = sSampleRate;
            want.format = AUDIO_S16SYS;
            want.channels = 2;
            want.samples = 1024;
            sPcDevice = SDL_OpenAudioDevice(nullptr, 0, &want, nullptr, 0);
            if (sPcDevice == 0)
            {
                Log(Debug::Warning) << "Groovy MiSTer: can't play sound on the PC: " << SDL_GetError();
                return;
            }
            SDL_PauseAudioDevice(sPcDevice, 0);
        }
    }

    ALCdevice* openLoopbackDevice()
    {
        if (alcIsExtensionPresent(nullptr, "ALC_SOFT_loopback") == ALC_FALSE)
        {
            Log(Debug::Warning) << "Groovy MiSTer: OpenAL has no loopback support, sound stays on the PC";
            return nullptr;
        }

        auto loopbackOpenDevice = reinterpret_cast<LPALCLOOPBACKOPENDEVICESOFT>(
            alcGetProcAddress(nullptr, "alcLoopbackOpenDeviceSOFT"));
        auto isRenderFormatSupported = reinterpret_cast<LPALCISRENDERFORMATSUPPORTEDSOFT>(
            alcGetProcAddress(nullptr, "alcIsRenderFormatSupportedSOFT"));
        sRenderSamples
            = reinterpret_cast<LPALCRENDERSAMPLESSOFT>(alcGetProcAddress(nullptr, "alcRenderSamplesSOFT"));
        if (loopbackOpenDevice == nullptr || isRenderFormatSupported == nullptr || sRenderSamples == nullptr)
        {
            Log(Debug::Warning) << "Groovy MiSTer: OpenAL loopback functions missing, sound stays on the PC";
            return nullptr;
        }

        ALCdevice* device = loopbackOpenDevice(nullptr);
        if (device == nullptr)
        {
            Log(Debug::Warning) << "Groovy MiSTer: couldn't open an OpenAL loopback device, sound stays on the PC";
            return nullptr;
        }
        if (isRenderFormatSupported(device, sSampleRate, ALC_STEREO_SOFT, ALC_SHORT_SOFT) == ALC_FALSE)
        {
            Log(Debug::Warning) << "Groovy MiSTer: OpenAL can't render 48 kHz stereo, sound stays on the PC";
            alcCloseDevice(device);
            return nullptr;
        }

        Log(Debug::Info) << "Groovy MiSTer: sound goes to the MiSTer (OpenAL loopback, 48 kHz stereo)";
        return device;
    }

    void addContextAttributes(std::vector<ALCint>& attributes)
    {
        attributes.push_back(ALC_FORMAT_CHANNELS_SOFT);
        attributes.push_back(ALC_STEREO_SOFT);
        attributes.push_back(ALC_FORMAT_TYPE_SOFT);
        attributes.push_back(ALC_SHORT_SOFT);
        attributes.push_back(ALC_FREQUENCY);
        attributes.push_back(sSampleRate);
    }

    void setDevice(ALCdevice* device, bool playOnPc)
    {
        const std::lock_guard<std::mutex> lock(sMutex);
        sDevice = device;
        sStarted = false;
        sPending = 0.0;

        if (device != nullptr && playOnPc && sPcDevice == 0)
            openPcDevice();
        if (device == nullptr && sPcDevice != 0)
        {
            SDL_CloseAudioDevice(sPcDevice);
            sPcDevice = 0;
        }
    }

    bool active()
    {
        const std::lock_guard<std::mutex> lock(sMutex);
        return sDevice != nullptr;
    }

    std::size_t render(std::vector<std::int16_t>& out)
    {
        const std::lock_guard<std::mutex> lock(sMutex);
        if (sDevice == nullptr || sRenderSamples == nullptr)
            return 0;

        // Render exactly as much sound as time has passed, so it keeps pace with the picture. The fraction of a
        // sample left over carries to the next frame.
        const auto now = std::chrono::steady_clock::now();
        if (!sStarted)
        {
            sStarted = true;
            sPending = sSampleRate / 60.0;
        }
        else
        {
            sPending += std::chrono::duration<double>(now - sLastRender).count() * sSampleRate;
        }
        sLastRender = now;

        // After a long pause (loading), don't try to catch up: that would just be a burst of sound.
        constexpr double maxFrames = sSampleRate / 10.0;
        sPending = std::min(sPending, maxFrames);

        const auto frames = static_cast<std::size_t>(sPending);
        sPending -= static_cast<double>(frames);
        if (frames == 0)
            return 0;

        out.resize(frames * 2);
        sRenderSamples(sDevice, out.data(), static_cast<ALCsizei>(frames));

        if (sPcDevice != 0)
        {
            // The PC's sound card runs on its own clock. If the queue has grown past 200 ms, start over from
            // now instead of drifting further behind.
            const Uint32 queued = SDL_GetQueuedAudioSize(sPcDevice);
            if (queued > static_cast<Uint32>(sSampleRate / 5 * 4))
                SDL_ClearQueuedAudio(sPcDevice);
            else if (queued < frames * 4)
            {
                // The queue ran dry (or this is the start): put 50 ms of silence in front, so small hiccups in
                // the frame rate don't turn into crackles.
                const std::vector<std::int16_t> silence(sSampleRate / 20 * 2, 0);
                SDL_QueueAudio(sPcDevice, silence.data(), static_cast<Uint32>(silence.size() * sizeof(std::int16_t)));
            }
            SDL_QueueAudio(sPcDevice, out.data(), static_cast<Uint32>(frames * 4));
        }

        return frames;
    }
}
