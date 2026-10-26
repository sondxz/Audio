#include "SPLPanel.h"
#include <cmath>

namespace lah::ui {
SPLPanel::SPLPanel(audio::LiveAudioEngine& e,std::function<double()> rateGetter)
    : engine(e),getRate(std::move(rateGetter)),worker(e.capture()) {
    for(auto* l:{&title,&note,&status,&details}){
        addAndMakeVisible(*l);l->setColour(juce::Label::textColourId,juce::Colour(0xffc6d1db));
        l->setFont(juce::FontOptions(l==&title?22.0f:l==&status?14.0f:13.0f));
        l->setMinimumHorizontalScale(1.0f);
    }
    title.setText("Acoustic level / calibration",juce::dontSendNotification);
    title.setColour(juce::Label::textColourId,juce::Colour(0xffedf3f7));
    note.setText("LAB PREVIEW / Not IEC 61672 certified. A/C/Z spectral weighting and Fast/Slow block estimates are approximate.",juce::dontSendNotification);
    note.setColour(juce::Label::textColourId,juce::Colour(0xffe6b96f));
    status.setText("Calibration required. Select the microphone and capture a steady 1 kHz reference.",juce::dontSendNotification);
    details.setText("Keep the same microphone, preamp gain and interface after calibration. Acoustic reference: 94 or 114 dB at 1 kHz.",juce::dontSendNotification);
    addAndMakeVisible(micInput);addAndMakeVisible(calDb);
    for(int i=0;i<dsp::channels;++i)micInput.addItem("MIC INPUT " + juce::String(i+1),i+1);
    observedMic=engine.controls().measurementChannel.load();
    micInput.setSelectedId(observedMic+1,juce::dontSendNotification);
    micInput.onChange=[this]{
        engine.controls().measurementChannel.store(micInput.getSelectedId()-1);
        synchronizeInput();
        if(onInputSelectionChanged)onInputSelectionChanged();
    };
    calDb.addItem("94 dB SPL / 1 kHz",1);
    calDb.addItem("114 dB SPL / 1 kHz",2);
    calDb.setSelectedId(1,juce::dontSendNotification);
    for(auto* b:{&calibrateButton,&resetButton})addAndMakeVisible(*b);
    calibrateButton.setButtonText("Capture reference");
    resetButton.setButtonText("Reset calibration");
    calibrateButton.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff24695e));
    micInput.setTooltip("Measurement microphone input, shared with the RTA / Transfer Function tab. Changing it clears calibration.");
    calDb.setTooltip("Select the acoustic calibrator's actual reference level at 1 kHz.");
    calibrateButton.setTooltip("Capture and validate a steady 1 kHz acoustic calibrator signal from the selected microphone.");
    resetButton.setTooltip("Clear the session's calibration and accumulated sound level statistics.");
    calibrateButton.onClick=[this]{calibrate();};
    resetButton.onClick=[this]{reset();};
    deviceGen=engine.getDeviceGeneration();
    startTimerHz(2);
}
void SPLPanel::reset(){
    calibration={};last={};stats={};worker.reset();
    status.setText("Calibration cleared. Capture a valid 1 kHz reference to enable acoustic readings.",juce::dontSendNotification);
    details.setText("Keep the same microphone, preamp gain and interface after calibration. Acoustic reference: 94 or 114 dB at 1 kHz.",juce::dontSendNotification);
    repaint();
}
bool SPLPanel::synchronizeInput(){
    const auto micChannel=engine.controls().measurementChannel.load();
    const auto generation=engine.getDeviceGeneration();
    if(micChannel==observedMic && generation==deviceGen &&
       (!calibration.valid || std::abs(calibration.rate-getRate())<=0.1))return false;
    observedMic=micChannel;deviceGen=generation;
    micInput.setSelectedId(micChannel+1,juce::dontSendNotification);
    reset();
    // Both tabs select the same capture stream. A change from either tab must
    // replace the complete calibration window before another attempt is allowed.
    calibrationEarliestCount=engine.capture().writtenCount()+32768+
        audio::LiveAudioEngine::maximumCallbackFrames;
    status.setText("Calibration INVALIDATED: input/device changed; waiting for fresh MIC samples",
                   juce::dontSendNotification);
    return true;
}
void SPLPanel::calibrate(){
    synchronizeInput();
    if(!engine.isDeviceRunning()){
        status.setText("Audio device disconnected",juce::dontSendNotification);return;
    }
    if(engine.capture().writtenCount()<calibrationEarliestCount){
        status.setText(juce::String::fromUTF8("Chờ đủ cửa sổ MIC mới sau khi đổi input/device trước khi calibration."),
                       juce::dontSendNotification);return;
    }
    if(!engine.capture().snapshot(ref,mic,32768)){
        status.setText(juce::String::fromUTF8("Chưa đủ mẫu từ MIC"),juce::dontSendNotification);return;
    }
    const auto fs=getRate();
    const auto micAtStart=observedMic;
    const auto generationAtStart=deviceGen;
    auto attempt=spl::calibrate1000Hz(mic,fs,calDb.getSelectedId()==2?114.0:94.0);
    if(engine.getDeviceGeneration()!=generationAtStart ||
       engine.controls().measurementChannel.load()!=micAtStart ||
       std::abs(getRate()-fs)>0.1){
        synchronizeInput();
        status.setText("Calibration rejected: input/device changed during capture",juce::dontSendNotification);
        return;
    }
    if(!attempt.valid){
        reset();status.setText("Calibration REJECTED: "+juce::String(attempt.error),juce::dontSendNotification);
        return;
    }
    calibration=attempt;deviceGen=engine.getDeviceGeneration();
    worker.configure(calibration);
    status.setText("Calibration accepted (provisional)   |   Reference: " +
                   juce::String(calibration.referenceDbfs,2)+" dBFS RMS  | +"+
                   juce::String(calibration.offset,2)+" dB offset",juce::dontSendNotification);
}
void SPLPanel::timerCallback(){
    synchronizeInput();
    if(calibration.valid){
        worker.snapshot(last,stats,captureGap);
        if(!last.valid)
            status.setText("SPL unavailable: " + juce::String(last.error),juce::dontSendNotification);
        else {
            status.setText("Receiving calibrated acoustic blocks. Keep the microphone, preamp gain and interface unchanged.",
                           juce::dontSendNotification);
            details.setText(captureGap?"Capture gap detected. Session statistics restarted; displayed levels remain provisional.":
                "Non-overlapping block estimates. Compare this calibrated chain with a reference sound level meter before relying on readings.",juce::dontSendNotification);
        }
    } else last.valid=false;
    repaint();
}
void SPLPanel::paint(juce::Graphics& g){
    g.fillAll(juce::Colour(0xff0d1015));
    const auto text=juce::Colour(0xffe0e9ef);
    const auto muted=juce::Colour(0xffa8b7c6);
    const auto accent=juce::Colour(0xff37d8b7);
    const auto amber=juce::Colour(0xffe6b96f);
    const auto drawCard=[&g](juce::Rectangle<float> card){
        g.setColour(juce::Colour(0xff171d24));g.fillRoundedRectangle(card,7.0f);
        g.setColour(juce::Colour(0xff303b47));g.drawRoundedRectangle(card,7.0f,1.0f);
    };
    g.setColour(muted);g.setFont(13.0f);
    g.drawText("Microphone input",18,63,215,22,juce::Justification::left);
    g.drawText("Acoustic calibrator",245,63,215,22,juce::Justification::left);
    const auto statusArea=juce::Rectangle<float>(18.0f,132.0f,static_cast<float>(getWidth()-36),38.0f);
    g.setColour(juce::Colour(0xff18232a));g.fillRoundedRectangle(statusArea,6.0f);
    g.setColour(juce::Colour(0xff2b4149));g.drawRoundedRectangle(statusArea,6.0f,1.0f);
    g.setColour(calibration.valid && last.valid?accent:amber);g.fillEllipse(31.0f,147.0f,8.0f,8.0f);

    const float width=static_cast<float>(getWidth()-36);
    const float primaryHeight=static_cast<float>(juce::jlimit(156,236,(getHeight()-270)/2));
    const float primaryWidth=(width-24.0f)/3.0f;
    for(int i=0;i<3;++i){
        const auto card=juce::Rectangle<float>(18.0f+i*(primaryWidth+12.0f),188.0f,primaryWidth,primaryHeight);
        drawCard(card);
        g.setColour(calibration.valid && last.valid?accent:juce::Colour(0xff657482));
        g.fillRoundedRectangle(card.getX()+16.0f,card.getY()+16.0f,4.0f,20.0f,2.0f);
        g.setColour(text);g.setFont(juce::FontOptions(15.0f,juce::Font::bold));
        g.drawText(i==0?"A-weighted":i==1?"C-weighted":"Z / flat",card.withTrimmedLeft(30.0f).withTrimmedTop(13.0f).removeFromTop(26.0f),juce::Justification::left);
        g.setColour(last.valid?accent:muted);g.setFont(juce::FontOptions(juce::jlimit(44.0f,66.0f,primaryHeight*0.27f),juce::Font::bold));
        const double v=i==0?last.dbA:i==1?last.dbC:last.dbZ;
        g.drawText(last.valid?juce::String(v,1):"--",card.withTrimmedTop(primaryHeight*0.27f).withHeight(primaryHeight*0.4f),juce::Justification::centred);
        g.setColour(text);g.setFont(14.0f);
        g.drawText(last.valid?"dB SPL / indicative":calibration.valid?"Waiting for valid audio":"Calibration required",card.withY(card.getBottom()-43.0f).withHeight(20.0f),juce::Justification::centred);
        g.setColour(muted);g.setFont(13.0f);
        g.drawText(last.valid?"Current non-overlapping block":"Acoustic level unavailable",card.withY(card.getBottom()-24.0f).withHeight(18.0f),juce::Justification::centred);
    }

    const float lowerTop=200.0f+primaryHeight;
    const float lowerHeight=getHeight()-52.0f-lowerTop;
    const float statisticsWidth=(width-12.0f)*0.62f;
    const auto statisticsCard=juce::Rectangle<float>(18.0f,lowerTop,statisticsWidth,lowerHeight);
    const auto referenceCard=juce::Rectangle<float>(30.0f+statisticsWidth,lowerTop,width-statisticsWidth-12.0f,lowerHeight);
    drawCard(statisticsCard);drawCard(referenceCard);
    g.setColour(text);g.setFont(juce::FontOptions(15.0f,juce::Font::bold));
    g.drawText("Session statistics",statisticsCard.reduced(16.0f,10.0f).removeFromTop(25.0f),juce::Justification::left);
    g.drawText("Calibration reference",referenceCard.reduced(16.0f,10.0f).removeFromTop(25.0f),juce::Justification::left);
    const juce::String metricLabels[]{"Leq A / dB","Leq C / dB","Leq Z / dB","Fast A / dB (approx.)","Slow A / dB (approx.)","Duration / s"};
    const double metricValues[]{stats.leqA,stats.leqC,stats.leqZ,stats.fastA,stats.slowA,stats.elapsedSeconds};
    const auto metrics=statisticsCard.withTrimmedLeft(16.0f).withTrimmedRight(16.0f).withTrimmedTop(43.0f).withTrimmedBottom(12.0f);
    for(int i=0;i<6;++i){
        const auto tile=juce::Rectangle<float>(metrics.getX()+(i%3)*metrics.getWidth()/3.0f,
            metrics.getY()+(i/3)*metrics.getHeight()/2.0f,metrics.getWidth()/3.0f,metrics.getHeight()/2.0f);
        g.setColour(muted);g.setFont(13.0f);
        g.drawText(metricLabels[i],tile.withHeight(20.0f),juce::Justification::left);
        g.setColour(stats.valid?text:muted);g.setFont(juce::FontOptions(lowerHeight>220.0f?30.0f:24.0f,juce::Font::bold));
        g.drawText(stats.valid?juce::String(metricValues[i],1):"--",tile.withTrimmedTop(21.0f).withHeight(36.0f),juce::Justification::left);
        if(i<3){g.setColour(juce::Colour(0xff2b3742));g.drawHorizontalLine(juce::roundToInt(tile.getBottom()-5.0f),tile.getX(),tile.getRight()-14.0f);}
    }
    const float rx=referenceCard.getX()+16.0f;
    const float rw=referenceCard.getWidth()-32.0f;
    g.setColour(calibration.valid?accent:amber);g.setFont(13.0f);
    g.drawText(calibration.valid?"Provisional / reference captured":"Waiting for a calibrator",juce::Rectangle<float>(rx,lowerTop+41.0f,rw,21.0f),juce::Justification::left);
    const auto row=[&](int index,const juce::String& label,const juce::String& value){
        const float yy=lowerTop+72.0f+index*24.0f;
        g.setColour(muted);g.setFont(13.0f);g.drawText(label,juce::Rectangle<float>(rx,yy,115.0f,19.0f),juce::Justification::left);
        g.setColour(text);g.drawText(value,juce::Rectangle<float>(rx+114.0f,yy,rw-114.0f,19.0f),juce::Justification::right);
    };
    row(0,"Mic / sample rate","IN "+juce::String(observedMic+1)+(calibration.valid?" / "+juce::String(calibration.rate/1000.0,1)+" kHz":""));
    const double referenceLevel=calibration.valid?calibration.calibratorDb:calDb.getSelectedId()==2?114.0:94.0;
    row(1,"Calibrator",juce::String(referenceLevel,0)+" dB / 1 kHz");
    if(calibration.valid){
        row(2,"Reference RMS",juce::String(calibration.referenceDbfs,2)+" dBFS");
        row(3,"Offset","+"+juce::String(calibration.offset,2)+" dB");
    } else {
        g.setColour(muted);g.setFont(13.0f);
        g.drawFittedText("Fit a steady 1 kHz calibrator to the mic, then capture the reference.",
            juce::Rectangle<float>(rx,lowerTop+122.0f,rw,lowerHeight-130.0f).toNearestInt(),juce::Justification::topLeft,3,1.0f);
    }
    if(lowerHeight>230.0f){
        g.setColour(juce::Colour(0xff2b3742));g.drawHorizontalLine(juce::roundToInt(lowerTop+188.0f),rx,rx+rw);
        g.setColour(muted);g.setFont(13.0f);
        g.drawFittedText("Calibration is session only. Input, device or sample rate changes clear it. External analog gain changes cannot be detected automatically.",
            juce::Rectangle<float>(rx,lowerTop+201.0f,rw,lowerHeight-213.0f).toNearestInt(),juce::Justification::topLeft,4,1.0f);
    }
}
void SPLPanel::resized(){
    const int w=getWidth();
    title.setBounds(14,10,w-28,32);
    note.setBounds(14,42,w-28,22);
    micInput.setBounds(18,87,215,34);
    calDb.setBounds(245,87,215,34);
    calibrateButton.setBounds(472,87,180,34);
    resetButton.setBounds(664,87,155,34);
    status.setBounds(43,134,w-70,34);
    details.setBounds(14,getHeight()-40,w-28,30);
}
}
