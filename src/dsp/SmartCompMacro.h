#pragma once

#include "BuiltinRackCore.h"

namespace lah::builtin {

using SmartCompMacroParameters=std::array<float,parameterCount>;
struct SmartCompMacroInference {
    bool matches=false; // false: the six existing parameters form a custom setting
    float amount=0;
};

// Native macro mapping, not a model of another vendor's compressor. All state
// remains in the six existing SmartComp parameters, preserving old scenes.
inline SmartCompMacroParameters mapSmartCompMacro(float amount)noexcept {
    amount=std::isfinite(amount)?std::clamp(amount,-100.0f,100.0f):0.0f;
    const float u=amount*.01f;
    if(u<0)return {-18.0f-6.0f*u,3.0f+1.5f*u,10.0f-10.0f*u,
        200.0f+80.0f*u,100.0f,-6.0f*u};
    return {-18.0f-12.0f*u,3.0f+3.0f*u,10.0f-5.0f*u,
        200.0f+200.0f*u,100.0f,0.0f};
}

// A wrong module is left untouched. No implicit module replacement or enable.
inline bool applySmartCompMacro(SlotControl& slot,float amount)noexcept {
    if(slot.type.load(std::memory_order_acquire)!=SmartComp)return false;
    const auto values=mapSmartCompMacro(amount);
    for(std::size_t p=0;p<values.size();++p)slot.params[p].store(values[p],std::memory_order_release);
    return true;
}

inline SmartCompMacroInference inferSmartCompMacro(const SmartCompMacroParameters& values)noexcept {
    for(float value:values)if(!std::isfinite(value))return {};
    const float u=-(values[0]+18.0f)/(values[0]>-18.0f?6.0f:12.0f);
    const float amount=u*100.0f;
    if(amount< -100.001f || amount>100.001f)return {};
    const auto expected=mapSmartCompMacro(amount);
    for(std::size_t p=0;p<values.size();++p)if(std::abs(values[p]-expected[p])>.001f)return {};
    return {true,std::clamp(amount,-100.0f,100.0f)};
}

inline SmartCompMacroInference inferSmartCompMacro(const SlotControl& slot)noexcept {
    if(slot.type.load(std::memory_order_acquire)!=SmartComp)return {};
    SmartCompMacroParameters values{};
    for(std::size_t p=0;p<values.size();++p)values[p]=slot.params[p].load(std::memory_order_acquire);
    return inferSmartCompMacro(values);
}

} // namespace lah::builtin
