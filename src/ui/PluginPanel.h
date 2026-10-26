#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "../audio/LiveAudioEngine.h"
#include <atomic>
#include <memory>

namespace lah::ui {
class PluginPanel final: public juce::Component, private juce::Timer {
public:
    PluginPanel(audio::LiveAudioEngine& audio, juce::AudioDeviceManager& devices);
    ~PluginPanel() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    struct ScanThread final: juce::Thread {
        ScanThread():juce::Thread("VST3 Directory Scanner") {}
        void run() override;
        juce::Array<juce::PluginDescription> found;
        std::atomic<bool> finished {false};
        juce::String error;
    };
    void timerCallback() override;
    void loadSelected();
    void unload();
    void toggleBypass();
    void refresh();
    audio::LiveAudioEngine& engine;
    juce::AudioDeviceManager& deviceManager;
    std::unique_ptr<ScanThread> scanner;
    juce::AudioPluginFormatManager formatManager;
    juce::Array<juce::PluginDescription> descriptions;
    juce::TextButton scan {"Scan installed VST3"},load {"Load selected (mute)"},unloadBtn {"Unload (mute)"},bypass {"BYPASS ON"};
    juce::ComboBox plugins;
    juce::Label title,state,disclaimer;
};
} // namespace lah::ui
