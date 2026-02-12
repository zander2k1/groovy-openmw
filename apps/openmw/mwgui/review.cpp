#include "review.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include <MyGUI_Button.h>
#include <MyGUI_Gui.h>
#include <MyGUI_ImageBox.h>
#include <MyGUI_InputManager.h>
#include <MyGUI_ScrollView.h>
#include <MyGUI_TextIterator.h>
#include <MyGUI_UString.h>

#include <components/esm3/loadbsgn.hpp>
#include <components/esm3/loadrace.hpp>
#include <components/esm3/loadspel.hpp>
#include <components/settings/values.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/inputmanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"
#include "../mwmechanics/actorutil.hpp"
#include "../mwmechanics/autocalcspell.hpp"
#include "../mwmechanics/npcstats.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/player.hpp"

#include "tooltips.hpp"

namespace
{
    MyGUI::Widget* createControllerHighlight(MyGUI::Widget* parent, const MyGUI::IntCoord& coord)
    {
        if (!parent)
            return nullptr;

        auto* highlight = parent->createWidget<MyGUI::Widget>("ControllerHighlight", coord, MyGUI::Align::Default);
        highlight->setNeedMouseFocus(false);
        highlight->setDepth(1);
        highlight->setVisible(false);
        return highlight;
    }

    MyGUI::IntCoord getRowCoord(MyGUI::Widget* left, MyGUI::Widget* right)
    {
        if (!left)
            return {};

        const MyGUI::IntCoord leftCoord = left->getCoord();
        if (!right)
            return leftCoord;

        const MyGUI::IntCoord rightCoord = right->getCoord();
        const int leftX = std::min(leftCoord.left, rightCoord.left);
        const int rightX = std::max(leftCoord.left + leftCoord.width, rightCoord.left + rightCoord.width);
        const int topY = std::min(leftCoord.top, rightCoord.top);
        const int bottomY = std::max(leftCoord.top + leftCoord.height, rightCoord.top + rightCoord.height);
        return { leftX, topY, rightX - leftX, bottomY - topY };
    }

    void adjustButtonSize(MyGUI::Button* button)
    {
        // adjust size of button to fit its text
        MyGUI::IntSize size = button->getTextSize();
        button->setSize(size.width + 24, button->getSize().height);
    }

    std::pair<MyGUI::TextBox*, MyGUI::TextBox*> getAttributeTextWidgets(MWGui::Widgets::MWAttribute* attribute)
    {
        if (!attribute)
            return { nullptr, nullptr };

        return { attribute->getNameWidget(), attribute->getValueWidget() };
    }

    std::pair<MyGUI::TextBox*, MyGUI::TextBox*> getDynamicStatTextWidgets(MWGui::Widgets::MWDynamicStat* stat)
    {
        if (!stat)
            return { nullptr, nullptr };

        return { stat->getTitleWidget(), stat->getValueWidget() };
    }

    MyGUI::TextBox* getSpellTextWidget(MWGui::Widgets::MWSpell* spell)
    {
        if (!spell)
            return nullptr;

        MyGUI::Widget* nameWidget = spell->findWidget("StatName");
        return nameWidget ? nameWidget->castType<MyGUI::TextBox>(false) : nullptr;
    }
}

