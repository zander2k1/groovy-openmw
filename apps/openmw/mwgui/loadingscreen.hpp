#ifndef MWGUI_LOADINGSCREEN_H
#define MWGUI_LOADINGSCREEN_H

#include <memory>

#include <osg/Timer>
#include <osg/ref_ptr>

#include "windowbase.hpp"

#include <components/loadinglistener/loadinglistener.hpp>

namespace osgViewer
{
    class Viewer;
}

namespace osg
{
    class Texture2D;
}

namespace Resource
{
    class ResourceSystem;
}

namespace MWGui
{
    class BackgroundImage;
    class CopyFramebufferToTextureCallback;
    class MessageBoxManager;

    class LoadingScreen : public WindowBase, public Loading::Listener
    {
    public:
        LoadingScreen(Resource::ResourceSystem* resourceSystem, osgViewer::Viewer* viewer);
        virtual ~LoadingScreen();

        /// Overridden from Loading::Listener, see the Loading::Listener documentation for usage details
        void setLabel(const std::string& label, bool important) override;
        void loadingOn() override;
        void loadingOff() override;
        void setProgressRange(size_t range) override;
        void setProgress(size_t value) override;
        void increaseProgress(size_t increase = 1) override;

        void setVisible(bool visible) override;
        void onResChange(int width, int height) override;
        void setMessageBoxManager(MessageBoxManager* messageBoxManager);

        double getTargetFrameRate() const;

    private:
        enum class SplashPolicy : unsigned char
        {
            Default,
            Never,
            Immediate,
            AfterDelay,
        };

        void findSplashScreens();
        bool needToDrawLoadingScreen();

        void setupCopyFramebufferToTextureCallback();

        Resource::ResourceSystem* mResourceSystem;
        MessageBoxManager* mMessageBoxManager = nullptr;
        osg::ref_ptr<osgViewer::Viewer> mViewer;

        double mTargetFrameRate;

        double mLastWallpaperChangeTime;
        double mLastRenderTime;
        osg::Timer mTimer;
        double mLoadingOnTime;

        bool mImportantLabel;

        int mNestedLoadingCount;

        size_t mProgress;
        size_t mXboxProgressPhase = 0;
        size_t mXboxProgressRange = 1;
        size_t mXboxProgress = 0;
        size_t mFakeProgress = 0;

        bool mUseXboxSplashScreens;
        bool mUseLongerLoadingScreens;
        bool mShowWallpaper;
        bool mHasCompletedStartupLoad = false;
        SplashPolicy mSplashPolicy = SplashPolicy::Default;
        bool mLongerLoadingCandidate = false;
        bool mFakeLoadingActive = false;
        double mLastSplashScreenEndTime = -100.0;
        double mFakeLoadingEndTime = 0.0;
        double mNextFakeProgressTime = 0.0;
        double mOldIcoMin = 0.0;
        unsigned int mOldIcoMax = 0;

        MyGUI::Widget* mLoadingBox;

        MyGUI::TextBox* mLoadingText;
        MyGUI::ScrollBar* mProgressBar;
        MyGUI::ScrollBar* mDefaultProgressBar;
        MyGUI::ScrollBar* mXboxProgressBar;
        BackgroundImage* mSplashImage;
        BackgroundImage* mSceneImage;

        std::vector<std::string> mSplashScreens;

        osg::ref_ptr<osg::Texture2D> mTexture;
        osg::ref_ptr<CopyFramebufferToTextureCallback> mCopyFramebufferToTextureCallback;
        std::unique_ptr<MyGUI::ITexture> mGuiTexture;

        void changeWallpaper();
        void updateXboxProgressBarLayout();
        void startLongerLoadingScreen();
        void updateFakeProgress();

        void draw();
    };

}

#endif
