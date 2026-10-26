#pragma once
#include "BuiltinRackCore.h"

namespace lah::builtin {
struct SlotSnapshot {
    int type=None;bool enabled=false;std::array<float,6> params{};
    bool extendedEq=false,eqInitialized=false;
    std::array<EqBandSnapshot,eqBandCount> eqBands=defaultEqBands();
    digitalfx::Snapshot digitalFxPro;
    dynamicspro::Snapshot dynamicsPro;
};
using SceneSnapshot=std::array<std::array<SlotSnapshot,slotCount>,channelCount>;
inline SlotSnapshot boundedSnapshot(SlotSnapshot state)noexcept{
    if(state.type<None || state.type>moduleCount)state={};
    const auto& info=metadata(state.type);
    state.enabled=state.enabled && state.type!=None;
    if(state.type!=Equalizer){state.extendedEq=false;state.eqInitialized=false;state.eqBands=defaultEqBands();}
    else{state.eqInitialized=state.eqInitialized || state.extendedEq;for(auto& band:state.eqBands)band=boundedEqBand(band);}
    state.digitalFxPro=state.type==DigitalFx?digitalfx::bounded(state.digitalFxPro):digitalfx::Snapshot{};
    state.dynamicsPro=state.type==Dynamics?dynamicspro::bounded(state.dynamicsPro):dynamicspro::Snapshot{};
    for(std::size_t p=0;p<state.params.size();++p){const auto& range=info.params[p];
        state.params[p]=std::isfinite(state.params[p])?std::clamp(state.params[p],range.minimum,range.maximum):range.defaultValue;}
    return state;
}
inline SlotSnapshot snapshotSlot(const SlotControl& source)noexcept {
    SlotSnapshot state;state.type=source.type.load(std::memory_order_acquire);state.enabled=source.enabled.load(std::memory_order_acquire);
    for(std::size_t p=0;p<state.params.size();++p)state.params[p]=source.params[p].load(std::memory_order_relaxed);
    state.extendedEq=source.extendedEq.load(std::memory_order_acquire);state.eqInitialized=source.eqInitialized.load(std::memory_order_acquire);
    for(std::size_t band=0;band<state.eqBands.size();++band)state.eqBands[band]=snapshotEqBand(source.eqBands[band]);
    state.digitalFxPro=digitalfx::snapshot(source.digitalFxPro);state.dynamicsPro=dynamicspro::snapshot(source.dynamicsPro);
    return boundedSnapshot(state);
}
// Muted control path: recalls exactly one insert, disarming only that insert.
inline void recallSlot(SlotControl& target,const SlotSnapshot& value)noexcept {
    const auto state=boundedSnapshot(value);configureSlot(target,state.type,false);
    for(std::size_t p=0;p<state.params.size();++p)target.params[p].store(state.params[p],std::memory_order_relaxed);
    for(std::size_t band=0;band<state.eqBands.size();++band)recallEqBand(target.eqBands[band],state.eqBands[band]);
    target.eqInitialized.store(state.eqInitialized,std::memory_order_relaxed);target.extendedEq.store(state.extendedEq,std::memory_order_release);
    digitalfx::recall(target.digitalFxPro,state.digitalFxPro);dynamicspro::recall(target.dynamicsPro,state.dynamicsPro);
    target.enabled.store(state.enabled,std::memory_order_release);
}
inline SceneSnapshot snapshot(const Controls& controls)noexcept{
    SceneSnapshot result;
    for(std::size_t ch=0;ch<result.size();++ch)for(std::size_t slot=0;slot<result[ch].size();++slot){
        result[ch][slot]=snapshotSlot(controls.slots[ch][slot]);
    }
    return result;
}
// Control path only: main transport is muted before recalling the full state.
inline void recall(Controls& controls,const SceneSnapshot& scene)noexcept{
    for(std::size_t ch=0;ch<scene.size();++ch)for(std::size_t slot=0;slot<scene[ch].size();++slot){
        recallSlot(controls.slots[ch][slot],scene[ch][slot]);
    }
}
}
