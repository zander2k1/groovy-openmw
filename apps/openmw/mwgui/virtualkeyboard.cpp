#include "virtualkeyboard.hpp"

#include <MyGUI_InputManager.h>
#include <MyGUI_RenderManager.h>
#include <MyGUI_UString.h>
#include <MyGUI_Widget.h>
#include <MyGUI_Window.h>

#include <components/esm/refid.hpp>
#include <components/settings/values.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"

namespace MWGui
{
    namespace
    {
        // Disable mouse focus for the keyboard tree.
        void disableMouseFocus(MyGUI::Widget* parent)
        {
            if (!parent)
                return;

            parent->setNeedMouseFocus(false);
            MyGUI::EnumeratorWidgetPtr enumerator = parent->getEnumerator();
            while (enumerator.next())
                enumerator.current()->setNeedMouseFocus(false);
        }
    }

    VirtualKeyboard::VirtualKeyboard()
        : WindowModal("openmw_virtual_keyboard.layout")
    {
        mDisableGamepadCursor = true;
        disableMouseFocus(mMainWidget);

        getWidget(mInputBar, "InputBar");
        mInputBar->setEditReadOnly(true);
        mInputBar->setNeedKeyFocus(false);

        if (Settings::gui().mControllerMenus)
        {
            mControllerButtons.mDpad = "#{Interface:Move}";
            mControllerButtons.mA = "#{Interface:Select}";
            mControllerButtons.mB = "#{Interface:Back}";
            mControllerButtons.mX = "#{Interface:Backspace}";
            mControllerButtons.mY = "#{Interface:Space}";
        }

        mNavRows.clear();
        mNavRows.resize(5);
        mShiftKeys.clear();

        mNavRows[0] = {
            registerShiftKey("Key0", "0", "!"),
            registerShiftKey("Key1", "1", "@"),
            registerShiftKey("Key2", "2", "$"),
            registerShiftKey("Key3", "3", "%"),
            registerShiftKey("Key4", "4", "&"),
            registerShiftKey("Key5", "5", "*"),
            registerShiftKey("Key6", "6", "("),
            registerShiftKey("Key7", "7", ")"),
            registerShiftKey("Key8", "8", ":"),
            registerShiftKey("Key9", "9", "-"),
        };

        mNavRows[1] = {
            registerShiftKey("KeyA", "a", "A"),
            registerShiftKey("KeyB", "b", "B"),
            registerShiftKey("KeyC", "c", "C"),
            registerShiftKey("KeyD", "d", "D"),
            registerShiftKey("KeyE", "e", "E"),
            registerShiftKey("KeyF", "f", "F"),
            registerShiftKey("KeyG", "g", "G"),
            registerShiftKey("KeyH", "h", "H"),
            registerShiftKey("KeyI", "i", "I"),
            registerShiftKey("KeyJ", "j", "J"),
        };

        mNavRows[2] = {
            registerShiftKey("KeyK", "k", "K"),
            registerShiftKey("KeyL", "l", "L"),
            registerShiftKey("KeyM", "m", "M"),
            registerShiftKey("KeyN", "n", "N"),
            registerShiftKey("KeyO", "o", "O"),
            registerShiftKey("KeyP", "p", "P"),
            registerShiftKey("KeyQ", "q", "Q"),
            registerShiftKey("KeyR", "r", "R"),
            registerShiftKey("KeyS", "s", "S"),
            registerShiftKey("KeyT", "t", "T"),
        };

        mNavRows[3] = {
            registerShiftKey("KeyU", "u", "U"),
            registerShiftKey("KeyV", "v", "V"),
            registerShiftKey("KeyW", "w", "W"),
            registerShiftKey("KeyX", "x", "X"),
            registerShiftKey("KeyY", "y", "Y"),
            registerShiftKey("KeyZ", "z", "Z"),
            registerTextKey("KeySpace", " ", " "),
        };

        mNavRows[4] = {
            registerActionKey("KeyBackspace", Action::Backspace, "Backspace"),
            registerActionKey("KeyShift", Action::Shift, "Shift"),
            registerActionKey("KeyDone", Action::Done, "Done"),
        };

        updateKeyCaptions();
        ensureRowsVisible();
        selectNav(0, 0);
        syncInputBar();
    }

    void VirtualKeyboard::setVisible(bool visible)
    {
        WindowBase::setVisible(visible);
        if (visible)
        {
            mShifted = false;
            mCursorBlinkTimer = 0.f;
            mCursorBlinkVisible = true;
            updateKeyCaptions();
            ensureRowsVisible();
            selectNav(0, 0);
            centerInMenuArea();
            syncInputBar();
        }
    }

