#include "FIRPanel.h"
#include <cmath>

namespace lah::ui {
FIRPanel::FIRPanel(audio::LiveAudioEngine& e,juce::AudioDeviceManager& d):audio(e),deviceManager(d){
    for(auto* l:{&title,&note,&status,&cutoffLabel}){
        addAndMakeVisible(*l);l->setColour(juce::Label::textColourId,juce::Colours::white);
    }
    title.setText("LINEAR-PHASE FIR / OFFLINE DESIGN + EXPERIMENTAL SHORT INSERT",juce::dontSendNotification);
    note.setText("129/257 taps OUT 1/2 LAB insert. OUT 3-32 align to reported FIR + VST3 latency; verify by loopback. NOT SHOW READY.",juce::dontSendNotification);
    note.setColour(juce::Label::textColourId,juce::Colour(0xfff0b45c));
    cutoffLabel.setText("Cutoff frequency (Hz)",juce::dontSendNotification);
    addAndMakeVisible(mode);mode.addItem("Low-pass",1);mode.addItem("High-pass",2);mode.setSelectedId(1);
    addAndMakeVisible(order);
    for(auto pair:{std::pair<int,int>{129,1},{257,2},{513,3},{1025,4},{2049,5},{4097,6}})
        order.addItem(juce::String(pair.first)+" taps",pair.second);
    order.setSelectedId(1);
    addAndMakeVisible(rate);rate.addItem("44.1 kHz",1);rate.addItem("48 kHz",2);rate.setSelectedId(2);
    addAndMakeVisible(cutoff);cutoff.setRange(20,18000,1);cutoff.setValue(1000);
    cutoff.setSliderStyle(juce::Slider::LinearHorizontal);
    cutoff.setTextBoxStyle(juce::Slider::TextBoxRight,false,90,29);
    for(auto* b:{&designButton,&exportButton,&armButton,&bypassButton})addAndMakeVisible(*b);
    designButton.onClick=[this]{design();};exportButton.onClick=[this]{exportCsv();};
    armButton.onClick=[this]{armFIR();};
    bypassButton.onClick=[this]{bypassFIR();};
    exportButton.setEnabled(false);
    const auto invalidateDesign=[this]{
        verified=false;exportButton.setEnabled(false);armButton.setEnabled(false);repaint();
    };
    for(auto* c:{&mode,&order,&rate})c->onChange=invalidateDesign;
    cutoff.onValueChange=invalidateDesign;
    status.setText(juce::String::fromUTF8("Chưa thiết kế filter"),juce::dontSendNotification);
    armButton.setEnabled(false);bypassButton.setEnabled(false);
    design();
    startTimerHz(2);
}
void FIRPanel::design(){
    try {
        constexpr int sizes[]{129,257,513,1025,2049,4097};
        const int size=sizes[std::clamp(order.getSelectedId()-1,0,5)];
        const double fs=rate.getSelectedId()==1?44100.0:48000.0;
        filter=lah::fir::make(mode.getSelectedId()==2?fir::Mode::Highpass:fir::Mode::Lowpass,
                              cutoff.getValue(),fs,size);
        verified=true;
        // Verify finite coefficients, symmetry and a passband anchor before making CSV available.
        for(std::size_t i=0;i<filter.taps.size();++i){
            if(!std::isfinite(filter.taps[i]) ||
                std::abs(filter.taps[i]-filter.taps[filter.taps.size()-1-i])>1e-10)
                verified=false;
        }
        const double anchor=(filter.mode==fir::Mode::Lowpass?0.0:fs/2.0);
        if(std::abs(fir::magnitudeDb(filter,anchor))>0.15)verified=false;
        status.setText(verified?juce::String::fromUTF8("FIR math checks PASS · Group delay = ")+juce::String(filter.groupDelayMs,3)+
                             juce::String::fromUTF8(" ms (linear phase) · Not sound-system validated")
                             :"FIR validation failed; export disabled",juce::dontSendNotification);
    } catch(const std::exception& err){
        verified=false;status.setText("ERROR: "+juce::String(err.what()),juce::dontSendNotification);
    }
    exportButton.setEnabled(verified);armButton.setEnabled(verified && filter.taps.size()<=257);repaint();
}
void FIRPanel::armFIR(){
    if(!verified || filter.taps.size()>257){
        status.setText(juce::String::fromUTF8("FIR live insert chỉ hỗ trợ 129/257 taps."),juce::dontSendNotification);return;
    }
    auto* device=deviceManager.getCurrentAudioDevice();
    if(device==nullptr || std::abs(device->getCurrentSampleRate()-filter.rate)>0.1){
        status.setText(juce::String::fromUTF8("Thiết bị chưa mở hoặc sample rate không trùng FIR; từ chối."),juce::dontSendNotification);return;
    }
    audio.setPassThroughEnabled(false);
    deviceManager.removeAudioCallback(&audio);
    juce::String error;
    const auto loaded=audio.installStereoFir(filter,device->getCurrentSampleRate(),error);
    deviceManager.addAudioCallback(&audio);
    status.setText(loaded?"FIR loaded / BYPASSED / AUDIO MUTED. Delay " +
        juce::String(filter.groupDelayMs,3)+juce::String::fromUTF8("ms. Bấm ENABLE FIR sau đó tự xác nhận AUDIO ở trang chính.")
        :"FIR rejected: "+error,juce::dontSendNotification);
    bypassButton.setEnabled(loaded);
}
void FIRPanel::bypassFIR(){
    if(!audio.hasStereoFir())return;
    const bool shouldBypass=!audio.isFirBypassed();
    // Structural DSP state is changed only with the callback detached, while muted.
    audio.setPassThroughEnabled(false);
    deviceManager.removeAudioCallback(&audio);
    deviceManager.addAudioCallback(&audio); // start always forces FIR bypass, flushes FIR delay line
    audio.setFirBypassed(shouldBypass);
    status.setText(shouldBypass?"FIR BYPASSED / AUDIO MUTED. Manual audio re-enable only.":
        "FIR armed for OUT 1/2 (AUDIO MUTED). Manual re-enable only. "+
        juce::String(audio.getFirLatencySamples())+" FIR latency samples; reported output compensation shown in main diagnostics.",juce::dontSendNotification);
    timerCallback();
}
void FIRPanel::timerCallback(){
    bypassButton.setEnabled(audio.hasStereoFir());
    bypassButton.setButtonText(audio.isFirBypassed()?"ENABLE FIR (MUTE)":"BYPASS FIR (MUTE)");
}
void FIRPanel::exportCsv(){
    if(!verified)return;
    auto folder=juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("LiveAudioHost");
    if(!folder.isDirectory()&&!folder.createDirectory()){
        status.setText(juce::String::fromUTF8("Lỗi tạo thư mục Documents/LiveAudioHost"),juce::dontSendNotification);return;
    }
    auto file=folder.getChildFile("fir-"+juce::String(filter.mode==fir::Mode::Lowpass?"lp-":"hp-")+
                                  juce::String(static_cast<int>(filter.cutoff))+"Hz-"+
                                  juce::String(static_cast<int>(filter.rate))+"Hz-"+
                                  juce::String(static_cast<int>(filter.taps.size()))+"taps.csv");
    juce::String output="tap_index,coefficient\n";
    for(std::size_t i=0;i<filter.taps.size();++i)
        output+=juce::String(static_cast<int>(i))+","+juce::String(filter.taps[i],16)+"\n";
    if(file.replaceWithText(output))status.setText("Exported " + file.getFullPathName(),juce::dontSendNotification);
    else status.setText(juce::String::fromUTF8("Không thể lưu file CSV"),juce::dontSendNotification);
}
void FIRPanel::paint(juce::Graphics& g){
    g.fillAll(juce::Colour(0xff0d1015));
    auto r=juce::Rectangle<float>(25.0f,210.0f,static_cast<float>(std::max(240,getWidth()-50)),
                                 static_cast<float>(std::max(170,getHeight()-275)));
    g.setColour(juce::Colour(0xff171c25));g.fillRoundedRectangle(r,9);
    g.setColour(juce::Colour(0xff41434c));
    for(int i=0;i<=5;++i){
        const auto y=r.getY()+r.getHeight()*i/5.0f;
        g.drawHorizontalLine(static_cast<int>(y),r.getX(),r.getRight());
    }
    for(int i=0;i<=5;++i){
        const auto x=r.getX()+r.getWidth()*i/5.0f;
        g.drawVerticalLine(static_cast<int>(x),r.getY(),r.getBottom());
    }
    if(verified){
        juce::Path p;
        for(int i=0;i<500;++i){
            const double hz=20.0*std::pow(1000.0,i/499.0);
            const auto mag=fir::magnitudeDb(filter,hz);
            const float x=r.getX()+r.getWidth()*i/499.0f;
            const float y=r.getBottom()-r.getHeight()*static_cast<float>(std::clamp((mag+100.0)/106.0,0.0,1.0));
            if(i==0)p.startNewSubPath(x,y);else p.lineTo(x,y);
        }
        g.setColour(juce::Colour(0xff39d7bc));g.strokePath(p,juce::PathStrokeType(2.1f));
    }
    g.setColour(juce::Colour(0xffb9c3cf));g.setFont(13);
    g.drawText(juce::String::fromUTF8("Frequency 20 Hz — 20 kHz (log)  |  Magnitude -100 to +6 dB  |  theoretical filter response"),
               r.removeFromBottom(28).reduced(10),juce::Justification::left);
}
void FIRPanel::resized(){
    const int w=getWidth();
    title.setBounds(23,8,w-46,32);note.setBounds(23,42,w-46,30);
    mode.setBounds(23,84,150,33);order.setBounds(180,84,146,33);
    rate.setBounds(332,84,145,33);cutoffLabel.setBounds(485,84,165,30);
    cutoff.setBounds(645,84,std::max(170,w-672),33);
    designButton.setBounds(23,130,172,35);exportButton.setBounds(200,130,155,35);
    armButton.setBounds(362,130,250,35);bypassButton.setBounds(620,130,260,35);
    status.setBounds(25,167,std::max(280,w-50),33);
}
}
