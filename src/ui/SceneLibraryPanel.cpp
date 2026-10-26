#include "SceneLibraryPanel.h"
#include "SessionState.h"

namespace lah::ui {
SceneLibraryPanel::SceneLibraryPanel(){
    setOpaque(true);
    for(auto* label:{&title,&description,&sceneTitle,&presetTitle,&status,&targetLabel,&presetLabel,&nameLabel,&quickLabel}){
        addAndMakeVisible(*label);label->setColour(juce::Label::textColourId,juce::Colour(0xffaebdcb));
        label->setFont(juce::FontOptions(14));label->setBorderSize(juce::BorderSize<int>(0));
    }
    title.setText("SCENES / NATIVE PRESETS",juce::dontSendNotification);title.setFont(juce::FontOptions(23,juce::Font::bold));
    description.setText("Eight complete mixer scenes. Native plugin and four-slot chain presets retain independent input/output settings.",juce::dontSendNotification);
    sceneTitle.setText("QUICK SCENES  /  double-click a name to rename",juce::dontSendNotification);
    presetTitle.setText("PLUGIN / CHAIN LIBRARY",juce::dontSendNotification);
    targetLabel.setText("TARGET  /  domain, channel, insert slot",juce::dontSendNotification);
    presetLabel.setText("SAVED NATIVE PRESET",juce::dontSendNotification);nameLabel.setText("PRESET NAME / TYPE",juce::dontSendNotification);
    quickLabel.setText("IMPORT / EXPORT SELECTED SCENE",juce::dontSendNotification);
    for(int index=0;index<8;++index){
        auto& name=sceneNames[static_cast<std::size_t>(index)];addAndMakeVisible(name);name.setText("Scene "+juce::String(index+1),juce::dontSendNotification);
        name.setEditable(false,true,true);name.setFont(juce::FontOptions(16,juce::Font::bold));name.setColour(juce::Label::textColourId,juce::Colour(0xffdcecf4));
        name.onTextChange=[this,index]{if(!refreshing)renameQuick(index);};
        auto& save=sceneSave[static_cast<std::size_t>(index)];auto& recall=sceneRecall[static_cast<std::size_t>(index)];
        addAndMakeVisible(save);addAndMakeVisible(recall);save.setButtonText("SAVE "+juce::String(index+1));recall.setButtonText("RECALL");
        save.onClick=[this,index]{saveQuick(index);};recall.onClick=[this,index]{recallQuick(index);};
        sceneSelect.addItem("Scene "+juce::String(index+1),index+1);
    }
    for(auto* component:std::array<juce::Component*,7>{&sceneSelect,&presetSelect,&presetKind,&domainSelect,&channelSelect,&slotSelect,&presetName})addAndMakeVisible(*component);
    for(auto* button:{&importScene,&exportScene,&presetSave,&presetLoad,&presetCopy,&presetPaste,&presetImport,&presetExport,&refreshButton})addAndMakeVisible(*button);
    sceneSelect.setSelectedId(1);presetKind.addItem("Single plugin",1);presetKind.addItem("Four-slot chain",2);presetKind.setSelectedId(1);
    domainSelect.addItem("INPUT",1);domainSelect.addItem("OUTPUT",2);domainSelect.setSelectedId(1);
    for(int channel=1;channel<=32;++channel)channelSelect.addItem("CH "+juce::String(channel),channel);channelSelect.setSelectedId(1);
    for(int slot=1;slot<=4;++slot)slotSelect.addItem("SLOT "+juce::String(slot),slot);slotSelect.setSelectedId(1);
    presetName.setText("My native preset",false);presetName.setInputRestrictions(48);
    presetKind.onChange=[this]{slotSelect.setEnabled(presetKind.getSelectedId()!=2);};
    importScene.onClick=[this]{chooseFile(Transfer::ImportScene);};exportScene.onClick=[this]{chooseFile(Transfer::ExportScene);};
    presetImport.onClick=[this]{chooseFile(Transfer::ImportPreset);};presetExport.onClick=[this]{chooseFile(Transfer::ExportPreset);};
    presetSave.onClick=[this]{savePreset();};presetLoad.onClick=[this]{loadPreset();};presetCopy.onClick=[this]{copyPreset();};presetPaste.onClick=[this]{pastePreset();};
    refreshButton.onClick=[this]{refreshLibrary();setStatus("Library refreshed.");};
    setStatus("Save a scene or a native preset. Recall and paste always mute audio; VST3 state is not included.");
}
void SceneLibraryPanel::setStatus(const juce::String& message,bool failure){status.setText(message,juce::dontSendNotification);status.setColour(juce::Label::textColourId,juce::Colour(failure?0xffffa073:0xff65dccb));}
bool SceneLibraryPanel::available(){if(storage.getFullPathName().isNotEmpty())return true;setStatus("Library storage is disabled in this preview.",true);return false;}
juce::File SceneLibraryPanel::quickFile(int index)const{return storage.getChildFile("scene-"+juce::String(index+1)+".xml");}
bool SceneLibraryPanel::hasQuickScene(int index)const{return index>=0&&index<8&&storage.getFullPathName().isNotEmpty()&&quickFile(index).existsAsFile();}
void SceneLibraryPanel::setStorageDirectory(const juce::File& directory){storage=directory;refreshLibrary();}
bool SceneLibraryPanel::saveIndex(juce::String& error){juce::XmlElement index("LiveAudioHostLibrary");index.setAttribute("schema",1);
    for(int slot=0;slot<8;++slot){auto* row=index.createNewChildElement("Scene");row->setAttribute("slot",slot);row->setAttribute("name",sceneNames[static_cast<std::size_t>(slot)].getText());}
    return session::writeFile(storage.getChildFile("index.xml"),index,error);
}
void SceneLibraryPanel::refreshLibrary(){
    refreshing=true;const auto selected=presetSelect.getSelectedId();presetSelect.clear(juce::dontSendNotification);presetFiles.clear();
    if(storage.getFullPathName().isNotEmpty()){
        juce::String error;const auto file=storage.getChildFile("index.xml");
        if(file.existsAsFile())if(auto index=session::readFile(file,error);index&&index->hasTagName("LiveAudioHostLibrary")&&index->getIntAttribute("schema")==1)
            for(auto* row:index->getChildIterator()){const int slot=row->getIntAttribute("slot",-1);if(row->hasTagName("Scene")&&slot>=0&&slot<8)
                sceneNames[static_cast<std::size_t>(slot)].setText(session::cleanName(row->getStringAttribute("name"),"Scene "+juce::String(slot+1)),juce::dontSendNotification);}
        auto files=storage.findChildFiles(juce::File::findFiles,false,"preset-*.xml");files.sort();
        for(const auto& candidate:files){if(presetFiles.size()>=256)break;auto xml=session::readFile(candidate,error);session::NativePreset preset;
            if(xml&&session::decodePreset(*xml,preset,error)){presetFiles.push_back(candidate);presetSelect.addItem((preset.chain?"CHAIN  /  ":"PLUGIN  /  ")+preset.name,static_cast<int>(presetFiles.size()));}}
    }
    for(int slot=0;slot<8;++slot)sceneRecall[static_cast<std::size_t>(slot)].setEnabled(hasQuickScene(slot));
    if(!presetFiles.empty())presetSelect.setSelectedId(std::clamp(selected,1,static_cast<int>(presetFiles.size())),juce::dontSendNotification);
    presetLoad.setEnabled(!presetFiles.empty());presetExport.setEnabled(!presetFiles.empty());presetPaste.setEnabled(copiedPreset!=nullptr);refreshing=false;repaint();
}
bool SceneLibraryPanel::storeQuickScene(int index,const juce::XmlElement& scene,const juce::String& name,juce::String& error){
    if(index<0||index>=8||storage.getFullPathName().isEmpty()){error="Invalid quick-scene target.";return false;}
    if(!session::decode(scene,error))return false;auto document=std::make_unique<juce::XmlElement>(scene);const auto cleaned=session::cleanName(name,"Scene "+juce::String(index+1));document->setAttribute("name",cleaned);
    if(!session::writeFile(quickFile(index),*document,error))return false;
    sceneNames[static_cast<std::size_t>(index)].setText(cleaned,juce::dontSendNotification);if(!saveIndex(error))return false;refreshLibrary();return true;
}
void SceneLibraryPanel::saveQuick(int index){if(!available()||!onCaptureScene)return;juce::String error;const auto xml=onCaptureScene();
    if(!xml||!storeQuickScene(index,*xml,sceneNames[static_cast<std::size_t>(index)].getText(),error)){setStatus("Scene save failed: "+error,true);return;}setStatus("Saved scene "+juce::String(index+1)+" / "+sceneNames[static_cast<std::size_t>(index)].getText());}
void SceneLibraryPanel::recallQuick(int index){if(!available()||!onRecallScene)return;juce::String error;const auto xml=session::readFile(quickFile(index),error);
    if(!xml||!onRecallScene(*xml,error)){setStatus("Recall failed: "+error,true);return;}setStatus("Recalled scene "+juce::String(index+1)+". Audio remains MUTED.");}
void SceneLibraryPanel::renameQuick(int index){if(!available())return;auto& name=sceneNames[static_cast<std::size_t>(index)];name.setText(session::cleanName(name.getText(),"Scene "+juce::String(index+1)),juce::dontSendNotification);
    juce::String error;if(!saveIndex(error)){setStatus(error,true);return;}setStatus("Renamed scene "+juce::String(index+1)+" to "+name.getText());}
void SceneLibraryPanel::savePreset(){if(!available()||!onCapturePreset)return;const auto xml=onCapturePreset(presetKind.getSelectedId()==2,domainSelect.getSelectedId()==2,channelSelect.getSelectedId()-1,slotSelect.getSelectedId()-1,presetName.getText());
    juce::String error;if(!xml||!session::writeFile(storage.getChildFile("preset-"+juce::Uuid().toString()+".xml"),*xml,error)){setStatus("Preset save failed: "+error,true);return;}refreshLibrary();setStatus("Saved native preset. Other channels and slots are unchanged.");}
void SceneLibraryPanel::loadPreset(){if(!onApplyPreset)return;const int index=presetSelect.getSelectedId()-1;if(index<0||index>=static_cast<int>(presetFiles.size()))return;
    juce::String error;const auto xml=session::readFile(presetFiles[static_cast<std::size_t>(index)],error);
    if(!xml||!onApplyPreset(*xml,domainSelect.getSelectedId()==2,channelSelect.getSelectedId()-1,slotSelect.getSelectedId()-1,error)){setStatus("Preset load failed: "+error,true);return;}setStatus("Native preset loaded to the selected target. Audio remains MUTED.");}
void SceneLibraryPanel::copyPreset(){if(!onCapturePreset)return;copiedPreset=onCapturePreset(presetKind.getSelectedId()==2,domainSelect.getSelectedId()==2,channelSelect.getSelectedId()-1,slotSelect.getSelectedId()-1,presetName.getText());
    presetPaste.setEnabled(copiedPreset!=nullptr);setStatus("Copied selected native settings. Select a destination and PASTE SETTINGS.");}
void SceneLibraryPanel::pastePreset(){if(!copiedPreset||!onApplyPreset)return;juce::String error;
    if(!onApplyPreset(*copiedPreset,domainSelect.getSelectedId()==2,channelSelect.getSelectedId()-1,slotSelect.getSelectedId()-1,error)){setStatus(error,true);return;}setStatus("Pasted native settings into the selected target. Audio remains MUTED.");}
void SceneLibraryPanel::chooseFile(Transfer transfer){
    if(!available()||chooser)return;const bool save=transfer==Transfer::ExportScene||transfer==Transfer::ExportPreset;
    const int quick=std::clamp(sceneSelect.getSelectedId()-1,0,7);std::unique_ptr<juce::XmlElement> document;juce::String error;
    if(save){
        if(transfer==Transfer::ExportScene){document=session::readFile(quickFile(quick),error);if(document)document->setAttribute("name",sceneNames[static_cast<std::size_t>(quick)].getText());}
        else{const int index=presetSelect.getSelectedId()-1;if(index>=0&&index<static_cast<int>(presetFiles.size()))document=session::readFile(presetFiles[static_cast<std::size_t>(index)],error);}
        if(!document){setStatus("Nothing valid to export: "+error,true);return;}
    }
    const auto serialized=document?document->toString():juce::String{};
    chooser=std::make_unique<juce::FileChooser>(save?"Export LiveAudioHost document":"Import LiveAudioHost document",storage.getChildFile(save?"LiveAudioHost-export.xml":""),"*.xml",true);
    auto safe=juce::Component::SafePointer<SceneLibraryPanel>(this);
    const int chooserFlags=(save?juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::warnAboutOverwriting:juce::FileBrowserComponent::openMode)|juce::FileBrowserComponent::canSelectFiles;
    chooser->launchAsync(chooserFlags,[safe,transfer,quick,serialized,save](const juce::FileChooser& selected){
        if(!safe)return;const auto file=selected.getResult();safe->chooser.reset();if(file.getFullPathName().isEmpty())return;
        juce::String failure;if(save){const auto xml=juce::XmlDocument::parse(serialized);if(!xml||!session::writeFile(file.withFileExtension("xml"),*xml,failure)){safe->setStatus(failure,true);return;}safe->setStatus("Exported "+file.getFileName());return;}
        const auto xml=session::readFile(file,failure);if(!xml){safe->setStatus(failure,true);return;}
        if(transfer==Transfer::ImportScene){if(!safe->storeQuickScene(quick,*xml,xml->getStringAttribute("name","Imported scene"),failure)){safe->setStatus(failure,true);return;}safe->setStatus("Imported scene into slot "+juce::String(quick+1)+". Recall it when ready.");}
        else{session::NativePreset preset;if(!session::decodePreset(*xml,preset,failure)||!session::writeFile(safe->storage.getChildFile("preset-"+juce::Uuid().toString()+".xml"),*xml,failure)){safe->setStatus(failure,true);return;}safe->refreshLibrary();safe->setStatus("Imported native preset. Select its destination before loading.");}
    });
}
bool SceneLibraryPanel::runValidation(juce::String& error){
    if(!available()||!onCaptureScene||!onRecallScene||!onCapturePreset||!onApplyPreset){error="Scene library callbacks/storage unavailable.";return false;}
    const auto saved=onCaptureScene();if(!saved){error="Cannot capture library validation scene.";return false;}
    for(int slot=0;slot<8;++slot)if(!storeQuickScene(slot,*saved,"Validation "+juce::String(slot+1),error))return false;
    for(int slot=0;slot<8;++slot){const auto xml=session::readFile(quickFile(slot),error);
        if(!xml||xml->getStringAttribute("name")!="Validation "+juce::String(slot+1)||!onRecallScene(*xml,error)){if(error.isEmpty())error="Quick-scene slot identity was not retained.";return false;}}
    sceneNames[0].setText("Renamed validation",juce::dontSendNotification);renameQuick(0);refreshLibrary();
    if(sceneNames[0].getText()!="Renamed validation"){error="Quick-scene rename was not persisted.";return false;}
    for(bool chain:{false,true})for(bool output:{false,true}){const auto preset=onCapturePreset(chain,output,7,3,"Validation preset");if(!preset||!onApplyPreset(*preset,!output,6,2,error))return false;}
    presetKind.setSelectedId(2,juce::sendNotificationSync);channelSelect.setSelectedId(8);slotSelect.setSelectedId(4);domainSelect.setSelectedId(1);
    presetCopy.onClick();if(!copiedPreset){error="Preset copy callback produced no settings.";return false;}domainSelect.setSelectedId(2);channelSelect.setSelectedId(7);presetPaste.onClick();
    presetName.setText("Validation chain",false);presetSave.onClick();refreshLibrary();if(presetFiles.empty()){error="Preset save did not create a valid library entry.";return false;}
    if(!onRecallScene(*saved,error))return false;
    auto corrupt=std::make_unique<juce::XmlElement>(*saved);corrupt->setAttribute("schema",999);if(session::decode(*corrupt,error)){error="Unknown scene schema was accepted.";return false;}
    error.clear();setStatus("Validation: eight scenes and four plugin/chain target transfers passed. Audio remains MUTED.");return true;
}
void SceneLibraryPanel::paint(juce::Graphics& g){g.fillAll(juce::Colour(0xff0e141c));g.setColour(juce::Colour(0xff18232f));g.fillRoundedRectangle(scenesCard.toFloat(),8);g.fillRoundedRectangle(presetsCard.toFloat(),8);
    for(std::size_t index=0;index<sceneCards.size();++index){g.setColour(juce::Colour(hasQuickScene(static_cast<int>(index))?0xff203b42:0xff202b38));g.fillRoundedRectangle(sceneCards[index].toFloat(),6);}}
void SceneLibraryPanel::resized(){
    auto area=getLocalBounds().reduced(18);title.setBounds(area.removeFromTop(30));description.setBounds(area.removeFromTop(34));area.removeFromTop(12);status.setBounds(area.removeFromBottom(48));area.removeFromBottom(10);
    scenesCard=area.removeFromLeft((area.getWidth()-16)*57/100);area.removeFromLeft(16);presetsCard=area;
    auto scenes=scenesCard.reduced(14);sceneTitle.setBounds(scenes.removeFromTop(27));scenes.removeFromTop(10);
    const int cellWidth=(scenes.getWidth()-12)/2;const int rowHeight=std::clamp((scenes.getHeight()-104)/4,70,92);
    for(int index=0;index<8;++index){const int column=index%2,row=index/2;auto cell=juce::Rectangle<int>(scenes.getX()+column*(cellWidth+12),scenes.getY()+row*(rowHeight+8),cellWidth,rowHeight);
        sceneCards[static_cast<std::size_t>(index)]=cell;cell=cell.reduced(9,7);sceneNames[static_cast<std::size_t>(index)].setBounds(cell.removeFromTop(25));cell.removeFromTop(5);
        sceneSave[static_cast<std::size_t>(index)].setBounds(cell.removeFromLeft((cell.getWidth()-8)/2));cell.removeFromLeft(8);sceneRecall[static_cast<std::size_t>(index)].setBounds(cell);}
    scenes.removeFromTop(4*(rowHeight+8)+8);quickLabel.setBounds(scenes.removeFromTop(22));auto buttons=scenes.removeFromTop(32);sceneSelect.setBounds(buttons.removeFromLeft(105));buttons.removeFromLeft(8);
    importScene.setBounds(buttons.removeFromLeft((buttons.getWidth()-8)/2));buttons.removeFromLeft(8);exportScene.setBounds(buttons);
    auto presets=presetsCard.reduced(16);presetTitle.setBounds(presets.removeFromTop(27));presets.removeFromTop(12);targetLabel.setBounds(presets.removeFromTop(22));auto target=presets.removeFromTop(33);
    const int width=(target.getWidth()-16)/3;domainSelect.setBounds(target.removeFromLeft(width));target.removeFromLeft(8);channelSelect.setBounds(target.removeFromLeft(width));target.removeFromLeft(8);slotSelect.setBounds(target);
    presets.removeFromTop(18);nameLabel.setBounds(presets.removeFromTop(22));presetName.setBounds(presets.removeFromTop(32));presets.removeFromTop(8);presetKind.setBounds(presets.removeFromTop(32));presets.removeFromTop(10);
    auto saveRow=presets.removeFromTop(32);presetSave.setBounds(saveRow.removeFromLeft((saveRow.getWidth()-8)/2));saveRow.removeFromLeft(8);presetCopy.setBounds(saveRow);presets.removeFromTop(18);
    presetLabel.setBounds(presets.removeFromTop(22));presetSelect.setBounds(presets.removeFromTop(34));presets.removeFromTop(10);auto loadRow=presets.removeFromTop(32);
    presetLoad.setBounds(loadRow.removeFromLeft((loadRow.getWidth()-8)/2));loadRow.removeFromLeft(8);presetPaste.setBounds(loadRow);presets.removeFromTop(10);auto transfer=presets.removeFromTop(32);
    presetImport.setBounds(transfer.removeFromLeft((transfer.getWidth()-8)/2));transfer.removeFromLeft(8);presetExport.setBounds(transfer);presets.removeFromTop(12);refreshButton.setBounds(presets.removeFromTop(30));
}
} // namespace lah::ui
