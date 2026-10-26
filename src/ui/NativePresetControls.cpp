#include "NativePresetControls.h"
#include "SessionState.h"

namespace lah::ui {
NativePresetControls::NativePresetControls(audio::LiveAudioEngine& audio,int channel,int slot,bool output,bool allowDisk)
    :engine(audio),channelIndex(channel),slotIndex(slot),outputDomain(output),diskEnabled(allowDisk),
     comparison(std::make_unique<builtin::LivePresetState>(rack(),channel,slot)) {
    setName("native-preset-controls");
    addAndMakeVisible(factorySelect);addAndMakeVisible(status);
    factorySelect.setName("native-factory-presets");factorySelect.setTextWhenNothingSelected("Factory starting points");
    factorySelect.setTooltip("Three starting points designed for these native algorithms: Default and two gentle suggestions. Not vendor presets. Applying preserves ON/BYPASS; feedback presets disarm automatic cuts.");
    factorySelect.onChange=[this]{if(!refreshing)applyFactory();};
    const std::array<juce::TextButton*,6> buttons{{&storeA,&selectA,&storeB,&selectB,&saveButton,&loadButton}};
    const std::array<const char*,6> names{{"native-store-a","native-recall-a","native-store-b","native-recall-b","native-preset-save","native-preset-load"}};
    for(std::size_t index=0;index<buttons.size();++index){auto* button=buttons[index];addAndMakeVisible(*button);button->setName(names[index]);
        button->setColour(juce::TextButton::buttonColourId,juce::Colour(0xff203143));button->setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xff23647a));
        button->setColour(juce::TextButton::textColourOffId,juce::Colour(0xffe8f3fa));button->setColour(juce::TextButton::textColourOnId,juce::Colours::white);}
    storeA.onClick=[this]{storeBank(0);};selectA.onClick=[this]{recallBank(0);};
    storeB.onClick=[this]{storeBank(1);};selectB.onClick=[this]{recallBank(1);};
    saveButton.onClick=[this]{chooseFile(true);};loadButton.onClick=[this]{chooseFile(false);};
    storeA.setTooltip("Capture current full plugin settings into local A without changing sound.");selectA.setTooltip("Recall A parameters to this slot, preserving ON/BYPASS.");
    storeB.setTooltip("Capture current full plugin settings into local B without changing sound.");selectB.setTooltip("Recall B parameters to this slot, preserving ON/BYPASS.");
    saveButton.setTooltip(diskEnabled?"Export this native plugin's full settings as XML.":"File access is disabled for this offline editor.");
    loadButton.setTooltip(diskEnabled?"Load a single preset of the SAME plugin type. ON/BYPASS stays unchanged; other slots are untouched.":"File access is disabled for this offline editor.");
    status.setFont(juce::FontOptions(10));status.setJustificationType(juce::Justification::centredLeft);
    rebindToCurrentSlot();
}
NativePresetControls::~NativePresetControls()=default;
builtin::Controls& NativePresetControls::rack()const noexcept{return outputDomain?engine.outputBuiltinControls():engine.builtinControls();}
juce::String NativePresetControls::targetText()const{return juce::String(outputDomain?"USB OUT ":"INPUT ")+juce::String(channelIndex+1)+" / INSERT "+juce::String(slotIndex+1);}
void NativePresetControls::rebindToCurrentSlot(){
    comparison->rebind();refreshing=true;factorySelect.clear(juce::dontSendNotification);
    if(comparison->valid()){const auto presets=builtin::factoryPresets(comparison->module());for(int index=0;index<3;++index)factorySelect.addItem(presets[static_cast<std::size_t>(index)].name,index+1);}
    factorySelect.setSelectedId(0,juce::dontSendNotification);refreshing=false;
    setStatus("A = B: opening settings");reloadFromSlot();
}
void NativePresetControls::reloadFromSlot(){
    const bool valid=comparison->valid();factorySelect.setEnabled(valid);storeA.setEnabled(valid);storeB.setEnabled(valid);selectA.setEnabled(valid);selectB.setEnabled(valid);
    saveButton.setEnabled(valid&&diskEnabled);loadButton.setEnabled(valid&&diskEnabled);
    selectA.setToggleState(valid&&comparison->matchesBank(0),juce::dontSendNotification);
    selectB.setToggleState(valid&&comparison->matchesBank(1),juce::dontSendNotification);
    if(!valid)setStatus("Slot changed - reopen editor",true);
    else if(!statusFailure){const bool a=comparison->matchesBank(0),b=comparison->matchesBank(1);
        status.setText(a&&b?"A = B":a?"A active":b?"B active":"Edited / custom",juce::dontSendNotification);}
}
void NativePresetControls::setStatus(juce::String message,bool failure){
    statusText=std::move(message);statusFailure=failure;status.setText(statusText,juce::dontSendNotification);
    status.setColour(juce::Label::textColourId,juce::Colour(failure?0xffffa171:0xff8fb7c7));
    status.setTooltip(targetText()+" | "+statusText);factorySelect.setTooltip("Our own native starting presets. "+targetText()+" | "+statusText);
}
void NativePresetControls::result(builtin::PresetResult applied,const juce::String& message){
    switch(applied){
        case builtin::PresetResult::Applied:setStatus(message);factorySelect.setSelectedId(0,juce::dontSendNotification);if(onApplied)onApplied();break;
        case builtin::PresetResult::WrongModule:setStatus("Preset belongs to another plugin",true);break;
        case builtin::PresetResult::StaleTarget:setStatus("Slot changed - reopen editor",true);break;
        default:setStatus("No valid preset settings",true);break;
    }
    reloadFromSlot();
}
void NativePresetControls::applyFactory(){const int index=factorySelect.getSelectedId()-1;if(index<0)return;
    result(comparison->applyFactory(index),"Applied starting point; ON/BYPASS preserved");}
