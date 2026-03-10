#ifndef OPENMW_LUAUI_WIDGETLIST
#define OPENMW_LUAUI_WIDGETLIST

#include <string>
#include <string_view>
#include <unordered_map>

namespace LuaUi
{
    void registerAllWidgets();

    const std::unordered_map<std::string, std::string>& widgetTypeToName();

    void clearGameInterface();
    void clearMenuInterface();

    bool isAnyElementVisibleOnLayer(std::string_view layerName);
}

#endif // OPENMW_LUAUI_WIDGETLIST
