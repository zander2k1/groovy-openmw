#include "barterofferdialog.hpp"

#include <algorithm>

#include <MyGUI_ImageBox.h>
#include <MyGUI_TextBox.h>

#include <components/settings/values.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/inputmanager.hpp"
#include "../mwbase/windowmanager.hpp"

namespace MWGui
{
    namespace
    {
        bool usesGamecubeTriggers()
        {
            return Settings::input().mControllerIconStyle.get() == "gamecube";
        }
    }

    BarterOfferDialog::BarterOfferDialog()
        : WindowModal("openmw_barter_offer_window.layout")
    {
        getWidget(mLabel, "TotalLabel");
        getWidget(mValue, "TotalValue");
        getWidget(mOfferText, "OfferText");
        getWidget(mCancelText, "CancelText");
        getWidget(mLessText, "LessText");
        getWidget(mMoreText, "MoreText");
        getWidget(mAIcon, "AIcon");
        getWidget(mBIcon, "BIcon");
        getWidget(mLessIcon, "LessIcon");
        getWidget(mMoreIcon, "MoreIcon");
    }

    void BarterOfferDialog::open(const std::string& label, int maxCount)
    {
        mMaxCount = maxCount;
        mShoulderHoldTime = 0.f;
        mShoulderRepeatTime = 0.f;
        mLeftTriggerHeld = false;
        mRightTriggerHeld = false;
        mLabel->setCaptionWithReplacing(label);

        MWBase::InputManager* inputMgr = MWBase::Environment::get().getInputManager();
        mAIcon->setImageTexture(inputMgr->getControllerButtonIcon(SDL_CONTROLLER_BUTTON_A));
        mBIcon->setImageTexture(inputMgr->getControllerButtonIcon(SDL_CONTROLLER_BUTTON_B));
        if (usesGamecubeTriggers())
        {
            mLessIcon->setImageTexture(inputMgr->getControllerAxisIcon(SDL_CONTROLLER_AXIS_TRIGGERLEFT));
            mMoreIcon->setImageTexture(inputMgr->getControllerAxisIcon(SDL_CONTROLLER_AXIS_TRIGGERRIGHT));
        }
        else
        {
            mLessIcon->setImageTexture(inputMgr->getControllerButtonIcon(SDL_CONTROLLER_BUTTON_LEFTSHOULDER));
            mMoreIcon->setImageTexture(inputMgr->getControllerButtonIcon(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER));
        }

        positionIconLeftOfText(mAIcon, mOfferText, true);
        positionIconLeftOfText(mBIcon, mCancelText, true);
        positionIconLeftOfText(mLessIcon, mLessText, false);
        positionIconLeftOfText(mMoreIcon, mMoreText, false);
        setCount(mMaxCount);
        setVisible(true);
        MWBase::Environment::get().getWindowManager()->updateControllerButtonsOverlay();
    }

    void BarterOfferDialog::setCount(int count)
    {
        mCount = std::clamp(count, 1, mMaxCount);
        mValue->setCaption(std::to_string(mCount));
    }

    void BarterOfferDialog::adjustCount(int direction)
    {
        int step = 1;
        if (mShoulderHoldTime >= 3.f)
            step = mMaxCount > 1000 ? 100 : 10;
        setCount(mCount + direction * step);
    }

    void BarterOfferDialog::positionIconLeftOfText(MyGUI::ImageBox* icon, MyGUI::TextBox* text, bool centerText)
    {
        constexpr int gap = 2;
        const int textLeft = centerText ? (text->getWidth() - text->getTextSize().width) / 2 : text->getLeft();
        icon->setPosition(textLeft - icon->getWidth() - gap, icon->getTop());
    }

    void BarterOfferDialog::onFrame(float duration)
    {
        MWBase::InputManager* inputMgr = MWBase::Environment::get().getInputManager();
        const bool gamecubeTriggers = usesGamecubeTriggers();
        const bool decrease = gamecubeTriggers
            ? inputMgr->getControllerAxisValue(SDL_CONTROLLER_AXIS_TRIGGERLEFT) >= 0.5f
            : inputMgr->isControllerButtonPressed(SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
        const bool increase = gamecubeTriggers
            ? inputMgr->getControllerAxisValue(SDL_CONTROLLER_AXIS_TRIGGERRIGHT) >= 0.5f
            : inputMgr->isControllerButtonPressed(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
        if (decrease == increase)
        {
            mShoulderHoldTime = 0.f;
            mShoulderRepeatTime = 0.f;
            return;
        }

        const float repeatRate = Settings::gui().mControllerMenuTurboRepeat * 7.5f;
        if (repeatRate <= 0.f)
            return;

        mShoulderHoldTime += duration;
        if (mShoulderHoldTime < 0.3f)
            return;

        mShoulderRepeatTime += duration;
        const float interval = 1.f / repeatRate;
        while (mShoulderRepeatTime >= interval)
        {
            mShoulderRepeatTime -= interval;
            adjustCount(decrease ? -1 : 1);
        }
    }

    bool BarterOfferDialog::onControllerButtonEvent(const SDL_ControllerButtonEvent& arg)
    {
        if (arg.button == SDL_CONTROLLER_BUTTON_A)
        {
            eventOkClicked(nullptr, mCount);
        }
        else if (arg.button == SDL_CONTROLLER_BUTTON_B)
        {
            setVisible(false);
            eventCancelClicked();
        }
        else if (!usesGamecubeTriggers() && arg.button == SDL_CONTROLLER_BUTTON_LEFTSHOULDER)
            adjustCount(-1);
        else if (!usesGamecubeTriggers() && arg.button == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)
            adjustCount(1);
        return true;
    }

    bool BarterOfferDialog::onControllerThumbstickEvent(const SDL_ControllerAxisEvent& arg)
    {
        if (!usesGamecubeTriggers())
            return false;

        constexpr int pressThreshold = 16000;
        constexpr int releaseThreshold = 8000;
        if (arg.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT)
        {
            if (!mLeftTriggerHeld && arg.value >= pressThreshold)
            {
                mLeftTriggerHeld = true;
                adjustCount(-1);
            }
            else if (mLeftTriggerHeld && arg.value <= releaseThreshold)
                mLeftTriggerHeld = false;
            return true;
        }
        if (arg.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT)
        {
            if (!mRightTriggerHeld && arg.value >= pressThreshold)
            {
                mRightTriggerHeld = true;
                adjustCount(1);
            }
            else if (mRightTriggerHeld && arg.value <= releaseThreshold)
                mRightTriggerHeld = false;
            return true;
        }
        return false;
    }
}
