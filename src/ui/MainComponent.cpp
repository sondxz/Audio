#include "MainComponent.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include "../dsp/BuiltinSceneState.h"
#include "../audio/UsbReturnProfile.h"
#include "SessionState.h"

namespace lah::ui
{
namespace
{
constexpr auto background = 0xff0b0d11;
constexpr auto panel = 0xff171a20;
constexpr auto accent = 0xff22cfae;

void setupLabel (juce::Label& label, const juce::String& text, float size, juce::Colour colour)
{
    label.setText (text, juce::dontSendNotification);
    label.setFont (juce::FontOptions (size));
    label.setColour (juce::Label::textColourId, colour);
}

void writeRack(juce::XmlElement& root,const char* tag,const builtin::Controls& controls)
{
    session::writeRack(root,tag,controls);
}

void readRack(const juce::XmlElement& root,const char* tag,builtin::Controls& controls)
{
    builtin::SceneSnapshot rackState{};juce::String error;
    if(session::readRack(root,tag,rackState,error))builtin::recall(controls,rackState);
}
}

MainComponent::MainComponent(bool initialiseDevice) : deviceSelector (deviceManager, 0, 128, 0, 128,
                                                  false, false, false, false),
    deviceInitialisationEnabled(initialiseDevice)
{
    setOpaque (true);
    measurementPanel.onInputSelectionChanged=[this]{splPanel.refreshCaptureSelection();};
    splPanel.onInputSelectionChanged=[this]{measurementPanel.refreshCaptureSelection();};
    // Rack-style graphite/teal palette inspired by professional live plugin racks.
    // Independent appearance; does not copy third-party artwork or branding.
    setLookAndFeel(&rackSkin);
    setSize (1500, 920);
    setupLabel (heading, juce::String::fromUTF8("LiveAudioHost •"), 19.0f, juce::Colours::white);
    setupLabel (subheading, "M32R USB / 1-8 CH", 12.0f,
                juce::Colour (0xffa6b8cc));
    setupLabel (statusLabel, juce::String::fromUTF8("Đang khởi tạo thiết bị..."), 15.0f, juce::Colours::white);
    setupLabel (diagnosticLabel, juce::String::fromUTF8("Diagnostics: chưa có dữ liệu"), 13.0f, juce::Colour (0xffc7d3e3));
    setupLabel (noticeLabel, juce::String::fromUTF8("AN TOÀN: mặc định mute. Hạ gain / tắt loa trước khi bật pass-through."),
                14.0f, juce::Colour (0xffffcb86));
    setupLabel (inputsTitle, "INPUT  /  dBFS peak", 15.0f, juce::Colours::white);
    setupLabel (outputsTitle, "OUTPUT  /  dBFS peak", 15.0f, juce::Colours::white);

    for (auto* label : { &heading, &subheading, &statusLabel, &diagnosticLabel, &noticeLabel })
        addAndMakeVisible (*label);
    setupLabel(pageTitle,"Mixer & routing",24,juce::Colours::white);
    setupLabel(pageHint,"",14,juce::Colour(0xff99adbd));
    setupLabel(sessionSummary,"No audio device",13,juce::Colour(0xffa8bac8));
    setupLabel(audioStateLabel,"OUTPUT MUTED",13,juce::Colour(accent));
    setupLabel(buildBadge,"LAB BUILD",12,juce::Colour(0xffeab771));
    for(auto* label:{&pageTitle,&pageHint,&sessionSummary,&audioStateLabel,&buildBadge})addAndMakeVisible(*label);
    const char* const navigationNames[]{"THIET BI","MIXER","FX","VST3","ANALYZER","SPL",
        "FIR","SUB ARRAY","CONG CU","COVERAGE","PHASE","PLUGIN","BUS","SCENES"};
    for(std::size_t index=0;index<navigation.size();++index){
        auto& button=navigation[index];addAndMakeVisible(button);
        button.setButtonText(navigationNames[index]);button.getProperties().set("navigation",true);
        button.setClickingTogglesState(false);button.onClick=[this,index]{selectPage(static_cast<int>(index));};
    }
    const char* const groupNames[]{"AUDIO ENGINE","MEASUREMENT","SYSTEM DESIGN"};
    for(std::size_t index=0;index<navigationGroups.size();++index){
        setupLabel(navigationGroups[index],groupNames[index],12,juce::Colour(0xff718598));addAndMakeVisible(navigationGroups[index]);
    }
    addAndMakeVisible(detailsButton);
    detailsButton.onClick=[this]{diagnosticsExpanded=!diagnosticsExpanded;detailsButton.setToggleState(diagnosticsExpanded,juce::dontSendNotification);resized();repaint();};
    diagnosticLabel.setVisible(false);

    addAndMakeVisible (tabs);
    tabs.setTabBarDepth(0);
    tabs.addTab ("Devices / ASIO", juce::Colour (0xff192330), &devicePage, false);
    juce::Component* const pages[]{&mixerPanel,&effectsPanel,&pluginPanel,&measurementPanel,&splPanel,
        &firPanel,&subPanel,&engineeringPanel,&frontFillPanel,&phasePanel,&builtinPanel,&busMasterPanel,&sceneLibrary};
    const char* const pageNames[]{"Mixer / DSP / Routing","Delay / Reverb FX","VST3 Experimental",
        "RTA / Transfer Function","SPL / Calibration","FIR DESIGN / LAB INSERT","Sub Array 2D / 3D",
        "Engineering Calculators","Front-Fill / Coverage","Phase / Waveform Lab","Built-in Processors","Bus / Master","Scenes / Presets"};
    const int minimumWidths[]{1100,900,800,1000,1000,1000,1100,1000,1000,1100,1000,1100,1100};
    const int minimumHeights[]{650,600,420,580,580,560,680,680,680,680,650,720,700};
    for(std::size_t index=0;index<toolViewports.size();++index){
        toolViewports[index].attach(*pages[index],minimumWidths[index],minimumHeights[index]);
        tabs.addTab(pageNames[index],juce::Colour(panel),&toolViewports[index],false);
    }
    selectPage(1);
    mixerPanel.onOpenBuiltin=[this](int channel,int slot,bool output){openNativePlugin(channel,slot,output);};
    mixerPanel.onToggleAudio=[this]{passButton.triggerClick();};
    mixerPanel.onPanic=[this]{panicButton.triggerClick();};
    sceneLibrary.onCaptureScene=[this]{return captureSession();};
    sceneLibrary.onRecallScene=[this](const juce::XmlElement& xml,juce::String& error){return recallSession(xml,error);};
    sceneLibrary.onCapturePreset=[this](bool chain,bool output,int channel,int slot,const juce::String& name){
        return session::capturePreset(output?engine.outputBuiltinControls():engine.builtinControls(),channel,slot,chain,name);
    };
    sceneLibrary.onApplyPreset=[this](const juce::XmlElement& xml,bool output,int channel,int slot,juce::String& error){
        session::NativePreset preset;if(!session::decodePreset(xml,preset,error))return false;
        engine.setPassThroughEnabled(false);session::applyPreset(preset,output?engine.outputBuiltinControls():engine.builtinControls(),channel,slot);
        refreshSessionControls();return true;
    };
    devicePage.addAndMakeVisible (deviceSelector);
    devicePage.addAndMakeVisible (inputViewport);
    devicePage.addAndMakeVisible (outputViewport);
    devicePage.addAndMakeVisible (inputsTitle);
    devicePage.addAndMakeVisible (outputsTitle);
    setupLabel(usbTitle,juce::String::fromUTF8("M32R • USB SEND / RETURN"),18,juce::Colours::white);
    setupLabel(usbDescription,juce::String::fromUTF8("Mixer → USB IN → plugin → USB OUT → Card return trên mixer"),14,juce::Colour(0xff9eb2c2));
    setupLabel(usbStatus,juce::String::fromUTF8("Chọn ASIO của card DN32, chọn 1–8 kênh rồi Áp dụng USB."),13,juce::Colour(0xff7ce2d5));
    connectMidasButton.setButtonText(juce::String::fromUTF8("TÌM M32R / ASIO"));
    applyUsbButton.setButtonText(juce::String::fromUTF8("ÁP DỤNG USB"));
    for(int count=1;count<=8;++count)usbChannelSelect.addItem(juce::String(count)+juce::String::fromUTF8(" kênh USB"),count);
    usbChannelSelect.setSelectedId(8,juce::dontSendNotification);
    for(auto* component:std::array<juce::Component*,6>{&usbTitle,&usbDescription,&usbStatus,&usbChannelSelect,&connectMidasButton,&applyUsbButton})devicePage.addAndMakeVisible(*component);
    connectMidasButton.onClick=[this]{configureUsbReturn(true);};
    applyUsbButton.onClick=[this]{configureUsbReturn(false);};
    applyUsbButton.setTooltip(juce::String::fromUTF8("MUTE audio, mở USB IN/OUT 1–N, đặt tuyến IN n → OUT n. Giữ các plugin đã chọn."));
    inputViewport.setViewedComponent (&inputMeters, false);
    outputViewport.setViewedComponent (&outputMeters, false);
    inputViewport.setScrollBarsShown (true, false);
    outputViewport.setScrollBarsShown (true, false);
    inputViewport.setScrollBarThickness (11);
    outputViewport.setScrollBarThickness (11);

    for (auto* button : { &passButton, &panicButton, &saveButton, &saveSceneButton, &loadSceneButton, &qaLogButton })
        addAndMakeVisible (*button);
    passButton.setColour (juce::TextButton::buttonColourId, juce::Colour (accent));
    passButton.setColour (juce::TextButton::textColourOffId, juce::Colour (background));
    panicButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xffad3b4c));
    saveButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff364b62));

    passButton.onClick = [this]
    {
        if (engine.isPassThroughEnabled())
            engine.setPassThroughEnabled (false);
        else
            askToEnableAudio();
        refreshButtonStates();
    };
    panicButton.onClick = [this]
    {
        engine.setPassThroughEnabled (false); // immediate atomic mute
        refreshButtonStates();
    };
    saveButton.onClick = [this] { saveDeviceSetup(); };
    saveSceneButton.onClick = [this] { saveScene(); };
    qaLogButton.onClick = [this] {toggleQaLogging();};
    loadSceneButton.onClick = [this] {
        if (! engine.isPassThroughEnabled()) { loadScene(); return; }
        auto safe = juce::Component::SafePointer<MainComponent> (this);
        juce::AlertWindow::showAsync (juce::MessageBoxOptions()
            .withIconType (juce::MessageBoxIconType::WarningIcon)
            .withTitle (juce::String::fromUTF8("Load Scene sẽ MUTE audio trước"))
            .withMessage (juce::String::fromUTF8("Đường tiếng đang chạy. Scene recall sẽ tắt audio và không tự bật lại."))
            .withButton (juce::String::fromUTF8("Mute và Load")).withButton (juce::String::fromUTF8("Hủy")).withAssociatedComponent (this),
            [safe] (int result) { if (result == 1) if (auto* self = safe.getComponent()) {
                self->engine.setPassThroughEnabled (false);self->loadScene();self->refreshButtonStates();
            }});
    };

    if(deviceInitialisationEnabled){
        std::unique_ptr<juce::XmlElement> saved;
        if (auto file = getSettingsFile(); file.existsAsFile())
            saved = juce::XmlDocument::parse (file);
        if(saved)usbChannelSelect.setSelectedId(std::clamp(saved->getIntAttribute("lahUsbChannels",8),1,8),juce::dontSendNotification);

        // Saved setup may activate many channels. If there is no saved setup,
        // JUCE picks a modest default; the user can enable more channels below.
        initError = deviceManager.initialise (2, 2, saved.get(), true);
        deviceManager.addAudioCallback (&engine);
    }else{
        statusLabel.setText("NATIVE RENDER SMOKE TEST / AUDIO DEVICE DISABLED",juce::dontSendNotification);
        diagnosticLabel.setText("No driver stream, calibration, plug-in or hardware validation in this mode.",juce::dontSendNotification);
    }
    if(deviceInitialisationEnabled){
        sceneLibrary.setStorageDirectory(getSettingsFile().getParentDirectory().getChildFile("library"));
        const auto legacy=getSceneFile();
        if(legacy.existsAsFile()){
            juce::String error;const auto xml=session::readFile(legacy,error);
            if(xml&&session::decode(*xml,error)){
                if(!sceneLibrary.hasQuickScene(0))sceneLibrary.storeQuickScene(0,*xml,"Previous scene",error);
                recallSession(*xml,error); // restore session while transport remains muted
            }
        }
    }
    refreshButtonStates();
    startTimerHz (20);
}