    void VirtualKeyboard::onFrame(float dt)
    {
        // Blink the caret in the input bar.
        mCursorBlinkTimer += dt;
        if (mCursorBlinkTimer >= 0.5f)
        {
            mCursorBlinkTimer = 0.f;
            mCursorBlinkVisible = !mCursorBlinkVisible;
        }
        syncInputBar();
    }

    int VirtualKeyboard::getHeight() const
    {
        MyGUI::Window* window = mMainWidget->castType<MyGUI::Window>();
        return window->getHeight();
    }

    void VirtualKeyboard::setTargetEdit(MyGUI::EditBox* edit)
    {
        mTargetEdit = edit;
        syncInputBar();
    }

    void VirtualKeyboard::clearTargetEdit()
    {
        mTargetEdit = nullptr;
        syncInputBar();
    }

    size_t VirtualKeyboard::registerTextKey(std::string_view widgetName, std::string_view text,
        std::string_view shifted)
    {
        MyGUI::Button* button = nullptr;
        getWidget(button, widgetName);
        button->eventMouseButtonClick += MyGUI::newDelegate(this, &VirtualKeyboard::onKeyClicked);

        KeySpec key{ button, std::string(text), std::string(shifted), Action::Text };
        mKeyLookup[button] = mKeys.size();
        size_t index = mKeys.size();
        mKeys.push_back(std::move(key));
        return index;
    }

    size_t VirtualKeyboard::registerShiftKey(std::string_view widgetName, std::string_view text,
        std::string_view shifted)
    {
        size_t index = registerTextKey(widgetName, text, shifted);
        mShiftKeys.push_back(index);
        return index;
    }

    size_t VirtualKeyboard::registerActionKey(std::string_view widgetName, Action action, std::string_view caption)
    {
        MyGUI::Button* button = nullptr;
        getWidget(button, widgetName);
        button->eventMouseButtonClick += MyGUI::newDelegate(this, &VirtualKeyboard::onKeyClicked);

        KeySpec key{ button, std::string(caption), std::string(caption), action };
        mKeyLookup[button] = mKeys.size();
        size_t index = mKeys.size();
        mKeys.push_back(std::move(key));
        return index;
    }

    void VirtualKeyboard::onKeyClicked(MyGUI::Widget* sender)
    {
        auto it = mKeyLookup.find(sender);
        if (it == mKeyLookup.end())
            return;

        MWBase::Environment::get().getWindowManager()->playSound(ESM::RefId::stringRefId("Menu Click"));

        KeySpec& key = mKeys[it->second];
        for (size_t row = 0; row < mNavRows.size(); ++row)
        {
            for (size_t col = 0; col < mNavRows[row].size(); ++col)
            {
                if (mNavRows[row][col] == it->second)
                {
                    selectNav(row, col);
                    break;
                }
            }
        }

        switch (key.mAction)
        {
            case Action::Text:
                injectText(mShifted ? key.mShiftText : key.mText);
                mShifted = false;
                updateKeyCaptions();
                break;
            case Action::Backspace:
                injectBackspace();
                mShifted = false;
                updateKeyCaptions();
                break;
            case Action::Shift:
                mShifted = !mShifted;
                updateKeyCaptions();
                break;
            case Action::Done:
                MWBase::Environment::get().getWindowManager()->toggleVirtualKeyboard();
                mShifted = false;
                updateKeyCaptions();
                break;
        }
    }

    void VirtualKeyboard::updateKeyCaptions()
    {
        for (size_t index : mShiftKeys)
        {
            KeySpec& key = mKeys[index];
            key.mButton->setCaption(mShifted ? key.mShiftText : key.mText);
        }
    }

    void VirtualKeyboard::ensureRowsVisible()
    {
        MyGUI::Widget* keyRows = nullptr;
        MyGUI::Widget* row0 = nullptr;
        getWidget(keyRows, "KeyRows");
        getWidget(row0, "Row0");

        const int rowHeight = row0->getHeight();
        const int rowCount = static_cast<int>(mNavRows.size());
        const int spacing = 4;
        const int requiredKeyRowsHeight = rowCount * rowHeight + (rowCount - 1) * spacing;
        if (requiredKeyRowsHeight > keyRows->getHeight())
            keyRows->setSize(keyRows->getWidth(), requiredKeyRowsHeight);

        const MyGUI::IntCoord keyRowsCoord = keyRows->getCoord();
        const int requiredMainHeight = keyRowsCoord.top + keyRows->getHeight();
        if (requiredMainHeight > mMainWidget->getHeight())
            mMainWidget->setSize(mMainWidget->getWidth(), requiredMainHeight);
    }

    void VirtualKeyboard::centerInMenuArea()
    {
        const MyGUI::IntSize viewSize = MyGUI::RenderManager::getInstance().getViewSize();
        int availableHeight = viewSize.height;
        MWBase::WindowManager* windowManager = MWBase::Environment::get().getWindowManager();
        availableHeight = windowManager->getControllerMenuHeight();

        MyGUI::IntCoord coord = mMainWidget->getCoord();
        coord.left = (viewSize.width - coord.width) / 2;
        coord.top = (availableHeight - coord.height) / 2;
        if (coord.top < 0)
            coord.top = 0;
        mMainWidget->setCoord(coord);
    }

