#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <functional>
#include <memory>
#include <vector>

namespace lah::ui {
class SceneLibraryPanel final:public juce::Component {
public:
    SceneLibraryPanel();
    void setStorageDirectory(const juce::File&);
    void refreshLibrary();
    bool storeQuickScene(int,const juce::XmlElement&,const juce::String& name,juce::String& error);
    bool hasQuickScene(int)const;
    bool runValidation(juce::String& error);
    void paint(juce::Graphics&)override;
    void resized()override;
    std::function<std::unique_ptr<juce::XmlElement>()> onCaptureScene;
    std::function<bool(const juce::XmlElement&,juce::String&)> onRecallScene;
    std::function<std::unique_ptr<juce::XmlElement>(bool chain,bool output,int channel,int slot,const juce::String& name)> onCapturePreset;
    std::function<bool(const juce::XmlElement&,bool output,int channel,int slot,juce::String&)> onApplyPreset;
private:
    enum class Transfer{ImportScene,ExportScene,ImportPreset,ExportPreset};
    void saveQuick(int);void recallQuick(int);void renameQuick(int);
    void savePreset();void loadPreset();void copyPreset();void pastePreset();
    void chooseFile(Transfer);bool saveIndex(juce::String&);bool available();
    void setStatus(const juce::String&,bool failure=false);
    juce::File quickFile(int)const;
    juce::File storage;
    std::array<juce::Label,8> sceneNames;
    std::array<juce::TextButton,8> sceneSave,sceneRecall;
    std::array<juce::Rectangle<int>,8> sceneCards;
    juce::Label title,description,sceneTitle,presetTitle,status,targetLabel,presetLabel,nameLabel,quickLabel;
    juce::ComboBox sceneSelect,presetSelect,presetKind,domainSelect,channelSelect,slotSelect;
    juce::TextEditor presetName;
    juce::TextButton importScene{"IMPORT SCENE"},exportScene{"EXPORT SCENE"};
    juce::TextButton presetSave{"SAVE PRESET"},presetLoad{"LOAD PRESET"},presetCopy{"COPY SETTINGS"},presetPaste{"PASTE SETTINGS"};
    juce::TextButton presetImport{"IMPORT PRESET"},presetExport{"EXPORT PRESET"},refreshButton{"REFRESH"};
    std::vector<juce::File> presetFiles;
    std::unique_ptr<juce::XmlElement> copiedPreset;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Rectangle<int> scenesCard,presetsCard;
    bool refreshing=false;
};
} // namespace lah::ui