void MainComponent::selectPage(int index)
{
    index=juce::jlimit(0,std::max(0,tabs.getNumTabs()-1),index);tabs.setCurrentTabIndex(index,false);
    for(std::size_t item=0;item<navigation.size();++item)navigation[item].setToggleState(static_cast<int>(item)==index,juce::dontSendNotification);
    const char* const titles[]{"Audio device","Mixer & routing","Delay & reverb","VST3 insert","Transfer function & RTA",
        "SPL & calibration","FIR designer","Subwoofer array","Engineering calculators","Speaker coverage","Phase & waveform","Built-in processors","Bus / Master","Scenes / Presets"};
    const char* const hints[]{"Choose the driver, channels and buffer. Input and output meters show actual device peaks.",
        "USB IN 1-8  >  INPUT INSERTS  >  ROUTING  >  OUTPUT INSERTS  >  USB RETURN", "Input sends feed the stereo delay and reverb returns on OUT 1/2.",
        "Experimental stereo insert on OUT 1/2. Load while audio is muted.","Inspect reference / microphone measurements, phase, magnitude and coherence.",
        "Calibrate the measurement chain before reading acoustic sound levels.","Design a linear-phase filter and inspect its response before insertion.",
        "Compare source geometry, polarity, delay and simulated probe responses.","Electrical, acoustic and signal-chain calculations with explicit input units.",
        "Plan source positions and nominal beam coverage. Click the map to inspect a path.","Explore how frequency, phase and delay change the sum of sine sources.",
        "Four ordered slots on every input and output. Choose a module, edit parameters and enable the slot.",
        "Eight software buses and a stereo master. These routes are separate from the mixer hardware.",
        "Eight quick scenes, import/export and native plugin/chain presets. Recall always mutes audio."};
    pageTitle.setText(titles[index],juce::dontSendNotification);pageHint.setText(hints[index],juce::dontSendNotification);
}