namespace MWGui
{
    ReviewDialog::ReviewDialog()
        : WindowModal("openmw_chargen_review.layout")
        , mUpdateSkillArea(false)
        , mControllerFocus(0)
    {
        // Centre dialog
        center();

        // Setup static stats
        getWidget(mNameWidget, "NameText");
        getWidget(mNameButton, "NameButton");
        if (mNameButton)
        {
            mNameButton->setUserString("ToolTipType", "Layout");
            mNameButton->setUserString("ToolTipLayout", "RaceToolTip");
            mNameButton->setUserString("ForceCollapsedTooltip", "true");
        }
        if (mNameWidget)
        {
            mNameWidget->setUserString("ToolTipType", "Layout");
            mNameWidget->setUserString("ToolTipLayout", "RaceToolTip");
            mNameWidget->setUserString("ForceCollapsedTooltip", "true");
            mNameWidget->changeWidgetSkin("SandTextRightController");
        }
        adjustButtonSize(mNameButton);
        mNameButton->eventMouseButtonClick += MyGUI::newDelegate(this, &ReviewDialog::onNameClicked);

        getWidget(mRaceWidget, "RaceText");
        getWidget(mRaceButton, "RaceButton");
        if (mRaceButton)
        {
            mRaceButton->setUserString("ToolTipType", "Layout");
            mRaceButton->setUserString("ToolTipLayout", "RaceToolTip");
        }
        if (mRaceWidget)
        {
            mRaceWidget->setUserString("ToolTipType", "Layout");
            mRaceWidget->setUserString("ToolTipLayout", "RaceToolTip");
            mRaceWidget->changeWidgetSkin("SandTextRightController");
        }
        adjustButtonSize(mRaceButton);
        mRaceButton->eventMouseButtonClick += MyGUI::newDelegate(this, &ReviewDialog::onRaceClicked);

        getWidget(mClassWidget, "ClassText");
        getWidget(mClassButton, "ClassButton");
        if (mClassButton)
        {
            mClassButton->setUserString("ToolTipType", "Layout");
            mClassButton->setUserString("ToolTipLayout", "ClassToolTip");
        }
        if (mClassWidget)
        {
            mClassWidget->setUserString("ToolTipType", "Layout");
            mClassWidget->setUserString("ToolTipLayout", "ClassToolTip");
            mClassWidget->changeWidgetSkin("SandTextRightController");
        }
        adjustButtonSize(mClassButton);
        mClassButton->eventMouseButtonClick += MyGUI::newDelegate(this, &ReviewDialog::onClassClicked);

        getWidget(mBirthSignWidget, "SignText");
        getWidget(mBirthSignButton, "SignButton");
        if (mBirthSignButton)
        {
            mBirthSignButton->setUserString("ToolTipType", "Layout");
            mBirthSignButton->setUserString("ToolTipLayout", "BirthSignToolTip");
            mBirthSignButton->setUserString("ToolTipDynamic", "Stats");
        }
        if (mBirthSignWidget)
        {
            mBirthSignWidget->setUserString("ToolTipType", "Layout");
            mBirthSignWidget->setUserString("ToolTipLayout", "BirthSignToolTip");
            mBirthSignWidget->setUserString("ToolTipDynamic", "Stats");
            mBirthSignWidget->changeWidgetSkin("SandTextRightController");
        }
        adjustButtonSize(mBirthSignButton);
        mBirthSignButton->eventMouseButtonClick += MyGUI::newDelegate(this, &ReviewDialog::onBirthSignClicked);

        auto setCollapsedPair = [](MyGUI::Button* button, MyGUI::TextBox* valueWidget) {
            if (!button && !valueWidget)
                return;
            const std::string label = button ? button->getCaption().asUTF8() : std::string();
            const std::string value = valueWidget ? valueWidget->getCaption().asUTF8() : std::string();
            if (label.empty())
                return;
            if (button)
            {
                button->setUserString("CollapsedLabel", label);
                button->setUserString("CollapsedValue", value);
            }
            if (valueWidget)
            {
                valueWidget->setUserString("CollapsedLabel", label);
                valueWidget->setUserString("CollapsedValue", value);
            }
        };

        setCollapsedPair(mNameButton, mNameWidget);
        setCollapsedPair(mRaceButton, mRaceWidget);
        setCollapsedPair(mClassButton, mClassWidget);
        setCollapsedPair(mBirthSignButton, mBirthSignWidget);

        // Setup dynamic stats
        getWidget(mHealth, "Health");
        mHealth->setTitle(MWBase::Environment::get().getWindowManager()->getGameSettingString("sHealth", {}));
        mHealth->setValue(45, 45);
        mHealth->setUserString("ToolTipDynamic", "Stats");
        {
            const auto [nameWidget, valueWidget] = getDynamicStatTextWidgets(mHealth);
            if (nameWidget)
            {
                const MyGUI::UString caption = nameWidget->getCaption();
                nameWidget->changeWidgetSkin("NormalTextController");
                nameWidget->setCaption(caption);
            }
            if (valueWidget)
            {
                const MyGUI::UString caption = valueWidget->getCaption();
                valueWidget->changeWidgetSkin("ProgressTextController");
                valueWidget->setCaption(caption);
            }
        }

        getWidget(mMagicka, "Magicka");
        mMagicka->setTitle(MWBase::Environment::get().getWindowManager()->getGameSettingString("sMagic", {}));
        mMagicka->setValue(50, 50);
        mMagicka->setUserString("ToolTipDynamic", "Stats");
        {
            const auto [nameWidget, valueWidget] = getDynamicStatTextWidgets(mMagicka);
            if (nameWidget)
            {
                const MyGUI::UString caption = nameWidget->getCaption();
                nameWidget->changeWidgetSkin("NormalTextController");
                nameWidget->setCaption(caption);
            }
            if (valueWidget)
            {
                const MyGUI::UString caption = valueWidget->getCaption();
                valueWidget->changeWidgetSkin("ProgressTextController");
                valueWidget->setCaption(caption);
            }
        }

        getWidget(mFatigue, "Fatigue");
        mFatigue->setTitle(MWBase::Environment::get().getWindowManager()->getGameSettingString("sFatigue", {}));
        mFatigue->setValue(160, 160);
        mFatigue->setUserString("ToolTipDynamic", "Stats");
        {
            const auto [nameWidget, valueWidget] = getDynamicStatTextWidgets(mFatigue);
            if (nameWidget)
            {
                const MyGUI::UString caption = nameWidget->getCaption();
                nameWidget->changeWidgetSkin("NormalTextController");
                nameWidget->setCaption(caption);
            }
            if (valueWidget)
            {
                const MyGUI::UString caption = valueWidget->getCaption();
                valueWidget->changeWidgetSkin("ProgressTextController");
                valueWidget->setCaption(caption);
            }
        }

        // Setup attributes

        MyGUI::Widget* attributes = getWidget("Attributes");
        const auto& store = MWBase::Environment::get().getWorld()->getStore().get<ESM::Attribute>();
        MyGUI::IntCoord coord{ 8, 4, 250, 18 };
        for (const ESM::Attribute& attribute : store)
        {
            auto* widget
                = attributes->createWidget<Widgets::MWAttribute>("MW_StatNameValue", coord, MyGUI::Align::Default);
            mAttributeWidgets.emplace(attribute.mId, widget);
            widget->setUserString("ToolTipDynamic", "Stats");
            widget->setUserString("ToolTipType", "Layout");
            widget->setUserString("ToolTipLayout", "AttributeToolTip");
            widget->setUserString("Caption_AttributeName", attribute.mName);
            widget->setUserString("Caption_AttributeDescription", attribute.mDescription);
            widget->setUserString("ImageTexture_AttributeImage", attribute.mIcon);
            widget->setAttributeId(attribute.mId);
            widget->setAttributeValue(Widgets::MWAttribute::AttributeValue());
            {
                const auto [nameWidget, valueWidget] = getAttributeTextWidgets(widget);
                if (nameWidget)
                    nameWidget->changeWidgetSkin("SandTextController");
                if (valueWidget)
                    valueWidget->changeWidgetSkin("SandTextRightController");
            }
            coord.top += coord.height;
            mAttributeList.push_back(widget);
        }

        // Setup skills
        getWidget(mSkillView, "SkillView");
        mSkillView->eventMouseWheel += MyGUI::newDelegate(this, &ReviewDialog::onMouseWheel);

        for (const ESM::Skill& skill : MWBase::Environment::get().getESMStore()->get<ESM::Skill>())
        {
            mSkillValues.emplace(skill.mId, MWMechanics::SkillValue());
            mSkillWidgetMap.emplace(skill.mId, std::make_pair<MyGUI::TextBox*, MyGUI::TextBox*>(nullptr, nullptr));
        }

        if (Settings::gui().mControllerMenus)
        {
            mControllerButtons = {};
            mControllerButtons.mA = "#{Interface:Select}";
            mControllerButtons.mB = "#{Interface:Back}";
            mControllerButtons.mX = "#{Interface:Done}";
            mControllerButtons.mXAfterB = true;
            mControllerButtons.mY = "#{Interface:Info}";
            mDisableGamepadCursor = true;
        }

        if (Settings::gui().mControllerMenus && useControllerSelectionHighlight())
        {
            MyGUI::Widget* rowParent = mNameButton ? mNameButton->getParent() : nullptr;
            if (rowParent)
            {
                if (mNameButton)
                    mNameHighlight = createControllerHighlight(rowParent, getRowCoord(mNameButton, mNameWidget));
                if (mRaceButton)
                    mRaceHighlight = createControllerHighlight(rowParent, getRowCoord(mRaceButton, mRaceWidget));
                if (mClassButton)
                    mClassHighlight = createControllerHighlight(rowParent, getRowCoord(mClassButton, mClassWidget));
                if (mBirthSignButton)
                    mBirthSignHighlight
                        = createControllerHighlight(rowParent, getRowCoord(mBirthSignButton, mBirthSignWidget));
            }
            for (Widgets::MWAttribute* attribute : mAttributeList)
            {
                if (attribute)
                {
                    MyGUI::Widget* parent = attribute->getParent();
                    mAttributeHighlights.push_back(createControllerHighlight(parent, attribute->getCoord()));
                }
                else
                    mAttributeHighlights.push_back(nullptr);
            }
        }
    }

    void ReviewDialog::onOpen()
    {
        WindowModal::onOpen();
        if (Settings::gui().mControllerMenus)
        {
            resetControllerItems();
            setActiveControllerWindow(true);
        }
        mUpdateSkillArea = true;
    }

    void ReviewDialog::onClose()
    {
        WindowModal::onClose();
        mUpdateSkillArea = false;
        mControllerItems.clear();
        mControllerLeftItems.clear();
        mControllerRightItems.clear();
        mControllerBaseStates.clear();
        mActiveControllerWindow = false;
    }

    void ReviewDialog::onFrame(float /*duration*/)
    {
        if (mUpdateSkillArea)
        {
            updateSkillArea();
            mUpdateSkillArea = false;
        }
    }

    void ReviewDialog::setPlayerName(const std::string& name)
    {
        mNameWidget->setCaption(name);
        const std::string label = mNameButton ? mNameButton->getCaption().asUTF8() : std::string();
        if (!label.empty())
        {
            if (mNameWidget)
            {
                mNameWidget->setUserString("CollapsedLabel", label);
                mNameWidget->setUserString("CollapsedValue", name);
            }
            if (mNameButton)
            {
                mNameButton->setUserString("CollapsedLabel", label);
                mNameButton->setUserString("CollapsedValue", name);
            }
        }
    }

