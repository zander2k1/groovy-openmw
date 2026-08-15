#ifndef OPENMW_COMPONENTS_MISC_GUISCALING_H
#define OPENMW_COMPONENTS_MISC_GUISCALING_H

#include <algorithm>
#include <optional>

namespace Misc
{
    struct RecommendedGuiScaling
    {
        float mInterfaceScaling;
        float mDialogueScaling;
        float mSettingsScaling;
    };

    inline std::optional<RecommendedGuiScaling> calculateRecommendedGuiScaling(int width, int height)
    {
        if (width <= 0 || height <= 0)
            return std::nullopt;

        // Scale from the largest 640x480 UI that fits within the selected resolution.
        const float interfaceScaling = std::min(width / 640.f, height / 480.f);
        return RecommendedGuiScaling{ interfaceScaling, interfaceScaling * 1.15f, interfaceScaling * 0.85f };
    }
}

#endif
