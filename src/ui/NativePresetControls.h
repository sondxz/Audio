#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../audio/LiveAudioEngine.h"
#include "../dsp/BuiltinFactoryPresets.h"
#include <functional>
#include <memory>

namespace lah::ui {
// Compact header for ONE editor target. Construction and factory/A/B operations
// do no disk I/O. diskEnabled gates both choosers; even the AppData path is only
// resolved after the user clicks SAVE or LOAD.
class NativePresetControls final : public juce::Component {
public:
    NativePresetControls(audio::LiveAudioEngine&,int channel,int slot,bool output,bool diskEnabled=false);
    ~NativePresetControls() override;
    void reloadFromSlot(); // refresh indicators; never overwrite stored A/B
    void rebindToCurrentSlot(); // intentional reset/new incarnation: recapture A=B
    bool runValidation(juce::String& error); // muted offline validation, no disk
    std::function<void()> onApplied;
    void resized()override;
    void paint(juce::Graphics&)override;
private:
    builtin::Controls& rack()const noexcept;
    void applyFactory();
    void storeBank(int);
    void recallBank(int);
    void result(builtin::PresetResult,const juce::String&);
    void setStatus(juce::String,bool failure=false);
    void chooseFile(bool save);
    juce::String targetText()const;
    audio::LiveAudioEngine& engine;
    const int channelIndex,slotIndex;
    const bool outputDomain,diskEnabled;
    std::unique_ptr<builtin::LivePresetState> comparison;
    juce::ComboBox factorySelect;
    juce::TextButton storeA{"STORE A"},selectA{"A"},storeB{"STORE B"},selectB{"B"},saveButton{"SAVE"},loadButton{"LOAD"};
    juce::Label status;
    std::unique_ptr<juce::FileChooser> chooser;
    bool refreshing=false,statusFailure=false;
    juce::String statusText;
};
} // namespace lah::ui