void MainComponent::configureUsbReturn(bool findMidas)
{
    engine.setPassThroughEnabled(false);
    refreshButtonStates();
    if(!deviceInitialisationEnabled){
        usbStatus.setText("Preview: audio hardware is disabled.",juce::dontSendNotification);return;
    }
    const int count=std::clamp(usbChannelSelect.getSelectedId(),1,8);
    juce::String deviceName;
    if(findMidas){
        juce::AudioIODeviceType* asio=nullptr;
        for(auto* type:deviceManager.getAvailableDeviceTypes())if(type->getTypeName()=="ASIO")asio=type;
        if(asio){
            asio->scanForDevices();
            for(const auto& name:asio->getDeviceNames())
                if(name.containsIgnoreCase("DN32") || name.containsIgnoreCase("M32")){deviceName=name;break;}
        }
        if(deviceName.isEmpty()){
            usbStatus.setText(juce::String::fromUTF8("Chưa tìm thấy ASIO DN32/M32. Kết nối USB card, cài driver đúng card rồi thử lại; hoặc chọn driver bên dưới."),juce::dontSendNotification);
            return;
        }
        deviceManager.setCurrentAudioDeviceType("ASIO",true);
    }
    auto setup=deviceManager.getAudioDeviceSetup();
    if(deviceName.isNotEmpty()){setup.inputDeviceName=deviceName;setup.outputDeviceName=deviceName;setup.sampleRate=0;setup.bufferSize=0;}
    else if(setup.inputDeviceName.isEmpty() || setup.outputDeviceName.isEmpty()){
        usbStatus.setText(juce::String::fromUTF8("Chọn thiết bị có cả ngõ vào và ngõ ra trước khi áp dụng USB."),juce::dontSendNotification);return;
    }
    setup.useDefaultInputChannels=false;setup.useDefaultOutputChannels=false;
    setup.inputChannels.clear();setup.outputChannels.clear();
    setup.inputChannels.setRange(0,count,true);setup.outputChannels.setRange(0,count,true);
    const auto error=deviceManager.setAudioDeviceSetup(setup,true);
    auto* device=deviceManager.getCurrentAudioDevice();
    if(error.isNotEmpty() || device==nullptr){
        usbStatus.setText(juce::String::fromUTF8("Không mở được USB: ")+error,juce::dontSendNotification);return;
    }
    if(device->getActiveInputChannels()!=setup.inputChannels || device->getActiveOutputChannels()!=setup.outputChannels){
        usbStatus.setText(juce::String::fromUTF8("Driver chưa mở đủ IN/OUT 1–")+juce::String(count)+juce::String::fromUTF8(". Kiểm tra ASIO/card; audio vẫn MUTE."),juce::dontSendNotification);return;
    }
    audio::applyUsbReturnProfile(engine.controls(),count);
    mixerPanel.setUsbChannelCount(count);mixerPanel.refreshControls();
    usbStatus.setText(device->getName()+" | USB IN/OUT 1-"+juce::String(count)+juce::String::fromUTF8(" đã mở. Chèn plugin, kiểm tra Card return trên M32R rồi bật audio."),juce::dontSendNotification);
    saveDeviceSetup();
    noticeLabel.setText(juce::String::fromUTF8("Đã đặt tuyến USB IN n → OUT n; audio MUTE. Áp dụng USB thay thế routing và tắt FX send chung."),juce::dontSendNotification);
    selectPage(1);
}

MainComponent::~MainComponent()
{
    stopTimer();
    nativePluginWindows.clear();
    setLookAndFeel(nullptr);
    if (qaLogStream) {qaLogStream->flush();qaLogStream.reset();}
    engine.setPassThroughEnabled (false);
    deviceManager.removeAudioCallback (&engine);
    if(deviceInitialisationEnabled)saveDeviceSetup();
    inputViewport.setViewedComponent (nullptr, false);
    outputViewport.setViewedComponent (nullptr, false);
}

void MainComponent::openNativePlugin(int channel,int slot,bool output)
{
    channel=juce::jlimit(0,31,channel);slot=juce::jlimit(0,3,slot);
    for(auto& window:nativePluginWindows)if(window->matches(channel,slot,output)){
        window->showEditor();return;
    }
    auto window=std::make_unique<NativePluginWindow>(engine,channel,slot,output,deviceInitialisationEnabled);
    window->showEditor();nativePluginWindows.push_back(std::move(window));
}

juce::File MainComponent::getSettingsFile() const
{
    if(!deviceInitialisationEnabled)return smokeStorage.getFullPathName().isNotEmpty()?smokeStorage.getChildFile("audio-device.xml"):juce::File{};
    const auto folder = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                            .getChildFile ("LiveAudioHost");
    return folder.getChildFile ("audio-device.xml");
}

void MainComponent::saveDeviceSetup()
{
    if(!deviceInitialisationEnabled)return;
    auto xml = deviceManager.createStateXml();
    if (xml == nullptr)
    {
        // Default settings are valid even when JUCE chooses not to serialise them.
        noticeLabel.setText (juce::String::fromUTF8("Cấu hình mặc định - không có tùy chỉnh cần lưu."),
                             juce::dontSendNotification);
        return;
    }
    xml->setAttribute("lahUsbChannels",usbChannelSelect.getSelectedId());
    auto file = getSettingsFile();
    auto parent = file.getParentDirectory();
    if (! parent.exists() && ! parent.createDirectory())
    {
        noticeLabel.setText (juce::String::fromUTF8("Không thể tạo thư mục lưu thiết lập."), juce::dontSendNotification);
        return;
    }
    // Replace via temporary sibling so a failed write doesn't erase last good settings.
    juce::TemporaryFile temp (file);
    if (xml->writeTo (temp.getFile()) && temp.overwriteTargetFileWithTemporary())
        noticeLabel.setText (juce::String::fromUTF8("Đã lưu cấu hình thiết bị."), juce::dontSendNotification);
    else
        noticeLabel.setText (juce::String::fromUTF8("Lỗi ghi cấu hình. Kiểm tra quyền thư mục."), juce::dontSendNotification);
}


juce::File MainComponent::getSceneFile() const
{
    if(!deviceInitialisationEnabled && smokeStorage.getFullPathName().isEmpty())return {};
    return getSettingsFile().getParentDirectory().getChildFile ("live-scene.xml");
}

std::unique_ptr<juce::XmlElement> MainComponent::captureSession()
{
    return session::capture(engine.controls(),engine.builtinControls(),engine.outputBuiltinControls(),mixerPanel.getChannelNames());
}

