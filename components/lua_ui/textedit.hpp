#ifndef OPENMW_LUAUI_TEXTEDIT
#define OPENMW_LUAUI_TEXTEDIT

#include <optional>
#include <vector>

#include <MyGUI_EditBox.h>

#include "widget.hpp"

namespace LuaUi
{
    class LuaTextEdit : public MyGUI::Widget, public WidgetExtension
    {
        MYGUI_RTTI_DERIVED(LuaTextEdit)

    public:
        bool isTextInput() override { return mEditBox && !mEditBox->getEditStatic(); }
        MyGUI::EditBox* keyboardTargetEdit() override { return mEditBox; }
        MyGUI::UString keyboardTargetText() const override;
        void setKeyboardTargetText(const MyGUI::UString& text) override;

    protected:
        void initialize() override;
        void deinitialize() override;
        void updateProperties() override;
        void updateCoord() override;
        void updateChildren() override;
        MyGUI::IntSize calculateSize() const override;

        const std::vector<std::string_view>& allUsedProperties() const override;

    private:
        void textChange(MyGUI::EditBox*);
        void activateEditBox();
        void mousePress(MyGUI::Widget*, int, int, MyGUI::MouseButton);
        void mouseRelease(MyGUI::Widget*, int, int, MyGUI::MouseButton);

        MyGUI::EditBox* mEditBox = nullptr;
        std::optional<MyGUI::UString> mKeyboardText;
        std::optional<MyGUI::UString> mLastPropertyText;
        bool mMultiline{ false };
        bool mAutoSize{ false };
    };
}

#endif // OPENMW_LUAUI_TEXTEDIT
