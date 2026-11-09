#include "groovyoutput.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <thread>

#include <SDL.h>

#include <osg/BufferObject>
#include <osg/FrameBufferObject>
#include <osg/GLExtensions>
#include <osg/State>

#include <components/debug/debuglog.hpp>
#include <components/settings/values.hpp>

#include "mwbase/environment.hpp"
#include "mwbase/windowmanager.hpp"
#include "mwsound/groovyaudio.hpp"

// Last, since it pulls in <windows.h> on Windows.
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif
#include <groovymister_wrapper.h>

#ifndef GL_BGR
#define GL_BGR 0x80E0
#endif
#ifndef GL_PIXEL_PACK_BUFFER_BINDING
#define GL_PIXEL_PACK_BUFFER_BINDING 0x88ED
#endif

namespace OMW
{
    namespace
    {
        // Try connecting this often while the MiSTer doesn't answer.
        constexpr std::chrono::seconds retryInterval(2);
        // The MiSTer answers every frame. No answer for this long means it's gone.
        constexpr std::chrono::seconds lostTimeout(3);

        // Horizontal timing for the 15 kHz modes: 640 active pixels of 858 at 13.5 MHz, the same picture
        // width and position as a GameCube or Wii (and close to a PS2) on a CRT TV, so the edges of the
        // picture stay clear of the TV's overscan.
        constexpr std::uint16_t hActive = 640;
        constexpr std::uint16_t hBegin = 696;
        constexpr std::uint16_t hEnd = 760;
        constexpr std::uint16_t hTotal = 858;
    }

    namespace
    {
        // Picture size setting: 0 means automatic, 95% on CRT TVs (they hide a few percent of each edge, so
        // the picture stays inside the "safe area" like console games did) and 100% on 480p PC monitors.
        double pictureScale(int size, GroovyOutput::Mode mode)
        {
            if (size <= 0)
                size = mode == GroovyOutput::Mode::Progressive480 ? 100 : 95;
            return std::clamp(size, 50, 100) / 100.0;
        }
    }

    GroovyOutput::GroovyOutput(const std::string& host, Mode mode, int syncLine, bool sendAudio)
        : mHost(host)
        , mMode(mode)
        , mSyncLine(std::clamp(syncLine, 0, 400))
        , mSendAudio(sendAudio)
        , mFlickerFilter(Settings::groovy().mFlickerFilter.get())
        , mLightFlickerFilter(Settings::groovy().mFlickerFilterStrength.get() == "light")
        , mFrameDelay(Settings::groovy().mFrameDelay.get()
                  // Tenths of a frame. 480i and 480p run at 59.94 frames (fields) a second, 240p at 60.
                  ? std::chrono::microseconds(Settings::groovy().mFrameDelayAmount.get() * 1668)
                  : std::chrono::microseconds(0))
        , mPictureScale(pictureScale(Settings::groovy().mPictureSize.get(), mode))
        , mShiftX(Settings::groovy().mPictureShiftX.get())
        , mShiftY(Settings::groovy().mPictureShiftY.get())
    {
        mWidth = hActive;
        mHeight = mode == Mode::Progressive240 ? 240 : 480;
        mInterlaced = mode == Mode::Interlaced480;
        mFrame.assign(static_cast<std::size_t>(mWidth) * mHeight * 3, 0);
    }

    struct GroovyOutput::Pad
    {
        SDL_Joystick* mJoystick = nullptr;
        int mInstance = -1;
    };

    GroovyOutput::~GroovyOutput()
    {
        mInputsReady = false;
        if (mConnected)
            gmw_close();
        // The virtual controllers go away when SDL shuts down (which may already have happened here).
        for (Pad*& pad : mPads)
        {
            delete pad;
            pad = nullptr;
        }
        // The GL objects go away with the window's GL context.
    }

    void GroovyOutput::disconnect()
    {
        const std::lock_guard<std::mutex> lock(mNetMutex);
        mInputsReady = false;
        gmw_reset();
        mConnected = false;
    }

