#include "PluginPanel.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <exception>
namespace lah::ui {
void PluginPanel::ScanThread::run(){
    try {
    juce::AudioPluginFormatManager m;
    m.addFormat (std::make_unique<juce::VST3PluginFormat>());
    juce::KnownPluginList list;
    auto folder=juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("LiveAudioHost");
    folder.createDirectory();
    if(auto* format=m.getFormat(0)) {
        juce::PluginDirectoryScanner s(list,*format,format->getDefaultLocationsToSearch(),true,
                                        folder.getChildFile("VST3-scan-crash-marker.txt"));
        juce::String name;
        while(!threadShouldExit() && s.scanNextFile(true,name)){}
        found=list.getTypes();
        if(s.getFailedFiles().size()>0)error=juce::String(s.getFailedFiles().size())+" plug-in(s) failed to scan";
    }
    }catch(const std::exception& exception){
        error="VST3 scan exception: "+juce::String::fromUTF8(exception.what());
    }catch(...){
        error="VST3 scan raised an unknown C++ exception";
    }
    finished.store(true,std::memory_order_release);
}
PluginPanel::PluginPanel(audio::LiveAudioEngine& audio,juce::AudioDeviceManager& devices)
    :engine(audio),deviceManager(devices){
    formatManager.addFormat(std::make_unique<juce::VST3PluginFormat>());
    for(auto* l:{&title,&state,&disclaimer}){addAndMakeVisible(*l);l->setColour(juce::Label::textColourId,juce::Colours::white);}
    title.setText(juce::String::fromUTF8("VST3 SINGLE STEREO INSERT — OUTPUT 1 / 2"),juce::dontSendNotification);
    disclaimer.setText(juce::String::fromUTF8("Thử nghiệm. Chưa cách ly crash plugin khỏi audio process; KHÔNG dùng live show."),juce::dontSendNotification);
    disclaimer.setColour(juce::Label::textColourId,juce::Colours::orange);
    state.setText(juce::String::fromUTF8("Chưa scan plugin."),juce::dontSendNotification);
    addAndMakeVisible(plugins);
    for(auto* b:{&scan,&load,&unloadBtn,&bypass})addAndMakeVisible(*b);
    scan.onClick=[this]{
        if(scanner != nullptr)return;
        descriptions.clear();plugins.clear(juce::dontSendNotification);
        scanner=std::make_unique<ScanThread>();
        state.setText(juce::String::fromUTF8("Đang scan VST3 ở thread khác (một số plugin có thể lỗi/hang)."),juce::dontSendNotification);
        scanner->startThread();scan.setEnabled(false);
    };
    load.onClick=[this]{loadSelected();};
    unloadBtn.onClick=[this]{unload();};
    bypass.onClick=[this]{toggleBypass();};
    bypass.setTooltip("Mute audio with PANIC before changing VST3 bypass. Enabling resets the plug-in; audio stays muted.");
    refresh();startTimerHz(4);
}
PluginPanel::~PluginPanel(){
    stopTimer();
    if(scanner != nullptr){scanner->signalThreadShouldExit();scanner->stopThread(4000);}
}
void PluginPanel::timerCallback(){
    if(scanner != nullptr && scanner->finished.load(std::memory_order_acquire) && !scanner->isThreadRunning()){
        descriptions=scanner->found;
        plugins.clear(juce::dontSendNotification);
        for(int i=0;i<descriptions.size();++i)
            plugins.addItem(descriptions.getReference(i).name,i+1);
        if(!descriptions.isEmpty())plugins.setSelectedId(1,juce::dontSendNotification);
        state.setText(juce::String::fromUTF8("Đã tìm thấy ")+juce::String(descriptions.size())+" VST3. "+scanner->error,juce::dontSendNotification);
        scanner.reset();scan.setEnabled(true);
    }
    refresh();
}
void PluginPanel::loadSelected(){
    const auto idx=plugins.getSelectedId()-1;
    if(!juce::isPositiveAndBelow(idx,descriptions.size())){state.setText(juce::String::fromUTF8("Chưa chọn VST3 hợp lệ."),juce::dontSendNotification);return;}
    auto* dev=deviceManager.getCurrentAudioDevice();
    if(dev==nullptr){state.setText(juce::String::fromUTF8("Chưa có audio device."),juce::dontSendNotification);return;}
    // Detach callback first so plugin cannot be destroyed while audio thread uses it.
    engine.setPassThroughEnabled(false);
    deviceManager.removeAudioCallback(&engine);
    const auto sr=dev->getCurrentSampleRate();const auto block=dev->getCurrentBufferSizeSamples();
    juce::String error;
    bool ok=false;
    try{
        auto plugin=formatManager.createPluginInstance(descriptions.getReference(idx),sr,block,error);
        ok=engine.installStereoPlugin(std::move(plugin),sr,block,error);
    }catch(const std::exception& exception){
        error="VST3 construction exception: "+juce::String::fromUTF8(exception.what());
    }catch(...){
        error="VST3 construction raised an unknown C++ exception";
    }
    // Reattach even when a third-party constructor throws; keep global AUDIO muted.
    deviceManager.addAudioCallback(&engine); // remains muted until confirmation in main header
    state.setText(ok?juce::String::fromUTF8("Đã load (BYPASS ON, AUDIO MUTED): ")+engine.getPluginName()
                    :juce::String::fromUTF8("VST3 lỗi: ")+error,juce::dontSendNotification);
    refresh();
}
void PluginPanel::unload(){
    engine.setPassThroughEnabled(false);
    deviceManager.removeAudioCallback(&engine);
    engine.removePlugin();
    deviceManager.addAudioCallback(&engine);
    state.setText(juce::String::fromUTF8("Đã unload plugin. Audio vẫn MUTE."),juce::dontSendNotification);
    refresh();
}
void PluginPanel::toggleBypass(){
    if(engine.isPassThroughEnabled()){
        state.setText(juce::String::fromUTF8("PANIC / MUTE audio trước khi đổi VST3 bypass."),juce::dontSendNotification);
        return;
    }
    if(engine.getPluginName()=="None" || deviceManager.getCurrentAudioDevice()==nullptr)return;
    if(!engine.isPluginBypassed()){
        engine.setPluginBypassed(true);
        state.setText("VST3 BYPASSED / AUDIO MUTED.",juce::dontSendNotification);
    }else{
        // Reattachment prepares and resets the plug-in off the processing callback.
        // Global mute remains set; only an explicit main-header action resumes audio.
        deviceManager.removeAudioCallback(&engine);
        deviceManager.addAudioCallback(&engine);
        if(engine.isDeviceRunning() && !engine.isPluginResetRequired())engine.setPluginBypassed(false);
        state.setText(!engine.isPluginBypassed()?
            juce::String::fromUTF8("VST3 reset + enabled / AUDIO MUTED. Xác nhận BẬT AUDIO ở trang chính."):
            juce::String::fromUTF8("VST3 không được kích hoạt; AUDIO VẪN MUTE. Kiểm tra audio device / safety lock."),
            juce::dontSendNotification);
    }
    refresh();
}
void PluginPanel::refresh(){
    const bool live=engine.isPassThroughEnabled();
    bypass.setButtonText(live?(engine.isPluginBypassed()?"BYPASS / MUTE TO CHANGE":"ACTIVE / MUTE TO CHANGE"):
        engine.isPluginBypassed()?"RESET + ENABLE VST3":"BYPASS VST3");
    bypass.setEnabled(!live && engine.isDeviceRunning() && engine.getPluginName()!="None");
    load.setEnabled(scanner==nullptr);
}
void PluginPanel::paint(juce::Graphics& g){g.fillAll(juce::Colour(0xff192330));}
void PluginPanel::resized(){
    title.setBounds(20,20,750,32);scan.setBounds(20,82,250,42);
    plugins.setBounds(285,82,420,42);
    load.setBounds(20,145,250,42);unloadBtn.setBounds(285,145,200,42);
    bypass.setBounds(500,145,215,42);
    state.setBounds(20,213,std::max(500,getWidth()-40),48);
    disclaimer.setBounds(20,286,std::max(500,getWidth()-40),55);
}
} // namespace lah::ui