void NativePresetControls::storeBank(int index){if(!comparison->storeBank(index)){setStatus("Slot changed - reopen editor",true);reloadFromSlot();return;}
    setStatus(juce::String("Stored ")+(index==0?"A":"B")+"; sound unchanged");reloadFromSlot();}
void NativePresetControls::recallBank(int index){result(comparison->recallBank(index),juce::String("Recalled ")+(index==0?"A":"B")+"; ON/BYPASS preserved");}
void NativePresetControls::chooseFile(bool save){
    if(!diskEnabled||chooser||!comparison->valid()){if(!comparison->valid())setStatus("Slot changed - reopen editor",true);return;}
    // Resolve a desktop storage path only after an explicit user action. No
    // folder creation or AppData reads occur in offline construction/tests.
    const auto folder=juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("LiveAudioHost").getChildFile("NativePresets");
    const auto frozenModule=comparison->module();const auto frozenGeneration=comparison->generation();
    juce::String serialized;
    if(save){const auto xml=session::capturePreset(rack(),channelIndex,slotIndex,false,juce::String(builtin::metadata(frozenModule).name)+" preset");serialized=xml->toString();}
    chooser=std::make_unique<juce::FileChooser>(save?"Save native plugin preset":"Load same-type native plugin preset",folder.getChildFile(save?"native-preset.xml":""),"*.xml",true);
    const int chooserFlags=(save?juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::warnAboutOverwriting:juce::FileBrowserComponent::openMode)|juce::FileBrowserComponent::canSelectFiles;
    juce::Component::SafePointer<NativePresetControls> safe(this);
    chooser->launchAsync(chooserFlags,[safe,save,frozenModule,frozenGeneration,serialized](const juce::FileChooser& selected){
        if(!safe)return;const auto file=selected.getResult();safe->chooser.reset();if(file.getFullPathName().isEmpty())return;
        if(!safe->comparison->valid()||safe->comparison->module()!=frozenModule||safe->comparison->generation()!=frozenGeneration){safe->setStatus("Slot changed while chooser was open",true);safe->reloadFromSlot();return;}
        juce::String error;
        if(save){const auto xml=juce::XmlDocument::parse(serialized);if(!xml||!session::writeFile(file.withFileExtension("xml"),*xml,error)){safe->setStatus("Save failed: "+error,true);return;}
            safe->setStatus("Saved "+file.getFileName());return;}
        const auto xml=session::readFile(file,error);session::NativePreset preset;
        if(!xml||!session::decodePreset(*xml,preset,error)){safe->setStatus("Load failed: "+error,true);return;}
        if(preset.chain){safe->setStatus("Choose a single plugin preset, not a chain",true);return;}
        safe->result(safe->comparison->apply(preset.slots[0]),"Loaded "+preset.name+"; ON/BYPASS preserved");
    });
}
void NativePresetControls::paint(juce::Graphics& g){g.setColour(juce::Colour(0xff13212e));g.fillRoundedRectangle(getLocalBounds().toFloat(),5);}
void NativePresetControls::resized(){
    auto row=getLocalBounds().reduced(4,2);if(getHeight()>=54){status.setBounds(row.removeFromBottom(18));row.removeFromBottom(2);}else status.setBounds(row.removeFromRight(std::clamp(getWidth()/6,65,130)));
    const int buttonWidth=getWidth()<660?43:51;const int bankWidth=29;const int totalButtons=buttonWidth*4+bankWidth*2+6*4;
    factorySelect.setBounds(row.removeFromLeft(std::max(90,row.getWidth()-totalButtons)));row.removeFromLeft(4);
    for(auto* button:{&storeA,&selectA,&storeB,&selectB,&saveButton,&loadButton}){const int width=button==&selectA||button==&selectB?bankWidth:buttonWidth;button->setBounds(row.removeFromLeft(width));row.removeFromLeft(4);}
}
bool NativePresetControls::runValidation(juce::String& error){
    if(engine.isPassThroughEnabled()||!comparison->valid()){error="Preset validation requires a muted valid native target.";return false;}
    const auto before=builtin::snapshot(rack());const auto original=*comparison->current();
    auto savedComparison=std::move(comparison);comparison=std::make_unique<builtin::LivePresetState>(rack(),channelIndex,slotIndex);
    bool pass=true;juce::String failure;
    const auto check=[&](bool condition,const char* message){if(!condition&&pass){pass=false;failure=message;}};
    for(int index=0;index<3;++index){factorySelect.setSelectedId(index+1,juce::sendNotificationSync);
        const auto current=comparison->current();check(current&&builtin::samePresetParameters(*current,builtin::factoryPresets(original.type)[static_cast<std::size_t>(index)].state),"Factory preset callback did not apply all parameters.");
        check(current&&current->enabled==original.enabled,"Factory preset changed ON/BYPASS.");}
    storeA.onClick();const auto a=comparison->current();factorySelect.setSelectedId(1,juce::sendNotificationSync);storeB.onClick();const auto b=comparison->current();
    selectA.onClick();check(a&&comparison->current()&&builtin::samePresetParameters(*a,*comparison->current()),"A recall lost full plugin settings.");
    selectB.onClick();check(b&&comparison->current()&&builtin::samePresetParameters(*b,*comparison->current()),"B recall lost full plugin settings.");
    for(int channel=0;channel<builtin::channelCount;++channel)for(int slot=0;slot<builtin::slotCount;++slot)if(channel!=channelIndex||slot!=slotIndex){
        const auto current=builtin::snapshotSlot(rack().slots[static_cast<std::size_t>(channel)][static_cast<std::size_t>(slot)]);
        const auto& expected=before[static_cast<std::size_t>(channel)][static_cast<std::size_t>(slot)];check(current.enabled==expected.enabled&&builtin::samePresetParameters(current,expected),"Preset workflow changed another slot.");}
    check(comparison->apply(original)==builtin::PresetResult::Applied,"Could not restore validation settings.");comparison=std::move(savedComparison);
    factorySelect.setSelectedId(0,juce::dontSendNotification);setStatus(pass?"Factory / A-B validation passed":failure,!pass);reloadFromSlot();if(onApplied)onApplied();
    if(!pass)error=failure;return pass;
}
} // namespace lah::ui
