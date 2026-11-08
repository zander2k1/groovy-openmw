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
    };
}

#endif
