#include "../src/ui/SessionState.h"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>

#define REQUIRE(condition) do { if (!(condition)) { std::cerr << "FAILED line " << __LINE__ << ": " #condition "\n"; std::exit(1); } } while(false)
namespace {
bool equalSlot(const lah::builtin::SlotSnapshot& a,const lah::builtin::SlotSnapshot& b){
    if(a.type!=b.type||a.enabled!=b.enabled||a.params!=b.params||a.extendedEq!=b.extendedEq||a.eqInitialized!=b.eqInitialized||
       !lah::digitalfx::equal(a.digitalFxPro,b.digitalFxPro)||a.dynamicsPro!=b.dynamicsPro)return false;
    for(std::size_t index=0;index<a.eqBands.size();++index){const auto& x=a.eqBands[index];const auto& y=b.eqBands[index];
        if(x.enabled!=y.enabled||x.type!=y.type||x.frequency!=y.frequency||x.gainDb!=y.gainDb||x.q!=y.q)return false;}return true;
}
}
int main(){
    using namespace lah;
    auto controls=std::make_unique<dsp::Controls>();auto input=std::make_unique<builtin::Controls>(),output=std::make_unique<builtin::Controls>();
    controls->inputGain[7].store(-4.3f);controls->outputGain[31].store(-9);controls->route[7*32+7].store(0);controls->route[31*32+7].store(.5f);
    controls->inputMute[3].store(1);controls->fxSend[3].store(.4f);controls->fxDelayMs.store(437);controls->referenceChannel.store(6);controls->measurementChannel.store(7);
    controls->busMaster.enabled.store(true);controls->busMaster.sends[3*32+7].store(.7f);controls->busMaster.gainDb[3].store(-3);controls->busMaster.masterLeft.store(6);controls->busMaster.masterRight.store(7);
    auto& digital=input->slots[1][1];builtin::configureSlot(digital,builtin::DigitalFx,true);digital.digitalFxPro.enabled.store(true);
    digital.digitalFxPro.params[digitalfx::EchoTimeMs].store(231.7f);digital.digitalFxPro.params[digitalfx::PreDelayMs].store(104.8f);
    auto& dynamics=output->slots[30][1];builtin::configureSlot(dynamics,builtin::Dynamics,true);
    dynamics.dynamicsPro.params[dynamicspro::CompThreshold].store(-25);dynamics.dynamicsPro.params[dynamicspro::GateHold].store(85);
    dynamics.dynamicsPro.params[dynamicspro::LimiterCeiling].store(-5); // Keep edited PRO values even while its mode is off.
    builtin::configureSlot(input->slots[7][2],builtin::Analog,true);input->slots[7][2].params[3].store(7);
    auto& eq=output->slots[31][3];builtin::configureSlot(eq,builtin::Equalizer,true);eq.extendedEq.store(true);eq.eqInitialized.store(true);
    for(int band=0;band<8;++band){eq.eqBands[static_cast<std::size_t>(band)].enabled.store(true);eq.eqBands[static_cast<std::size_t>(band)].type.store(band%5);
        eq.eqBands[static_cast<std::size_t>(band)].frequency.store(60.0f*(band+1));eq.eqBands[static_cast<std::size_t>(band)].gainDb.store(static_cast<float>(band)-4);eq.eqBands[static_cast<std::size_t>(band)].q.store(.7f+band*.1f);}
    juce::StringArray names;for(int channel=0;channel<32;++channel)names.add("Input "+juce::String(channel+1));names.set(7,juce::String::fromUTF8("  Giọng hát chính  "));
    const auto xml=session::capture(*controls,*input,*output,names);juce::String error;
    const auto textRoundtrip=juce::XmlDocument::parse(xml->toString());REQUIRE(textRoundtrip!=nullptr);
    const auto decoded=session::decode(*textRoundtrip,error);REQUIRE(decoded!=nullptr);
    auto restored=std::make_unique<dsp::Controls>();auto restoredIn=std::make_unique<builtin::Controls>(),restoredOut=std::make_unique<builtin::Controls>();
    session::apply(*decoded,*restored,*restoredIn,*restoredOut);
    for(const auto& field:session::fields())for(int channel=0;channel<32;++channel)
        REQUIRE(((*controls).*field.member)[static_cast<std::size_t>(channel)].load()==((*restored).*field.member)[static_cast<std::size_t>(channel)].load());
    for(std::size_t route=0;route<controls->route.size();++route)REQUIRE(controls->route[route].load()==restored->route[route].load());
    REQUIRE(restored->referenceChannel.load()==6&&restored->measurementChannel.load()==7);
    REQUIRE(decoded->names[7]==names[7].trim());REQUIRE(restored->busMaster.enabled.load());REQUIRE(restored->busMaster.sends[3*32+7].load()==.7f);
    REQUIRE(restored->busMaster.masterLeft.load()==6&&restored->busMaster.masterRight.load()==7);
    REQUIRE(equalSlot(builtin::snapshotSlot(eq),builtin::snapshotSlot(restoredOut->slots[31][3])));
    REQUIRE(equalSlot(builtin::snapshotSlot(input->slots[7][2]),builtin::snapshotSlot(restoredIn->slots[7][2])));
    REQUIRE(equalSlot(builtin::snapshotSlot(digital),builtin::snapshotSlot(restoredIn->slots[1][1])));
    REQUIRE(equalSlot(builtin::snapshotSlot(dynamics),builtin::snapshotSlot(restoredOut->slots[30][1])));
    REQUIRE(xml->getIntAttribute("schema")==3);
    for(const auto& source:std::array<std::pair<builtin::Controls*,std::pair<int,int>>,2>{{{input.get(),{1,1}},{output.get(),{30,1}}}}){
        const auto preset=session::capturePreset(*source.first,source.second.first,source.second.second,false,"Full PRO state");
        REQUIRE(preset->getIntAttribute("schema")==3);session::NativePreset parsed;REQUIRE(session::decodePreset(*preset,parsed,error));
        REQUIRE(equalSlot(parsed.slots[0],builtin::snapshotSlot(source.first->slots[static_cast<std::size_t>(source.second.first)][static_cast<std::size_t>(source.second.second)])));
        auto corrupt=std::make_unique<juce::XmlElement>(*preset);auto* row=corrupt->getChildByName("Slot");REQUIRE(row!=nullptr);
        auto* node=row->getChildByName(source.first==input.get()?"DigitalFxPro":"DynamicsPro");REQUIRE(node!=nullptr);node->removeAttribute("p0");
        REQUIRE(!session::decodePreset(*corrupt,parsed,error));error.clear();
    }

    // Mode-off advanced values are saved for the next eight-band enable too.
    eq.extendedEq.store(false);const auto disabled=session::capture(*controls,*input,*output,names);const auto decodedDisabled=session::decode(*disabled,error);
    REQUIRE(decodedDisabled&&!decodedDisabled->output[31][3].extendedEq&&decodedDisabled->output[31][3].eqInitialized);
    REQUIRE(equalSlot(builtin::snapshotSlot(eq),decodedDisabled->output[31][3]));

    juce::XmlElement legacy("LiveAudioHostScene");legacy.setAttribute("schema",1);legacy.setAttribute("inputGain7",999);legacy.setAttribute("eqHz7","nan");
    const auto migrated=session::decode(legacy,error);REQUIRE(migrated!=nullptr);REQUIRE(migrated->mixer->inputGain[7].load()==12);
    REQUIRE(std::isfinite(migrated->mixer->eqHz[7].load()));REQUIRE(!migrated->mixer->busMaster.enabled.load());
    REQUIRE(migrated->output[31][3].type==builtin::None);REQUIRE(migrated->names[31]=="CH32");
    auto future=std::make_unique<juce::XmlElement>(*xml);future->setAttribute("schema",999);REQUIRE(!session::decode(*future,error));
    auto duplicate=std::make_unique<juce::XmlElement>(*xml);auto* rack=duplicate->getChildByName("BuiltinRack");rack->addChildElement(new juce::XmlElement(*rack->getFirstChildElement()));REQUIRE(!session::decode(*duplicate,error));
    auto badName=std::make_unique<juce::XmlElement>(*xml);auto* invalidName=badName->getChildByName("ChannelNames")->createNewChildElement("Channel");invalidName->setAttribute("index",32);REQUIRE(!session::decode(*badName,error));
    auto badBus=std::make_unique<juce::XmlElement>(*xml);badBus->getChildByName("BusMaster")->removeAttribute("gainL");REQUIRE(!session::decode(*badBus,error));
    auto badBand=std::make_unique<juce::XmlElement>(*xml);auto* outputRack=badBand->getChildByName("OutputBuiltinRack");auto* last=outputRack->getChildElement(127);auto* extra=last->createNewChildElement("EqBand");extra->setAttribute("index",7);REQUIRE(!session::decode(*badBand,error));

    for(bool chain:{false,true}){
        const auto presetXml=session::capturePreset(*output,31,3,chain,"My EQ preset");session::NativePreset preset;REQUIRE(session::decodePreset(*presetXml,preset,error));
        REQUIRE(preset.chain==chain);auto target=std::make_unique<builtin::Controls>();builtin::configureSlot(target->slots[2][2],builtin::VocalColor,true);
        const auto untouched=builtin::snapshotSlot(target->slots[2][2]);session::applyPreset(preset,*target,7,1);
        REQUIRE(equalSlot(untouched,builtin::snapshotSlot(target->slots[2][2])));
        REQUIRE(equalSlot(builtin::snapshotSlot(eq),builtin::snapshotSlot(target->slots[7][chain?3:1])));
        auto invalid=std::make_unique<juce::XmlElement>(*presetXml);invalid->setAttribute("kind","unknown");REQUIRE(!session::decodePreset(*invalid,preset,error));
    }
    const auto files=juce::File::getCurrentWorkingDirectory().getChildFile("session-state-test-artifacts");REQUIRE(files.createDirectory().wasOk());
    const auto goodFile=files.getChildFile("roundtrip.xml");REQUIRE(session::writeFile(goodFile,*xml,error));REQUIRE(session::readFile(goodFile,error)!=nullptr);
    const auto badFile=files.getChildFile("untrusted.xml");REQUIRE(badFile.replaceWithText("<!DOCTYPE a [<!ENTITY x 'bad'>]><LiveAudioHostScene schema='2'/>"));REQUIRE(!session::readFile(badFile,error));
    REQUIRE(badFile.replaceWithText("<LiveAudioHostScene"));REQUIRE(!session::readFile(badFile,error));
    const auto stateAfterFailure=builtin::snapshotSlot(restoredOut->slots[31][3]);REQUIRE(equalSlot(builtin::snapshotSlot(eq),stateAfterFailure)==false); // prior valid enabled-mode scene stayed unchanged
    REQUIRE(stateAfterFailure.extendedEq&&stateAfterFailure.eqInitialized);
    std::cout<<"PASS: complete scene/routing/name/bus/EQ roundtrip, schema1 migration, bounded legacy values, rejected corrupt/duplicate/untrusted documents, plugin/chain isolation\n";
}