    GroovyOutput::Mode GroovyOutput::parseMode(const std::string& name)
    {
        if (name == "240p")
            return Mode::Progressive240;
        if (name == "480p")
            return Mode::Progressive480;
        return Mode::Interlaced480;
    }

    void GroovyOutput::swapBuffersImplementation(osg::GraphicsContext* gc)
    {
        const auto now = std::chrono::steady_clock::now();
        ++mStats.mSwaps;
        if (mLastSwapTime != std::chrono::steady_clock::time_point{})
            mStats.mMaxSwapGapMs = std::max(mStats.mMaxSwapGapMs,
                std::chrono::duration<double, std::milli>(now - mLastSwapTime).count());
        mLastSwapTime = now;
        if (now - mLastStatsTime >= std::chrono::seconds(5))
            logStats(now);

        // The window (and the first frames) come before the sound engine starts. If we connected without sound
        // and the sound is ready now, connect again with sound.
        if (mConnected && mSendAudio && !mInitAudio && MWSound::GroovyAudio::active())
        {
            disconnect();
            mQuiet = true;
            mLastAttempt = now - retryInterval;
        }

        if (!mConnected && now - mLastAttempt >= retryInterval)
            connect();

        // Grab the frame while it's still in the back buffer, show it on the PC, then send it.
        const bool captured = mConnected && capture(*gc);
        if (captured)
            ++mStats.mCaptures;

        gc->swapBuffersImplementation();

        // The sound for this frame (also played on the PC, if enabled). Rendered even while the MiSTer isn't
        // connected, so the game is never silent.
        const std::size_t audioFrames = MWSound::GroovyAudio::render(mAudio);
        mStats.mAudioFrames += audioFrames;
        mStats.mMaxAudioFrames = std::max(mStats.mMaxAudioFrames, audioFrames);

        if (captured)
        {
            if (!mModeSet)
                switchres();
            const auto sendStart = std::chrono::steady_clock::now();
            send();
            mStats.mMaxSendMs = std::max(mStats.mMaxSendMs,
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - sendStart).count());
            sendAudio(audioFrames);

            // Frame delay: the CRT frame has just started. Waiting here pushes reading the controller and drawing
            // the next frame later into it, so less time passes between a button press and the picture showing
            // it. The Groovy library shortens its own wait to match, so the picture still goes out on time.
            if (mFrameDelay.count() > 0 && mConnected)
            {
                const auto until = std::chrono::steady_clock::now() + mFrameDelay;
                // Sleep most of it, then wait out the last bit precisely (Windows sleeps are coarse).
                if (mFrameDelay > std::chrono::milliseconds(2))
                    std::this_thread::sleep_for(mFrameDelay - std::chrono::milliseconds(2));
                while (std::chrono::steady_clock::now() < until)
                    std::this_thread::yield();
            }
        }
    }

    void GroovyOutput::logStats(std::chrono::steady_clock::time_point now)
    {
        const double seconds = mLastStatsTime == std::chrono::steady_clock::time_point{}
            ? 0.0
            : std::chrono::duration<double>(now - mLastStatsTime).count();
        mLastStatsTime = now;
        if (seconds <= 0.0)
            return;

        gmw_fpgaStatus status{};
        if (mConnected)
            gmw_getStatus(&status);

        Log(Debug::Info) << std::dec << "Groovy MiSTer stats: " << seconds << " s, connected " << mConnected
                         << ", swaps " << mStats.mSwaps << " (longest gap " << mStats.mMaxSwapGapMs << " ms)"
                         << ", captured " << mStats.mCaptures << ", sent " << mStats.mSends << ", answered "
                         << mStats.mEchoes << ", longest send " << mStats.mMaxSendMs << " ms (wait for CRT "
                         << mStats.mMaxWaitMs << " ms), window " << mStats.mSourceWidth << "x"
                         << mStats.mSourceHeight << ", our frame " << mFrameNumber << ", MiSTer frame "
                         << status.frame << ", answered frame " << status.frameEcho << ", vcount " << status.vCount
                         << ", bits ready " << int(status.vramReady) << " endframe " << int(status.vramEndFrame)
                         << " synced " << int(status.vramSynced) << " queue " << int(status.vramQueue)
                         << " frameskip " << int(status.vgaFrameskip) << " audio " << int(status.audio)
                         << ", sound " << mStats.mAudioFrames << " samples (max " << mStats.mMaxAudioFrames
                         << " per frame)";
        mStats = Stats{};
    }

    void GroovyOutput::message(const std::string& text)
    {
        Log(Debug::Info) << "Groovy MiSTer: " << text;
        if (MWBase::WindowManager* windowManager = MWBase::Environment::get().getWindowManager())
            windowManager->scheduleMessageBox("Groovy MiSTer: " + text);
    }

    void GroovyOutput::connect()
    {
        mLastAttempt = std::chrono::steady_clock::now();

        if (mHost.empty())
        {
            if (!mNoAnswerShown)
                message("set your MiSTer's IP address ('mister ip' in the [Groovy] section of settings.cfg)");
            mNoAnswerShown = true;
            return;
        }

        const std::unique_lock<std::mutex> lock(mNetMutex);

        // LZ4-compressed frames, 48 kHz stereo sound (or none), RGB888, standard 1500-byte packets.
        const bool audio = mSendAudio && MWSound::GroovyAudio::active();
        // Controllers plugged into the MiSTer (sent when Joysticks is on in the Groovy core's menu). The MiSTer
        // only learns where to send them from this hello while it handles the init below, so it must come first.
        gmw_bindInputs(mHost.c_str());

        if (gmw_init(mHost.c_str(), 1, audio ? MWSound::GroovyAudio::sSampleRate : 0, audio ? 2 : 0, 0, 1500) < 0)
        {
            // Ask the MiSTer to start the Groovy core (if the groovy_wake helper is installed there). The helper
            // ignores repeats while the core is loading.
            const bool startCore = Settings::groovy().mStartCore.get();
            if (startCore)
                gmw_wakeCore(mHost.c_str());

            // Shown once; it keeps retrying quietly, so it's fine to start the Groovy core later.
            if (!mNoAnswerShown)
                message(startCore ? "no answer from " + mHost + ", starting the Groovy core on the MiSTer..."
                                  : "no answer from " + mHost + ". Is the Groovy core running on the MiSTer? Retrying...");
            mNoAnswerShown = true;
            return;
        }

        if (!mQuiet)
            message(std::string(mLost ? "reconnected to " : "connected to ") + mHost);
        mQuiet = false;
        mInitAudio = audio;
        mConnected = true;
        mLost = false;
        mNoAnswerShown = false;
        mModeSet = false;
        mAudioChecked = false;
        mAudioOffAnswers = 0;
        mFrameNumber = 0;
        mLastEcho = 0;
        mLastMisterFrame = 0;
        mLastEchoTime = mLastSendTime = mLastAttempt;

        mInputsReady = true;
    }

    void GroovyOutput::switchres()
    {
        std::uint16_t vActive = 480;
        std::uint16_t vBegin = 487;
        std::uint16_t vEnd = 493;
        std::uint16_t vTotal = 525;
        std::uint8_t interlace = 1;
        double pixelClock = 59.94 * hTotal * (vTotal / 2.0) / 1e6; // 13.5 MHz, 59.94 Hz

        if (mMode == Mode::Progressive240)
        {
            vActive = 240;
            vBegin = 244;
            vEnd = 247;
            vTotal = 262;
            interlace = 0;
            pixelClock = 60.0 * hTotal * vTotal / 1e6;
        }
        else if (mMode == Mode::Progressive480)
        {
            // Standard 480p: 27 MHz, 31.47 kHz, 59.94 Hz.
            vBegin = 489;
            vEnd = 495;
            interlace = 0;
            pixelClock = 27.0;
        }

        gmw_switchres(pixelClock, hActive, hBegin, hEnd, hTotal, vActive, vBegin, vEnd, vTotal, interlace);
        mModeSet = true;
        Log(Debug::Info) << std::dec << "Groovy MiSTer: video mode " << mWidth << "x" << mHeight << (mInterlaced ? "i" : "p");
    }

    bool GroovyOutput::ensureTarget(osg::GraphicsContext& gc, Target& target, int width, int height)
    {
        if (target.mFbo != 0 && target.mWidth == width && target.mHeight == height)
            return true;

        osg::GLExtensions* ext = gc.getState()->get<osg::GLExtensions>();
        if (target.mFbo == 0)
        {
            ext->glGenFramebuffers(1, &target.mFbo);
            ext->glGenRenderbuffers(1, &target.mRbo);
        }

        ext->glBindRenderbuffer(GL_RENDERBUFFER_EXT, target.mRbo);
        ext->glRenderbufferStorage(GL_RENDERBUFFER_EXT, GL_RGBA8, width, height);
        ext->glBindRenderbuffer(GL_RENDERBUFFER_EXT, 0);

        ext->glBindFramebuffer(GL_FRAMEBUFFER_EXT, target.mFbo);
        ext->glFramebufferRenderbuffer(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, target.mRbo);
        const GLenum status = ext->glCheckFramebufferStatus(GL_FRAMEBUFFER_EXT);
        ext->glBindFramebuffer(GL_FRAMEBUFFER_EXT, gc.getDefaultFboId());

        if (status != GL_FRAMEBUFFER_COMPLETE_EXT)
        {
            Log(Debug::Error) << "Groovy MiSTer: framebuffer incomplete (" << status << ")";
            return false;
        }
        target.mWidth = width;
        target.mHeight = height;
        return true;
    }

    bool GroovyOutput::capture(osg::GraphicsContext& gc)
    {
        if (mGlFailed)
            return false;

        osg::State* state = gc.getState();
        const osg::GraphicsContext::Traits* traits = gc.getTraits();
        if (state == nullptr || traits == nullptr || traits->width <= 0 || traits->height <= 0)
        {
            if (!mCaptureSkipLogged)
                Log(Debug::Warning) << "Groovy MiSTer: frame skipped, the window has no size or state";
            mCaptureSkipLogged = true;
            return false;
        }

        osg::GLExtensions* ext = state->get<osg::GLExtensions>();
        if (ext == nullptr || !ext->isFrameBufferObjectSupported || ext->glBlitFramebuffer == nullptr)
        {
            mGlFailed = true;
            message("your graphics driver can't copy frames (framebuffer blit is missing)");
            return false;
        }

        const int srcWidth = traits->width;
        const int srcHeight = traits->height;
        mStats.mSourceWidth = srcWidth;
        mStats.mSourceHeight = srcHeight;

        // Fit the window into a 4:3 CRT picture, with black bars if the window has a different shape.
        const double crtAspect = 4.0 / 3.0;
        const double srcAspect = static_cast<double>(srcWidth) / srcHeight;
        int destWidth = mWidth;
        int destHeight = mHeight;
        if (srcAspect > crtAspect)
            destHeight = static_cast<int>(std::lround(mHeight * crtAspect / srcAspect));
        else
            destWidth = static_cast<int>(std::lround(mWidth * srcAspect / crtAspect));
        // Picture size setting: shrink the picture so a TV's overscan doesn't cut off the edges.
        destWidth = static_cast<int>(std::lround(destWidth * mPictureScale));
        destHeight = static_cast<int>(std::lround(destHeight * mPictureScale));
        destWidth = std::clamp(destWidth & ~1, 2, mWidth);
        destHeight = std::clamp(destHeight & ~1, 2, mHeight);
        // Centered, then moved by the shift setting, but always kept inside the picture.
        const int destX = std::clamp((mWidth - destWidth) / 2 + mShiftX, 0, mWidth - destWidth);
        const int destY = std::clamp((mHeight - destHeight) / 2 + mShiftY, 0, mHeight - destHeight);

        // New size or position: clear the old picture, so the borders are black.
        if (destWidth != mFinal.mWidth || destHeight != mFinal.mHeight || destX != mDestX || destY != mDestY)
            std::fill(mFrame.begin(), mFrame.end(), static_cast<std::uint8_t>(0));
        mDestX = destX;
        mDestY = destY;

        // Blits respect the scissor test, which the GUI may have left on.
        const GLboolean scissor = glIsEnabled(GL_SCISSOR_TEST);
        if (scissor)
            glDisable(GL_SCISSOR_TEST);

        const GLuint defaultFbo = gc.getDefaultFboId();
        GLuint readFbo = defaultFbo;
        int width = srcWidth;
        int height = srcHeight;
        bool ok = true;

        // Multisampled windows can't be scaled directly: resolve to a normal image first.
        if (traits->samples > 1)
        {
            ok = ensureTarget(gc, mResolve, width, height);
            if (ok)
            {
                ext->glBindFramebuffer(GL_READ_FRAMEBUFFER_EXT, readFbo);
                ext->glBindFramebuffer(GL_DRAW_FRAMEBUFFER_EXT, mResolve.mFbo);
                ext->glBlitFramebuffer(
                    0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
                readFbo = mResolve.mFbo;
            }
        }

        // Halve the size while it's more than twice the final size, so every window pixel counts towards
        // the result (a single linear scale only samples 2x2 pixels, which shimmers).
        for (std::size_t step = 0; ok && step < 4 && (width > destWidth * 2 || height > destHeight * 2); ++step)
        {
            const int nextWidth = std::max(destWidth, width / 2);
            const int nextHeight = std::max(destHeight, height / 2);
            if (mSteps.size() <= step)
                mSteps.emplace_back();
            ok = ensureTarget(gc, mSteps[step], nextWidth, nextHeight);
            if (!ok)
                break;
            ext->glBindFramebuffer(GL_READ_FRAMEBUFFER_EXT, readFbo);
            ext->glBindFramebuffer(GL_DRAW_FRAMEBUFFER_EXT, mSteps[step].mFbo);
            ext->glBlitFramebuffer(
                0, 0, width, height, 0, 0, nextWidth, nextHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);
            readFbo = mSteps[step].mFbo;
            width = nextWidth;
            height = nextHeight;
        }

        if (ok)
            ok = ensureTarget(gc, mFinal, destWidth, destHeight);

        if (ok)
        {
            // Final size, flipped vertically so it reads back top row first.
            ext->glBindFramebuffer(GL_READ_FRAMEBUFFER_EXT, readFbo);
            ext->glBindFramebuffer(GL_DRAW_FRAMEBUFFER_EXT, mFinal.mFbo);
            ext->glBlitFramebuffer(
                0, 0, width, height, 0, destHeight, destWidth, 0, GL_COLOR_BUFFER_BIT, GL_LINEAR);

            // Read it straight into place inside the CRT picture, in the MiSTer's BGR order.
            ext->glBindFramebuffer(GL_READ_FRAMEBUFFER_EXT, mFinal.mFbo);
            GLint packBuffer = 0;
            GLint packAlignment = 4;
            GLint packRowLength = 0;
            glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &packBuffer);
            glGetIntegerv(GL_PACK_ALIGNMENT, &packAlignment);
            glGetIntegerv(GL_PACK_ROW_LENGTH, &packRowLength);
            if (packBuffer != 0)
                ext->glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, 0);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glPixelStorei(GL_PACK_ROW_LENGTH, mWidth);

            std::uint8_t* dest = mFrame.data() + (static_cast<std::size_t>(destY) * mWidth + destX) * 3;
            glReadPixels(0, 0, destWidth, destHeight, GL_BGR, GL_UNSIGNED_BYTE, dest);

            glPixelStorei(GL_PACK_ROW_LENGTH, packRowLength);
            glPixelStorei(GL_PACK_ALIGNMENT, packAlignment);
            if (packBuffer != 0)
                ext->glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, static_cast<GLuint>(packBuffer));
        }

        ext->glBindFramebuffer(GL_FRAMEBUFFER_EXT, defaultFbo);
        if (scissor)
            glEnable(GL_SCISSOR_TEST);

        if (!ok)
        {
            mGlFailed = true;
            message("couldn't set up frame capture on this graphics card; see openmw.log");
        }
        return ok;
    }

    void GroovyOutput::send()
    {
        gmw_fpgaStatus status;
        gmw_getStatus(&status);

        // Which field (even or odd lines) the CRT shows next, for interlaced modes (Calamity's formula, as in
        // GroovyMAME and Groovy Dolphin).
        std::uint8_t field = 0;
        if (mInterlaced)
            field = static_cast<std::uint8_t>((status.vgaF1 ? 0u : 1u) ^ ((mFrameNumber - status.frame) % 2u));

        ++mFrameNumber;
        if (status.frame >= mFrameNumber)
            mFrameNumber = status.frame + 1;

        char* buffer = gmw_get_pBufferBlit(field);
        if (buffer == nullptr)
        {
            Log(Debug::Warning) << "Groovy MiSTer: no send buffer";
            return;
        }

        const std::size_t rowBytes = static_cast<std::size_t>(mWidth) * 3;
        if (mInterlaced && mFlickerFilter)
        {
            // One field: every other line, each blended 1:2:1 with the lines above and below it (the lines of
            // the other field). A detail one line tall then shows in both fields instead of blinking on and
            // off 30 times a second. This is the flicker filter interlaced consoles used.
            std::size_t out = 0;
            for (int y = field; y < mHeight; y += 2, ++out)
            {
                const std::uint8_t* above = mFrame.data() + static_cast<std::size_t>(std::max(y - 1, 0)) * rowBytes;
                const std::uint8_t* line = mFrame.data() + static_cast<std::size_t>(y) * rowBytes;
                const std::uint8_t* below
                    = mFrame.data() + static_cast<std::size_t>(std::min(y + 1, mHeight - 1)) * rowBytes;
                auto* dest = reinterpret_cast<std::uint8_t*>(buffer + out * rowBytes);
                if (mLightFlickerFilter)
                {
                    // Light: 1:4:1, the line itself keeps two thirds of its weight.
                    for (std::size_t i = 0; i < rowBytes; ++i)
                        dest[i] = static_cast<std::uint8_t>((above[i] + 4 * line[i] + below[i] + 3) / 6);
                }
                else
                {
                    for (std::size_t i = 0; i < rowBytes; ++i)
                        dest[i] = static_cast<std::uint8_t>((above[i] + 2 * line[i] + below[i] + 2) / 4);
                }
            }
        }
        else if (mInterlaced)
        {
            // One field: every other line, starting with the field's first line.
            std::size_t out = 0;
            for (int y = field; y < mHeight; y += 2, ++out)
                std::memcpy(buffer + out * rowBytes, mFrame.data() + static_cast<std::size_t>(y) * rowBytes, rowBytes);
        }
        else
        {
            std::memcpy(buffer, mFrame.data(), mFrame.size());
        }

        gmw_blit(mFrameNumber, field, static_cast<std::uint16_t>(mSyncLine), 15000, 0);
        ++mStats.mSends;
        // Wait for the CRT's next frame: this paces OpenMW to the CRT's refresh rate.
        const auto waitStart = std::chrono::steady_clock::now();
        gmw_waitSync();
        mStats.mMaxWaitMs = std::max(mStats.mMaxWaitMs,
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - waitStart).count());
        checkConnection();
    }

    void GroovyOutput::sendAudio(std::size_t frames)
    {
        if (!mConnected || !mSendAudio || frames == 0)
            return;

        // Right after the picture, like Groovy Dolphin. At most 100 ms (19200 bytes) per frame.
        const std::size_t bytes = frames * 2 * sizeof(std::int16_t);
        char* buffer = gmw_get_pBufferAudio();
        if (buffer == nullptr)
            return;
        std::memcpy(buffer, mAudio.data(), bytes);
        gmw_audio(static_cast<std::uint16_t>(bytes));
    }

    void GroovyOutput::checkConnection()
    {
        const auto now = std::chrono::steady_clock::now();
        gmw_fpgaStatus status;
        gmw_getStatus(&status);

        // A long gap between frames (loading, window minimized) isn't the MiSTer going silent.
        if (now - mLastSendTime > std::chrono::seconds(1))
            mLastEchoTime = now;
        mLastSendTime = now;

        // The MiSTer's frame counter went backwards: the Groovy core was restarted. It needs a fresh connection
        // (video mode and all) before it shows anything again.
        if (status.frame + 60 < mLastMisterFrame)
        {
            message("the Groovy core restarted, reconnecting...");
            disconnect();
            mLost = true;
            mNoAnswerShown = true;
            mLastAttempt = now - retryInterval; // retry right away
            return;
        }
        mLastMisterFrame = status.frame;

        if (status.frameEcho != mLastEcho)
        {
            mLastEcho = status.frameEcho;
            mLastEchoTime = now;
            ++mStats.mEchoes;

            // The MiSTer's answers say whether the Groovy core's own Audio option is on (it's off by default,
            // which is easy to miss). Give it a few seconds of answers before warning: the first ones can come
            // before the core has set up its sound.
            if (status.audio)
                mAudioChecked = true;
            else if (!mAudioChecked && mSendAudio && mInitAudio && ++mAudioOffAnswers > 300)
            {
                mAudioChecked = true;
                message("sound is off in the Groovy core. On the MiSTer, open the core's menu and turn Audio on.");
            }
        }
        else if (now - mLastEchoTime > lostTimeout)
        {
            message("lost connection to the MiSTer, reconnecting...");
            disconnect();
            mLost = true;
            mNoAnswerShown = true; // already told the player
            mLastAttempt = now - retryInterval; // retry right away
        }
    }

    namespace
    {
        // The MiSTer sends each stick axis as a signed 8-bit value.
        Sint16 toAxis(char value)
        {
            const int raw = static_cast<signed char>(value);
            return static_cast<Sint16>(std::clamp(raw * 258, -32768, 32767));
        }
    }

    void GroovyOutput::updateInputs()
    {
#if SDL_VERSION_ATLEAST(2, 24, 0)
        // Never make the game wait: if the draw thread is (re)connecting right now, try again next frame.
        std::unique_lock<std::mutex> lock(mNetMutex, std::try_to_lock);
        if (!lock.owns_lock())
            return;

        gmw_fpgaJoyInputs inputs{};
        if (mInputsReady)
        {
            gmw_pollInputs();
            gmw_getJoyInputs(&inputs);
        }
        lock.unlock();

        // Nothing has ever arrived: Joysticks is probably off in the Groovy core. Don't add empty controllers.
        if (inputs.joyFrame == 0 && mPads[0] == nullptr)
            return;

        const std::array<std::uint16_t, 2> maps{ inputs.joy1, inputs.joy2 };
        const std::array<std::array<char, 4>, 2> sticks{ {
            { inputs.joy1LXAnalog, inputs.joy1LYAnalog, inputs.joy1RXAnalog, inputs.joy1RYAnalog },
            { inputs.joy2LXAnalog, inputs.joy2LYAnalog, inputs.joy2RXAnalog, inputs.joy2RYAnalog },
        } };

        for (std::size_t port = 0; port < mPads.size(); ++port)
        {
            const std::uint16_t map = maps[port];
            const auto& stick = sticks[port];
            const bool used = map != 0 || stick[0] != 0 || stick[1] != 0 || stick[2] != 0 || stick[3] != 0;

            // Create the controller the first time its port is used (port 1 as soon as inputs arrive).
            if (mPads[port] == nullptr)
            {
                if (port == 1 && !used)
                    continue;

                SDL_VirtualJoystickDesc desc;
                SDL_zero(desc);
                desc.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
                desc.type = SDL_JOYSTICK_TYPE_GAMECONTROLLER;
                desc.naxes = SDL_CONTROLLER_AXIS_MAX;
                desc.nbuttons = SDL_CONTROLLER_BUTTON_MAX;
                desc.name = port == 0 ? "MiSTer Controller 1" : "MiSTer Controller 2";

                auto* pad = new Pad;
                mPads[port] = pad;
                const int index = SDL_JoystickAttachVirtualEx(&desc);
                if (index < 0)
                {
                    Log(Debug::Error) << "Groovy MiSTer: couldn't add a virtual controller: " << SDL_GetError();
                    continue;
                }
                pad->mJoystick = SDL_JoystickOpen(index);
                pad->mInstance = pad->mJoystick != nullptr ? SDL_JoystickInstanceID(pad->mJoystick) : -1;
                Log(Debug::Info) << "Groovy MiSTer: added " << desc.name;
            }

            SDL_Joystick* joystick = mPads[port]->mJoystick;
            if (joystick == nullptr)
                continue;

            // The MiSTer's buttons in its standard order (Button 1 = right face button, 2 = bottom, 3 = top,
            // 4 = left, 5/6 = shoulders, 7 = Select, 8 = Start, 9/10 = extra), on an Xbox-style layout.
            const auto set = [&](SDL_GameControllerButton button, std::uint16_t bit) {
                SDL_JoystickSetVirtualButton(joystick, button, (map & bit) != 0 ? 1 : 0);
            };
            set(SDL_CONTROLLER_BUTTON_DPAD_UP, GMW_JOY_UP);
            set(SDL_CONTROLLER_BUTTON_DPAD_DOWN, GMW_JOY_DOWN);
            set(SDL_CONTROLLER_BUTTON_DPAD_LEFT, GMW_JOY_LEFT);
            set(SDL_CONTROLLER_BUTTON_DPAD_RIGHT, GMW_JOY_RIGHT);
            set(SDL_CONTROLLER_BUTTON_B, GMW_JOY_B1);
            set(SDL_CONTROLLER_BUTTON_A, GMW_JOY_B2);
            set(SDL_CONTROLLER_BUTTON_Y, GMW_JOY_B3);
            set(SDL_CONTROLLER_BUTTON_X, GMW_JOY_B4);
            set(SDL_CONTROLLER_BUTTON_LEFTSHOULDER, GMW_JOY_B5);
            set(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, GMW_JOY_B6);
            set(SDL_CONTROLLER_BUTTON_BACK, GMW_JOY_B7);
            set(SDL_CONTROLLER_BUTTON_START, GMW_JOY_B8);

            // Buttons 9 and 10 become the triggers (fully pressed or released).
            SDL_JoystickSetVirtualAxis(
                joystick, SDL_CONTROLLER_AXIS_TRIGGERLEFT, (map & GMW_JOY_B9) != 0 ? 32767 : 0);
            SDL_JoystickSetVirtualAxis(
                joystick, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, (map & GMW_JOY_B10) != 0 ? 32767 : 0);

            SDL_JoystickSetVirtualAxis(joystick, SDL_CONTROLLER_AXIS_LEFTX, toAxis(stick[0]));
            SDL_JoystickSetVirtualAxis(joystick, SDL_CONTROLLER_AXIS_LEFTY, toAxis(stick[1]));
            SDL_JoystickSetVirtualAxis(joystick, SDL_CONTROLLER_AXIS_RIGHTX, toAxis(stick[2]));
            SDL_JoystickSetVirtualAxis(joystick, SDL_CONTROLLER_AXIS_RIGHTY, toAxis(stick[3]));
        }
#endif
    }
}