    void ReviewDialog::setRace(const ESM::RefId& raceId)
    {
        mRaceId = raceId;

        const ESM::Race* race = MWBase::Environment::get().getESMStore()->get<ESM::Race>().search(mRaceId);
        if (race)
        {
            ToolTips::createRaceToolTip(mRaceWidget, race);
            if (mRaceButton)
                ToolTips::createRaceToolTip(mRaceButton, race);
            mRaceWidget->setCaption(race->mName);
            const std::string label = mRaceButton ? mRaceButton->getCaption().asUTF8() : std::string();
            if (!label.empty())
            {
                if (mRaceWidget)
                {
                    mRaceWidget->setUserString("CollapsedLabel", label);
                    mRaceWidget->setUserString("CollapsedValue", race->mName);
                }
                if (mRaceButton)
                {
                    mRaceButton->setUserString("CollapsedLabel", label);
                    mRaceButton->setUserString("CollapsedValue", race->mName);
                }
            }
        }

        mUpdateSkillArea = true;
    }

    void ReviewDialog::setClass(const ESM::Class& playerClass)
    {
        mClass = playerClass;
        mClassWidget->setCaption(mClass.mName);
        ToolTips::createClassToolTip(mClassWidget, mClass);
        if (mClassButton)
            ToolTips::createClassToolTip(mClassButton, mClass);
        const std::string label = mClassButton ? mClassButton->getCaption().asUTF8() : std::string();
        if (!label.empty())
        {
            if (mClassWidget)
            {
                mClassWidget->setUserString("CollapsedLabel", label);
                mClassWidget->setUserString("CollapsedValue", mClass.mName);
            }
            if (mClassButton)
            {
                mClassButton->setUserString("CollapsedLabel", label);
                mClassButton->setUserString("CollapsedValue", mClass.mName);
            }
        }
    }

    void ReviewDialog::setBirthSign(const ESM::RefId& signId)
    {
        mBirthSignId = signId;

        const ESM::BirthSign* sign
            = MWBase::Environment::get().getESMStore()->get<ESM::BirthSign>().search(mBirthSignId);
        if (sign)
        {
            mBirthSignWidget->setCaption(sign->mName);
            ToolTips::createBirthsignToolTip(mBirthSignWidget, mBirthSignId);
            if (mBirthSignButton)
                ToolTips::createBirthsignToolTip(mBirthSignButton, mBirthSignId);
            mBirthSignWidget->setUserString("ToolTipDynamic", "Stats");
            if (mBirthSignButton)
                mBirthSignButton->setUserString("ToolTipDynamic", "Stats");
            const std::string label = mBirthSignButton ? mBirthSignButton->getCaption().asUTF8() : std::string();
            if (!label.empty())
            {
                if (mBirthSignWidget)
                {
                    mBirthSignWidget->setUserString("CollapsedLabel", label);
                    mBirthSignWidget->setUserString("CollapsedValue", sign->mName);
                }
                if (mBirthSignButton)
                {
                    mBirthSignButton->setUserString("CollapsedLabel", label);
                    mBirthSignButton->setUserString("CollapsedValue", sign->mName);
                }
            }
        }

        mUpdateSkillArea = true;
    }

    void ReviewDialog::setHealth(const MWMechanics::DynamicStat<float>& value)
    {
        int current = std::max(0, static_cast<int>(value.getCurrent()));
        int modified = static_cast<int>(value.getModified());

        mHealth->setValue(current, modified);
        std::string valStr = MyGUI::utility::toString(current) + " / " + MyGUI::utility::toString(modified);
        mHealth->setUserString("Caption_HealthDescription", "#{sHealthDesc}\n" + valStr);
        const std::string label
            = std::string(MWBase::Environment::get().getWindowManager()->getGameSettingString("sHealth", "Health"));
        mHealth->setUserString("CollapsedLabel", label);
        mHealth->setUserString("CollapsedValue", valStr);
    }

    void ReviewDialog::setMagicka(const MWMechanics::DynamicStat<float>& value)
    {
        int current = std::max(0, static_cast<int>(value.getCurrent()));
        int modified = static_cast<int>(value.getModified());

        mMagicka->setValue(current, modified);
        std::string valStr = MyGUI::utility::toString(current) + " / " + MyGUI::utility::toString(modified);
        mMagicka->setUserString("Caption_HealthDescription", "#{sMagDesc}\n" + valStr);
        const std::string label
            = std::string(MWBase::Environment::get().getWindowManager()->getGameSettingString("sMagic", "Magicka"));
        mMagicka->setUserString("CollapsedLabel", label);
        mMagicka->setUserString("CollapsedValue", valStr);
    }

    void ReviewDialog::setFatigue(const MWMechanics::DynamicStat<float>& value)
    {
        int current = static_cast<int>(value.getCurrent());
        int modified = static_cast<int>(value.getModified());

        mFatigue->setValue(current, modified);
        std::string valStr = MyGUI::utility::toString(current) + " / " + MyGUI::utility::toString(modified);
        mFatigue->setUserString("Caption_HealthDescription", "#{sFatDesc}\n" + valStr);
        const std::string label
            = std::string(MWBase::Environment::get().getWindowManager()->getGameSettingString("sFatigue", "Fatigue"));
        mFatigue->setUserString("CollapsedLabel", label);
        mFatigue->setUserString("CollapsedValue", valStr);
    }

    void ReviewDialog::setAttribute(ESM::RefId attributeId, const MWMechanics::AttributeValue& value)
    {
        auto attr = mAttributeWidgets.find(attributeId);
        if (attr == mAttributeWidgets.end())
            return;

        Widgets::MWAttribute* widget = attr->second;
        if (!widget)
            return;

        if (widget->getAttributeValue() != value)
        {
            widget->setAttributeValue(value);
            mUpdateSkillArea = true;
        }

        const float modified = value.getModified();
        const float base = value.getBase();
        std::string state = "normal";
        if (modified > base)
            state = "increased";
        else if (modified < base)
            state = "decreased";

        const auto [nameWidget, valueWidget] = getAttributeTextWidgets(widget);
        if (valueWidget)
            setControllerWidgetBaseState(valueWidget, state);

        const std::string label = widget->isUserString("Caption_AttributeName")
            ? std::string(widget->getUserString("Caption_AttributeName"))
            : std::string();
        if (!label.empty())
        {
            widget->setUserString("CollapsedLabel", label);
            widget->setUserString("CollapsedValue", std::to_string(static_cast<int>(value.getModified())));
        }
    }