void MainComponent::refreshSessionControls()
{
    builtinPanel.refreshControls();mixerPanel.refreshControls();effectsPanel.refreshControls();busMasterPanel.refreshControls();
    measurementPanel.refreshCaptureSelection();splPanel.refreshCaptureSelection();
    for(auto& window:nativePluginWindows)if(auto* editor=window->getEditor())editor->refreshControls();
    refreshButtonStates();
}

bool MainComponent::recallSession(const juce::XmlElement& xml,juce::String& error)
{
    const auto state=session::decode(xml,error);if(!state)return false;
    engine.setPassThroughEnabled(false);
    session::apply(*state,engine.controls(),engine.builtinControls(),engine.outputBuiltinControls());
    mixerPanel.setChannelNames(state->names);refreshSessionControls();
    noticeLabel.setText(juce::String::fromUTF8("Đã recall session. Audio vẫn MUTE; kiểm tra routing trước khi bật."),juce::dontSendNotification);
    return true;
}

void MainComponent::saveScene()
{
    const auto file=getSceneFile();if(file.getFullPathName().isEmpty())return;
    const auto xml=captureSession();juce::String error;
    if(!xml||!session::writeFile(file,*xml,error))noticeLabel.setText("Scene save failed: "+error,juce::dontSendNotification);
    else noticeLabel.setText(juce::String::fromUTF8("Đã lưu session hiện tại. Dùng SCENES để quản lý tám scene và preset."),juce::dontSendNotification);
}

void MainComponent::loadScene()
{
    const auto file=getSceneFile();if(file.getFullPathName().isEmpty())return;
    juce::String error;const auto xml=session::readFile(file,error);
    if(!xml||!recallSession(*xml,error))noticeLabel.setText("Scene recall failed: "+error,juce::dontSendNotification);
}

void MainComponent::askToEnableAudio()
{
    if (engine.isSafetyLatched()) {
        auto safe = juce::Component::SafePointer<MainComponent>(this);
        juce::AlertWindow::showAsync(juce::MessageBoxOptions()
            .withIconType(juce::MessageBoxIconType::WarningIcon)
            .withTitle(juce::String::fromUTF8("AUDIO SAFETY LOCK - đã tự MUTE"))
            .withMessage(juce::String::fromUTF8("Ghi nhận ít nhất 8 callback liên tiếp vượt deadline hoặc thông số callback lỗi. "
                         "Không thể xác nhận đã có XRun do driver. Hãy kiểm tra QA log/CPU/buffer trước khi mở khóa. "
                         "AUDIO SẼ VẪN MUTE sau khi mở khóa."))
            .withButton(juce::String::fromUTF8("Tôi đã kiểm tra - RESET LOCK"))
            .withButton(juce::String::fromUTF8("Hủy"))
            .withAssociatedComponent(this),
            [safe](int choice) {if(choice==1)if(auto* self=safe.getComponent()) {
                self->engine.setPassThroughEnabled(false);
                self->engine.acknowledgeSafetyWhileMuted();
                self->refreshButtonStates();
            }});
        return;
    }
    if (showingConfirmation || deviceManager.getCurrentAudioDevice() == nullptr || ! engine.isDeviceRunning())
        return;
    showingConfirmation = true;
    auto safe = juce::Component::SafePointer<MainComponent> (this);
    const auto deviceGenerationAtConfirmation = engine.getDeviceGeneration();
    juce::AlertWindow::showAsync (juce::MessageBoxOptions()
        .withIconType (juce::MessageBoxIconType::WarningIcon)
        .withTitle (juce::String::fromUTF8("Xác nhận BẬT âm thanh"))
        .withMessage (juce::String::fromUTF8("Đây là tín hiệu AUDIO THẬT. Giữ hệ thống loa/monitor ở mức nhỏ trước khi bật. "
                      "Matrix mặc định 1:1; DSP và FX có thể thay đổi routing/gain. Safety clip guard không thay thế limiter loa."))
        .withButton (juce::String::fromUTF8("Xác nhận BẬT"))
        .withButton (juce::String::fromUTF8("Hủy"))
        .withAssociatedComponent (this),
        [safe, deviceGenerationAtConfirmation] (int result)
        {
            if (auto* self = safe.getComponent())
            {
                self->showingConfirmation = false;
                if (result == 1 && self->engine.getDeviceGeneration() == deviceGenerationAtConfirmation
                    && self->engine.isDeviceRunning() &&
                    self->deviceManager.getCurrentAudioDevice() != nullptr)
                    self->engine.setPassThroughEnabled (true);
                self->refreshButtonStates();
            }
        });
}

void MainComponent::refreshButtonStates()
{
    const bool deviceAvailable = deviceManager.getCurrentAudioDevice() != nullptr && engine.isDeviceRunning();
    passButton.setEnabled (deviceAvailable && ! showingConfirmation);
    passButton.setButtonText (engine.isSafetyLatched() ? "SAFETY LOCK / RESET" :
                              engine.isPassThroughEnabled() ? juce::String::fromUTF8("TẮT AUDIO") : juce::String::fromUTF8("BẬT AUDIO"));
    panicButton.setEnabled (true);
    audioStateLabel.setText(engine.isSafetyLatched()?"SAFETY LOCKED":engine.isPassThroughEnabled()?"AUDIO LIVE":"OUTPUT MUTED",juce::dontSendNotification);
    audioStateLabel.setColour(juce::Label::textColourId,juce::Colour(engine.isSafetyLatched()?0xffff6b7e:engine.isPassThroughEnabled()?0xffffc77b:accent));
}

