#include "fallback.hpp"

#include <string>
#include <vector>

#include <components/debug/debuglog.hpp>
#include <components/misc/strings/algorithm.hpp>
#include <components/misc/strings/conversion.hpp>

#include <array>

#include "validate.hpp"

namespace Fallback
{
    std::map<std::string, int, std::less<>> Map::mIntFallbackMap;
    std::map<std::string, float, std::less<>> Map::mFloatFallbackMap;
    std::map<std::string, std::string, std::less<>> Map::mNonNumericFallbackMap;

    void applyXboxFontColours(std::map<std::string, std::string>& fallback)
    {
        static constexpr std::array<std::pair<std::string_view, std::string_view>, 46> sPalette{ { { "normal",
                                                                                                       "235,165,45" },
            { "normal_over", "249,208,134" }, { "normal_pressed", "253,239,215" }, { "active", "202,168,117" },
            { "active_over", "202,168,117" }, { "active_pressed", "202,168,117" }, { "xbox_active", "96,112,202" },
            { "disabled", "179,168,135" }, { "disabled_over", "249,208,134" }, { "disabled_pressed", "253,239,215" },
            { "link", "90,96,139" }, { "link_over", "114,120,163" }, { "link_pressed", "139,145,180" },
            { "journal_link", "24,43,146" }, { "journal_link_over", "44,70,222" },
            { "journal_link_pressed", "117,135,234" }, { "journal_topic", "0,0,0" },
            { "journal_topic_over", "24,43,146" }, { "journal_topic_pressed", "44,70,222" }, { "answer", "150,50,30" },
            { "answer_over", "249,208,134" }, { "answer_pressed", "253,239,215" }, { "header", "202,168,117" },
            { "notify", "249,208,134" }, { "big_normal", "169,128,65" }, { "big_normal_over", "202,168,117" },
            { "big_normal_pressed", "225,206,179" }, { "big_link", "90,96,139" }, { "big_link_over", "111,117,162" },
            { "big_link_pressed", "136,141,179" }, { "big_answer", "150,50,30" }, { "big_answer_over", "202,168,117" },
            { "big_answer_pressed", "225,206,179" }, { "big_header", "202,168,117" }, { "big_notify", "202,168,117" },
            { "background", "0,0,0" }, { "focus", "80,80,80" }, { "health", "150,50,30" }, { "magic", "53,69,159" },
            { "fatigue", "68,166,83" }, { "misc", "0,205,205" }, { "weapon_fill", "150,50,30" },
            { "magic_fill", "150,50,30" }, { "positive", "249,208,134" }, { "negative", "150,50,30" },
            { "count", "249,208,134" } } };

        for (const auto& [name, value] : sPalette)
            fallback.insert_or_assign("FontColor_color_" + std::string(name), std::string(value));
    }

    void Map::init(const std::map<std::string, std::string>& fallback)
    {
        for (const auto& [key, value] : fallback)
        {
            if (isAllowedIntFallbackKey(key))
                mIntFallbackMap.emplace(key, Misc::StringUtils::toNumeric<int>(value, 0));
            else if (isAllowedFloatFallbackKey(key))
                mFloatFallbackMap.emplace(key, Misc::StringUtils::toNumeric<float>(value, 0.0f));
            else if (isAllowedNonNumericFallbackKey(key))
                mNonNumericFallbackMap.emplace(key, value);
            else if (!isAllowedUnusedFallbackKey(key))
                Log(Debug::Error) << "Ignoring unknown fallback: " << key;
        }
    }

    std::string_view Map::getString(std::string_view fall)
    {
        const auto it = mNonNumericFallbackMap.find(fall);
        if (it == mNonNumericFallbackMap.end())
        {
            if (!isAllowedNonNumericFallbackKey(fall))
                throw std::logic_error("Requested invalid string fallback: " + std::string(fall));
            return {};
        }
        return it->second;
    }

    float Map::getFloat(std::string_view fall)
    {
        const auto it = mFloatFallbackMap.find(fall);
        if (it == mFloatFallbackMap.end())
        {
            if (!isAllowedFloatFallbackKey(fall))
                throw std::logic_error("Requested invalid float fallback: " + std::string(fall));
            return {};
        }
        return it->second;
    }

    int Map::getInt(std::string_view fall)
    {
        const auto it = mIntFallbackMap.find(fall);
        if (it == mIntFallbackMap.end())
        {
            if (!isAllowedIntFallbackKey(fall))
                throw std::logic_error("Requested invalid int fallback: " + std::string(fall));
            return {};
        }
        return it->second;
    }

    bool Map::getBool(std::string_view fall)
    {
        return getInt(fall) != 0;
    }

    osg::Vec4f Map::getColour(std::string_view fall)
    {
        const std::string_view sum = getString(fall);

        if (!sum.empty())
        {
            std::vector<std::string> ret;
            Misc::StringUtils::split(sum, ret, ",");

            if (ret.size() == 3)
            {
                const auto r = Misc::StringUtils::toNumeric<float>(ret[0]);
                const auto g = Misc::StringUtils::toNumeric<float>(ret[1]);
                const auto b = Misc::StringUtils::toNumeric<float>(ret[2]);

                if (r.has_value() && g.has_value() && b.has_value())
                {
                    return osg::Vec4f(*r / 255.0f, *g / 255.0f, *b / 255.0f, 1.0f);
                }
            }

            Log(Debug::Error) << "Error: '" << fall << "' setting value (" << sum
                              << ") is not a valid color, using middle gray as a fallback";
        }

        return osg::Vec4f(0.5f, 0.5f, 0.5f, 1.f);
    }
}
