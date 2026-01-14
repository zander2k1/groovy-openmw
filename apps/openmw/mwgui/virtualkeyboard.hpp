#ifndef MWGUI_VIRTUALKEYBOARD_H
#define MWGUI_VIRTUALKEYBOARD_H

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <MyGUI_Button.h>
#include <MyGUI_EditBox.h>

#include "windowbase.hpp"

namespace MWGui
{
    class VirtualKeyboard : public WindowModal
    {
    public:
        VirtualKeyboard();

        void setVisible(bool visible) override;
        void onFrame(float dt) override;
        int getHeight() const;

        void setTargetEdit(MyGUI::EditBox* edit);
        void clearTargetEdit();
        bool onControllerButtonEvent(const SDL_ControllerButtonEvent& arg) override;

    private:
        enum class Action
        {
            Text,
            Backspace,
            Shift,
            Done
        };

        struct KeySpec
        {
            MyGUI::Button* mButton;
            std::string mText;
            std::string mShiftText;
            Action mAction;
        };

        MyGUI::EditBox* mInputBar = nullptr;
        MyGUI::EditBox* mTargetEdit = nullptr;
        std::vector<KeySpec> mKeys;
        std::unordered_map<MyGUI::Widget*, size_t> mKeyLookup;
        std::vector<std::vector<size_t>> mNavRows;
        std::vector<size_t> mShiftKeys;
        size_t mNavRow = 0;
        size_t mNavCol = 0;
        bool mShifted = false;
        float mCursorBlinkTimer = 0.f;
        bool mCursorBlinkVisible = true;

        size_t registerTextKey(std::string_view widgetName, std::string_view text, std::string_view shifted);
        size_t registerShiftKey(std::string_view widgetName, std::string_view text, std::string_view shifted);
        size_t registerActionKey(std::string_view widgetName, Action action, std::string_view caption);
        void onKeyClicked(MyGUI::Widget* sender);
        void updateKeyCaptions();
        void ensureRowsVisible();
        void centerInMenuArea();
        void syncInputBar();
        void injectText(std::string_view text);
        void injectBackspace();
        void selectNav(size_t row, size_t col);
        size_t mapColumn(size_t fromRow, size_t toRow) const;
    };
}

#endif
