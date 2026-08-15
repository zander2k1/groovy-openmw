#ifndef MWGUI_BARTEROFFERDIALOG_H
#define MWGUI_BARTEROFFERDIALOG_H

#include "windowbase.hpp"

namespace MyGUI
{
    class ImageBox;
    class TextBox;
}

namespace MWGui
{
    class BarterOfferDialog : public WindowModal
    {
    public:
        BarterOfferDialog();

        void open(const std::string& label, int maxCount);
        void setCount(int count);

        MyGUI::delegates::MultiDelegate<MyGUI::Widget*, std::size_t> eventOkClicked;
        MyGUI::delegates::MultiDelegate<> eventCancelClicked;

        ControllerButtons* getControllerButtons() override { return nullptr; }
        void onFrame(float duration) override;

    private:
        MyGUI::TextBox* mLabel;
        MyGUI::TextBox* mValue;
        MyGUI::TextBox* mOfferText;
        MyGUI::TextBox* mCancelText;
        MyGUI::TextBox* mLessText;
        MyGUI::TextBox* mMoreText;
        MyGUI::ImageBox* mAIcon;
        MyGUI::ImageBox* mBIcon;
        MyGUI::ImageBox* mLessIcon;
        MyGUI::ImageBox* mMoreIcon;
        int mMaxCount = 1;
        int mCount = 1;
        float mShoulderHoldTime = 0.f;
        float mShoulderRepeatTime = 0.f;
        bool mLeftTriggerHeld = false;
        bool mRightTriggerHeld = false;

        void adjustCount(int direction);
        void positionIconLeftOfText(MyGUI::ImageBox* icon, MyGUI::TextBox* text, bool centerText);
        bool onControllerButtonEvent(const SDL_ControllerButtonEvent& arg) override;
        bool onControllerThumbstickEvent(const SDL_ControllerAxisEvent& arg) override;
    };
}

#endif
