#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "NativePluginEditor.h"

namespace lah::ui {

// One editor per rack position: opening another channel never retargets an
// existing editor while the operator is adjusting its controls.
class NativePluginWindow final : public juce::DocumentWindow, private juce::Timer {
public:
    NativePluginWindow(audio::LiveAudioEngine& engine,int channel,int slot,bool output,bool presentOnDesktop=true)
        :DocumentWindow("Plugin",juce::Colour(0xff0c141d),DocumentWindow::closeButton,presentOnDesktop),
         audioEngine(engine),channelIndex(juce::jlimit(0,31,channel)),
         slotIndex(juce::jlimit(0,3,slot)),outputDomain(output)
    {
        setUsingNativeTitleBar(true);
        setContentOwned(new NativePluginEditor(engine,channelIndex,slotIndex,outputDomain,presentOnDesktop),true);
        setResizable(true,false);
        setResizeLimits(780,540,1500,1000);
        const auto* display=juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
        const auto area=display?display->userBounds.toNearestInt():juce::Rectangle<int>(0,0,1280,800);
        centreWithSize(std::max(780,std::min(940,area.getWidth()-40)),
                       std::max(540,std::min(650,area.getHeight()-70)));
        updateTitle();
        startTimerHz(5);
    }
    bool matches(int channel,int slot,bool output)const noexcept {
        return channelIndex==channel && slotIndex==slot && outputDomain==output;
    }
    void showEditor() {
        if(auto* editor=getEditor())editor->refreshControls();
        updateTitle();setVisible(true);setMinimised(false);toFront(true);
    }
    NativePluginEditor* getEditor() const noexcept {
        return dynamic_cast<NativePluginEditor*>(getContentComponent());
    }
    void closeButtonPressed()override {setVisible(false);}
private:
    void timerCallback()override {if(isVisible())updateTitle();}
    void updateTitle() {
        const auto& controls=outputDomain?audioEngine.outputBuiltinControls():audioEngine.builtinControls();
        const int type=controls.slots[static_cast<std::size_t>(channelIndex)][static_cast<std::size_t>(slotIndex)].type.load();
        setName(juce::String::fromUTF8(builtin::metadata(type).name)+"  |  "+
            (outputDomain?"USB OUT ":"CH ")+juce::String(channelIndex+1)+" / INSERT "+juce::String(slotIndex+1));
    }
    audio::LiveAudioEngine& audioEngine;
    int channelIndex,slotIndex;
    bool outputDomain;
};

} // namespace lah::ui
