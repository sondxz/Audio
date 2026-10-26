#include "../src/ui/RackOperations.h"
#include <cstdlib>
#include <iostream>

#define REQUIRE(condition) do {if(!(condition)){std::cerr<<"FAILED line "<<__LINE__<<": " #condition "\n";std::exit(1);}}while(false)

namespace {
using namespace lah::builtin;
bool same(const SlotSnapshot& a,const SlotSnapshot& b){
    return a.type==b.type&&a.enabled==b.enabled&&a.params==b.params&&a.extendedEq==b.extendedEq&&
        a.eqInitialized==b.eqInitialized&&a.eqBands==b.eqBands;
}
void sameRack(const SceneSnapshot& a,const SceneSnapshot& b){
    for(std::size_t channel=0;channel<a.size();++channel)for(std::size_t slot=0;slot<a[channel].size();++slot)
        REQUIRE(same(a[channel][slot],b[channel][slot]));
}
}

int main(){
    using namespace lah::builtin;
    Controls input,output;lah::ui::RackOperations operations;
    auto& eq=input.slots[7][2];configureSlot(eq,Equalizer,true);eq.params[3].store(-8);
    REQUIRE(setExtendedEqEnabled(eq,true));
    for(int band=0;band<eqBandCount;++band)
        recallEqBand(eq.eqBands[static_cast<std::size_t>(band)],{band%2==0,band%5,120.0f+band*1300.0f,-9.0f+band,1.0f+band*.1f});
    configureSlot(input.slots[7][1],Dynamics,false);input.slots[7][1].params[0].store(-31);
    configureSlot(input.slots[0][0],AutoFeedback,true);input.slots[0][0].feedbackArmed.store(true);
    configureSlot(output.slots[31][3],Analog,true);output.slots[31][3].params[1].store(7);
    const auto initialInput=snapshot(input),initialOutput=snapshot(output);
    REQUIRE(!operations.hasClipboard());
    REQUIRE(!operations.paste(output,0,0,false));sameRack(snapshot(output),initialOutput);

    // Copy is read-only and can run during active audio. Paste requires muted
    // transport and carries every field across input/output domains.
    REQUIRE(operations.copy(input,7,2));REQUIRE(operations.hasClipboard());
    sameRack(snapshot(input),initialInput);REQUIRE(input.slots[0][0].feedbackArmed.load());
    REQUIRE(!operations.paste(output,31,3,true));sameRack(snapshot(output),initialOutput);
    eq.eqBands[6].gainDb.store(12); // Clipboard is a snapshot, not a live reference.
    REQUIRE(operations.paste(output,31,3,false));
    REQUIRE(same(snapshotSlot(output.slots[31][3]),initialInput[7][2]));
    for(int channel=0;channel<channelCount;++channel)for(int slot=0;slot<slotCount;++slot)
        if(channel!=31||slot!=3)REQUIRE(same(snapshotSlot(output.slots[static_cast<std::size_t>(channel)][static_cast<std::size_t>(slot)]),initialOutput[static_cast<std::size_t>(channel)][static_cast<std::size_t>(slot)]));

    const auto beforeMove=snapshot(input);
    REQUIRE(!operations.move(input,7,2,-1,true));sameRack(snapshot(input),beforeMove);
    REQUIRE(operations.move(input,7,2,-1,false));
    auto moved=snapshot(input);
    REQUIRE(same(moved[7][1],beforeMove[7][2])&&same(moved[7][2],beforeMove[7][1]));
    for(int channel=0;channel<channelCount;++channel)for(int slot=0;slot<slotCount;++slot)
        if(channel!=7||(slot!=1&&slot!=2))REQUIRE(same(moved[static_cast<std::size_t>(channel)][static_cast<std::size_t>(slot)],beforeMove[static_cast<std::size_t>(channel)][static_cast<std::size_t>(slot)]));
    REQUIRE(input.slots[0][0].feedbackArmed.load());
    REQUIRE(operations.move(input,7,1,1,false));sameRack(snapshot(input),beforeMove);

    // Eight-band edits remain intact even when the EQ is temporarily in legacy
    // mode; moving/copying must not silently discard its initialized state.
    REQUIRE(setExtendedEqEnabled(eq,false));const auto legacyEq=snapshotSlot(eq);
    REQUIRE(!legacyEq.extendedEq&&legacyEq.eqInitialized);
    REQUIRE(operations.copy(input,7,2));REQUIRE(operations.paste(output,18,1,false));
    REQUIRE(same(snapshotSlot(output.slots[18][1]),legacyEq));
    REQUIRE(setExtendedEqEnabled(output.slots[18][1],true));
    REQUIRE(snapshotSlot(output.slots[18][1]).eqBands==legacyEq.eqBands);

    const auto beforeRemove=snapshot(output);
    REQUIRE(!operations.remove(output,18,1,true));sameRack(snapshot(output),beforeRemove);
    REQUIRE(operations.remove(output,18,1,false));
    REQUIRE(same(snapshotSlot(output.slots[18][1]),boundedSnapshot(SlotSnapshot{})));
    for(int channel=0;channel<channelCount;++channel)for(int slot=0;slot<slotCount;++slot)
        if(channel!=18||slot!=1)REQUIRE(same(snapshotSlot(output.slots[static_cast<std::size_t>(channel)][static_cast<std::size_t>(slot)]),beforeRemove[static_cast<std::size_t>(channel)][static_cast<std::size_t>(slot)]));

    const auto beforeInvalid=snapshot(input);
    REQUIRE(!operations.copy(input,-1,0));REQUIRE(!operations.copy(input,32,0));REQUIRE(!operations.copy(input,0,4));
    REQUIRE(!operations.move(input,7,0,-1,false));REQUIRE(!operations.move(input,7,3,1,false));
    REQUIRE(!operations.move(input,7,2,2,false));REQUIRE(!operations.paste(input,32,0,false));
    REQUIRE(!operations.remove(input,0,-1,false));sameRack(snapshot(input),beforeInvalid);
    REQUIRE(input.slots[0][0].feedbackArmed.load());
    std::cout<<"PASS: complete slot move/copy/paste/remove, eight-band+legacy EQ state, cross-domain clipboard, "
               "muted structural guards, boundary rejection and unrelated-slot/feedback-arm isolation\n";
}