void MainComponent::timerCallback()
{
    std::array<float, audio::LiveAudioEngine::visualChannelLimit> inputLevels {}, outputLevels {};
    for (int i = 0; i < audio::LiveAudioEngine::visualChannelLimit; ++i)
    {
        inputLevels[static_cast<std::size_t> (i)] = engine.consumeInputPeak (i);
        outputLevels[static_cast<std::size_t> (i)] = engine.consumeOutputPeak (i);
    }
    inputMeters.update (inputLevels.data(), engine.getInputChannelCount());
    outputMeters.update (outputLevels.data(), engine.getOutputChannelCount());
    mixerPanel.setUsbChannelCount(engine.isDeviceRunning()?std::clamp(std::min(engine.getInputChannelCount(),engine.getOutputChannelCount()),0,8):8);
    mixerPanel.setMeterLevels(inputLevels.data(),engine.getInputChannelCount(),outputLevels.data(),engine.getOutputChannelCount());

    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        const auto sampleRate = device->getCurrentSampleRate();
        const auto buffer = device->getCurrentBufferSizeSamples();
        const auto inputLatency = device->getInputLatencyInSamples();
        const auto outputLatency = device->getOutputLatencyInSamples();
        const auto toMs = [sampleRate] (int samples)
        {
            return sampleRate > 0.0 && samples >= 0
                       ? juce::String (1000.0 * samples / sampleRate, 2) + " ms"
                       : "n/a";
        };
        const auto details = "Driver: " + deviceManager.getCurrentAudioDeviceType()
                           + " | " + device->getName()
                           + " | " + juce::String (sampleRate, 0) + " Hz"
                           + " | Buffer: " + juce::String (buffer) + " samples";
        statusLabel.setText (details, juce::dontSendNotification);
        sessionSummary.setText("CPU "+juce::String(100*deviceManager.getCpuUsage(),1)+"%   |   "+juce::String(sampleRate/1000,1)+" kHz   |   "+juce::String(buffer)+" smp   |   XRun "+juce::String(deviceManager.getXRunCount()),juce::dontSendNotification);
        sessionSummary.setColour(juce::Label::textColourId,juce::Colour(deviceManager.getXRunCount()>0?0xffeab771:0xffa8bac8));
        diagnosticLabel.setText (
            "In latency (driver): " + toMs (inputLatency)
            + "    |    Out latency (driver): " + toMs (outputLatency)
            + "    |    Buffer: " + toMs (buffer)
            + "\nVST3 reported latency: " + toMs (engine.getPluginLatencySamples())
            + "    |    OUT3-32 compensation (reported): " + toMs(engine.getAppliedCompensationSamples())
            + "    |    Max speaker delay (user): " + juce::String ([this] {
                  float d = 0.0f;
                  for (const auto& v : engine.controls().outputDelayMs)
                      d = std::max (d, v.load (std::memory_order_relaxed));
                  return d;
              }(), 2) + " ms"
            + "\nCPU (JUCE): " + juce::String (100.0 * deviceManager.getCpuUsage(), 1) + "%"
            + "    |    XRuns (reported/estimated): " + juce::String (deviceManager.getXRunCount())
            + "    |    Callbacks: " + juce::String (static_cast<juce::int64> (engine.getCallbackCount()))
            + "    |    Device errors: " + juce::String (engine.getErrorCount())
            + "\nMax callback: " + juce::String(engine.getLongestCallbackMs(),3) + " ms"
            + "    |    Over deadline: " + juce::String(static_cast<juce::int64>(engine.getDeadlineOverruns()))
            + "    |    Overrun streak: " + juce::String(engine.getConsecutiveProcessingOverruns())
            + (engine.isSafetyLatched() ? "    |    SAFETY LOCKED: OUTPUT MUTED" : ""),
            juce::dontSendNotification);
    }
    else
    {
        sessionSummary.setText("Audio device offline",juce::dontSendNotification);
        statusLabel.setText ((initError.isNotEmpty() ? juce::String (juce::String::fromUTF8("Không mở được thiết bị: ")) + initError : juce::String::fromUTF8("KHÔNG CÓ AUDIO DEVICE - chọn thiết bị ở bên dưới.")), juce::dontSendNotification);
        diagnosticLabel.setText (juce::String::fromUTF8("Không có driver latency / CPU / XRuns để đo."), juce::dontSendNotification);
    }
    writeQaLog();
    refreshButtonStates();
}

void MainComponent::toggleQaLogging()
{
    if (qaLogStream) {
        qaLogStream->flush();
        qaLogStream.reset();
        qaLogButton.setButtonText("QA LOG");
        noticeLabel.setText(juce::String::fromUTF8("QA log đã đóng trong Documents/LiveAudioHost. Chỉ là nhật ký trạng thái, không chứng minh đã đạt test."),juce::dontSendNotification);
        return;
    }
    auto folder=juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                     .getChildFile("LiveAudioHost");
    if (!folder.isDirectory() && !folder.createDirectory()) {
        noticeLabel.setText(juce::String::fromUTF8("Không thể tạo thư mục QA logs."),juce::dontSendNotification);return;
    }
    const auto target=folder.getChildFile("qa-session-"+
        juce::Time::getCurrentTime().formatted("%Y%m%d-%H%M%S")+".csv").getNonexistentSibling();
    qaLogStream=target.createOutputStream();
    if (!qaLogStream || !qaLogStream->openedOk()) {
        qaLogStream.reset();noticeLabel.setText(juce::String::fromUTF8("Không thể mở file QA CSV."),juce::dontSendNotification);return;
    }
    qaLogStream->writeText("timestamp_utc,driver,device,sample_rate,buffer_samples,input_channels,output_channels,muted,safety_locked,device_generation,callback_count,deadline_overruns,consecutive_overruns,max_callback_ms,juce_xruns,device_xruns,device_errors,cpu_percent\n",false,false,nullptr);
    lastQaLogMs=0;
    qaLogButton.setButtonText("STOP LOG");
    noticeLabel.setText("QA recording: "+target.getFullPathName()+juce::String::fromUTF8(" (log do GUI ghi mỗi giây)"),juce::dontSendNotification);
}

void MainComponent::writeQaLog()
{
    if (!qaLogStream) return;
    const auto now=juce::Time::currentTimeMillis();
    if(lastQaLogMs!=0 && static_cast<std::uint64_t>(now)-lastQaLogMs<1000)return;
    lastQaLogMs=static_cast<std::uint64_t>(now);
    auto* device=deviceManager.getCurrentAudioDevice();
    const auto escape=[](juce::String v){return "\""+v.replace("\"","\"\"")+"\"";};
    juce::String line=escape(juce::Time::getCurrentTime().toISO8601(true)) +","+
        escape(deviceManager.getCurrentAudioDeviceType())+","+
        escape(device?device->getName():"none")+","+
        juce::String(device?device->getCurrentSampleRate():0.0)+","+
        juce::String(device?device->getCurrentBufferSizeSamples():0)+","+
        juce::String(engine.getInputChannelCount())+","+
        juce::String(engine.getOutputChannelCount())+","+
        juce::String(engine.isPassThroughEnabled()?0:1)+","+
        juce::String(engine.isSafetyLatched()?1:0)+","+
        juce::String(static_cast<juce::int64>(engine.getDeviceGeneration()))+","+
        juce::String(static_cast<juce::int64>(engine.getCallbackCount()))+","+
        juce::String(static_cast<juce::int64>(engine.getDeadlineOverruns()))+","+
        juce::String(engine.getConsecutiveProcessingOverruns())+","+
        juce::String(engine.getLongestCallbackMs(),5)+","+
        juce::String(deviceManager.getXRunCount())+","+
        juce::String(device?device->getXRunCount():-1)+","+
        juce::String(engine.getErrorCount())+","+
        juce::String(deviceManager.getCpuUsage()*100.0,2)+"\n";
    qaLogStream->writeText(line,false,false,nullptr);
    qaLogStream->flush(); // GUI thread only; never the callback
}