    void VirtualKeyboard::syncInputBar()
    {
        if (!mTargetEdit)
        {
            mInputBar->setCaption({});
            return;
        }

        std::string caption = mTargetEdit->getCaption().asUTF8();
        if (mCursorBlinkVisible)
            caption += "|";
        mInputBar->setCaption(MyGUI::UString(caption));
    }

    void VirtualKeyboard::injectText(std::string_view text)
    {
        if (!mTargetEdit)
            return;
        MyGUI::InputManager::getInstance().setKeyFocusWidget(mTargetEdit);
        MyGUI::UString ustring(text);
        MyGUI::UString::utf32string utf32string = ustring.asUTF32();
        for (MyGUI::UString::utf32string::const_iterator it = utf32string.begin(); it != utf32string.end(); ++it)
            MyGUI::InputManager::getInstance().injectKeyPress(MyGUI::KeyCode::None, *it);
        syncInputBar();
    }

    void VirtualKeyboard::injectBackspace()
    {
        if (!mTargetEdit)
            return;
        MyGUI::InputManager::getInstance().setKeyFocusWidget(mTargetEdit);
        MyGUI::InputManager::getInstance().injectKeyPress(MyGUI::KeyCode::Backspace);
        MyGUI::InputManager::getInstance().injectKeyRelease(MyGUI::KeyCode::Backspace);
        syncInputBar();
    }

    void VirtualKeyboard::selectNav(size_t row, size_t col)
    {
        if (row >= mNavRows.size() || mNavRows[row].empty())
            return;

        col = std::min(col, mNavRows[row].size() - 1);
        if (mNavRow < mNavRows.size())
        {
            size_t prevIndex = mNavRows[mNavRow][mNavCol];
            mKeys[prevIndex].mButton->setStateSelected(false);
        }

        mNavRow = row;
        mNavCol = col;
        size_t index = mNavRows[mNavRow][mNavCol];
        mKeys[index].mButton->setStateSelected(true);
    }

    size_t VirtualKeyboard::mapColumn(size_t fromRow, size_t toRow) const
    {
        if (fromRow >= mNavRows.size() || toRow >= mNavRows.size())
            return 0;

        const size_t fromSize = mNavRows[fromRow].size();
        const size_t toSize = mNavRows[toRow].size();
        if (toSize == 0)
            return 0;
        if (fromSize <= 1)
            return 0;

        // Keep navigation centered when row sizes differ.
        const size_t numerator = mNavCol * (toSize - 1);
        const size_t denominator = fromSize - 1;
        size_t mapped = (numerator + (denominator / 2)) / denominator;
        return std::min(mapped, toSize - 1);
    }

    bool VirtualKeyboard::onControllerButtonEvent(const SDL_ControllerButtonEvent& arg)
    {
        if (arg.button == SDL_CONTROLLER_BUTTON_DPAD_LEFT)
        {
            if (mNavCol > 0)
                selectNav(mNavRow, mNavCol - 1);
            else
                selectNav(mNavRow, mNavRows[mNavRow].size() - 1);
            return true;
        }
        if (arg.button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT)
        {
            selectNav(mNavRow, (mNavCol + 1) % mNavRows[mNavRow].size());
            return true;
        }
        if (arg.button == SDL_CONTROLLER_BUTTON_DPAD_UP)
        {
            size_t targetRow = mNavRow > 0 ? mNavRow - 1 : mNavRows.size() - 1;
            selectNav(targetRow, mapColumn(mNavRow, targetRow));
            return true;
        }
        if (arg.button == SDL_CONTROLLER_BUTTON_DPAD_DOWN)
        {
            size_t targetRow = (mNavRow + 1) % mNavRows.size();
            selectNav(targetRow, mapColumn(mNavRow, targetRow));
            return true;
        }
        if (arg.button == SDL_CONTROLLER_BUTTON_A)
        {
            size_t index = mNavRows[mNavRow][mNavCol];
            onKeyClicked(mKeys[index].mButton);
            return true;
        }
        if (arg.button == SDL_CONTROLLER_BUTTON_B)
        {
            MWBase::Environment::get().getWindowManager()->toggleVirtualKeyboard();
            return true;
        }
        if (arg.button == SDL_CONTROLLER_BUTTON_X)
        {
            injectBackspace();
            return true;
        }
        if (arg.button == SDL_CONTROLLER_BUTTON_Y)
        {
            injectText(" ");
            mShifted = false;
            updateKeyCaptions();
            return true;
        }

        return false;
    }
}
