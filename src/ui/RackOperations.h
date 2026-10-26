#pragma once

#include "../dsp/BuiltinSceneState.h"
#include <optional>

namespace lah::ui {

// Control-thread operations on complete native slot snapshots. Structural
// edits require muted transport; the caller must pass the current state again
// when a delayed menu callback executes. Other rack positions stay untouched.
class RackOperations {
public:
    bool hasClipboard()const noexcept{return clipboard.has_value();}
    bool copy(const builtin::Controls& controls,int channel,int slot)noexcept {
        if(!valid(channel,slot))return false;
        clipboard=builtin::snapshotSlot(controls.slots[static_cast<std::size_t>(channel)][static_cast<std::size_t>(slot)]);
        return true;
    }
    bool paste(builtin::Controls& controls,int channel,int slot,bool audioRunning)const noexcept {
        if(audioRunning||!clipboard||!valid(channel,slot))return false;
        builtin::recallSlot(controls.slots[static_cast<std::size_t>(channel)][static_cast<std::size_t>(slot)],*clipboard);
        return true;
    }
    bool move(builtin::Controls& controls,int channel,int slot,int direction,bool audioRunning)const noexcept {
        if(audioRunning||(direction!=-1&&direction!=1)||!valid(channel,slot)||!valid(channel,slot+direction))return false;
        auto& first=controls.slots[static_cast<std::size_t>(channel)][static_cast<std::size_t>(slot)];
        auto& second=controls.slots[static_cast<std::size_t>(channel)][static_cast<std::size_t>(slot+direction)];
        const auto a=builtin::snapshotSlot(first),b=builtin::snapshotSlot(second);
        builtin::recallSlot(first,b);builtin::recallSlot(second,a);
        return true;
    }
    bool remove(builtin::Controls& controls,int channel,int slot,bool audioRunning)const noexcept {
        if(audioRunning||!valid(channel,slot))return false;
        builtin::recallSlot(controls.slots[static_cast<std::size_t>(channel)][static_cast<std::size_t>(slot)],builtin::SlotSnapshot{});
        return true;
    }
private:
    static bool valid(int channel,int slot)noexcept{return channel>=0&&channel<builtin::channelCount&&slot>=0&&slot<builtin::slotCount;}
    std::optional<builtin::SlotSnapshot> clipboard;
};
}
