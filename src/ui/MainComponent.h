#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "../audio/LiveAudioEngine.h"
#include "MeterBank.h"
#include "MixerPanel.h"
#include "MeasurementPanel.h"
#include "SubPanel.h"
#include "EffectsPanel.h"
#include "PluginPanel.h"
#include "EngineeringPanel.h"
#include "FrontFillPanel.h"
#include "SPLPanel.h"
#include "FIRPanel.h"
#include "PhasePanel.h"
#include "StudioLookAndFeel.h"
#include "BuiltinRackPanel.h"
#include "NativePluginWindow.h"
#include "BusMasterPanel.h"
#include "SceneLibraryPanel.h"
#include <algorithm>
#include <array>
#include <vector>

namespace lah::ui
{
class PanelViewport final : public juce::Viewport
{
public:
    void attach(juce::Component& component,int width,int height){
        minimumWidth=width;minimumHeight=height;
        setScrollBarsShown(true,true);
        setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::never);
        setViewedComponent(&component,false);
        resized();
    }
    void resized() override {
        if(auto* content=getViewedComponent())
            content->setSize(std::max(minimumWidth,getWidth()-getScrollBarThickness()),
                             std::max(minimumHeight,getHeight()-getScrollBarThickness()));
        juce::Viewport::resized();
    }
private:
    int minimumWidth=1,minimumHeight=1;
};

class MainComponent final : public juce::Component, private juce::Timer
{
public:
    explicit MainComponent(bool initialiseDevice=true);
    ~MainComponent() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    // Native offscreen rendering only: no device is opened or settings saved.
    bool writeSmokeArtifacts(const juce::File& directory,juce::String& error);

private:
    void timerCallback() override;
    void askToEnableAudio();
    void saveDeviceSetup();
    void saveScene();
    void loadScene();
    juce::File getSceneFile() const;
    juce::File getSettingsFile() const;
    void refreshButtonStates();
    void toggleQaLogging();
    void writeQaLog();
    void selectPage(int index);
    void configureUsbReturn(bool findMidas);
    void openNativePlugin(int channel,int slot,bool output);
    std::unique_ptr<juce::XmlElement> captureSession();
    bool recallSession(const juce::XmlElement&,juce::String& error);
    void refreshSessionControls();
    static constexpr int busMasterPage=12,sceneLibraryPage=13,pageCount=14;

    StudioLookAndFeel rackSkin;
    juce::AudioDeviceManager deviceManager;
    audio::LiveAudioEngine engine;
    juce::AudioDeviceSelectorComponent deviceSelector;
    juce::Component devicePage;
    MixerPanel mixerPanel {engine};
    MeasurementPanel measurementPanel {engine, [this]() -> double {
        if (auto* d = deviceManager.getCurrentAudioDevice()) return d->getCurrentSampleRate();
        return 0.0;
    }};
    SPLPanel splPanel {engine, [this]() -> double {
        if (auto* d=deviceManager.getCurrentAudioDevice())return d->getCurrentSampleRate();
        return 0.0;
    }};
    FIRPanel firPanel {engine,deviceManager};
    SubPanel subPanel;
    EngineeringPanel engineeringPanel;
    FrontFillPanel frontFillPanel;
    PhasePanel phasePanel;
    EffectsPanel effectsPanel {engine.controls()};
    PluginPanel pluginPanel {engine, deviceManager};
    BuiltinRackPanel builtinPanel {engine,deviceManager};
    BusMasterPanel busMasterPanel {engine};
    SceneLibraryPanel sceneLibrary;
    std::array<PanelViewport,pageCount-1> toolViewports;
    juce::TabbedComponent tabs {juce::TabbedButtonBar::TabsAtTop};
    std::array<juce::TextButton,pageCount> navigation;
    std::array<juce::Label,3> navigationGroups;
    juce::Label pageTitle,pageHint,sessionSummary,audioStateLabel,buildBadge;
    juce::TextButton detailsButton{"Diagnostics"};
    bool diagnosticsExpanded=false;
    MeterBank inputMeters { "IN" };
    MeterBank outputMeters { "OUT" };
    juce::Viewport inputViewport, outputViewport;
    juce::TextButton passButton { juce::String::fromUTF8("BẬT PASS-THROUGH") };
    juce::TextButton panicButton { "PANIC / MUTE" };
    juce::TextButton saveButton { "Save Driver" };
    juce::TextButton saveSceneButton { "Save Scene" };
    juce::TextButton loadSceneButton { "Load Scene" };
    juce::TextButton qaLogButton { "QA LOG" };
    std::unique_ptr<juce::FileOutputStream> qaLogStream;
    std::uint64_t lastQaLogMs {0};
    juce::Label heading, subheading, statusLabel, diagnosticLabel, noticeLabel;
    juce::Label inputsTitle, outputsTitle;
    juce::ComboBox usbChannelSelect;
    juce::Label usbTitle,usbDescription,usbStatus;
    juce::TextButton connectMidasButton {"TIM M32R / ASIO"},applyUsbButton {"AP DUNG USB"};
    juce::String initError;
    bool showingConfirmation { false };
    bool deviceInitialisationEnabled {true};
    juce::File smokeStorage;
    std::vector<std::unique_ptr<NativePluginWindow>> nativePluginWindows;
};
} // namespace lah::ui
