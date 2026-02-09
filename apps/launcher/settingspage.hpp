#ifndef SETTINGSPAGE_H
#define SETTINGSPAGE_H

#include <QCompleter>
#include <QStringListModel>

#include "ui_settingspage.h"

namespace Config
{
    class GameSettings;
}

namespace Launcher
{
    class SettingsPage : public QWidget, private Ui::SettingsPage
    {
        Q_OBJECT

    public:
        explicit SettingsPage(Config::GameSettings& gameSettings, QWidget* parent = nullptr);

        bool loadSettings();
        void saveSettings();

    public slots:
        void slotLoadedCellsChanged(QStringList cellNames);
        void onResolutionChanged(int width, int height);

    private slots:
        void on_recommendedScalingDefaultsCheckBox_stateChanged(int state);
        void on_skipMenuCheckBox_stateChanged(int state);
        void on_runScriptAfterStartupBrowseButton_clicked();
        void onInterfaceScalingChanged(double value);
        void onDialogueScalingChanged(double value);
        void slotAnimSourcesToggled(bool checked);
        void slotPostProcessToggled(bool checked);
        void slotSkyBlendingToggled(bool checked);
        void slotShadowDistLimitToggled(bool checked);
        void slotDistantLandToggled(bool checked);
        void slotControllerMenusToggled(bool checked);

    private:
        bool applyRecommendedScalingDefaults(int width, int height);
        void setScalingValues(double interfaceScaling, double dialogueScaling);

        Config::GameSettings& mGameSettings;
        QCompleter mCellNameCompleter;
        QStringListModel mCellNameCompleterModel;
        double mLastInterfaceScaling = 1.0;
        int mLastResolutionWidth = 0;
        int mLastResolutionHeight = 0;
        int mLastRecommendedResolutionWidth = 0;
        int mLastRecommendedResolutionHeight = 0;
        bool mLoadingSettings = false;

        /**
         * Load the cells associated with the given content files for use in autocomplete
         * @param filePaths the file paths of the content files to be examined
         */
        void loadCellsForAutocomplete(QStringList filePaths);
    };
}
#endif