    void ReviewDialog::setSkillValue(ESM::RefId id, const MWMechanics::SkillValue& value)
    {
        mSkillValues[id] = value;
        auto it = mSkillWidgetMap.find(id);
        if (it != mSkillWidgetMap.end())
        {
            const std::pair<MyGUI::TextBox*, MyGUI::TextBox*>& widgets = it->second;
            MyGUI::TextBox* nameWidget = widgets.first;
            MyGUI::TextBox* valueWidget = widgets.second;
            if (nameWidget && valueWidget)
            {
                float modified = value.getModified();
                float base = value.getBase();
                std::string text = MyGUI::utility::toString(static_cast<int>(modified));
                std::string state = "normal";
                if (modified > base)
                    state = "increased";
                else if (modified < base)
                    state = "decreased";

                valueWidget->setCaption(text);
                valueWidget->_setWidgetState(state);
                setControllerWidgetBaseState(valueWidget, state);

                auto setSkillProgress = [&](MyGUI::Widget* widget, float progress) {
                    MWWorld::Ptr player = MWMechanics::getPlayer();
                    if (player.isEmpty())
                        return;
                    const ESM::NPC* npc = player.get<ESM::NPC>()->mBase;
                    if (!npc)
                        return;
                    const MWWorld::ESMStore& esmStore = *MWBase::Environment::get().getESMStore();
                    const ESM::Class* npcClass = esmStore.get<ESM::Class>().find(npc->mClass);
                    if (!npcClass)
                        return;
                    const float requirement
                        = player.getClass().getNpcStats(player).getSkillProgressRequirement(id, *npcClass);
                    if (requirement <= 0.f)
                        return;
                    const int progressPercent = static_cast<int>(progress / requirement * 100.f + 0.5f);
                    widget->setUserString(
                        "Caption_SkillProgressText", MyGUI::utility::toString(progressPercent) + "/100");
                    widget->setUserString("RangePosition_SkillProgress", MyGUI::utility::toString(progressPercent));
                };

                if (value.getBase() < 100)
                {
                    nameWidget->setUserString("Visible_SkillMaxed", "false");
                    nameWidget->setUserString("UserData^Hidden_SkillMaxed", "true");
                    nameWidget->setUserString("Visible_SkillProgressVBox", "true");
                    nameWidget->setUserString("UserData^Hidden_SkillProgressVBox", "false");

                    valueWidget->setUserString("Visible_SkillMaxed", "false");
                    valueWidget->setUserString("UserData^Hidden_SkillMaxed", "true");
                    valueWidget->setUserString("Visible_SkillProgressVBox", "true");
                    valueWidget->setUserString("UserData^Hidden_SkillProgressVBox", "false");

                    setSkillProgress(nameWidget, value.getProgress());
                    setSkillProgress(valueWidget, value.getProgress());
                }
                else
                {
                    nameWidget->setUserString("Visible_SkillMaxed", "true");
                    nameWidget->setUserString("UserData^Hidden_SkillMaxed", "false");
                    nameWidget->setUserString("Visible_SkillProgressVBox", "false");
                    nameWidget->setUserString("UserData^Hidden_SkillProgressVBox", "true");

                    valueWidget->setUserString("Visible_SkillMaxed", "true");
                    valueWidget->setUserString("UserData^Hidden_SkillMaxed", "false");
                    valueWidget->setUserString("Visible_SkillProgressVBox", "false");
                    valueWidget->setUserString("UserData^Hidden_SkillProgressVBox", "true");
                }

                const std::string label = nameWidget->isUserString("Caption_SkillName")
                    ? std::string(nameWidget->getUserString("Caption_SkillName"))
                    : nameWidget->getCaption().asUTF8();
                nameWidget->setUserString("CollapsedLabel", label);
                valueWidget->setUserString("CollapsedLabel", label);
                nameWidget->setUserString("CollapsedValue", text);
                valueWidget->setUserString("CollapsedValue", text);
            }
        }

        mUpdateSkillArea = true;
    }

    void ReviewDialog::configureSkills(const std::vector<ESM::RefId>& major, const std::vector<ESM::RefId>& minor)
    {
        mMajorSkills = major;
        mMinorSkills = minor;

        // Update misc skills with the remaining skills not in major or minor
        std::set<ESM::RefId> skillSet;
        std::copy(major.begin(), major.end(), std::inserter(skillSet, skillSet.begin()));
        std::copy(minor.begin(), minor.end(), std::inserter(skillSet, skillSet.begin()));
        mMiscSkills.clear();
        const auto& store = MWBase::Environment::get().getWorld()->getStore().get<ESM::Skill>();
        for (const ESM::Skill& skill : store)
        {
            if (!skillSet.contains(skill.mId))
                mMiscSkills.push_back(skill.mId);
        }

        mUpdateSkillArea = true;
    }

    void ReviewDialog::addSeparator(MyGUI::IntCoord& coord1, MyGUI::IntCoord& coord2)
    {
        MyGUI::ImageBox* separator = mSkillView->createWidget<MyGUI::ImageBox>(
            "MW_HLine", MyGUI::IntCoord(10, coord1.top, coord1.width + coord2.width - 4, 18), MyGUI::Align::Default);
        separator->eventMouseWheel += MyGUI::newDelegate(this, &ReviewDialog::onMouseWheel);

        mSkillWidgets.push_back(separator);

        coord1.top += separator->getHeight();
        coord2.top += separator->getHeight();
    }

    MyGUI::Widget* ReviewDialog::addGroup(std::string_view label, MyGUI::IntCoord& coord1, MyGUI::IntCoord& coord2)
    {
        MyGUI::TextBox* groupWidget = mSkillView->createWidget<MyGUI::TextBox>("SandBrightText",
            MyGUI::IntCoord(0, coord1.top, coord1.width + coord2.width, coord1.height), MyGUI::Align::Default);
        groupWidget->eventMouseWheel += MyGUI::newDelegate(this, &ReviewDialog::onMouseWheel);
        groupWidget->setCaption(MyGUI::UString(label));
        mSkillWidgets.push_back(groupWidget);

        const int lineHeight = Settings::gui().mFontSize + 2;
        coord1.top += lineHeight;
        coord2.top += lineHeight;
        return groupWidget;
    }

    std::pair<MyGUI::TextBox*, MyGUI::TextBox*> ReviewDialog::addValueItem(std::string_view text,
        const std::string& value, const std::string& state, MyGUI::IntCoord& coord1, MyGUI::IntCoord& coord2)
    {
        MyGUI::TextBox* skillNameWidget;
        MyGUI::TextBox* skillValueWidget;

        skillNameWidget = mSkillView->createWidget<MyGUI::TextBox>("SandTextController", coord1, MyGUI::Align::Default);
        skillNameWidget->setCaption(MyGUI::UString(text));
        skillNameWidget->eventMouseWheel += MyGUI::newDelegate(this, &ReviewDialog::onMouseWheel);

        skillValueWidget
            = mSkillView->createWidget<MyGUI::TextBox>("SandTextRightController", coord2, MyGUI::Align::Default);
        skillValueWidget->setCaption(value);
        skillValueWidget->_setWidgetState(state);
        skillValueWidget->eventMouseWheel += MyGUI::newDelegate(this, &ReviewDialog::onMouseWheel);

        mSkillWidgets.push_back(skillNameWidget);
        mSkillWidgets.push_back(skillValueWidget);

        const int lineHeight = Settings::gui().mFontSize + 2;
        coord1.top += lineHeight;
        coord2.top += lineHeight;

        if (Settings::gui().mControllerMenus)
        {
            setControllerWidgetBaseState(skillNameWidget, "normal");
            setControllerWidgetBaseState(skillValueWidget, state);
        }

        return { skillNameWidget, skillValueWidget };
    }

