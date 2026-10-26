#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace lah::ui {
class StudioLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    StudioLookAndFeel(){
        const juce::Colour text(0xffe6edf3),muted(0xff98a8b8),surface(0xff1b232d),edge(0xff34414f),accent(0xff2bd7b4);
        setDefaultSansSerifTypefaceName("Segoe UI");
        setColour(juce::TextButton::buttonColourId,surface);
        setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xff173e39));
        setColour(juce::TextButton::textColourOffId,text);setColour(juce::TextButton::textColourOnId,accent);
        setColour(juce::ComboBox::backgroundColourId,juce::Colour(0xff151d26));
        setColour(juce::ComboBox::textColourId,text);setColour(juce::ComboBox::outlineColourId,edge);
        setColour(juce::ComboBox::arrowColourId,muted);
        setColour(juce::TextEditor::backgroundColourId,juce::Colour(0xff111922));
        setColour(juce::TextEditor::textColourId,text);setColour(juce::TextEditor::outlineColourId,edge);
        setColour(juce::TextEditor::focusedOutlineColourId,accent);
        setColour(juce::Slider::backgroundColourId,juce::Colour(0xff2a3643));
        setColour(juce::Slider::thumbColourId,accent);setColour(juce::Slider::trackColourId,accent);
        setColour(juce::Slider::textBoxTextColourId,text);
        setColour(juce::Slider::textBoxBackgroundColourId,juce::Colour(0xff111922));
        setColour(juce::Slider::textBoxOutlineColourId,edge);
        setColour(juce::Label::textColourId,text);
        setColour(juce::ScrollBar::thumbColourId,juce::Colour(0xff425365));
        setColour(juce::ScrollBar::backgroundColourId,juce::Colours::transparentBlack);
        setColour(juce::PopupMenu::backgroundColourId,juce::Colour(0xff18212b));
        setColour(juce::PopupMenu::textColourId,text);setColour(juce::PopupMenu::highlightedBackgroundColourId,juce::Colour(0xff204c45));
        setColour(juce::ToggleButton::textColourId,text);setColour(juce::ToggleButton::tickColourId,accent);
    }
    juce::Font getTextButtonFont(juce::TextButton& button,int height) override {
        if(button.getProperties().contains("navigation"))return juce::FontOptions(14.5f);
        return juce::FontOptions(height<=25?12.5f:14.0f);
    }
    juce::Font getLabelFont(juce::Label& label) override {
        return label.getFont().withHeight(std::max(13.0f,label.getFont().getHeight()));
    }
    juce::Font getComboBoxFont(juce::ComboBox&) override {return juce::FontOptions(14.0f);}
    juce::Font getPopupMenuFont() override {return juce::FontOptions(14.0f);}
    void drawButtonBackground(juce::Graphics& g,juce::Button& button,const juce::Colour& colour,bool over,bool down) override {
        auto bounds=button.getLocalBounds().toFloat().reduced(.5f);
        if(button.getProperties().contains("navigation")){
            const bool selected=button.getToggleState();
            if(selected || over){g.setColour(selected?juce::Colour(0xff193a35):juce::Colour(0xff1c2732));g.fillRoundedRectangle(bounds,6);}
            if(selected){g.setColour(juce::Colour(0xff2bd7b4));g.fillRoundedRectangle(bounds.getX()+1,bounds.getY()+8,3,bounds.getHeight()-16,1.5f);}
            return;
        }
        g.setColour(colour.withMultipliedBrightness(down?.8f:over?1.2f:1.0f).withMultipliedAlpha(button.isEnabled()?1.0f:.4f));
        g.fillRoundedRectangle(bounds,6);
        g.setColour(juce::Colour(0xff5a6c7e).withAlpha(button.isEnabled()?.36f:.16f));g.drawRoundedRectangle(bounds,6,1);
    }
    void drawButtonText(juce::Graphics& g,juce::TextButton& button,bool,bool) override {
        g.setFont(getTextButtonFont(button,button.getHeight()));
        g.setColour(button.findColour(button.getToggleState()?juce::TextButton::textColourOnId:juce::TextButton::textColourOffId).withMultipliedAlpha(button.isEnabled()?1.0f:.4f));
        const bool nav=button.getProperties().contains("navigation");
        g.drawFittedText(button.getButtonText(),button.getLocalBounds().reduced(nav?16:7,3),nav?juce::Justification::centredLeft:juce::Justification::centred,1,.9f);
    }
    void drawComboBox(juce::Graphics& g,int width,int height,bool,int,int,int,int,juce::ComboBox& box) override {
        const auto bounds=juce::Rectangle<float>(.5f,.5f,static_cast<float>(width)-1,static_cast<float>(height)-1);
        g.setColour(box.findColour(juce::ComboBox::backgroundColourId));g.fillRoundedRectangle(bounds,5);
        g.setColour(box.findColour(juce::ComboBox::outlineColourId));g.drawRoundedRectangle(bounds,5,1);
        const auto x=static_cast<float>(width)-16,y=static_cast<float>(height)*.5f;
        juce::Path chevron;chevron.startNewSubPath(x-4,y-2);chevron.lineTo(x,y+2);chevron.lineTo(x+4,y-2);
        g.setColour(box.findColour(juce::ComboBox::arrowColourId));g.strokePath(chevron,juce::PathStrokeType(1.5f));
    }
    void positionComboBoxText(juce::ComboBox& box,juce::Label& label) override {
        label.setBounds(10,1,std::max(1,box.getWidth()-34),std::max(1,box.getHeight()-2));label.setFont(getComboBoxFont(box));
    }
};
}
