#include "textcolours.hpp"

#include <MyGUI_LanguageManager.h>

#include <string>

namespace MWGui
{
    MyGUI::Colour getTextColour(const std::string& type)
    {
        return MyGUI::Colour::parse(MyGUI::LanguageManager::getInstance().replaceTags("#{fontcolour=" + type + "}"));
    }

    void TextColours::loadColours()
    {
        header = getTextColour("header");
        normal = getTextColour("normal");
        notify = getTextColour("notify");

        bigHeader = getTextColour("big_header");
        bigNormal = getTextColour("big_normal");
        bigNormalOver = getTextColour("big_normal_over");
        bigNormalPressed = getTextColour("big_normal_pressed");
        bigNotify = getTextColour("big_notify");
        bigLink = getTextColour("big_link");
        bigLinkOver = getTextColour("big_link_over");
        bigLinkPressed = getTextColour("big_link_pressed");

        link = getTextColour("link");
        linkOver = getTextColour("link_over");
        linkPressed = getTextColour("link_pressed");

        answer = getTextColour("answer");
        answerOver = getTextColour("answer_over");
        answerPressed = getTextColour("answer_pressed");

        bigAnswer = getTextColour("big_answer");
        bigAnswerOver = getTextColour("big_answer_over");
        bigAnswerPressed = getTextColour("big_answer_pressed");

        journalLink = getTextColour("journal_link");
        journalLinkOver = getTextColour("journal_link_over");
        journalLinkPressed = getTextColour("journal_link_pressed");

        journalTopic = getTextColour("journal_topic");
        journalTopicOver = getTextColour("journal_topic_over");
        journalTopicPressed = getTextColour("journal_topic_pressed");
    }
}