    MyGUI::TextBox* ReviewDialog::addItem(const std::string& text, MyGUI::IntCoord& coord1, MyGUI::IntCoord& coord2)
    {
        MyGUI::TextBox* skillNameWidget;

        skillNameWidget = mSkillView->createWidget<MyGUI::TextBox>(
            "SandTextController", coord1 + MyGUI::IntSize(coord2.width, 0), MyGUI::Align::Default);
        skillNameWidget->setCaption(text);
        skillNameWidget->eventMouseWheel += MyGUI::newDelegate(this, &ReviewDialog::onMouseWheel);

        mSkillWidgets.push_back(skillNameWidget);

        const int lineHeight = Settings::gui().mFontSize + 2;
        coord1.top += lineHeight;
        coord2.top += lineHeight;

        return skillNameWidget;
    }

    MyGUI::Widget* ReviewDialog::addItem(const ESM::Spell* spell, MyGUI::IntCoord& coord1, MyGUI::IntCoord& coord2)
    {
        Widgets::MWSpellPtr widget = mSkillView->createWidget<Widgets::MWSpell>(
            "MW_StatName", coord1 + MyGUI::IntSize(coord2.width, 0), MyGUI::Align::Default);
        widget->setSpellId(spell->mId);
        widget->setUserString("ToolTipType", "Spell");
        widget->setUserString("Spell", spell->mId.serialize());
        widget->eventMouseWheel += MyGUI::newDelegate(this, &ReviewDialog::onMouseWheel);
        if (MyGUI::TextBox* nameWidget = getSpellTextWidget(widget))
            nameWidget->changeWidgetSkin("SandTextController");

        mSkillWidgets.push_back(widget);

        const int lineHeight = Settings::gui().mFontSize + 2;
        coord1.top += lineHeight;
        coord2.top += lineHeight;

        return widget;
    }

    void ReviewDialog::addSkills(const std::vector<ESM::RefId>& skills, const std::string& titleId,
        const std::string& titleDefault, MyGUI::IntCoord& coord1, MyGUI::IntCoord& coord2)
    {
        // Add a line separator if there are items above
        if (!mSkillWidgets.empty())
        {
            addSeparator(coord1, coord2);
        }

        MyGUI::Widget* groupHeader = addGroup(
            MWBase::Environment::get().getWindowManager()->getGameSettingString(titleId, titleDefault), coord1, coord2);

        const MWWorld::ESMStore& esmStore = *MWBase::Environment::get().getESMStore();
        for (const ESM::RefId& skillId : skills)
        {
            const ESM::Skill* skill = esmStore.get<ESM::Skill>().search(skillId);
            if (!skill) // Skip unknown skills
                continue;

            auto skillValue = mSkillValues.find(skill->mId);
            if (skillValue == mSkillValues.end())
            {
                Log(Debug::Error) << "Failed to update stats review window: can not find value for skill "
                                  << skill->mId;
                continue;
            }

            const MWMechanics::SkillValue& stat = skillValue->second;
            float base = stat.getBase();
            float modified = stat.getModified();

            std::string state = "normal";
            if (modified > base)
                state = "increased";
            else if (modified < base)
                state = "decreased";
            const int rowTop = coord1.top;
            auto widgets = addValueItem(
                skill->mName, MyGUI::utility::toString(static_cast<int>(modified)), state, coord1, coord2);

            mSkillWidgetMap[skill->mId] = widgets;

            const ESM::Attribute* attr
                = esmStore.get<ESM::Attribute>().find(ESM::Attribute::indexToRefId(skill->mData.mAttribute));
            const std::string attrName = attr ? MyGUI::TextIterator::toTagsString(attr->mName).asUTF8() : std::string();
            const std::string skillName = MyGUI::TextIterator::toTagsString(skill->mName).asUTF8();
            const std::string attributeCaption
                = attrName.empty() ? std::string() : "#{sGoverningAttribute}: " + attrName;

            for (MyGUI::TextBox* widget : { widgets.first, widgets.second })
            {
                if (!widget)
                    continue;
                widget->setUserString("ToolTipDynamic", "Stats");
                widget->setUserString("ToolTipType", "Layout");
                widget->setUserString("ToolTipLayout", "SkillToolTip");
                widget->setUserString("Caption_SkillName", skillName);
                widget->setUserString("Caption_SkillDescription", skill->mDescription);
                widget->setUserString("Caption_SkillAttribute", attributeCaption);
                widget->setUserString("ImageTexture_SkillImage", skill->mIcon);
                widget->setUserString("Range_SkillProgress", "100");
            }

            setSkillValue(skill->mId, stat);

            if (Settings::gui().mControllerMenus && widgets.first)
            {
                MyGUI::Widget* highlight = nullptr;
                if (useControllerSelectionHighlight())
                {
                    const MyGUI::IntCoord rowCoord(coord1.left, rowTop, coord1.width + coord2.width, coord1.height);
                    highlight = mSkillView->createWidget<MyGUI::Widget>(
                        "ControllerHighlight", rowCoord, MyGUI::Align::Default);
                    highlight->setNeedMouseFocus(false);
                    highlight->setDepth(1);
                    highlight->setVisible(false);
                    mSkillWidgets.push_back(highlight);
                }

                addControllerItem(
                    true, widgets.first, { widgets.first, widgets.second }, highlight, false, groupHeader);
            }
        }
    }

