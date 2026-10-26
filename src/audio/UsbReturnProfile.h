#pragma once

#include "../dsp/MixDspCore.h"

namespace lah::audio {

// The caller mutes audio before changing the routing profile. Physical device
// masks are configured and verified separately by the device manager.
// A rejected channel count must leave every existing control unchanged.
inline bool applyUsbReturnProfile(dsp::Controls& controls,int requestedChannels) noexcept {
    if(requestedChannels<1 || requestedChannels>8)return false;
    for(int input=0;input<dsp::channels;++input){
        controls.inputPan[static_cast<std::size_t>(input)].store(0.0f,std::memory_order_relaxed);
        controls.fxSend[static_cast<std::size_t>(input)].store(0.0f,std::memory_order_relaxed);
        for(int output=0;output<dsp::channels;++output)
            controls.route[static_cast<std::size_t>(output*dsp::channels+input)].store(
                input==output && input<requestedChannels?1.0f:0.0f,std::memory_order_relaxed);
    }
    controls.delayReturn.store(0.0f,std::memory_order_relaxed);
    controls.reverbReturn.store(0.0f,std::memory_order_relaxed);
    dsp::recallBusMaster(controls.busMaster,dsp::BusMasterSnapshot{});
    return true;
}

} // namespace lah::audio
