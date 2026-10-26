#include "../src/dsp/BuiltinFactoryPresets.h"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <new>

namespace {bool counting=false;std::size_t allocations=0;}
void* operator new(std::size_t size){if(counting)++allocations;if(auto* memory=std::malloc(size?size:1))return memory;throw std::bad_alloc();}
void* operator new[](std::size_t size){return ::operator new(size);}
void operator delete(void* memory)noexcept{std::free(memory);}void operator delete[](void* memory)noexcept{std::free(memory);}
void operator delete(void* memory,std::size_t)noexcept{std::free(memory);}void operator delete[](void* memory,std::size_t)noexcept{std::free(memory);}
#define REQUIRE(condition) do{if(!(condition)){counting=false;std::cerr<<"FAILED line "<<__LINE__<<": " #condition "\n";std::exit(1);}}while(false)

namespace {
using namespace lah::builtin;
bool same(const SlotSnapshot& a,const SlotSnapshot& b){return a.enabled==b.enabled&&samePresetParameters(a,b);}
void sameRack(const SceneSnapshot& a,const SceneSnapshot& b){for(std::size_t channel=0;channel<a.size();++channel)for(std::size_t slot=0;slot<a[channel].size();++slot)REQUIRE(same(a[channel][slot],b[channel][slot]));}
void factoryAndBankIsolation(){
    Controls input,output;
    configureSlot(input.slots[0][0],AutoFeedback,true);input.slots[0][0].feedbackArmed.store(true);
    configureSlot(output.slots[31][3],Analog,true);output.slots[31][3].params[1].store(6);
    for(int module=1;module<=moduleCount;++module){const auto presets=factoryPresets(module);
        REQUIRE(!samePresetParameters(presets[0].state,presets[1].state));REQUIRE(!samePresetParameters(presets[0].state,presets[2].state));
        for(const auto& preset:presets){REQUIRE(preset.state.type==module&&preset.state.enabled&&preset.name[0]);
            REQUIRE(same(preset.state,boundedSnapshot(preset.state)));}
        for(bool enabled:{false,true}){
            auto& slot=input.slots[7][2];configureSlot(slot,module,enabled);const auto initial=snapshot(input),otherDomain=snapshot(output);
            LivePresetState state(input,7,2);const auto token=slot.feedbackResetGeneration.load();
            REQUIRE(state.valid()&&state.matchesBank(0)&&state.matchesBank(1));
            for(int index=0;index<3;++index){slot.feedbackArmed.store(module==FeedbackGuard||module==AutoFeedback);
                counting=true;REQUIRE(state.applyFactory(index)==PresetResult::Applied);counting=false;
                const auto current=snapshotSlot(slot);REQUIRE(current.enabled==enabled&&samePresetParameters(current,presets[static_cast<std::size_t>(index)].state));
                REQUIRE(slot.type.load()==module&&slot.feedbackResetGeneration.load()==token);
                REQUIRE(!slot.feedbackArmed.load());REQUIRE(input.slots[0][0].feedbackArmed.load());}
            REQUIRE(state.storeBank(0));const auto a=*state.current();REQUIRE(state.applyFactory(1)==PresetResult::Applied);REQUIRE(state.storeBank(1));const auto b=*state.current();
            REQUIRE(!samePresetParameters(a,b));REQUIRE(state.recallBank(0)==PresetResult::Applied&&samePresetParameters(*state.current(),a));
            // A/B preserve CURRENT bypass status rather than recalling the saved flag.
            slot.enabled.store(!enabled);REQUIRE(state.recallBank(1)==PresetResult::Applied);REQUIRE(slot.enabled.load()==!enabled&&samePresetParameters(*state.current(),b));
            const auto beforeReject=snapshot(input);auto wrong=b;wrong.type=module==Analog?Equalizer:Analog;
            REQUIRE(state.apply(wrong)==PresetResult::WrongModule);REQUIRE(state.applyFactory(3)==PresetResult::InvalidPreset);
            REQUIRE(!state.storeBank(-1)&&state.recallBank(2)==PresetResult::EmptyBank);sameRack(snapshot(input),beforeReject);
            for(int channel=0;channel<channelCount;++channel)for(int position=0;position<slotCount;++position)
                if(channel!=7||position!=2)REQUIRE(same(snapshotSlot(input.slots[static_cast<std::size_t>(channel)][static_cast<std::size_t>(position)]),initial[static_cast<std::size_t>(channel)][static_cast<std::size_t>(position)]));
            sameRack(snapshot(output),otherDomain);
            // Even replacing a slot with the same type invalidates an old editor/chooser.
            configureSlot(slot,module,enabled);const auto replaced=snapshot(input);
            REQUIRE(!state.valid()&&state.apply(b)==PresetResult::StaleTarget&&!state.storeBank(0));sameRack(snapshot(input),replaced);
            state.rebind();REQUIRE(state.valid()&&state.matchesBank(0)&&state.matchesBank(1));
        }
    }
    REQUIRE(allocations==0);LivePresetState invalid(input,-1,0),none(input,31,0);
    REQUIRE(!invalid.valid()&&!none.valid());REQUIRE(invalid.applyFactory(0)==PresetResult::StaleTarget);
}
void extendedEqAndBounds(){
    Controls controls;auto& slot=controls.slots[7][2];configureSlot(slot,Equalizer,true);REQUIRE(setExtendedEqEnabled(slot,true));
    for(int band=0;band<eqBandCount;++band)recallEqBand(slot.eqBands[static_cast<std::size_t>(band)],{band%2==0,band%5,100.0f+band*1300,static_cast<float>(band)-4,.5f+band*.2f});
    REQUIRE(setExtendedEqEnabled(slot,false));const auto original=snapshotSlot(slot);LivePresetState state(controls,7,2);
    REQUIRE(original.eqInitialized&&!original.extendedEq);REQUIRE(state.applyFactory(1)==PresetResult::Applied);REQUIRE(slot.extendedEq.load());
    REQUIRE(state.storeBank(1));REQUIRE(state.recallBank(0)==PresetResult::Applied);REQUIRE(same(snapshotSlot(slot),original));
    REQUIRE(state.recallBank(1)==PresetResult::Applied&&state.matchesBank(1));REQUIRE(state.recallBank(0)==PresetResult::Applied);
    auto corrupt=original;corrupt.params[0]=std::numeric_limits<float>::quiet_NaN();corrupt.params[3]=10000;
    corrupt.extendedEq=true;corrupt.eqBands[2]={true,100,-50,std::numeric_limits<float>::infinity(),-5};
    REQUIRE(state.apply(corrupt)==PresetResult::Applied);const auto bounded=boundedSnapshot(corrupt);
    REQUIRE(samePresetParameters(snapshotSlot(slot),bounded)&&slot.enabled.load());
    // Two editor banks on the same slot are independent local snapshots.
    LivePresetState second(controls,7,2);const auto secondA=*second.bank(0);
    REQUIRE(state.recallBank(0)==PresetResult::Applied);REQUIRE(second.recallBank(0)==PresetResult::Applied);
    REQUIRE(samePresetParameters(snapshotSlot(slot),secondA));
    // Input/output references cannot accidentally share a target or A/B state.
    Controls output;configureSlot(output.slots[7][2],Equalizer,false);LivePresetState outputState(output,7,2);
    const auto inputBefore=snapshot(controls);REQUIRE(outputState.apply(original)==PresetResult::Applied);
    REQUIRE(!output.slots[7][2].enabled.load());sameRack(snapshot(controls),inputBefore);
}
void extendedProcessorBanks(){
    Controls input,output;
    for(int module:{DigitalFx,Dynamics}){
        auto& slot=input.slots[7][2];configureSlot(slot,module,true);
        if(module==DigitalFx){slot.digitalFxPro.enabled.store(true);const auto& specs=lah::digitalfx::specs();
            for(std::size_t p=0;p<specs.size();++p){const auto& spec=specs[p];const float value=spec.minimum+(spec.maximum-spec.minimum)*static_cast<float>((p%5)+1)/6;
                slot.digitalFxPro.params[p].store(value);}}
        else{slot.dynamicsPro.enabled.store(true);const auto& specs=lah::dynamicspro::parameterSpecs();
            auto custom=lah::dynamicspro::snapshot(slot.dynamicsPro);for(std::size_t p=0;p<specs.size();++p){const auto& spec=specs[p];custom.params[p]=spec.minimum+(spec.maximum-spec.minimum)*static_cast<float>((p%5)+1)/6;}
            lah::dynamicspro::recall(slot.dynamicsPro,custom);}
        LivePresetState state(input,7,2);const auto activeCustom=*state.current();
        REQUIRE(module==DigitalFx?activeCustom.digitalFxPro.enabled:activeCustom.dynamicsPro.enabled);
        REQUIRE(state.applyFactory(1)==PresetResult::Applied);REQUIRE(state.storeBank(1));const auto activeFactory=*state.current();
        REQUIRE(!samePresetParameters(activeCustom,activeFactory));
        // Mode ON -> ON comparisons preserve every PRO parameter as well as
        // legacy controls; applying uses no temporary disabled-mode recall.
        counting=true;REQUIRE(state.recallBank(0)==PresetResult::Applied);counting=false;REQUIRE(same(snapshotSlot(slot),activeCustom));
        if(module==DigitalFx)slot.digitalFxPro.enabled.store(false);else slot.dynamicsPro.enabled.store(false);
        REQUIRE(state.storeBank(0));const auto inactiveCustom=*state.current();
        REQUIRE(!samePresetParameters(inactiveCustom,activeCustom));
        REQUIRE(state.recallBank(1)==PresetResult::Applied&&same(snapshotSlot(slot),activeFactory));
        REQUIRE(state.recallBank(0)==PresetResult::Applied&&same(snapshotSlot(slot),inactiveCustom));
        configureSlot(output.slots[31][3],module,false);LivePresetState destination(output,31,3);const auto inputBefore=snapshot(input);
        REQUIRE(destination.apply(inactiveCustom)==PresetResult::Applied);REQUIRE(!output.slots[31][3].enabled.load());
        REQUIRE(samePresetParameters(*destination.current(),inactiveCustom));sameRack(snapshot(input),inputBefore);
        REQUIRE(destination.apply(activeCustom)==PresetResult::Applied);REQUIRE(!output.slots[31][3].enabled.load());
        REQUIRE(samePresetParameters(*destination.current(),activeCustom));sameRack(snapshot(input),inputBefore);
        auto changed=activeCustom;if(module==DigitalFx)changed.digitalFxPro.params[lah::digitalfx::EchoFeedback]+=1;else changed.dynamicsPro.params[lah::dynamicspro::CompKnee]+=1;
        REQUIRE(!samePresetParameters(changed,activeCustom));
        if(module==DigitalFx){changed.digitalFxPro.params[lah::digitalfx::EchoTimeMs]=100000;changed.digitalFxPro.params[lah::digitalfx::ReverbMix]=std::numeric_limits<float>::quiet_NaN();}
        else{changed.dynamicsPro.params[lah::dynamicspro::GateRange]=100000;changed.dynamicsPro.params[lah::dynamicspro::CompKnee]=std::numeric_limits<float>::quiet_NaN();}
        REQUIRE(destination.apply(changed)==PresetResult::Applied);REQUIRE(samePresetParameters(*destination.current(),boundedSnapshot(changed)));
    }
    REQUIRE(allocations==0);
}
void activeAudioAndTransition(){
    auto rack=std::make_unique<Rack>();Controls controls;
    configureSlot(controls.slots[5][0],Equalizer,true);configureSlot(controls.slots[6][0],Equalizer,true);
    controls.slots[5][0].params[3].store(-3);controls.slots[6][0].params[3].store(-3);
    rack->prepare(48000,128);std::uint64_t frame=0;
    for(int module=1;module<=moduleCount;++module){configureSlot(controls.slots[7][2],module,true);LivePresetState state(controls,7,2);
        for(int preset=0;preset<3;++preset){counting=true;REQUIRE(state.applyFactory(preset)==PresetResult::Applied);
            for(int block=0;block<24;++block){rack->beginBlock(controls);for(int sample=0;sample<128;++sample,++frame){
                const float input=static_cast<float>(.1*std::sin(2*pi*997*frame/48000.0)+.03*std::sin(2*pi*80*frame/48000.0));
                const float actual=rack->processSample(7,input);REQUIRE(std::isfinite(actual)&&std::abs(actual)<=16);
                REQUIRE(rack->processSample(5,input)==rack->processSample(6,input));REQUIRE(rack->processSample(31,input)==input);
            }}counting=false;REQUIRE(controls.slots[7][2].enabled.load()&&controls.slots[7][2].type.load()==module);}
    }
    auto& slot=controls.slots[7][2];configureSlot(slot,VocalColor,true);slot.params[4].store(0);slot.params[5].store(0);LivePresetState state(controls,7,2);
    rack->beginBlock(controls);float previous=0;for(int sample=0;sample<128;++sample)previous=rack->processSample(7,.1f);
    REQUIRE(std::abs(previous-.1f)<1e-6f);auto changed=*state.current();changed.params[5]=6;
    counting=true;REQUIRE(state.apply(changed)==PresetResult::Applied);rack->beginBlock(controls);
    for(int sample=0;sample<128;++sample){const float actual=rack->processSample(7,.1f);REQUIRE(std::abs(actual-previous)<.002f);previous=actual;}
    counting=false;REQUIRE(std::abs(previous-.1f*gain(6))<1e-6f);REQUIRE(allocations==0);
}
}
int main(){factoryAndBankIsolation();extendedEqAndBounds();extendedProcessorBanks();activeAudioAndTransition();
    std::cout<<"PASS: 17 modules / 51 native starting presets; full-state A-B including inactive EQ8; "
        "Digital FX PRO18 + Dynamics PRO19 active/inactive state, cross-domain capture and bounded live apply; "
        "same-type + stale-generation rejection; preserve bypass/type; feedback disarm and unrelated-arm isolation; "
        "input/output + editor-bank isolation; live finite DSP and exact 64-sample gain transition; zero allocations\n";}