    void ReviewDialog::updateSkillArea()
    {
        if (!mSkillView)
            return;

        if (Settings::gui().mControllerMenus)
            resetControllerItems();

        for (MyGUI::Widget* skillWidget : mSkillWidgets)
        {
            MyGUI::Gui::getInstance().destroyWidget(skillWidget);
        }
        mSkillWidgets.clear();

        const int valueSize = 40;
        MyGUI::IntCoord coord1(10, 0, mSkillView->getWidth() - (10 + valueSize) - 24, 18);
        MyGUI::IntCoord coord2(coord1.left + coord1.width, coord1.top, valueSize, coord1.height);

        if (!mMajorSkills.empty())
            addSkills(mMajorSkills, "sSkillClassMajor", "Major Skills", coord1, coord2);

        if (!mMinorSkills.empty())
            addSkills(mMinorSkills, "sSkillClassMinor", "Minor Skills", coord1, coord2);

        if (!mMiscSkills.empty())
            addSkills(mMiscSkills, "sSkillClassMisc", "Misc Skills", coord1, coord2);

        // starting spells
        std::vector<ESM::RefId> spells;

        const ESM::Race* race = nullptr;
        if (!mRaceId.empty())
            race = MWBase::Environment::get().getESMStore()->get<ESM::Race>().find(mRaceId);

        std::map<ESM::RefId, MWMechanics::AttributeValue> attributes;
        for (const auto& [key, value] : mAttributeWidgets)
            attributes[key] = value->getAttributeValue();

        const bool hasRequiredAttributes = attributes.find(ESM::Attribute::Intelligence) != attributes.end()
            && attributes.find(ESM::Attribute::Willpower) != attributes.end()
            && attributes.find(ESM::Attribute::Luck) != attributes.end();

        if (hasRequiredAttributes)
        {
            std::vector<ESM::RefId> selectedSpells = MWMechanics::autoCalcPlayerSpells(mSkillValues, attributes, race);
            for (ESM::RefId& spellId : selectedSpells)
            {
                if (std::find(spells.begin(), spells.end(), spellId) == spells.end())
                    spells.push_back(spellId);
            }
        }

        if (race)
        {
            for (const ESM::RefId& spellId : race->mPowers.mList)
            {
                if (std::find(spells.begin(), spells.end(), spellId) == spells.end())
                    spells.push_back(spellId);
            }
        }

        if (!mBirthSignId.empty())
        {
            const ESM::BirthSign* sign
                = MWBase::Environment::get().getESMStore()->get<ESM::BirthSign>().find(mBirthSignId);
            if (sign)
            {
                for (const auto& spellId : sign->mPowers.mList)
                {
                    if (std::find(spells.begin(), spells.end(), spellId) == spells.end())
                        spells.push_back(spellId);
                }
            }
        }

        if (!mSkillWidgets.empty())
            addSeparator(coord1, coord2);
        MyGUI::Widget* abilityHeader
            = addGroup(MWBase::Environment::get().getWindowManager()->getGameSettingString("sTypeAbility", "Abilities"),
                coord1, coord2);
        for (auto& spellId : spells)
        {
            const ESM::Spell* spell = MWBase::Environment::get().getESMStore()->get<ESM::Spell>().find(spellId);
            if (!spell)
                continue;
            if (spell->mData.mType == ESM::Spell::ST_Ability)
            {
                const int rowTop = coord1.top;
                MyGUI::Widget* widget = addItem(spell, coord1, coord2);
                if (Settings::gui().mControllerMenus && widget)
                {
                    MyGUI::Widget* highlight = nullptr;
                    if (useControllerSelectionHighlight())
                    {
                        const MyGUI::IntCoord rowCoord(coord1.left, rowTop, coord1.width + coord2.width, coord1.height);
                        highlight = mSkillView->createWidget<MyGUI::Widget>(
                            "ControllerHighlight", rowCoord, MyGUI::Align::Default);
                        highlight->setNeedMouseFocus(false);
                        highlight->setDepth(1);
                        highlight->setVisible(false);
                        mSkillWidgets.push_back(highlight);
                    }
                    MyGUI::TextBox* nameWidget = getSpellTextWidget(widget->castType<Widgets::MWSpell>(false));
                    addControllerItem(true, widget, { nameWidget }, highlight, false, abilityHeader);
                }
            }
        }

        addSeparator(coord1, coord2);
        MyGUI::Widget* powerHeader
            = addGroup(MWBase::Environment::get().getWindowManager()->getGameSettingString("sTypePower", "Powers"),
                coord1, coord2);
        for (auto& spellId : spells)
        {
            const ESM::Spell* spell = MWBase::Environment::get().getESMStore()->get<ESM::Spell>().find(spellId);
            if (!spell)
                continue;
            if (spell->mData.mType == ESM::Spell::ST_Power)
            {
                const int rowTop = coord1.top;
                MyGUI::Widget* widget = addItem(spell, coord1, coord2);
                if (Settings::gui().mControllerMenus && widget)
                {
                    MyGUI::Widget* highlight = nullptr;
                    if (useControllerSelectionHighlight())
                    {
                        const MyGUI::IntCoord rowCoord(coord1.left, rowTop, coord1.width + coord2.width, coord1.height);
                        highlight = mSkillView->createWidget<MyGUI::Widget>(
                            "ControllerHighlight", rowCoord, MyGUI::Align::Default);
                        highlight->setNeedMouseFocus(false);
                        highlight->setDepth(1);
                        highlight->setVisible(false);
                        mSkillWidgets.push_back(highlight);
                    }
                    MyGUI::TextBox* nameWidget = getSpellTextWidget(widget->castType<Widgets::MWSpell>(false));
                    addControllerItem(true, widget, { nameWidget }, highlight, false, powerHeader);
                }
            }
        }

        addSeparator(coord1, coord2);
        MyGUI::Widget* spellHeader
            = addGroup(MWBase::Environment::get().getWindowManager()->getGameSettingString("sTypeSpell", "Spells"),
                coord1, coord2);
        for (auto& spellId : spells)
        {
            const ESM::Spell* spell = MWBase::Environment::get().getESMStore()->get<ESM::Spell>().find(spellId);
            if (!spell)
                continue;
            if (spell->mData.mType == ESM::Spell::ST_Spell)
            {
                const int rowTop = coord1.top;
                MyGUI::Widget* widget = addItem(spell, coord1, coord2);
                if (Settings::gui().mControllerMenus && widget)
                {
                    MyGUI::Widget* highlight = nullptr;
                    if (useControllerSelectionHighlight())
                    {
                        const MyGUI::IntCoord rowCoord(coord1.left, rowTop, coord1.width + coord2.width, coord1.height);
                        highlight = mSkillView->createWidget<MyGUI::Widget>(
                            "ControllerHighlight", rowCoord, MyGUI::Align::Default);
                        highlight->setNeedMouseFocus(false);
                        highlight->setDepth(1);
                        highlight->setVisible(false);
                        mSkillWidgets.push_back(highlight);
                    }
                    MyGUI::TextBox* nameWidget = getSpellTextWidget(widget->castType<Widgets::MWSpell>(false));
                    addControllerItem(true, widget, { nameWidget }, highlight, false, spellHeader);
                }
            }
        }

        // Canvas size must be expressed with VScroll disabled, otherwise MyGUI would expand the scroll area when the
        // scrollbar is hidden
        mSkillView->setVisibleVScroll(false);
        mSkillView->setCanvasSize(mSkillView->getWidth(), std::max(mSkillView->getHeight(), coord1.top));
        mSkillView->setVisibleVScroll(true);

        if (Settings::gui().mControllerMenus && mActiveControllerWindow && !mControllerItems.empty())
        {
            mControllerFocus = std::min(mControllerFocus, mControllerItems.size() - 1);
            updateControllerFocus(mControllerItems.size(), mControllerFocus);
            if (mControllerItems[mControllerFocus].mRightPane)
                mLastRightFocus = mControllerFocus;
            else
                mLastLeftFocus = mControllerFocus;
        }
    }

    // widget controls

    void ReviewDialog::onOkClicked(MyGUI::Widget* /*sender*/)
    {
        eventDone(this);
    }

    void ReviewDialog::onBackClicked(MyGUI::Widget* /*sender*/)
    {
        eventBack();
    }

    void ReviewDialog::onNameClicked(MyGUI::Widget* /*sender*/)
    {
        eventActivateDialog(NAME_DIALOG);
    }

    void ReviewDialog::onRaceClicked(MyGUI::Widget* /*sender*/)
    {
        eventActivateDialog(RACE_DIALOG);
    }

    void ReviewDialog::onClassClicked(MyGUI::Widget* /*sender*/)
    {
        eventActivateDialog(CLASS_DIALOG);
    }

    void ReviewDialog::onBirthSignClicked(MyGUI::Widget* /*sender*/)
    {
        eventActivateDialog(BIRTHSIGN_DIALOG);
    }

    void ReviewDialog::onMouseWheel(MyGUI::Widget* /*sender*/, int rel)
    {
        if (mSkillView->getViewOffset().top + rel * 0.3 > 0)
            mSkillView->setViewOffset(MyGUI::IntPoint(0, 0));
        else
            mSkillView->setViewOffset(
                MyGUI::IntPoint(0, static_cast<int>(mSkillView->getViewOffset().top + rel * 0.3)));
    }

