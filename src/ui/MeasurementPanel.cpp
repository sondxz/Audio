#include "MeasurementPanel.h"
#include <algorithm>
#include <cmath>

namespace lah::ui {
MeasurementPanel::MeasurementPanel(audio::LiveAudioEngine& engine,std::function<double()> sampleRate)
  : audio(engine),getRate(std::move(sampleRate)),worker(engine.capture()) {
    for(auto* l:{&refLabel,&micLabel,&note,&summary}) {
        addAndMakeVisible(*l);
        l->setFont(juce::FontOptions(l==&summary?14.0f:13.0f));
        l->setMinimumHorizontalScale(1.0f);
        l->setColour(juce::Label::textColourId,juce::Colour(0xffc6d1db));
    }
    refLabel.setText("Reference input",juce::dontSendNotification);
    micLabel.setText("Measurement microphone",juce::dontSendNotification);
    note.setText("Use separate REF and MIC inputs. RTA is digital dBFS/bin RMS; calibrated acoustic levels are on the SPL tab.",juce::dontSendNotification);
    note.setColour(juce::Label::textColourId,juce::Colour(0xffa8b7c6));
    summary.setText("Waiting for microphone and reference samples. Check the two input selections.",juce::dontSendNotification);
    for(auto* cb:{&refSelect,&micSelect}){
        addAndMakeVisible(*cb);
        for(int i=0;i<dsp::channels;++i)cb->addItem("INPUT "+juce::String(i+1),i+1);
    }
    observedReference=audio.controls().referenceChannel.load();
    observedMicrophone=audio.controls().measurementChannel.load();
    refSelect.setSelectedId(observedReference+1,juce::dontSendNotification);
    micSelect.setSelectedId(observedMicrophone+1,juce::dontSendNotification);
    addAndMakeVisible(viewSelect);
    viewSelect.addItem("Transfer",1);
    viewSelect.addItem("RTA spectrum",2);
    viewSelect.addItem("All plots",3);
    viewSelect.setSelectedId(1,juce::dontSendNotification);
    viewSelect.onChange=[this]{repaint();};
    viewSelect.setTooltip("Transfer: large phase and magnitude plots. RTA spectrum: microphone level. All plots: four analysis views.");
    addAndMakeVisible(exportButton);
    exportButton.setButtonText("Export CSV");
    for (auto* b:{&snapshotAButton,&snapshotBButton,&clearSnapshotsButton})addAndMakeVisible(*b);
    exportButton.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff24695e));
    refSelect.setTooltip("Electrical reference for the transfer function. Use an input separate from the measurement mic.");
    micSelect.setTooltip("Microphone capture input shared with the SPL / Calibration tab.");
    snapshotAButton.setTooltip("Save the current mic RTA and available transfer function as session snapshot A.");
    snapshotBButton.setTooltip("Save the current mic RTA and available transfer function as session snapshot B.");
    clearSnapshotsButton.setTooltip("Remove both saved session traces.");
    exportButton.setTooltip("Export current measurements and saved A/B traces to Documents/LiveAudioHost.");
    snapshotAButton.onClick=[this]{takeSnapshot(0);};
    snapshotBButton.onClick=[this]{takeSnapshot(1);};
    clearSnapshotsButton.onClick=[this]{hasSnapshot[0]=hasSnapshot[1]=false;repaint();};
    exportButton.onClick=[this]{exportCsv();};
    refSelect.onChange=[this]{audio.controls().referenceChannel.store(refSelect.getSelectedId()-1);synchronizeInputs();};
    micSelect.onChange=[this]{
        audio.controls().measurementChannel.store(micSelect.getSelectedId()-1);synchronizeInputs();
        if(onInputSelectionChanged)onInputSelectionChanged();
    };
    lastDeviceGeneration=audio.getDeviceGeneration();
    startTimerHz(3);
}
void MeasurementPanel::synchronizeInputs(){
    const auto reference=audio.controls().referenceChannel.load();
    const auto microphone=audio.controls().measurementChannel.load();
    const auto generation=audio.getDeviceGeneration();
    if(reference==observedReference && microphone==observedMicrophone && generation==lastDeviceGeneration)return;
    observedReference=reference;observedMicrophone=microphone;lastDeviceGeneration=generation;
    refSelect.setSelectedId(reference+1,juce::dontSendNotification);
    micSelect.setSelectedId(microphone+1,juce::dontSendNotification);
    worker.invalidate(audio::LiveAudioEngine::maximumCallbackFrames);result={};
    repaint();
}
void MeasurementPanel::takeSnapshot(int slot){
    synchronizeInputs();
    if(slot<0||slot>1||!result.hasRta){
        summary.setText("No trustworthy MIC snapshot available",juce::dontSendNotification);return;
    }
    snapshots[slot]=result;hasSnapshot[slot]=true;
    summary.setText(slot==0?"Captured snapshot A (mic RTA + valid TF if available)":
                             "Captured snapshot B (mic RTA + valid TF if available)",juce::dontSendNotification);
    repaint();
}
void MeasurementPanel::timerCallback(){
    worker.setSampleRate(getRate());
    synchronizeInputs();
    if (!worker.latest(result,workerGeneration)) return;
    if (!result.hasRta) {
        summary.setText("Waiting for 32,768 fresh MIC/REF samples. Check microphone signal and routing.",
                        juce::dontSendNotification);
        repaint(); return;
    }
    const auto levels="REF  "+juce::String(result.referenceRmsDbfs,1)+" dBFS RMS\nMIC  "+juce::String(result.measurementRmsDbfs,1)+" dBFS RMS\n\n";
    if(!result.valid)summary.setText(levels+"RTA only. TF requires a reference above -80 dBFS and valid paired samples.",juce::dontSendNotification);
    else summary.setText(levels+"Reliable TF bins\n"+juce::String(result.reliableBins)+" / "+
                         juce::String(result.validBins)+(result.meanCoherence<0.6?"\n\nLow coherence":""),juce::dontSendNotification);
    repaint();
}
void MeasurementPanel::exportCsv(){
    synchronizeInputs();
    if(!result.hasRta){summary.setText(juce::String::fromUTF8("Không có RTA hợp lệ để export."),juce::dontSendNotification);return;}
    const auto folder=juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                          .getChildFile("LiveAudioHost");
    if(!folder.isDirectory() && !folder.createDirectory()){
        summary.setText(juce::String::fromUTF8("Không thể tạo thư mục Documents/LiveAudioHost."),juce::dontSendNotification);return;
    }
    const auto file=folder.getChildFile("TF-RTA-"+juce::Time::getCurrentTime().formatted("%Y%m%d-%H%M%S")+".csv")
                          .getNonexistentSibling();
    juce::String csv="frequency_hz,rta_dbfs_per_bin_rms,tf_magnitude_db,tf_phase_deg_compensated,coherence,bin_valid,bin_reliable,delay_valid,delay_ms\n";
    // Persist original calibrated-vs-digital classification and A/B traces.
    csv=csv.trimCharactersAtEnd("\n")+
        ",snapshot_a_frequency_hz,snapshot_b_frequency_hz,snapshot_a_rta_dbfs_bin,snapshot_b_rta_dbfs_bin,snapshot_a_magnitude_db,snapshot_b_magnitude_db\n";
    for(int k=1;k<=measurement::fftSize/2;++k){
        const auto idx=static_cast<std::size_t>(k);
        const bool good=result.valid && result.binValid[idx];
        csv+=juce::String(k*result.sampleRate/measurement::fftSize,4)+","+
            juce::String(result.rtaDb[idx],4)+","+
            (good?juce::String(result.magnitudeDb[idx],4):juce::String())+","+
            (good&&result.delayValid?juce::String(result.phaseDeg[idx],4):juce::String())+","+
            (good?juce::String(result.coherence[idx],4):juce::String())+","+
            juce::String(good?1:0)+","+
            juce::String(good && result.binReliable[idx]?1:0)+","+
            juce::String(result.delayValid?1:0)+","+
            juce::String(result.delayMs,4)+","+
            (hasSnapshot[0]?juce::String(k*snapshots[0].sampleRate/measurement::fftSize,4):juce::String())+","+
            (hasSnapshot[1]?juce::String(k*snapshots[1].sampleRate/measurement::fftSize,4):juce::String())+","+
            (hasSnapshot[0]?juce::String(snapshots[0].rtaDb[idx],4):juce::String())+","+
            (hasSnapshot[1]?juce::String(snapshots[1].rtaDb[idx],4):juce::String())+","+
            (hasSnapshot[0]&&snapshots[0].binReliable[idx]?juce::String(snapshots[0].magnitudeDb[idx],4):juce::String())+","+
            (hasSnapshot[1]&&snapshots[1].binReliable[idx]?juce::String(snapshots[1].magnitudeDb[idx],4):juce::String())+"\n";
    }
    summary.setText(file.replaceWithText(csv)?"Saved: "+file.getFullPathName():juce::String::fromUTF8("Lỗi export CSV."),
                    juce::dontSendNotification);
}

