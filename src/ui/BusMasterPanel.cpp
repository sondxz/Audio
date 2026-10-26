#include "BusMasterPanel.h"
#include "../audio/UsbReturnProfile.h"

namespace lah::ui {
namespace {
void slider(juce::Slider& s,double low,double high,double step,bool vertical=false){
    s.setRange(low,high,step);s.setSliderStyle(vertical?juce::Slider::LinearVertical:juce::Slider::LinearHorizontal);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow,false,78,20);
    s.setColour(juce::Slider::trackColourId,juce::Colour(0xff36c8d8));
}
float toSend(double db){return db<=-60?0:std::pow(10.0f,static_cast<float>(db)*.05f);}
double fromSend(float value){return value<=0?-60:20*std::log10(value);}
}
BusMasterPanel::BusMasterPanel(audio::LiveAudioEngine& e):engine(e),controls(e.controls().busMaster){
    title.setText("8 SOFTWARE BUSES / STEREO MASTER",juce::dontSendNotification);title.setFont(juce::Font(juce::FontOptions(22).withStyle("Bold")));
    hint.setText("POST INPUT PLUGINS > BUS / STEREO MASTER > USB OUTPUT PLUGINS > OUTPUT GAIN",juce::dontSendNotification);
    hint.setColour(juce::Label::textColourId,juce::Colour(0xff93afc2));
    for(auto* c:std::initializer_list<juce::Component*>{&title,&hint,&status,&sourceLabel,&sourcePanLabel,&masterSendLabel,&enabled,&masterRoute,&usbRoute,&source,&sourcePan,&sourceMaster,&leftOutput,&rightOutput,&masterMute,&masterTitle,&leftLabel,&rightLabel})addAndMakeVisible(c);
    sourceLabel.setText("INPUT SENDS",juce::dontSendNotification);sourcePanLabel.setText("MASTER PAN",juce::dontSendNotification);masterSendLabel.setText("DIRECT TO MASTER (dB)",juce::dontSendNotification);
    for(int i=0;i<32;++i)source.addItem("CH "+juce::String(i+1),i+1);
    source.setSelectedId(1);source.onChange=[this]{selectedSource=std::clamp(source.getSelectedId()-1,0,31);selectSource();};
    slider(sourcePan,-1,1,.01);sourcePan.onValueChange=[this]{controls.inputPan[selectedSource].store(static_cast<float>(sourcePan.getValue()));};
    slider(sourceMaster,-60,6,.1);sourceMaster.onValueChange=[this]{controls.masterSends[selectedSource].store(toSend(sourceMaster.getValue()));};
    enabled.onClick=[this]{const bool turnOn=!controls.enabled.load();engine.setPassThroughEnabled(false);controls.enabled.store(turnOn);status.setText("Routing changed. Audio is muted; inspect routes, then enable Audio on Mixer.",juce::dontSendNotification);refreshControls();};
    masterRoute.onClick=[this]{configureMasterRouting();};usbRoute.onClick=[this]{configureUsbRouting();};
    for(int b=0;b<8;++b){
        busLabels[b].setText("BUS "+juce::String(b+1),juce::dontSendNotification);busLabels[b].setJustificationType(juce::Justification::centred);
        sendLabels[b].setText("SEND (dB)",juce::dontSendNotification);panLabels[b].setText("PAN TO MASTER",juce::dontSendNotification);
        for(auto* c:std::initializer_list<juce::Component*>{&busLabels[b],&sendLabels[b],&panLabels[b],&busGain[b],&busPan[b],&sourceSends[b],&busOutputs[b],&busMute[b],&busMaster[b]})addAndMakeVisible(c);
        slider(busGain[b],-60,12,.1,true);slider(busPan[b],-1,1,.01);slider(sourceSends[b],-60,6,.1);
        busGain[b].onValueChange=[this,b]{controls.gainDb[b].store(static_cast<float>(busGain[b].getValue()));};
        busPan[b].onValueChange=[this,b]{controls.pan[b].store(static_cast<float>(busPan[b].getValue()));};
        sourceSends[b].onValueChange=[this,b]{controls.sends[b*32+selectedSource].store(toSend(sourceSends[b].getValue()));};
        busMute[b].setButtonText("MUTE");busMute[b].onClick=[this,b]{controls.mute[b].store(controls.mute[b].load()>.5f?0.0f:1.0f);refreshControls();};
        busMaster[b].setButtonText("TO MASTER");busMaster[b].onClick=[this,b]{controls.toMaster[b].store(controls.toMaster[b].load()>0?0.0f:1.0f);refreshControls();};
        busOutputs[b].addItem("NO DIRECT OUT",1);for(int o=0;o<32;++o)busOutputs[b].addItem("USB OUT "+juce::String(o+1),o+2);
        busOutputs[b].onChange=[this,b]{engine.setPassThroughEnabled(false);controls.output[b].store(busOutputs[b].getSelectedId()-2);status.setText("Bus output changed. Audio remains muted.",juce::dontSendNotification);};
    }
    masterTitle.setText("STEREO MASTER",juce::dontSendNotification);masterTitle.setFont(juce::Font(juce::FontOptions(18).withStyle("Bold")));
    leftLabel.setText("LEFT",juce::dontSendNotification);rightLabel.setText("RIGHT",juce::dontSendNotification);
    for(int lr=0;lr<2;++lr){addAndMakeVisible(masterGain[lr]);slider(masterGain[lr],-60,12,.1,true);
        masterGain[lr].onValueChange=[this,lr]{controls.masterGainDb[lr].store(static_cast<float>(masterGain[lr].getValue()));};
        auto& output=lr==0?leftOutput:rightOutput;output.addItem("NO OUTPUT",1);for(int o=0;o<32;++o)output.addItem("OUT "+juce::String(o+1),o+2);
        output.onChange=[this,lr]{auto& selection=lr==0?leftOutput:rightOutput;auto& other=lr==0?controls.masterRight:controls.masterLeft;
            const int next=selection.getSelectedId()-2;
            if(next>=0&&other.load()==next){status.setText("Choose different USB outputs for Master L and R.",juce::dontSendNotification);refreshControls();return;}
            engine.setPassThroughEnabled(false);(lr==0?controls.masterLeft:controls.masterRight).store(next);status.setText("Master output changed. Audio remains muted.",juce::dontSendNotification);
        };
    }
    masterMute.onClick=[this]{controls.masterMute.store(!controls.masterMute.load());refreshControls();};
    refreshControls();startTimerHz(15);
}
void BusMasterPanel::selectSource(){
    sourcePan.setValue(controls.inputPan[selectedSource].load(),juce::dontSendNotification);
    sourceMaster.setValue(fromSend(controls.masterSends[selectedSource].load()),juce::dontSendNotification);
    for(int b=0;b<8;++b)sourceSends[b].setValue(fromSend(controls.sends[b*32+selectedSource].load()),juce::dontSendNotification);
}
void BusMasterPanel::refreshControls(){
    enabled.setButtonText(controls.enabled.load()?"BUS AUDIO ON":"BUS AUDIO OFF");enabled.setToggleState(controls.enabled.load(),juce::dontSendNotification);
    selectSource();
    for(int b=0;b<8;++b){busGain[b].setValue(controls.gainDb[b].load(),juce::dontSendNotification);busPan[b].setValue(controls.pan[b].load(),juce::dontSendNotification);
        busMute[b].setToggleState(controls.mute[b].load()>.5f,juce::dontSendNotification);busMaster[b].setToggleState(controls.toMaster[b].load()>0,juce::dontSendNotification);
        busOutputs[b].setSelectedId(controls.output[b].load()+2,juce::dontSendNotification);
    }
    for(int lr=0;lr<2;++lr)masterGain[lr].setValue(controls.masterGainDb[lr].load(),juce::dontSendNotification);
    leftOutput.setSelectedId(controls.masterLeft.load()+2,juce::dontSendNotification);rightOutput.setSelectedId(controls.masterRight.load()+2,juce::dontSendNotification);
    masterMute.setToggleState(controls.masterMute.load(),juce::dontSendNotification);
}
void BusMasterPanel::configureMasterRouting(){
    engine.setPassThroughEnabled(false);auto& mix=engine.controls();
    for(auto& route:mix.route)route.store(0);for(auto& send:mix.fxSend)send.store(0);mix.delayReturn.store(0);mix.reverbReturn.store(0);
    dsp::BusMasterSnapshot state;state.enabled=true;for(int i=0;i<8;++i)state.masterSends[i]=1;
    dsp::recallBusMaster(controls,state);refreshControls();
    status.setText("CH1-8 now feed stereo Master only. USB OUT1=L / OUT2=R. Audio remains muted.",juce::dontSendNotification);
}
void BusMasterPanel::configureUsbRouting(){
    engine.setPassThroughEnabled(false);const int count=engine.getInputChannelCount()>0?std::clamp(std::min(engine.getInputChannelCount(),engine.getOutputChannelCount()),1,8):8;
    audio::applyUsbReturnProfile(engine.controls(),count);refreshControls();status.setText("USB 1:1 returns restored; buses off. Audio remains muted.",juce::dontSendNotification);
}
void BusMasterPanel::timerCallback(){for(int i=0;i<10;++i)levels[i]=std::max(engine.consumeBusMasterPeak(i),levels[i]*.78f);repaint();}
void BusMasterPanel::paint(juce::Graphics& g){
    g.fillAll(juce::Colour(0xff0b121c));
    for(int i=0;i<10;++i){auto r=meters[i];g.setColour(juce::Colour(0xff04090e));g.fillRect(r);
        const float level=std::clamp(levels[i],0.0f,1.0f);g.setColour(levels[i]>=.999f?juce::Colour(0xffff6464):juce::Colour(0xff36d6ab));
        g.fillRect(r.removeFromBottom(static_cast<int>(level*r.getHeight())));
    }
}
void BusMasterPanel::resized(){
    const int w=getWidth();title.setBounds(18,10,w-36,30);hint.setBounds(18,42,w-36,24);
    enabled.setBounds(18,74,150,28);masterRoute.setBounds(180,74,190,28);usbRoute.setBounds(382,74,120,28);
    sourceLabel.setBounds(18,110,120,22);source.setBounds(18,136,115,28);sourcePanLabel.setBounds(145,110,140,22);sourcePan.setBounds(145,132,185,48);
    masterSendLabel.setBounds(350,110,230,22);sourceMaster.setBounds(350,132,200,48);
    const int busWidth=std::max(85,(w-220)/8),start=14,top=190,base=getHeight()-66;
    for(int b=0;b<8;++b){const int x=start+b*busWidth;busLabels[b].setBounds(x,top,busWidth-8,24);
        sendLabels[b].setBounds(x+4,top+28,busWidth-12,20);sourceSends[b].setBounds(x+4,top+49,busWidth-12,44);
        busMaster[b].setBounds(x+4,top+101,busWidth-12,24);busMute[b].setBounds(x+4,top+130,busWidth-12,24);
        panLabels[b].setBounds(x+4,top+160,busWidth-12,20);busPan[b].setBounds(x+4,top+182,busWidth-12,42);
        busGain[b].setBounds(x+4,top+232,busWidth-32,std::max(70,base-top-270));meters[b]={x+busWidth-26,top+236,10,std::max(45,base-top-290)};
        busOutputs[b].setBounds(x+4,base-27,busWidth-12,26);
    }
    const int mx=start+8*busWidth+10,mw=std::max(145,w-mx-14);masterTitle.setBounds(mx,top,mw,28);masterMute.setBounds(mx,top+34,mw,26);
    leftLabel.setBounds(mx,top+68,mw/2,22);rightLabel.setBounds(mx+mw/2,top+68,mw/2,22);
    leftOutput.setBounds(mx,top+94,mw/2-4,26);rightOutput.setBounds(mx+mw/2,top+94,mw/2-4,26);
    for(int lr=0;lr<2;++lr){int x=mx+lr*mw/2;masterGain[lr].setBounds(x,top+132,mw/2-19,std::max(100,base-top-152));meters[8+lr]={x+mw/2-17,top+136,10,std::max(70,base-top-184)};}
    status.setBounds(18,getHeight()-52,w-36,42);
}
bool BusMasterPanel::runControlValidation(juce::String& error){
    if(engine.isDeviceRunning()){error="Bus UI audit requires a closed device.";return false;}
    const auto saved=dsp::snapshotBusMaster(controls);const auto mixRoutes=[this]{std::array<float,1024> r{};for(int i=0;i<1024;++i)r[i]=engine.controls().route[i].load();return r;}();
    std::array<float,32> savedPan{},savedFx{};for(int i=0;i<32;++i){savedPan[i]=engine.controls().inputPan[i].load();savedFx[i]=engine.controls().fxSend[i].load();}
    const float savedDelay=engine.controls().delayReturn.load(),savedReverb=engine.controls().reverbReturn.load();
    source.setSelectedId(8,juce::sendNotificationSync);sourceSends[7].setValue(-6,juce::sendNotificationSync);
    if(std::abs(controls.sends[7*32+7].load()-std::pow(10.0f,-.3f))>1e-5f||controls.sends[7*32].load()!=saved.sends[7*32]){error="Bus source send target mismatch.";return false;}
    busGain[7].setValue(-12,juce::sendNotificationSync);masterGain[1].setValue(-3,juce::sendNotificationSync);
    if(controls.gainDb[7].load()!=-12||controls.masterGainDb[1].load()!=-3){error="Bus/master fader binding failed.";return false;}
    configureMasterRouting();if(!controls.enabled.load()||engine.controls().route[0].load()!=0||engine.isPassThroughEnabled()){error="Master routing profile failed.";return false;}
    configureUsbRouting();if(controls.enabled.load()||engine.controls().route[0].load()!=1){error="USB return profile failed to disable buses.";return false;}
    for(int i=0;i<1024;++i)engine.controls().route[i].store(mixRoutes[i]);
    for(int i=0;i<32;++i){engine.controls().inputPan[i].store(savedPan[i]);engine.controls().fxSend[i].store(savedFx[i]);}
    engine.controls().delayReturn.store(savedDelay);engine.controls().reverbReturn.store(savedReverb);
    dsp::recallBusMaster(controls,saved);source.setSelectedId(1,juce::sendNotificationSync);refreshControls();return true;
}
}