    void ReviewDialog::resetControllerItems()
    {
        if (mActiveControllerWindow && !mControllerItems.empty())
            updateControllerFocus(mControllerFocus, mControllerItems.size());

        mControllerItems.clear();
        mControllerLeftItems.clear();
        mControllerRightItems.clear();
        mControllerBaseStates.clear();

        if (!Settings::gui().mControllerMenus)
            return;

        if (mNameButton || mNameWidget)
        {
            MyGUI::Widget* tooltip = mNameButton ? static_cast<MyGUI::Widget*>(mNameButton) : mNameWidget;
            addControllerItem(false, tooltip, { mNameWidget }, mNameHighlight, false);
        }
        if (mRaceButton || mRaceWidget)
        {
            MyGUI::Widget* tooltip = mRaceButton ? static_cast<MyGUI::Widget*>(mRaceButton) : mRaceWidget;
            addControllerItem(false, tooltip, { mRaceWidget }, mRaceHighlight, false);
        }
        if (mClassButton || mClassWidget)
        {
            MyGUI::Widget* tooltip = mClassButton ? static_cast<MyGUI::Widget*>(mClassButton) : mClassWidget;
            addControllerItem(false, tooltip, { mClassWidget }, mClassHighlight, false);
        }
        if (mBirthSignButton || mBirthSignWidget)
        {
            MyGUI::Widget* tooltip
                = mBirthSignButton ? static_cast<MyGUI::Widget*>(mBirthSignButton) : mBirthSignWidget;
            addControllerItem(false, tooltip, { mBirthSignWidget }, mBirthSignHighlight, false);
        }

        if (mHealth)
        {
            const auto [nameWidget, valueWidget] = getDynamicStatTextWidgets(mHealth);
            addControllerItem(false, mHealth, { nameWidget, valueWidget });
        }
        if (mMagicka)
        {
            const auto [nameWidget, valueWidget] = getDynamicStatTextWidgets(mMagicka);
            addControllerItem(false, mMagicka, { nameWidget, valueWidget });
        }
        if (mFatigue)
        {
            const auto [nameWidget, valueWidget] = getDynamicStatTextWidgets(mFatigue);
            addControllerItem(false, mFatigue, { nameWidget, valueWidget });
        }

        for (size_t i = 0; i < mAttributeList.size(); ++i)
        {
            Widgets::MWAttribute* attribute = mAttributeList[i];
            MyGUI::Widget* highlight = (i < mAttributeHighlights.size()) ? mAttributeHighlights[i] : nullptr;
            const auto [nameWidget, valueWidget] = getAttributeTextWidgets(attribute);
            addControllerItem(false, attribute, { nameWidget, valueWidget }, highlight, false);
        }
    }

    void ReviewDialog::addControllerItem(bool rightPane, MyGUI::Widget* tooltipWidget,
        const std::initializer_list<MyGUI::TextBox*>& textWidgets, MyGUI::Widget* highlight,
        bool highlightFollowsTooltip, MyGUI::Widget* groupHeader)
    {
        addControllerItem(rightPane, tooltipWidget,
            std::vector<MyGUI::TextBox*>(textWidgets.begin(), textWidgets.end()), highlight, highlightFollowsTooltip,
            groupHeader);
    }

    void ReviewDialog::addControllerItem(bool rightPane, MyGUI::Widget* tooltipWidget,
        const std::vector<MyGUI::TextBox*>& textWidgets, MyGUI::Widget* highlight, bool highlightFollowsTooltip,
        MyGUI::Widget* groupHeader)
    {
        ControllerItem item;
        item.mTooltipWidget = tooltipWidget;
        item.mHighlight = highlight;
        item.mHighlightFollowsTooltip = highlight == nullptr ? true : highlightFollowsTooltip;
        item.mGroupHeader = groupHeader;
        item.mRightPane = rightPane;
        item.mTextWidgets = textWidgets;

        const size_t index = mControllerItems.size();
        mControllerItems.push_back(item);
        (rightPane ? mControllerRightItems : mControllerLeftItems).push_back(index);

        for (MyGUI::TextBox* widget : item.mTextWidgets)
        {
            if (widget == nullptr)
                continue;
            if (mControllerBaseStates.find(widget) == mControllerBaseStates.end())
                mControllerBaseStates.emplace(widget, "normal");
        }

        if (item.mHighlight == nullptr && useControllerSelectionHighlight() && item.mTooltipWidget != nullptr)
        {
            if (MyGUI::Widget* parent = item.mTooltipWidget->getParent())
            {
                item.mHighlight = createControllerHighlight(parent, item.mTooltipWidget->getCoord());
                item.mHighlightFollowsTooltip = true;
                mControllerItems[index].mHighlight = item.mHighlight;
                mControllerItems[index].mHighlightFollowsTooltip = item.mHighlightFollowsTooltip;
            }
        }

        if (item.mTooltipWidget != nullptr)
            item.mTooltipWidget->setNeedMouseFocus(true);
    }

    void ReviewDialog::setControllerWidgetBaseState(MyGUI::TextBox* widget, std::string_view state)
    {
        if (widget == nullptr)
            return;

        auto it = mControllerBaseStates.find(widget);
        if (it != mControllerBaseStates.end())
            it->second = std::string(state);
        else
            mControllerBaseStates.emplace(widget, std::string(state));

        if (useControllerSelectionHighlight() && isControllerWidgetFocused(widget))
            widget->_setWidgetState("highlighted");
    }

    void ReviewDialog::updateControllerFocus(size_t prevFocus, size_t newFocus)
    {
        MWBase::WindowManager* winMgr = MWBase::Environment::get().getWindowManager();
        if (Settings::gui().mControllerMenus)
            winMgr->setCursorVisible(false);

        if (prevFocus < mControllerItems.size())
        {
            if (MyGUI::Widget* tooltip = mControllerItems[prevFocus].mTooltipWidget)
            {
                if (auto* button = tooltip->castType<MyGUI::Button>(false))
                    button->setStateSelected(false);
            }
            for (MyGUI::TextBox* widget : mControllerItems[prevFocus].mTextWidgets)
            {
                if (widget == nullptr)
                    continue;
                const auto it = mControllerBaseStates.find(widget);
                widget->_setWidgetState(it != mControllerBaseStates.end() ? it->second : "normal");
            }

            if (MyGUI::Widget* highlight = mControllerItems[prevFocus].mHighlight)
                highlight->setVisible(false);
        }

        if (!mActiveControllerWindow || newFocus >= mControllerItems.size())
            return;

        const ControllerItem& item = mControllerItems[newFocus];

        if (useControllerSelectionHighlight())
        {
            if (MyGUI::Widget* tooltip = item.mTooltipWidget)
            {
                if (auto* button = tooltip->castType<MyGUI::Button>(false))
                    button->setStateSelected(true);
            }
            for (MyGUI::TextBox* widget : item.mTextWidgets)
            {
                if (widget != nullptr)
                {
                    widget->_setWidgetState("highlighted");
                }
            }

            if (item.mHighlight != nullptr)
            {
                if (item.mHighlightFollowsTooltip && item.mTooltipWidget != nullptr)
                    item.mHighlight->setCoord(item.mTooltipWidget->getCoord());
                item.mHighlight->setVisible(true);
            }
        }

        if (item.mRightPane && item.mTooltipWidget != nullptr)
            scrollSkillViewToWidget(item.mTooltipWidget, item.mGroupHeader);

        winMgr->restoreControllerTooltips();

        if (Settings::gui().mControllerMenus)
            winMgr->setCursorVisible(false);
        if (item.mTooltipWidget != nullptr)
        {
            MWBase::Environment::get().getInputManager()->warpMouseToWidget(item.mTooltipWidget);
            const MyGUI::IntCoord coord = item.mTooltipWidget->getAbsoluteCoord();
            const int mouseX = coord.left + coord.width / 2;
            const int mouseY = coord.top + coord.height / 4;
            MyGUI::InputManager::getInstance().injectMouseMove(mouseX, mouseY, 0);
            MyGUI::InputManager::getInstance().injectMouseMove(mouseX, mouseY, 0);
        }
    }

