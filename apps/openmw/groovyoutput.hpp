#ifndef OPENMW_APPS_OPENMW_GROOVYOUTPUT_H
#define OPENMW_APPS_OPENMW_GROOVYOUTPUT_H

#include <osg/GL>
#include <osg/GraphicsContext>

#include <array>
#include <atomic>
#include <chrono>
#include <mutex>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace OMW
{
    /// Groovy MiSTer output: sends every finished frame to a MiSTer FPGA running the Groovy_MiSTer core
    /// (https://github.com/psakhis/Groovy_MiSTer), which shows it on a CRT.
    ///
    /// Installed as the window's swap callback, so it runs on the draw thread right before each buffer
    /// swap, when the back buffer holds the complete frame (world, post-processing and GUI). The frame is
    /// scaled down on the GPU to the CRT's resolution, read back, and sent over the network. Waiting for
    /// the MiSTer's vsync afterwards paces OpenMW to the CRT's refresh rate, like vsync on a monitor.
    class GroovyOutput : public osg::GraphicsContext::SwapCallback
    {
    public:
        enum class Mode
        {
            Interlaced480, // 480i, 15 kHz
            Progressive240, // 240p, 15 kHz
            Progressive480, // 480p, 31 kHz (VGA monitors only)
        };

        GroovyOutput(const std::string& host, Mode mode, int syncLine, bool sendAudio);
        ~GroovyOutput() override;

        void swapBuffersImplementation(osg::GraphicsContext* gc) override;

        static Mode parseMode(const std::string& name);

        /// Controllers plugged into the MiSTer: reads their state and passes it on to virtual SDL game
        /// controllers, so OpenMW sees them like any other controller. Call on the main thread, before input.
        void updateInputs();

    private:
        struct Target
        {
            GLuint mFbo = 0;
            GLuint mRbo = 0;
            int mWidth = 0;
            int mHeight = 0;
        };

        void connect();
        void switchres();
        bool capture(osg::GraphicsContext& gc);
        void send();
        void sendAudio(std::size_t frames);
        void checkConnection();
        void message(const std::string& text);
        void logStats(std::chrono::steady_clock::time_point now);

        bool ensureTarget(osg::GraphicsContext& gc, Target& target, int width, int height);
        void disconnect();

        // Guards connecting and disconnecting (draw thread) against reading the controllers (main thread).
        std::mutex mNetMutex;
        std::atomic<bool> mInputsReady{ false };

        // Virtual SDL controllers for the MiSTer's two controller ports (created when a port is first used).
        struct Pad;
        std::array<Pad*, 2> mPads{};

        const std::string mHost;
        const Mode mMode;
        const int mSyncLine;
        const bool mSendAudio;
        // 480i: blend each line a little with its neighbours, so thin horizontal lines don't flicker.
        const bool mFlickerFilter;
        const bool mLightFlickerFilter;
        // Frame delay: wait this long after each CRT frame before letting the next frame start (0 = off).
        const std::chrono::microseconds mFrameDelay;
        // Picture size on the CRT (fraction of the full picture) and shift in pixels.
        const double mPictureScale;
        const int mShiftX;
        const int mShiftY;
        int mDestX = -1;
        int mDestY = -1;

        // Size of the picture sent to the MiSTer.
        int mWidth = 640;
        int mHeight = 480;
        bool mInterlaced = true;

        // Frame in BGR order, top row first, mWidth x mHeight. The game is fitted inside at 4:3 with black
        // bars if the window has a different shape.
        std::vector<std::uint8_t> mFrame;

        // GPU scaling: resolve (for multisampled windows), halving steps for better quality, final size.
        Target mResolve;
        std::vector<Target> mSteps;
        Target mFinal;
        bool mGlFailed = false;

        bool mConnected = false;
        bool mLost = false;
        bool mNoAnswerShown = false;
        bool mAudioChecked = false;
        unsigned mAudioOffAnswers = 0;
        bool mInitAudio = false; // connected with sound
        bool mQuiet = false; // reconnecting only to add sound: no message

        // Sound for the current frame, from MWSound::GroovyAudio (interleaved stereo 16-bit).
        std::vector<std::int16_t> mAudio;
        bool mModeSet = false;
        std::uint32_t mFrameNumber = 0;
        std::uint32_t mLastEcho = 0;
        std::uint32_t mLastMisterFrame = 0;
        std::chrono::steady_clock::time_point mLastAttempt{};
        std::chrono::steady_clock::time_point mLastEchoTime{};
        std::chrono::steady_clock::time_point mLastSendTime{};

        // Numbers written to openmw.log every few seconds, to find out what goes wrong when the picture on the
        // CRT stops updating.
        struct Stats
        {
            unsigned mSwaps = 0;
            unsigned mCaptures = 0;
            unsigned mSends = 0;
            unsigned mEchoes = 0;
            double mMaxSwapGapMs = 0;
            double mMaxSendMs = 0;
            double mMaxWaitMs = 0;
            std::size_t mAudioFrames = 0;
            std::size_t mMaxAudioFrames = 0;
            int mSourceWidth = 0;
            int mSourceHeight = 0;
        };
        Stats mStats;
        bool mCaptureSkipLogged = false;
        std::chrono::steady_clock::time_point mLastStatsTime{};
        std::chrono::steady_clock::time_point mLastSwapTime{};
    };
}

#endif
