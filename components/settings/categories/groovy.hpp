#ifndef OPENMW_COMPONENTS_SETTINGS_CATEGORIES_GROOVY_H
#define OPENMW_COMPONENTS_SETTINGS_CATEGORIES_GROOVY_H

#include <components/settings/sanitizerimpl.hpp>
#include <components/settings/settingvalue.hpp>

#include <string>

namespace Settings
{
    // Groovy MiSTer: stream the game to a MiSTer FPGA running the Groovy_MiSTer core, which shows it
    // on a CRT. See [Groovy] in settings-default.cfg.
    struct GroovyCategory : WithIndex
    {
        using WithIndex::WithIndex;

        SettingValue<bool> mEnabled{ mIndex, "Groovy", "enabled" };
        SettingValue<std::string> mMisterIp{ mIndex, "Groovy", "mister ip" };
        SettingValue<std::string> mVideoMode{ mIndex, "Groovy", "video mode",
            makeEnumSanitizerString({ "480i", "240p", "480p" }) };
        SettingValue<int> mSyncLine{ mIndex, "Groovy", "sync line", makeClampSanitizerInt(0, 400) };
        SettingValue<bool> mSendAudio{ mIndex, "Groovy", "send audio" };
        SettingValue<bool> mPcAudio{ mIndex, "Groovy", "play audio on pc" };
        SettingValue<bool> mFlickerFilter{ mIndex, "Groovy", "flicker filter" };
        SettingValue<bool> mAutoCrtSetup{ mIndex, "Groovy", "automatic crt setup" };
        SettingValue<bool> mFrameDelay{ mIndex, "Groovy", "frame delay" };
        SettingValue<int> mFrameDelayAmount{ mIndex, "Groovy", "frame delay amount", makeClampSanitizerInt(1, 9) };
        SettingValue<int> mPictureSize{ mIndex, "Groovy", "picture size", makeClampSanitizerInt(50, 100) };
        SettingValue<int> mPictureShiftX{ mIndex, "Groovy", "picture shift x", makeClampSanitizerInt(-64, 64) };
        SettingValue<int> mPictureShiftY{ mIndex, "Groovy", "picture shift y", makeClampSanitizerInt(-64, 64) };
    };
}

#endif