bool MainComponent::writeSmokeArtifacts(const juce::File& directory,juce::String& error)
{
    if(deviceInitialisationEnabled || deviceManager.getCurrentAudioDevice()!=nullptr || engine.isDeviceRunning()){
        error="Smoke rendering requires MainComponent(false) with no audio device.";return false;
    }
    stopTimer();
    if(!directory.isDirectory() && !directory.createDirectory()){
        error="Cannot create smoke artifact directory: "+directory.getFullPathName();return false;
    }
    smokeStorage=directory.getChildFile("session-validation");
    sceneLibrary.setStorageDirectory(smokeStorage.getChildFile("library"));
    if(!mixerPanel.runWorkflowValidation(error)||!busMasterPanel.runControlValidation(error)||!sceneLibrary.runValidation(error))return false;
    // Exercise the same XML helpers as Save/Load, including the highest new ID
    // on an output slot and legacy scenes that have no output rack.
    builtin::Controls xmlInput,xmlOutput,restoredInput,restoredOutput;
    builtin::configureSlot(xmlInput.slots[7][2],builtin::Analog,true);
    builtin::configureSlot(xmlOutput.slots[7][3],builtin::FourKLine,true);
    xmlOutput.slots[7][3].params[1].store(3.0f);
    juce::XmlElement sceneCheck("LiveAudioHostScene");
    writeRack(sceneCheck,"BuiltinRack",xmlInput);writeRack(sceneCheck,"OutputBuiltinRack",xmlOutput);
    const auto parsedCheck=juce::XmlDocument::parse(sceneCheck.toString());
    if(!parsedCheck){error="Scene XML serialization failed.";return false;}
    readRack(*parsedCheck,"BuiltinRack",restoredInput);readRack(*parsedCheck,"OutputBuiltinRack",restoredOutput);
    if(restoredInput.slots[7][2].type.load()!=builtin::Analog ||
       restoredOutput.slots[7][3].type.load()!=builtin::FourKLine ||
       !restoredOutput.slots[7][3].enabled.load() || restoredOutput.slots[7][3].params[1].load()!=3.0f){
        error="Input/output scene XML round-trip failed.";return false;
    }
    juce::XmlElement legacyCheck("LiveAudioHostScene");readRack(legacyCheck,"OutputBuiltinRack",restoredOutput);
    if(restoredOutput.slots[7][3].type.load()!=builtin::None){error="Legacy scene output rack clear failed.";return false;}

    if(!directory.isDirectory() && !directory.createDirectory()){
        error="Cannot create smoke artifact directory: "+directory.getFullPathName();return false;
    }
    juce::String manifest="png,tab_index,tab_name,content_width,content_height\n";
    juce::String report="LiveAudioHost "+juce::String(LAH_VERSION_STRING)+" native JUCE render smoke\n"
        "Scope: component construction and PNG rendering, not audio streaming or hardware acceptance.\n"
        "Content layouts: default 1350x850, minimum 1150x720 (excluding OS window decorations).\n"
        "Bounds warnings flag transformed visible-child bounds extending beyond their immediate parent; viewport scrolling is allowed and controls inside it are checked.\n"
        "Tolerance: at most one pixel of JUCE tab-button rounding within TabbedButtonBar; no tolerance for application controls.\n\n";
    const auto names=tabs.getTabNames();
    juce::PNGImageFormat png;
    int warningCount=0,imageCount=0;
    for(const auto size:{juce::Point<int>{1350,850},juce::Point<int>{1150,720}}){
        setSize(size.x,size.y);
        for(int index=0;index<tabs.getNumTabs();++index){
            selectPage(index);
            const int scenarios=index==1?2:size.x!=1350?(index==11?2:0):index==7?7:index==8?12:index==9?6:index==10?5:index==11?builtin::moduleCount*2:0;
            for(int scenario=-1;scenario<scenarios;++scenario){
            juce::Component* target=this;
            if(scenario>=0){
                if(index==1){
                    mixerPanel.setChannelViewCount(scenario==1?16:8);
                    for(int channel=0;channel<8;++channel)
                        builtin::configureSlot(engine.builtinControls().slots[static_cast<std::size_t>(channel)][0],channel+1,true);
                    builtin::configureSlot(engine.outputBuiltinControls().slots[0][0],builtin::FourKLine,true);
                    builtin::configureSlot(engine.outputBuiltinControls().slots[0][1],builtin::Analog,true);
                    mixerPanel.refreshControls();
                }
                else if(index==7){subPanel.setValidationScenario(scenario);target=&subPanel;}
                else if(index==8){engineeringPanel.setValidationScenario(scenario);target=&engineeringPanel;}
                else if(index==9){frontFillPanel.setValidationScenario(scenario);target=&frontFillPanel;}
                else if(index==10){phasePanel.setTestScenario(scenario);target=&phasePanel;}
                else if(index==11){builtinPanel.setValidationScenario(size.x==1350?scenario:scenario+2);target=&builtinPanel;}
            }
            const auto filename=index==1&&scenario==1?"mixer-16ch-"+juce::String(size.x)+"x"+juce::String(size.y)+".png":juce::String(size.x)+"x"+juce::String(size.y)+"-tab-"+
                juce::String(index+1).paddedLeft('0',2)+(scenario<0?juce::String{}:"-mode-"+juce::String(scenario+1).paddedLeft('0',2))+".png";
            const auto snapshot=target->createComponentSnapshot(target->getLocalBounds());
            auto stream=directory.getChildFile(filename).createOutputStream();
            if(!snapshot.isValid() || !stream || !stream->openedOk() || !stream->setPosition(0) ||
               !stream->truncate().wasOk() || !png.writeImageToStream(snapshot,*stream)){
                error="Failed to render/write "+filename;return false;
            }
            stream->flush();++imageCount;
            manifest+=filename+","+juce::String(index+1)+",\""+names[index].replace("\"","\"\"")+"\","+
                juce::String(target->getWidth())+","+juce::String(target->getHeight())+"\n";
            std::function<void(juce::Component&,const juce::String&)> checkBounds;
            checkBounds=[&](juce::Component& parent,const juce::String& path){
                if(auto* viewport=dynamic_cast<juce::Viewport*>(&parent)){
                    if(auto* content=viewport->getViewedComponent())checkBounds(*content,path+" / scrollable-content");
                    return;
                }
                for(int childIndex=0;childIndex<parent.getNumChildComponents();++childIndex){
                    auto* child=parent.getChildComponent(childIndex);
                    if(!child->isVisible() || child->getBounds().isEmpty())continue;
                    auto label=child->getName();
                    if(auto* text=dynamic_cast<juce::Label*>(child))label=text->getText();
                    else if(auto* button=dynamic_cast<juce::Button*>(child))label=button->getButtonText();
                    if(label.isEmpty())label="child-"+juce::String(childIndex);
                    const auto childPath=path+" / "+label.substring(0,70).replaceCharacters("\r\n","  ");
                    // JUCE DrawableButton fits decorations with an affine transform;
                    // raw component bounds describe the unscaled source artwork.
                    const auto visibleBounds=parent.getLocalArea(child,child->getLocalBounds());
                    const bool tabRounding=dynamic_cast<juce::TabbedButtonBar*>(&parent)!=nullptr &&
                        parent.getLocalBounds().expanded(1).contains(visibleBounds);
                    if(!parent.getLocalBounds().contains(visibleBounds) && !tabRounding){
                        ++warningCount;
                        report+=filename+" | "+childPath+" | transformed_bounds="+visibleBounds.toString()+
                            " | parent="+parent.getLocalBounds().toString()+"\n";
                    }
                    checkBounds(*child,childPath);
                }
            };
            checkBounds(*this,names[index]);
            }
            if(index==1){
                mixerPanel.setChannelViewCount(8);
                for(auto& channel:engine.builtinControls().slots)for(auto& slot:channel)builtin::configureSlot(slot,builtin::None,false);
                for(auto& channel:engine.outputBuiltinControls().slots)for(auto& slot:channel)builtin::configureSlot(slot,builtin::None,false);
                mixerPanel.refreshControls();
            }
            else if(index==7)subPanel.setValidationScenario(0);
            else if(index==8)engineeringPanel.setValidationScenario(0);
            else if(index==9)frontFillPanel.setValidationScenario(0);
            else if(index==10)phasePanel.setTestScenario(0);
            else if(index==11){
                for(auto& channel:engine.builtinControls().slots)for(auto& slot:channel)builtin::configureSlot(slot,builtin::None,false);
                for(auto& channel:engine.outputBuiltinControls().slots)for(auto& slot:channel)builtin::configureSlot(slot,builtin::None,false);
                builtinPanel.refreshControls();
            }
        }
    }
    if(!phasePanel.writeValidationArtifacts(directory,error))return false;
    imageCount+=2;
    // Use the exact mixer callback to exercise floating-window identity and
    // control wiring without adding native windows to the desktop in smoke mode.
    selectPage(1);
    builtin::configureSlot(engine.builtinControls().slots[7][2],builtin::Dynamics,true);
    builtin::configureSlot(engine.outputBuiltinControls().slots[7][3],builtin::Equalizer,true);
    mixerPanel.onOpenBuiltin(7,2,false);
    auto* firstEditorWindow=nativePluginWindows.back().get();
    mixerPanel.onOpenBuiltin(7,3,true);
    auto* outputEditorWindow=nativePluginWindows.back().get();
    mixerPanel.onOpenBuiltin(7,2,false);
    if(nativePluginWindows.size()!=2 || nativePluginWindows.front().get()!=firstEditorWindow ||
       tabs.getCurrentTabIndex()!=1 || firstEditorWindow==outputEditorWindow){
        error="Plugin windows changed the mixer page or reused an unrelated target.";return false;
    }
    firstEditorWindow->closeButtonPressed();
    if(firstEditorWindow->isVisible() || !outputEditorWindow->isVisible()){
        error="Closing an input editor affected another output editor.";return false;
    }
    mixerPanel.onOpenBuiltin(7,2,false);
    if(nativePluginWindows.size()!=2 || !firstEditorWindow->isVisible()){
        error="Reopening an insert did not restore its existing editor.";return false;
    }
    std::function<void(juce::Component&)> checkEditorBounds;
    checkEditorBounds=[&](juce::Component& parent){
        for(int index=0;index<parent.getNumChildComponents();++index){
            auto* child=parent.getChildComponent(index);
            if(!child->isVisible() || child->getBounds().isEmpty())continue;
            if(!parent.getLocalBounds().contains(parent.getLocalArea(child,child->getLocalBounds()))){
                ++warningCount;report+="Plugin editor bounds warning: "+child->getName()+" / "+child->getBounds().toString()+"\n";
            }
            checkEditorBounds(*child);
        }
    };
    int editorControlChecks=0;
    for(bool output:{false,true})for(int module=1;module<=builtin::moduleCount;++module){
        auto& slot=(output?engine.outputBuiltinControls():engine.builtinControls()).slots[7][output?3:2];
        builtin::configureSlot(slot,module,true);
        mixerPanel.onOpenBuiltin(7,output?3:2,output);
        auto* window=output?outputEditorWindow:firstEditorWindow;
        auto* editor=window->getEditor();
        if(!editor || editor->getTargetChannel()!=7 || editor->getTargetSlot()!=(output?3:2) ||
           editor->isOutputTarget()!=output || editor->getSelectedModule()!=module){
            error="Floating plugin editor is attached to the wrong rack slot.";return false;
        }
        if(!editor->runControlValidation(error))return false;
        ++editorControlChecks;
        for(const auto editorSize:{juce::Point<int>{940,650},juce::Point<int>{780,540}}){
            window->setSize(editorSize.x,editorSize.y);
            checkEditorBounds(*editor);
            const auto filename="plugin-"+juce::String(output?"output-":"input-")+juce::String(module).paddedLeft('0',2)+
                "-"+juce::String(editorSize.x)+"x"+juce::String(editorSize.y)+".png";
            const auto snapshot=editor->createComponentSnapshot(editor->getLocalBounds());
            auto stream=directory.getChildFile(filename).createOutputStream();
            if(!stream || !stream->setPosition(0) || !stream->truncate().wasOk() || !png.writeImageToStream(snapshot,*stream)){
                error="Cannot save floating editor render: "+filename;return false;
            }
            stream->flush();++imageCount;
            manifest+=filename+",2,\""+juce::String::fromUTF8(builtin::metadata(module).name)+" editor\","+
                juce::String(editor->getWidth())+","+juce::String(editor->getHeight())+"\n";
        }
        if(module==builtin::Equalizer){
            const auto original=builtin::snapshotSlot(slot);juce::TextButton* mode=nullptr;
            for(int child=0;child<editor->getNumChildComponents();++child)if(editor->getChildComponent(child)->getName()=="eq-mode")mode=dynamic_cast<juce::TextButton*>(editor->getChildComponent(child));
            if(!mode||!mode->onClick){error="Eight-band EQ mode control is unavailable.";return false;}
            if(!slot.extendedEq.load())mode->onClick();
            if(!slot.extendedEq.load()){error="Eight-band EQ GUI toggle did not update the slot.";return false;}
            for(const auto editorSize:{juce::Point<int>{940,650},juce::Point<int>{780,540}}){
                window->setSize(editorSize.x,editorSize.y);checkEditorBounds(*editor);
                const auto filename="plugin-"+juce::String(output?"output-":"input-")+"05-8band-"+juce::String(editorSize.x)+"x"+juce::String(editorSize.y)+".png";
                const auto snapshot=editor->createComponentSnapshot(editor->getLocalBounds());auto stream=directory.getChildFile(filename).createOutputStream();
                if(!stream||!stream->setPosition(0)||!stream->truncate().wasOk()||!png.writeImageToStream(snapshot,*stream)){error="Cannot save extended EQ render: "+filename;return false;}
                stream->flush();++imageCount;manifest+=filename+",2,\"Eight-band Equalizer editor\","+juce::String(editor->getWidth())+","+juce::String(editor->getHeight())+"\n";
            }
            builtin::recallSlot(slot,original);editor->refreshControls();
        }
    }
    // A combined native component render makes the non-modal mixer workflow
    // reviewable. No simulated audio levels are introduced into this preview.
    setSize(1350,850);selectPage(1);
    builtin::configureSlot(engine.builtinControls().slots[1][0],builtin::SmartComp,true);
    mixerPanel.refreshControls();mixerPanel.onOpenBuiltin(1,0,false);
    auto* previewWindow=nativePluginWindows.back().get();previewWindow->setSize(880,600);
    auto preview=createComponentSnapshot(getLocalBounds());
    {
        juce::Graphics previewGraphics(preview);
        previewGraphics.setColour(juce::Colours::black.withAlpha(.55f));
        previewGraphics.fillRoundedRectangle(239,139,892,612,9);
        previewGraphics.setOpacity(1.0f);
        previewGraphics.drawImageAt(previewWindow->createComponentSnapshot(previewWindow->getLocalBounds()),235,135);
    }
    auto previewStream=directory.getChildFile("plugin-editor-on-mixer.png").createOutputStream();
    if(!previewStream || !previewStream->setPosition(0) || !previewStream->truncate().wasOk() || !png.writeImageToStream(preview,*previewStream)){
        error="Cannot save the plugin-on-mixer render.";return false;
    }
    previewStream->flush();++imageCount;
    nativePluginWindows.clear();
    for(auto* controls:{&engine.builtinControls(),&engine.outputBuiltinControls()})
        for(auto& channel:controls->slots)for(auto& slot:channel)builtin::configureSlot(slot,builtin::None,false);
    const bool noDevice=deviceManager.getCurrentAudioDevice()==nullptr && !engine.isDeviceRunning() &&
        !engine.isPassThroughEnabled() && engine.getCallbackCount()==0;
    // Check Unicode code points, independently of rendering/font fallback.
    const auto passText=passButton.getButtonText();
    const bool unicodeCorrect=heading.getText().containsChar(0x2022) &&
        passText.length()==9 && passText[1]==0x1eac;
    report+="\nImages written: "+juce::String(imageCount)+"\nBounds warnings: "+juce::String(warningCount)+
        "\nAudio device remained closed; zero callbacks; output muted: "+juce::String(noDevice?"YES":"NO")+
        "\nUnicode heading bullet U+2022 and audio label U+1EAC: "+juce::String(unicodeCorrect?"YES":"NO")+
        "\nNo numerical SPL supplied or calibrated in smoke mode.\n";
    report+="Input/output rack XML round-trip (Analog / 4K Line) and legacy output clear: PASS\n";
    report+="Mixer names/16-channel workflow, bus/master controls, eight quick scenes and native preset transfers: PASS\n";
    report+="Scene validation storage: "+smokeStorage.getFullPathName()+" (no user AppData access).\n";
    report+="Floating editor control validations: "+juce::String(editorControlChecks)+" / "+juce::String(builtin::moduleCount*2)+
        " PASS (knobs, bounds, bypass, reset, unrelated-slot isolation).\n";
    report+="Window identity, simultaneous input/output editors, close/reopen, mixer page retained: PASS\n";
    if(!directory.getChildFile("manifest.csv").replaceWithText(manifest) ||
       !directory.getChildFile("smoke-report.txt").replaceWithText(report)){
        error="Failed to save smoke manifest/report.";return false;
    }
    if(!noDevice){error="Unexpected audio activity during smoke rendering.";return false;}
    if(!unicodeCorrect){error="Unicode UI text did not preserve expected code points.";return false;}
    if(warningCount>0){error="Visible UI controls exceed their parent bounds; see smoke-report.txt.";return false;}
    return true;
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (background));
    g.setColour(juce::Colour(0xff171c20));g.fillRect(0,0,getWidth(),44);
    g.setColour(juce::Colour(0xff2b3c43));
    g.drawHorizontalLine(43,0.0f,static_cast<float>(getWidth()));
    g.drawHorizontalLine(getHeight()-51,12.0f,static_cast<float>(getWidth()-12));
    if(diagnosticsExpanded){
        g.setColour(juce::Colour(0xff141e28));g.fillRoundedRectangle(12.0f,static_cast<float>(getHeight()-230),static_cast<float>(getWidth()-24),144.0f,7.0f);
    }
}

