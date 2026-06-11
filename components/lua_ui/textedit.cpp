#include "textedit.hpp"

#include <algorithm>

#include <MyGUI_InputManager.h>

#include "alignment.hpp"

namespace LuaUi
{
    void LuaTextEdit::initialize()
    {
        mEditBox = createWidget<MyGUI::EditBox>("LuaTextEdit", MyGUI::IntCoord(0, 0, 0, 0), MyGUI::Align::Default);
        mEditBox->eventEditTextChange += MyGUI::newDelegate(this, &LuaTextEdit::textChange);
        mEditBox->eventMouseButtonPressed += MyGUI::newDelegate(this, &LuaTextEdit::mousePress);
        mEditBox->eventMouseButtonReleased += MyGUI::newDelegate(this, &LuaTextEdit::mouseRelease);
        mEditBox->setMaxTextLength(std::numeric_limits<std::size_t>::max());
        registerEvents(mEditBox);
        WidgetExtension::initialize();
    }

    void LuaTextEdit::deinitialize()
    {
        mEditBox->eventEditTextChange -= MyGUI::newDelegate(this, &LuaTextEdit::textChange);
        mEditBox->eventMouseButtonPressed -= MyGUI::newDelegate(this, &LuaTextEdit::mousePress);
        mEditBox->eventMouseButtonReleased -= MyGUI::newDelegate(this, &LuaTextEdit::mouseRelease);
        clearEvents(mEditBox);
        WidgetExtension::deinitialize();
    }

    void LuaTextEdit::updateProperties()
    {
        mEditBox->setFontHeight(propertyValue("textSize", 10));
        mEditBox->setTextColour(propertyValue("textColor", MyGUI::Colour(0, 0, 0, 1)));
        mEditBox->setEditWordWrap(propertyValue("wordWrap", false));

        Alignment horizontal(propertyValue("textAlignH", Alignment::Start));
        Alignment vertical(propertyValue("textAlignV", Alignment::Start));
        mEditBox->setTextAlign(alignmentToMyGui(horizontal, vertical));

        mMultiline = propertyValue("multiline", false);
        mEditBox->setEditMultiLine(mMultiline);

        bool readOnly = propertyValue("readOnly", false);
        mEditBox->setEditStatic(readOnly);

        mAutoSize = (readOnly || !mMultiline) && propertyValue("autoSize", false);

        // change caption last, for multiline and wordwrap to apply
        MyGUI::UString propertyText = propertyValue("text", std::string());
        if (mKeyboardText && propertyText == *mKeyboardText)
            mKeyboardText.reset();
        else if (mKeyboardText && mLastPropertyText && propertyText != *mLastPropertyText)
            mKeyboardText.reset();

        if (mKeyboardText)
            mEditBox->setCaption(*mKeyboardText);
        else
            mEditBox->setCaption(propertyText);
        mLastPropertyText = propertyText;

        WidgetExtension::updateProperties();
    }

    MyGUI::UString LuaTextEdit::keyboardTargetText() const
    {
        return mKeyboardText.value_or(mEditBox ? mEditBox->getOnlyText() : MyGUI::UString());
    }

    void LuaTextEdit::setKeyboardTargetText(const MyGUI::UString& text)
    {
        mKeyboardText = text;
        if (!mEditBox)
            return;
        mEditBox->setOnlyText(text);
        mEditBox->setCaption(text);
        mEditBox->setTextCursor(std::min(mEditBox->getTextCursor(), text.size()));
        textChange(mEditBox);
    }

    void LuaTextEdit::textChange(MyGUI::EditBox*)
    {
        protectedCall([this](LuaUtil::LuaView& view) {
            triggerEvent("textChanged", sol::make_object(view.sol(), mEditBox->getCaption().asUTF8()));
        });
    }

    void LuaTextEdit::activateEditBox()
    {
        MyGUI::InputManager& input = MyGUI::InputManager::getInstance();
        if (input.getKeyFocusWidget() != mEditBox)
            input.setKeyFocusWidget(nullptr);
        input.setKeyFocusWidget(mEditBox);
        mEditBox->setTextCursor(std::min(mEditBox->getTextCursor(), mEditBox->getOnlyText().size()));
    }

    void LuaTextEdit::mousePress(MyGUI::Widget*, int, int, MyGUI::MouseButton button)
    {
        if (button == MyGUI::MouseButton::Left && isTextInput())
            activateEditBox();
    }

    void LuaTextEdit::mouseRelease(MyGUI::Widget*, int, int, MyGUI::MouseButton button)
    {
        if (button == MyGUI::MouseButton::Left && isTextInput())
            activateEditBox();
    }

    void LuaTextEdit::updateCoord()
    {
        WidgetExtension::updateCoord();
        mEditBox->setSize(widget()->getSize());
    }

    void LuaTextEdit::updateChildren()
    {
        WidgetExtension::updateChildren();
        // otherwise it won't be focusable
        mEditBox->detachFromWidget();
        mEditBox->attachToWidget(this);
    }

    MyGUI::IntSize LuaTextEdit::calculateSize() const
    {
        MyGUI::IntSize normalSize = WidgetExtension::calculateSize();
        if (mAutoSize)
        {
            mEditBox->setSize(normalSize);
            int targetHeight = mMultiline ? mEditBox->getTextSize().height : mEditBox->getFontHeight();
            normalSize.height = std::max(normalSize.height, targetHeight);
        }
        return normalSize;
    }

    const std::vector<std::string_view>& LuaTextEdit::allUsedProperties() const
    {
        static std::vector<std::string_view> usedProps = std::invoke([this] {
            std::vector<std::string_view> props = { "text", "textSize", "textColor", "wordWrap", "textAlignH",
                "textAlignV", "multiline", "readOnly", "autoSize" };
            auto baseProps = WidgetExtension::allUsedProperties();
            props.insert(props.end(), baseProps.begin(), baseProps.end());
            return props;
        });
        return usedProps;
    }
}
