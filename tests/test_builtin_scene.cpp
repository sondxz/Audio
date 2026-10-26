#include "../src/dsp/BuiltinSceneState.h"
#include <cassert>
#include <limits>
#include <iostream>
int main(){
    lah::builtin::Controls source,target;
    for(int ch=0;ch<32;++ch)for(int slot=0;slot<4;++slot){
        auto& item=source.slots[static_cast<std::size_t>(ch)][static_cast<std::size_t>(slot)];
        const int type=1+(ch*4+slot)%lah::builtin::moduleCount;lah::builtin::configureSlot(item,type,(ch+slot)%2==0);
        const auto& metadata=lah::builtin::metadata(type);
        for(int p=0;p<6;++p)item.params[static_cast<std::size_t>(p)].store(metadata.params[static_cast<std::size_t>(p)].minimum);
    }
    const auto before=lah::builtin::snapshot(source);lah::builtin::recall(target,before);
    const auto after=lah::builtin::snapshot(target);
    std::array<bool,lah::builtin::moduleCount+1> covered{};
    for(std::size_t ch=0;ch<before.size();++ch)for(std::size_t slot=0;slot<before[ch].size();++slot){
        assert(before[ch][slot].type==1+static_cast<int>(ch*4+slot)%lah::builtin::moduleCount);
        covered[static_cast<std::size_t>(before[ch][slot].type)]=true;
        assert(before[ch][slot].type==after[ch][slot].type);assert(before[ch][slot].enabled==after[ch][slot].enabled);
        assert(before[ch][slot].params==after[ch][slot].params);
    }
    for(int type=1;type<=lah::builtin::moduleCount;++type)assert(covered[static_cast<std::size_t>(type)]);
    auto invalid=before;invalid[0][0].type=999;invalid[0][0].enabled=true;
    invalid[0][1].type=lah::builtin::Equalizer;
    invalid[0][1].params[0]=std::numeric_limits<float>::quiet_NaN();invalid[0][1].params[1]=999;
    lah::builtin::recall(target,invalid);
    assert(target.slots[0][0].type.load()==lah::builtin::None && !target.slots[0][0].enabled.load());
    assert(target.slots[0][1].params[0].load()==lah::builtin::metadata(lah::builtin::Equalizer).params[0].defaultValue);
    assert(target.slots[0][1].params[1].load()==12);
    // Older scenes have no native rack child: the default snapshot clears
    // existing inserts instead of accidentally retaining a previous rack.
    lah::builtin::recall(target,{});
    for(const auto& ch:target.slots)for(const auto& slot:ch)assert(slot.type.load()==0 && !slot.enabled.load());
    std::cout<<"PASS: native scene 128-slot round-trip, bounds/NaN/unknown module and legacy clear\n";
}