void MainComponent::resized()
{
    const auto w = getWidth();
    const auto h = getHeight();
    heading.setBounds(14,6,178,30);subheading.setBounds(198,10,152,23);
    statusLabel.setBounds(360,9,std::max(100,w-684),24);statusLabel.setFont(juce::FontOptions(12.0f));
    passButton.setBounds(w-310,7,138,30);panicButton.setBounds(w-164,7,150,30);
    const int navigationWidth=(w-28)/static_cast<int>(navigation.size());
    for(std::size_t index=0;index<navigation.size();++index)
        navigation[index].setBounds(14+static_cast<int>(index)*navigationWidth,51,navigationWidth-5,28);
    for(auto& group:navigationGroups)group.setVisible(false);
    pageTitle.setFont(juce::FontOptions(16.0f,juce::Font::bold));
    pageTitle.setBounds(16,85,208,24);pageHint.setBounds(228,85,w-244,24);pageHint.setFont(juce::FontOptions(12.0f));
    noticeLabel.setBounds(16,h-81,w-188,25);noticeLabel.setFont(juce::FontOptions(12.0f));
    buildBadge.setBounds(w-160,h-80,146,24);
    audioStateLabel.setBounds(16,h-44,142,30);
    diagnosticLabel.setVisible(diagnosticsExpanded);
    diagnosticLabel.setBounds(24,h-221,w-48,126);diagnosticLabel.setFont(juce::FontOptions(14.0f));
    sessionSummary.setBounds(166,h-44,std::max(100,w-696),30);
    detailsButton.setBounds(w-520,h-45,96,32);
    saveSceneButton.setBounds(w-416,h-45,96,32);loadSceneButton.setBounds(w-312,h-45,96,32);
    qaLogButton.setBounds(w-208,h-45,96,32);saveButton.setBounds(w-104,h-45,88,32);
    const int drawer=diagnosticsExpanded?148:0;
    tabs.setBounds(12,114,w-24,std::max(250,h-199-drawer));
    const int pageWidth=tabs.getWidth()-4,pageHeight=tabs.getHeight()-4;
    usbTitle.setBounds(16,12,std::max(200,pageWidth-590),27);
    usbDescription.setBounds(16,45,pageWidth-32,25);
    usbStatus.setBounds(16,76,pageWidth-32,44);
    usbChannelSelect.setBounds(pageWidth-562,12,128,30);
    connectMidasButton.setBounds(pageWidth-420,12,192,30);
    applyUsbButton.setBounds(pageWidth-214,12,198,30);
    const int deviceLeft=std::clamp(pageWidth*45/100,360,540);
    deviceSelector.setBounds(16,140,deviceLeft-32,std::max(300,pageHeight-156));
    const int meterStart=deviceLeft+12;
    const int meterArea=std::max(200,pageWidth-meterStart-16);
    const int meterWidth = (meterArea - 12) / 2;
    inputsTitle.setBounds(meterStart+3,140,meterWidth-8,30);
    outputsTitle.setBounds(meterStart+meterWidth+15,140,meterWidth-8,30);
    inputViewport.setBounds(meterStart,178,meterWidth,std::max(180,pageHeight-198));
    outputViewport.setBounds(meterStart+meterWidth+12,178,meterWidth,std::max(180,pageHeight-198));
    inputMeters.setSize (std::max (150, meterWidth - 12), inputMeters.getHeight());
    outputMeters.setSize (std::max (150, meterWidth - 12), outputMeters.getHeight());
}
} // namespace lah::ui