void MeasurementPanel::drawTrace(juce::Graphics& g,const juce::Rectangle<float>& bounds,
                 const std::array<double,measurement::fftSize/2+1>& data,double low,double high,
                 double rate,juce::Colour colour,
                 const std::array<bool,measurement::fftSize/2+1>* mask) {
    juce::Path trace;bool hasPoint=false;
    for(int i=1;i<=measurement::fftSize/2;++i){
        const double hz=i*rate/measurement::fftSize;
        if(hz<20 || hz>std::min(20000.0,rate/2.0) ||
           !std::isfinite(data[static_cast<std::size_t>(i)]) ||
           (mask != nullptr && !(*mask)[static_cast<std::size_t>(i)])){
            hasPoint=false;
            continue;
        }
        const float x=bounds.getX()+bounds.getWidth()*static_cast<float>(std::clamp(std::log(hz/20.0)/std::log(1000.0),0.0,1.0));
        const float y=bounds.getBottom()-bounds.getHeight()*static_cast<float>(std::clamp((data[static_cast<std::size_t>(i)]-low)/(high-low),0.0,1.0));
        if(!hasPoint){trace.startNewSubPath(x,y);hasPoint=true;}else trace.lineTo(x,y);
    }
    juce::Graphics::ScopedSaveState saved(g);
    g.reduceClipRegion(bounds.toNearestInt());
    g.setColour(colour);g.strokePath(trace,juce::PathStrokeType(1.6f));
}
void MeasurementPanel::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff0d1015));
    const auto drawFloatText=[&g](const juce::String& text,float x,float y,float w,float h,juce::Justification alignment){
        g.drawText(text,juce::Rectangle<float>(x,y,w,h),alignment);
    };
    g.setColour(juce::Colour(0xffedf3f7));g.setFont(juce::FontOptions(22.0f,juce::Font::bold));
    g.drawText("System measurement",18,12,420,28,juce::Justification::left);
    g.setColour(juce::Colour(0xffa8b7c6));g.setFont(13.0f);
    g.drawText("Dual-channel transfer function and microphone spectrum",18,42,500,20,juce::Justification::left);

    const juce::Colour traceColours[]{juce::Colour(0xff37d8b7),juce::Colour(0xff82b8ff),juce::Colour(0xffe8a2d6)};
    const juce::String legendLabels[]{"Live",hasSnapshot[0]?"A saved":"A empty",hasSnapshot[1]?"B saved":"B empty"};
    int legendX=getWidth()-330;
    for(int i=0;i<3;++i){
        g.setColour(i==0 || hasSnapshot[i-1]?traceColours[i]:juce::Colour(0xff657482));
        g.fillRoundedRectangle(static_cast<float>(legendX),23.0f,18.0f,3.0f,1.5f);
        g.setFont(13.0f);g.drawText(legendLabels[i],legendX+25,13,80,23,juce::Justification::left);
        legendX+=106;
    }

    const auto drawCard=[&g](juce::Rectangle<float> card){
        g.setColour(juce::Colour(0xff171d24));g.fillRoundedRectangle(card,7.0f);
        g.setColour(juce::Colour(0xff303b47));g.drawRoundedRectangle(card,7.0f,1.0f);
    };
    const auto railHeight=static_cast<float>(getHeight()-124);
    drawCard({18.0f,88.0f,176.0f,railHeight});
    const float qualityX=static_cast<float>(getWidth()-208);
    drawCard({qualityX,88.0f,190.0f,242.0f});
    drawCard({qualityX,342.0f,190.0f,static_cast<float>(getHeight()-378)});
    g.setFont(juce::FontOptions(15.0f,juce::Font::bold));g.setColour(juce::Colour(0xffe0e9ef));
    g.drawText("Signal routing",30,100,152,23,juce::Justification::left);
    g.drawText("Display",30,264,152,23,juce::Justification::left);
    g.drawText("Trace snapshots",30,338,152,23,juce::Justification::left);
    drawFloatText("Signal quality",qualityX+14.0f,100.0f,166.0f,23.0f,juce::Justification::left);
    drawFloatText("Reference status",qualityX+14.0f,354.0f,166.0f,23.0f,juce::Justification::left);
    g.setColour(juce::Colour(0xffa8b7c6));g.setFont(13.0f);
    drawFloatText("Delay",qualityX+14.0f,136.0f,162.0f,20.0f,juce::Justification::left);
    drawFloatText("Mean coherence",qualityX+14.0f,222.0f,162.0f,20.0f,juce::Justification::left);
    g.setFont(juce::FontOptions(27.0f,juce::Font::bold));g.setColour(juce::Colour(0xff37d8b7));
    drawFloatText(result.valid && result.delayValid?juce::String(result.delayMs,2)+" ms":"--",qualityX+14.0f,157.0f,162.0f,36.0f,juce::Justification::left);
    g.setColour(juce::Colour(0xffe6b96f));
    drawFloatText(result.valid?juce::String(result.meanCoherence,2):"--",qualityX+14.0f,244.0f,162.0f,34.0f,juce::Justification::left);
    g.setFont(13.0f);g.setColour(juce::Colour(0xffa8b7c6));
    drawFloatText(result.valid && result.delayValid?"Phase compensated":"Awaiting verified delay",qualityX+14.0f,194.0f,162.0f,19.0f,juce::Justification::left);
    drawFloatText(result.valid?"TF display gate: 0.6":"Awaiting paired samples",qualityX+14.0f,281.0f,162.0f,19.0f,juce::Justification::left);
    drawFloatText(result.sampleRate>=8000?juce::String(result.sampleRate/1000.0,1)+" kHz capture":"No analysis sample rate",qualityX+14.0f,304.0f,166.0f,18.0f,juce::Justification::left);

    const auto view=viewSelect.getSelectedId();
    const int graphCount=view==3?4:view==2?1:2;
    const float gap=12.0f;
    const float chartWidth=view==3?(getWidth()-426.0f-gap)/2.0f:getWidth()-426.0f;
    const float chartHeight=view==2?railHeight:(railHeight-gap)/2.0f;
    const double lows[]{-100.0,-48.0,-180.0,0.0};
    const double highs[]{0.0,24.0,180.0,1.0};
    const juce::String titles[]{"Microphone RTA","Transfer magnitude","Transfer phase","Coherence"};
    const juce::String units[]{"dBFS / bin RMS","dB","degrees / compensated","0 to 1"};
    for(int display=0;display<graphCount;++display){
        const int graph=view==3?display:view==2?0:display==0?2:1;
        const auto card=juce::Rectangle<float>(206.0f+(view==3?display%2:0)*(chartWidth+gap),
                88.0f+(view==3?display/2:display)*(chartHeight+gap),chartWidth,chartHeight);
        const auto plot=card.withTrimmedLeft(54.0f).withTrimmedRight(20.0f)
                            .withTrimmedTop(52.0f).withTrimmedBottom(42.0f);
        drawCard(card);
        g.setColour(juce::Colour(0xffe0e9ef));g.setFont(juce::FontOptions(15.0f,juce::Font::bold));
        g.drawText(titles[graph],card.withTrimmedLeft(14.0f).removeFromTop(30.0f),juce::Justification::left);
        g.setColour(juce::Colour(0xffa8b7c6));g.setFont(13.0f);
        g.drawText(units[graph],card.withTrimmedLeft(14.0f).withTrimmedTop(27.0f).removeFromTop(19.0f),juce::Justification::left);

        const int horizontalDivisions=graph==1?3:2;
        for(int k=0;k<=horizontalDivisions;++k){
            const auto value=highs[graph]-(highs[graph]-lows[graph])*k/horizontalDivisions;
            const auto yy=plot.getY()+plot.getHeight()*k/horizontalDivisions;
            g.setColour(juce::Colour(0xff2c3742));
            g.drawHorizontalLine(juce::roundToInt(yy),plot.getX(),plot.getRight());
            g.setColour(juce::Colour(0xffa8b7c6));g.setFont(13.0f);
            drawFloatText(juce::String(value,graph==3?1:0),card.getX()+4.0f,yy-8.0f,42.0f,16.0f,juce::Justification::right);
        }
        for(auto hz:{20,100,1000,10000,20000}){
            if(plot.getWidth()<320.0f && hz==10000)continue;
            const float x=plot.getX()+plot.getWidth()*static_cast<float>(std::log(hz/20.0)/std::log(1000.0));
            g.setColour(juce::Colour(0xff27323d));g.drawVerticalLine(juce::roundToInt(x),plot.getY(),plot.getBottom());
            g.setColour(juce::Colour(0xffa8b7c6));g.setFont(13.0f);
            const auto label=hz>=1000?juce::String(hz/1000)+"k":juce::String(hz);
            drawFloatText(label,x-20.0f,plot.getBottom()+4.0f,40.0f,17.0f,juce::Justification::centred);
        }
        g.setColour(juce::Colour(0xff8e9eae));g.setFont(13.0f);
        g.drawText("Frequency (Hz)",plot.withY(plot.getBottom()+22.0f).withHeight(17.0f),juce::Justification::centred);
        if(graph==3){
            const auto yy=plot.getBottom()-plot.getHeight()*0.6f;
            const float dashes[]{4.0f,4.0f};g.setColour(juce::Colour(0xffa68451));
            g.drawDashedLine(juce::Line<float>(plot.getX(),yy,plot.getRight(),yy),dashes,2,1.0f);
            g.setFont(13.0f);drawFloatText("TF gate 0.6",plot.getX()+7.0f,yy-18.0f,110.0f,16.0f,juce::Justification::left);
        }
        if(graph==0 && result.hasRta)drawTrace(g,plot,result.rtaDb,lows[graph],highs[graph],result.sampleRate,traceColours[0]);
        if(graph==1 && result.valid)drawTrace(g,plot,result.magnitudeDb,lows[graph],highs[graph],result.sampleRate,juce::Colour(0xff64dce7),&result.binReliable);
        if(graph==2 && result.valid && result.delayValid)drawTrace(g,plot,result.phaseDeg,lows[graph],highs[graph],result.sampleRate,traceColours[0],&result.binReliable);
        if(graph==3 && result.valid)drawTrace(g,plot,result.coherence,lows[graph],highs[graph],result.sampleRate,juce::Colour(0xffe6b96f),&result.binValid);
        bool savedTrace=false;
        for(int k=0;k<2;++k){
            if(!hasSnapshot[k] || snapshots[k].sampleRate<8000)continue;
            const auto& snap=snapshots[k];
            const auto hue=traceColours[k+1].withAlpha(0.8f);
            if(graph==0 && snap.hasRta){drawTrace(g,plot,snap.rtaDb,lows[graph],highs[graph],snap.sampleRate,hue);savedTrace=true;}
            if(graph==1 && snap.valid && snap.reliableBins>0){drawTrace(g,plot,snap.magnitudeDb,lows[graph],highs[graph],snap.sampleRate,hue,&snap.binReliable);savedTrace=true;}
            if(graph==2 && snap.valid && snap.reliableBins>0 && snap.delayValid){drawTrace(g,plot,snap.phaseDeg,lows[graph],highs[graph],snap.sampleRate,hue,&snap.binReliable);savedTrace=true;}
            if(graph==3 && snap.valid){drawTrace(g,plot,snap.coherence,lows[graph],highs[graph],snap.sampleRate,hue,&snap.binValid);savedTrace=true;}
        }
        const bool liveTrace=graph==0?result.hasRta:result.valid && (graph==3 || result.reliableBins>0) && (graph!=2 || result.delayValid);
        if(!liveTrace && !savedTrace){
            const juce::String empty=graph==0?"Waiting for microphone samples":graph==2?
                (!result.delayValid?"Phase appears after delay is verified":"No bins meet the 0.6 coherence gate"):
                (result.valid?"No bins meet the 0.6 coherence gate":"Waiting for a valid reference + mic measurement");
            auto message=plot.withSizeKeepingCentre(plot.getWidth()-12.0f,45.0f);
            g.setColour(juce::Colour(0xff171d24).withAlpha(0.94f));g.fillRoundedRectangle(message,4.0f);
            g.setColour(juce::Colour(0xffc0cbd5));g.setFont(13.0f);
            g.drawFittedText(empty,message.toNearestInt().reduced(6,3),juce::Justification::centred,2,1.0f);
        }
    }
}
void MeasurementPanel::resized(){
    refLabel.setBounds(26,130,160,21);refSelect.setBounds(30,154,152,34);
    micLabel.setBounds(26,197,160,21);micSelect.setBounds(30,221,152,34);
    viewSelect.setBounds(30,291,152,34);
    snapshotAButton.setBounds(30,367,72,34);
    snapshotBButton.setBounds(110,367,72,34);
    clearSnapshotsButton.setBounds(30,409,152,32);
    exportButton.setBounds(30,456,152,36);
    summary.setJustificationType(juce::Justification::topLeft);
    summary.setBounds(getWidth()-198,383,172,getHeight()-425);
    note.setBounds(14,getHeight()-29,getWidth()-28,22);
}
} // namespace lah::ui
