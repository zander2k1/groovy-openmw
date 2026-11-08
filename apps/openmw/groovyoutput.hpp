#ifndef OPENMW_APPS_OPENMW_GROOVYOUTPUT_H
#define OPENMW_APPS_OPENMW_GROOVYOUTPUT_H

#include <osg/GL>
#include <osg/GraphicsContext>

#include <chrono>
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

        GroovyOutput(const std::string& host, Mode mode, int syncLine);
        ~GroovyOutput() override;

        void swapBuffersImplementation(osg::GraphicsContext* gc) override;

        static Mode parseMode(const std::string& name);

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
        void checkConnection();
        void message(const std::string& text);

        bool ensureTarget(osg::GraphicsContext& gc, Target& target, int width, int height);

        const std::string mHost;
        const Mode mMode;
        const int mSyncLine;

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
        bool mModeSet = false;
        std::uint32_t mFrameNumber = 0;
        std::uint32_t mLastEcho = 0;
        std::chrono::steady_clock::time_point mLastAttempt{};
        std::chrono::steady_clock::time_point mLastEchoTime{};
        std::chrono::steady_clock::time_point mLastSendTime{};
    };
}

#endif
