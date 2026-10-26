#include <juce_gui_extra/juce_gui_extra.h>
#include "../ui/MainComponent.h"
#include "MutedStreamCheck.h"
#include "M32MutedStreamCheck.h"
#include "../ui/NativePluginAudit.h"

class LiveAudioHostApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "LiveAudioHost"; }
    const juce::String getApplicationVersion() override { return LAH_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override {
        const auto arguments=juce::StringArray::fromTokens(getCommandLineParameters(),true);
        // Native render tests never open hardware and must actually run even
        // when the user's normal application is already open.
        return (arguments.size()==2 && (arguments[0]=="--smoke-test" || arguments[0]=="--plugin-audio-check" || arguments[0]=="--inspect-audio")) ||
            (arguments.size()==4 && arguments[0]=="--m32-muted-check");
    }
    void anotherInstanceStarted(const juce::String&) override {
        if(window){window->setVisible(true);window->setMinimised(false);window->toFront(true);}
    }

    void initialise (const juce::String& commandLine) override
    {
        lookAndFeel.setColour (juce::ResizableWindow::backgroundColourId, juce::Colour (0xff111820));
        lookAndFeel.setColour (juce::Label::textColourId, juce::Colours::white);
        lookAndFeel.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff253449));
        lookAndFeel.setColour (juce::ComboBox::textColourId, juce::Colours::white);
        lookAndFeel.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff253449));
        lookAndFeel.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff354961));
        juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
        const auto arguments=juce::StringArray::fromTokens(commandLine,true);
        if(arguments.size()>0 && arguments[0]=="--m32-muted-check"){
            if(arguments.size()!=4 || !arguments[1].containsOnly("0123456789") || !arguments[2].containsOnly("0123456789") ||
               arguments[1].getIntValue()<1 || arguments[1].getIntValue()>8 || arguments[2].getIntValue()<1 || arguments[2].getIntValue()>30){
                setApplicationReturnValue(2);quit();return;
            }
            const auto destination=juce::File::getCurrentWorkingDirectory().getChildFile(arguments[3].unquoted());
            m32Check=std::make_unique<M32MutedStreamCheck>(arguments[1].getIntValue(),arguments[2].getIntValue(),destination,
                [this](int result){setApplicationReturnValue(result);quit();});
            m32Check->start();return;
        }
        if(arguments.size()>0 && arguments[0]=="--muted-stream-check"){
            if(arguments.size()!=3 || arguments[1].isEmpty() || !arguments[1].containsOnly("0123456789") ||
               arguments[1].getIntValue()<1 || arguments[1].getIntValue()>30){
                setApplicationReturnValue(2);quit();return;
            }
            const auto destination=juce::File::getCurrentWorkingDirectory().getChildFile(arguments[2].unquoted());
            streamCheck=std::make_unique<MutedStreamCheck>(arguments[1].getIntValue(),destination,
                [this](int result){setApplicationReturnValue(result);quit();});
            streamCheck->start();return;
        }
        if(arguments.size()>0 && arguments[0]=="--inspect-audio"){
            if(arguments.size()!=2){setApplicationReturnValue(2);quit();return;}
            const auto destination=juce::File::getCurrentWorkingDirectory().getChildFile(arguments[1].unquoted());
            juce::AudioDeviceManager manager;
            juce::Array<juce::var> backends;
            for(auto* type:manager.getAvailableDeviceTypes()){
                type->scanForDevices();
                auto backend=std::make_unique<juce::DynamicObject>();
                backend->setProperty("type",type->getTypeName());
                juce::Array<juce::var> inputs,outputs;
                for(const auto& name:type->getDeviceNames(true))inputs.add(name);
                for(const auto& name:type->getDeviceNames(false))outputs.add(name);
                backend->setProperty("inputs",juce::var(inputs));
                backend->setProperty("outputs",juce::var(outputs));
                backends.add(juce::var(backend.release()));
            }
            auto inventory=std::make_unique<juce::DynamicObject>();
            inventory->setProperty("version",LAH_VERSION_STRING);
            inventory->setProperty("timestamp",juce::Time::getCurrentTime().toISO8601(true));
           #if defined(JUCE_ASIO) && JUCE_ASIO
            inventory->setProperty("asio_compiled",true);
           #else
            inventory->setProperty("asio_compiled",false);
           #endif
            const bool closed=manager.getCurrentAudioDevice()==nullptr;
            inventory->setProperty("audio_device_opened",!closed);
            inventory->setProperty("scope","JUCE backend enumeration only; no stream, channel count, sample-rate, latency or hardware validation.");
            inventory->setProperty("backends",juce::var(backends));
            const auto json=juce::JSON::toString(juce::var(inventory.release()),false);
            const bool saved=destination.getParentDirectory().createDirectory() && destination.replaceWithText(json);
            setApplicationReturnValue(saved && closed?0:1);
            quit();return;
        }
        if(arguments.size()>0 && arguments[0]=="--plugin-audio-check"){
            if(arguments.size()!=2){setApplicationReturnValue(2);quit();return;}
            const auto destination=juce::File::getCurrentWorkingDirectory().getChildFile(arguments[1].unquoted());
            juce::String error;
            const bool ok=lah::ui::runNativePluginAudioAudit(destination,error);
            const auto errorFile=destination.getChildFile("audit-error.txt");
            if(ok)errorFile.deleteFile();
            else errorFile.replaceWithText(error);
            setApplicationReturnValue(ok?0:1);quit();return;
        }
        if(arguments.size()>0 && arguments[0]=="--smoke-test"){
            if(arguments.size()!=2){setApplicationReturnValue(2);quit();return;}
            const auto destination=juce::File::getCurrentWorkingDirectory().getChildFile(arguments[1].unquoted());
            auto component=std::make_unique<lah::ui::MainComponent>(false);
            juce::String error;
            const bool ok=component->writeSmokeArtifacts(destination,error);
            const auto errorFile=destination.getChildFile("smoke-error.txt");
            if(ok)errorFile.deleteFile();
            else errorFile.replaceWithText(error);
            setApplicationReturnValue(ok?0:1);
            quit();return;
        }
        window = std::make_unique<MainWindow>();
    }

    void shutdown() override
    {
        streamCheck.reset();
        m32Check.reset();
        window.reset();
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    void systemRequestedQuit() override { quit(); }

private:
    class MainWindow final : public juce::DocumentWindow
    {
    public:
        MainWindow() : DocumentWindow ("LiveAudioHost - Real-Time Audio",
                                       juce::Colour (0xff111820),
                                       DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new lah::ui::MainComponent(), true);
            setResizable (true, false);
            const auto* display=juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
            const auto area=display?display->userBounds.toNearestInt():juce::Rectangle<int>(0,0,1600,960);
            setResizeLimits (std::min(1280,area.getWidth()-32),std::min(800,area.getHeight()-48),2800,1800);
            centreWithSize (std::min(1600,area.getWidth()-32),std::min(960,area.getHeight()-48));
            setVisible (true);
        }
        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
    };

    juce::LookAndFeel_V4 lookAndFeel;
    std::unique_ptr<MainWindow> window;
    std::unique_ptr<MutedStreamCheck> streamCheck;
    std::unique_ptr<M32MutedStreamCheck> m32Check;
};

START_JUCE_APPLICATION (LiveAudioHostApplication)
