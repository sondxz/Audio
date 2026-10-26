#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../audio/LiveAudioEngine.h"
#include "RackOperations.h"
#include <array>
#include <functional>
#include <memory>

namespace lah::ui {
class RackInsertButton final:public juce::TextButton {
public:
    std::function<void()> onContextMenu;
    void mouseDown(const juce::MouseEvent& event)override {
        if(event.mods.isPopupMenu()){if(onContextMenu)onContextMenu();return;}
        juce::TextButton::mouseDown(event);
    }
};
class MixerPanel final: public juce::Component {
public:
    explicit MixerPanel(audio::LiveAudioEngine& e);
    ~MixerPanel() override;
    void resized() override;
    void refreshControls();
    void setUsbChannelCount(int count);
    juce::StringArray getChannelNames()const{return channelNames;}
    void setChannelNames(const juce::StringArray& names);
    void setChannelViewCount(int count);
    int getChannelViewCount()const noexcept{return visibleStripCount;}
    void resetPeakHolds();
    // Offline control validation only; refuses an active audio device.
    bool runWorkflowValidation(juce::String& error);
    void setMeterLevels(const float* inputLevels,int inputCount,const float* outputLevels,int outputCount);
    std::function<void(int,int,bool)> onOpenBuiltin;
    std::function<void()> onToggleAudio,onPanic;
    void paint(juce::Graphics&) override;
private:
    struct Param { juce::Label label;juce::Slider slider; };
    void selectedChannels();
    void refreshConsole();
    void rebuildStripBank();
    void showInsertMenu(int channel,int slot,bool output,juce::Component& anchor);
    bool performInsertAction(int channel,int slot,bool output,int action);
    void reloadMatrix();
    void setStripView(bool showStrips);
    void addParameter(Param& p,const juce::String& name,double lo,double hi,double step,
                      std::function<float()> getter,std::function<void(float)> setter);
    audio::LiveAudioEngine& engine;
    dsp::Controls& control;
    class ChannelStrip;
    std::array<std::unique_ptr<ChannelStrip>,16> channelStrips;
    juce::Viewport stripsViewport;
    juce::Component stripsContent;
    juce::StringArray channelNames;
    RackOperations rackOperations;
    juce::TextButton stripViewButton {"MIXER"},detailViewButton {"DSP / ROUTING"};
    juce::TextButton eightView {"8 CH"},sixteenView {"16 CH"},resetPeaks {"RESET PEAKS"};
    juce::Label chainTitle;
    std::array<RackInsertButton,4> chainOverview;
    juce::ComboBox stripBank;
    juce::Label bankLabel,masterTitle,insertNote,audioState;
    juce::TextButton audioPower {"AUDIO"},panic {"PANIC / MUTE"};
    juce::TextButton outputPeakReset {"PEAK"};
    std::array<RackInsertButton,4> outputInserts;
    std::array<juce::TextButton,4> outputBypass;
    Param masterGain,masterDelay;
    juce::Rectangle<int> masterCard,masterMeter;
    float selectedOutputPeak=-1.0f;
    int meterOutputCount=0;
    std::array<float,32> outputPeaks {};
    std::array<float,32> inputPeakHolds {},outputPeakHolds {};
    std::array<bool,32> inputClips {},outputClips {};
    bool stripView=true;
    int stripPage=0,usbChannelCount=8,visibleStripCount=8;
    int in=0,out=0,inPage=0,outPage=0;
    juce::Label inputLabel,outputLabel,routeLabel,noteLabel,matLabel;
    juce::Label inGroupLabel,outGroupLabel,selectedRoute,matrixHint;
    std::array<juce::Label,8> matrixInputs,matrixOutputs;
    juce::Rectangle<int> inputCard,outputCard,matrixCard;
    juce::ComboBox inSelect,outSelect,inGroup,outGroup;
    std::array<Param,8> inputParams;
    std::array<Param,4> outputParams;
    Param routeGain;
    Param pan;
    juce::TextButton muteInput {"INPUT MUTE OFF"};
    juce::TextButton polarity {"Polarity: +"};
    std::array<juce::TextButton,64> matrixButtons;
};
} // namespace lah::ui