    bool ReviewDialog::isControllerWidgetFocused(MyGUI::TextBox* widget) const
    {
        if (widget == nullptr || mControllerItems.empty())
            return false;
        if (mControllerFocus >= mControllerItems.size())
            return false;
        const ControllerItem& item = mControllerItems[mControllerFocus];
        return std::find(item.mTextWidgets.begin(), item.mTextWidgets.end(), widget) != item.mTextWidgets.end();
    }

    void ReviewDialog::scrollSkillViewToWidget(MyGUI::Widget* widget, MyGUI::Widget* headerWidget)
    {
        if (widget == nullptr || mSkillView == nullptr)
            return;

        const MyGUI::IntCoord itemCoord = widget->getAbsoluteCoord();
        const MyGUI::IntCoord viewCoord = mSkillView->getAbsoluteCoord();
        const MyGUI::IntCoord headerCoord = headerWidget ? headerWidget->getAbsoluteCoord() : itemCoord;

        int newOffset = mSkillView->getViewOffset().top;
        if (headerWidget != nullptr && headerCoord.top < viewCoord.top)
        {
            newOffset += viewCoord.top - headerCoord.top;
        }
        else if (itemCoord.top < viewCoord.top)
        {
            newOffset += viewCoord.top - itemCoord.top;
        }
        else if (itemCoord.top + itemCoord.height > viewCoord.top + viewCoord.height)
        {
            newOffset -= (itemCoord.top + itemCoord.height) - (viewCoord.top + viewCoord.height);
        }
        else
            return;

        const int minOffset = std::min(0, mSkillView->getHeight() - mSkillView->getCanvasSize().height);
        newOffset = std::clamp(newOffset, minOffset, 0);
        mSkillView->setViewOffset(MyGUI::IntPoint(0, newOffset));
    }

    size_t ReviewDialog::findClosestControllerItem(bool rightPane, MyGUI::Widget* widget) const
    {
        const std::vector<size_t>& list = rightPane ? mControllerRightItems : mControllerLeftItems;
        if (list.empty() || widget == nullptr)
            return mControllerFocus;

        const int targetTop = widget->getAbsoluteCoord().top;
        size_t bestIndex = list.front();
        int bestDistance = std::numeric_limits<int>::max();

        for (size_t index : list)
        {
            const ControllerItem& item = mControllerItems[index];
            if (item.mTooltipWidget == nullptr)
                continue;

            const int itemTop = item.mTooltipWidget->getAbsoluteCoord().top;
            const int distance = std::abs(itemTop - targetTop);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                bestIndex = index;
            }
        }

        return bestIndex;
    }

    void ReviewDialog::setControllerFocus(size_t newFocus)
    {
        if (newFocus >= mControllerItems.size())
            return;

        const size_t prevFocus = mControllerFocus;
        mControllerFocus = newFocus;
        updateControllerFocus(prevFocus, mControllerFocus);
        if (mControllerItems[mControllerFocus].mRightPane)
            mLastRightFocus = mControllerFocus;
        else
            mLastLeftFocus = mControllerFocus;
    }

    void ReviewDialog::setActiveControllerWindow(bool active)
    {
        WindowBase::setActiveControllerWindow(active);

        if (!Settings::gui().mControllerMenus)
            return;

        if (active && !mControllerItems.empty())
        {
            mControllerFocus = std::min(mControllerFocus, mControllerItems.size() - 1);
            updateControllerFocus(mControllerItems.size(), mControllerFocus);
        }
        else if (!active && !mControllerItems.empty())
        {
            updateControllerFocus(mControllerFocus, mControllerItems.size());
        }
    }

    MyGUI::Widget* ReviewDialog::getControllerFocusTooltipWidget() const
    {
        if (!mActiveControllerWindow || mControllerItems.empty())
            return nullptr;
        if (mControllerFocus >= mControllerItems.size())
            return nullptr;
        return mControllerItems[mControllerFocus].mTooltipWidget;
    }

    bool ReviewDialog::onControllerButtonEvent(const SDL_ControllerButtonEvent& arg)
    {
        if (mControllerItems.empty())
            return true;

        if (arg.button == SDL_CONTROLLER_BUTTON_A)
        {
            MyGUI::Widget* focusWidget = mControllerItems[mControllerFocus].mTooltipWidget;
            if (focusWidget == mNameButton)
                onNameClicked(mNameButton);
            else if (focusWidget == mRaceButton)
                onRaceClicked(mRaceButton);
            else if (focusWidget == mClassButton)
                onClassClicked(mClassButton);
            else if (focusWidget == mBirthSignButton)
                onBirthSignClicked(mBirthSignButton);
            return true;
        }
        else if (arg.button == SDL_CONTROLLER_BUTTON_B)
        {
            onBackClicked(nullptr);
        }
        else if (arg.button == SDL_CONTROLLER_BUTTON_X)
        {
            onOkClicked(nullptr);
        }
        else if (arg.button == SDL_CONTROLLER_BUTTON_DPAD_UP || arg.button == SDL_CONTROLLER_BUTTON_DPAD_DOWN)
        {
            const bool rightPane = mControllerItems[mControllerFocus].mRightPane;
            MWBase::Environment::get().getWindowManager()->restoreControllerTooltips();
            const std::vector<size_t>& list = rightPane ? mControllerRightItems : mControllerLeftItems;
            if (list.empty())
                return true;

            const auto it = std::find(list.begin(), list.end(), mControllerFocus);
            size_t index = 0;
            if (it != list.end())
                index = static_cast<size_t>(std::distance(list.begin(), it));

            const int delta = (arg.button == SDL_CONTROLLER_BUTTON_DPAD_UP) ? -1 : 1;
            index = wrap(index, list.size(), delta);
            setControllerFocus(list[index]);
            if (rightPane)
                mLastLeftFocus = mControllerItems.size();
            else
                mLastRightFocus = mControllerItems.size();
        }
        else if (arg.button == SDL_CONTROLLER_BUTTON_DPAD_LEFT || arg.button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT)
        {
            const bool targetRightPane = (arg.button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
            MWBase::Environment::get().getWindowManager()->restoreControllerTooltips();
            if (targetRightPane == mControllerItems[mControllerFocus].mRightPane)
                return true;

            const std::vector<size_t>& list = targetRightPane ? mControllerRightItems : mControllerLeftItems;
            size_t newFocus = mControllerItems.size();
            if (!list.empty())
            {
                const size_t remembered = targetRightPane ? mLastRightFocus : mLastLeftFocus;
                if (remembered < mControllerItems.size() && mControllerItems[remembered].mRightPane == targetRightPane
                    && std::find(list.begin(), list.end(), remembered) != list.end())
                {
                    newFocus = remembered;
                }
                else
                {
                    newFocus
                        = findClosestControllerItem(targetRightPane, mControllerItems[mControllerFocus].mTooltipWidget);
                }
            }
            if (newFocus < mControllerItems.size())
                setControllerFocus(newFocus);
        }

        return true;
    }
}
