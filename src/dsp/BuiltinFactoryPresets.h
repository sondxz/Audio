#pragma once
#include "BuiltinSceneState.h"
#include <optional>

namespace lah::builtin {
// Our own conservative starting points for the native algorithms. These are
// neither vendor presets nor a promise that a source will need these settings.
struct FactoryPreset {const char* name="Default";SlotSnapshot state{};};
inline std::array<FactoryPreset,3> factoryPresets(int module)noexcept {
    std::array<FactoryPreset,3> result{};
    if(module<1||module>moduleCount)return result;
    for(auto& preset:result){preset.state.type=module;preset.state.enabled=true;
        for(int p=0;p<parameterCount;++p)preset.state.params[static_cast<std::size_t>(p)]=metadata(module).params[static_cast<std::size_t>(p)].defaultValue;}
    auto& first=result[1];auto& second=result[2];
    switch(module){
        case VocalClean:first.name="Speech - gentle";first.state.params={100,-2,2,-15,2,0};second.name="Singing - light control";second.state.params={75,-1.5f,3,-18,2.5f,0};break;
        case VocalColor:first.name="Warm voice";first.state.params={75,1.5f,.5f,2,70,0};second.name="Clear voice";second.state.params={95,0,2,1,80,0};break;
        case FeedbackGuard:first.name="Manual 1 kHz dip";first.state.params={1000,-3,16,60,-3,100};second.name="Manual 3.15 kHz dip";second.state.params={3150,-6,24,50,-3,100};break;
        case AutoFeedback:first.name="Manual 1 kHz dip";first.state.params={1000,-3,20,1000,5,0};second.name="Manual 3.15 kHz dip";second.state.params={3150,-6,28,1200,4,0};break;
        case Equalizer:{
            first.name="Voice - eight bands";first.state.extendedEq=first.state.eqInitialized=true;
            first.state.eqBands[0]={true,EqHighPass,80,0,.70710678f};first.state.eqBands[2]={true,EqBell,250,-2,1};
            first.state.eqBands[4]={true,EqBell,3000,1.5f,.8f};first.state.eqBands[6]={true,EqHighShelf,8000,1,.70710678f};
            second.name="Low-mid relief";second.state.params={35,0,350,-2.5f,1.1f,.5f};break;}
        case SmartEq:first.name="Voice - subtle";first.state.params={85,2,.5f,2,400,0};second.name="Music - gentle";second.state.params={35,1.5f,.5f,1.5f,600,0};break;
        case Dynamics:{first.name="Speech - PRO";first.state.params={-18,2,15,180,-75,0};first.state.dynamicsPro.enabled=true;
            auto& a=first.state.dynamicsPro.params;a[dynamicspro::GateThreshold]=-55;a[dynamicspro::GateRange]=12;a[dynamicspro::GateRatio]=1.5f;
            a[dynamicspro::GateAttack]=4;a[dynamicspro::GateHold]=80;a[dynamicspro::GateRelease]=250;
            a[dynamicspro::CompThreshold]=-18;a[dynamicspro::CompRatio]=2;a[dynamicspro::CompAttack]=15;a[dynamicspro::CompRelease]=180;a[dynamicspro::CompKnee]=6;
            a[dynamicspro::LimiterCeiling]=-1;a[dynamicspro::LimiterRelease]=100;
            second.name="Gentle bus - PRO";second.state.params={-14,1.6f,30,300,-80,0};second.state.dynamicsPro.enabled=true;
            auto& b=second.state.dynamicsPro.params;b[dynamicspro::GateOn]=0;b[dynamicspro::CompThreshold]=-14;b[dynamicspro::CompRatio]=1.6f;
            b[dynamicspro::CompAttack]=30;b[dynamicspro::CompRelease]=300;b[dynamicspro::CompKnee]=8;
            b[dynamicspro::LimiterCeiling]=-1.5f;b[dynamicspro::LimiterRelease]=120;break;}
        case SmartComp:first.name="Voice - parallel";first.state.params={-20,2,15,250,65,0};second.name="Music - light glue";second.state.params={-14,1.6f,30,350,80,0};break;
        case OpticalComp:first.name="Voice - smooth";first.state.params={-20,2.5f,18,650,0,0};second.name="Bass - gentle";second.state.params={-16,3,25,500,1,0};break;
        case Fx:first.name="Short slap";first.state.params={12,95,.12f,5500,0,0};second.name="Soft vocal echo";second.state.params={18,280,.25f,4500,0,0};break;
        case DigitalFx:{first.name="Vocal echo - PRO";first.state.params={16,1,10,65,.08f,0};first.state.digitalFxPro.enabled=true;
            auto& a=first.state.digitalFxPro.params;a[digitalfx::EchoTimeMs]=220;a[digitalfx::EchoFeedback]=22;a[digitalfx::EchoMix]=16;
            a[digitalfx::EchoLowCut]=140;a[digitalfx::EchoHighCut]=5200;a[digitalfx::PreDelayMs]=20;a[digitalfx::DecaySeconds]=1.4f;
            a[digitalfx::DampingHz]=5000;a[digitalfx::RoomSize]=.9f;a[digitalfx::Diffusion]=75;a[digitalfx::ReverbMix]=10;a[digitalfx::EchoToReverb]=12;a[digitalfx::Ducking]=20;
            second.name="Room combo - PRO";second.state.params={12,2,15,160,.2f,0};second.state.digitalFxPro.enabled=true;
            auto& b=second.state.digitalFxPro.params;b[digitalfx::EchoTimeMs]=75;b[digitalfx::EchoFeedback]=12;b[digitalfx::EchoMix]=8;
            b[digitalfx::EchoLowCut]=180;b[digitalfx::EchoHighCut]=6500;b[digitalfx::PreDelayMs]=8;b[digitalfx::DecaySeconds]=.75f;
            b[digitalfx::RoomSize]=.65f;b[digitalfx::Diffusion]=80;b[digitalfx::ReverbMix]=14;b[digitalfx::EchoToReverb]=15;b[digitalfx::Ducking]=10;break;}
        case Reverb96:first.name="Short room";first.state.params={12,.7f,6500,8,.65f,0};second.name="Soft vocal space";second.state.params={17,1.7f,4800,25,1.1f,0};break;
        case SubPro:first.name="Tight low end";first.state.params={30,95,1.5f,0,0,-3};second.name="Warm low end";second.state.params={25,120,2,0,1,-3};break;
        case DrumLive:first.name="Kick - gentle";first.state.params={30,2,1,-65,180,2};second.name="Percussion - light";second.state.params={80,0,1.5f,-70,220,1.5f};break;
        case PcAudio:first.name="Speech playback";first.state.params={70,0,1,-15,2,-1};second.name="Music playback";second.state.params={30,1,.5f,-10,1.5f,-1};break;
        case Analog:first.name="Subtle warmth";first.state.params={30,1,.5f,1.5f,70,0};second.name="Soft air";second.state.params={40,0,1.5f,1,80,0};break;
        case FourKLine:first.name="Voice contour";first.state.params={80,0,-1.5f,1.5f,1,1};second.name="Music contour";second.state.params={30,1,-.5f,.5f,1,1.5f};break;
        default:break;
    }
    for(auto& preset:result)preset.state=boundedSnapshot(preset.state);
    return result;
}

inline bool samePresetParameters(const SlotSnapshot& a,const SlotSnapshot& b)noexcept {
    return a.type==b.type&&a.params==b.params&&a.extendedEq==b.extendedEq&&a.eqInitialized==b.eqInitialized&&a.eqBands==b.eqBands&&
        digitalfx::equal(a.digitalFxPro,b.digitalFxPro)&&a.dynamicsPro==b.dynamicsPro;
}
enum class PresetResult {Applied,StaleTarget,WrongModule,EmptyBank,InvalidPreset};

// One immutable rack/channel/slot. A and B start as the opening settings; STORE
// captures without changing sound, A/B recalls only that bank's parameters.
// configureSlot increments feedbackResetGeneration for EVERY native module, so
// even same-type slot replacement invalidates an old comparison/async chooser.
class LivePresetState {
public:
    LivePresetState(Controls& rack,int channel,int slot)noexcept:controls(rack),channelIndex(channel),slotIndex(slot){rebind();}
    bool valid()const noexcept {
        if(!inBounds()||expectedModule<1||expectedModule>moduleCount)return false;
        const auto& slot=target();return slot.type.load(std::memory_order_acquire)==expectedModule&&
            slot.feedbackResetGeneration.load(std::memory_order_acquire)==expectedGeneration;
    }
    int module()const noexcept{return expectedModule;}
    std::uint32_t generation()const noexcept{return expectedGeneration;}
    void rebind()noexcept {
        banks={};expectedModule=None;expectedGeneration=0;
        if(!inBounds())return;
        expectedModule=target().type.load(std::memory_order_acquire);
        expectedGeneration=target().feedbackResetGeneration.load(std::memory_order_acquire);
        if(valid()){banks[0]=snapshotSlot(target());banks[1]=banks[0];}
    }
    std::optional<SlotSnapshot> current()const noexcept {return valid()?std::optional<SlotSnapshot>(snapshotSlot(target())):std::nullopt;}
    const std::optional<SlotSnapshot>& bank(int index)const noexcept {return banks[static_cast<std::size_t>(index==1?1:0)];}
    bool matchesBank(int index)const noexcept {const auto value=current();return index>=0&&index<2&&value&&banks[static_cast<std::size_t>(index)]&&samePresetParameters(*value,*banks[static_cast<std::size_t>(index)]);}
    bool storeBank(int index)noexcept {
        if(index<0||index>1||!valid())return false;
        banks[static_cast<std::size_t>(index)]=snapshotSlot(target());return true;
    }
    PresetResult recallBank(int index)noexcept {
        if(index<0||index>1||!banks[static_cast<std::size_t>(index)])return PresetResult::EmptyBank;
        return apply(*banks[static_cast<std::size_t>(index)]);
    }
    PresetResult applyFactory(int index)noexcept {
        if(index<0||index>2)return PresetResult::InvalidPreset;
        return apply(factoryPresets(expectedModule)[static_cast<std::size_t>(index)].state);
    }
    PresetResult apply(const SlotSnapshot& raw)noexcept {
        if(!valid())return PresetResult::StaleTarget;
        if(raw.type!=expectedModule)return PresetResult::WrongModule;
        const auto state=boundedSnapshot(raw);auto& slot=target();
        // Never use recallSlot/configureSlot here: those are structural muted
        // operations. Enabled/type remain untouched. The DSP observes these
        // atomics and uses its existing 64-sample parameter transition.
        if(expectedModule==FeedbackGuard||expectedModule==AutoFeedback)slot.feedbackArmed.store(false,std::memory_order_release);
        for(std::size_t p=0;p<state.params.size();++p)slot.params[p].store(state.params[p],std::memory_order_relaxed);
        for(std::size_t band=0;band<state.eqBands.size();++band)recallEqBand(slot.eqBands[band],state.eqBands[band]);
        slot.eqInitialized.store(state.eqInitialized,std::memory_order_relaxed);
        slot.extendedEq.store(state.extendedEq,std::memory_order_release);
        // Parameter stores precede the final mode flag. In particular an
        // already-enabled PRO mode never visits OFF during an A/B or preset edit.
        for(std::size_t p=0;p<state.digitalFxPro.params.size();++p)slot.digitalFxPro.params[p].store(state.digitalFxPro.params[p],std::memory_order_relaxed);
        slot.digitalFxPro.enabled.store(state.digitalFxPro.enabled,std::memory_order_release);
        for(std::size_t p=0;p<state.dynamicsPro.params.size();++p)slot.dynamicsPro.params[p].store(state.dynamicsPro.params[p],std::memory_order_relaxed);
        slot.dynamicsPro.enabled.store(state.dynamicsPro.enabled,std::memory_order_release);
        return PresetResult::Applied;
    }
private:
    bool inBounds()const noexcept{return channelIndex>=0&&channelIndex<channelCount&&slotIndex>=0&&slotIndex<slotCount;}
    SlotControl& target()const noexcept{return controls.slots[static_cast<std::size_t>(channelIndex)][static_cast<std::size_t>(slotIndex)];}
    Controls& controls;const int channelIndex,slotIndex;
    int expectedModule=None;std::uint32_t expectedGeneration=0;
    std::array<std::optional<SlotSnapshot>,2> banks{};
};
} // namespace lah::builtin
