#pragma once
#include <juce_core/juce_core.h>
#include "../dsp/BuiltinSceneState.h"
#include "../dsp/MixDspCore.h"
#include "BusMasterSceneState.h"
#include <memory>
#include <string_view>

namespace lah::session {
using ChannelValues=std::array<std::atomic<float>,dsp::channels>;
struct Field {const char* name;ChannelValues dsp::Controls::*member;float low,high;};
inline const std::array<Field,17>& fields(){
    static const std::array<Field,17> value{{
        {"inputGain",&dsp::Controls::inputGain,-60,12},{"inputMute",&dsp::Controls::inputMute,0,1},
        {"inputPan",&dsp::Controls::inputPan,-1,1},{"eqDb",&dsp::Controls::eqDb,-18,18},
        {"eqHz",&dsp::Controls::eqHz,20,20000},{"eqQ",&dsp::Controls::eqQ,.1f,10},
        {"hpfHz",&dsp::Controls::hpfHz,0,1000},{"compThreshold",&dsp::Controls::compThreshold,-60,0},
        {"compRatio",&dsp::Controls::compRatio,1,20},{"compAttack",&dsp::Controls::compAttackMs,.2f,100},
        {"compRelease",&dsp::Controls::compReleaseMs,1,1500},{"gateThreshold",&dsp::Controls::gateThreshold,-100,0},
        {"outputGain",&dsp::Controls::outputGain,-60,12},{"outputDelay",&dsp::Controls::outputDelayMs,0,250},
        {"outputPolarity",&dsp::Controls::outputPolarity,-1,1},{"outputCeiling",&dsp::Controls::outputCeiling,-24,0},
        {"fxSend",&dsp::Controls::fxSend,0,1}}};return value;
}
inline juce::String cleanName(juce::String name,const juce::String& fallback,int maximum=48){
    juce::String result;for(auto c:name)if(c>=32&&c!=127)result+=juce::String::charToString(c);
    result=result.trim().substring(0,maximum);return result.isEmpty()?fallback:result;
}
inline float bounded(double value,float low,float high,float fallback){
    return std::isfinite(value)?std::clamp(static_cast<float>(value),low,high):fallback;
}
inline void writeSlot(juce::XmlElement& row,const builtin::SlotSnapshot& raw){
    const auto state=builtin::boundedSnapshot(raw);
    row.setAttribute("module",state.type);row.setAttribute("enabled",state.enabled);
    for(int p=0;p<6;++p)row.setAttribute("p"+juce::String(p),static_cast<double>(state.params[static_cast<std::size_t>(p)]));
    row.setAttribute("extendedEq",state.extendedEq);row.setAttribute("eqInitialized",state.eqInitialized);
    if(state.type==builtin::Equalizer && state.eqInitialized)for(std::size_t index=0;index<state.eqBands.size();++index){
        const auto& band=state.eqBands[index];auto* child=row.createNewChildElement("EqBand");
        child->setAttribute("index",static_cast<int>(index));child->setAttribute("enabled",band.enabled);child->setAttribute("type",band.type);
        child->setAttribute("frequency",static_cast<double>(band.frequency));child->setAttribute("gainDb",static_cast<double>(band.gainDb));child->setAttribute("q",static_cast<double>(band.q));
    }
    auto writePro=[&](const char* tag,const auto& pro){auto* node=row.createNewChildElement(tag);node->setAttribute("schema",1);node->setAttribute("enabled",pro.enabled);
        for(std::size_t p=0;p<pro.params.size();++p)node->setAttribute("p"+juce::String(static_cast<int>(p)),static_cast<double>(pro.params[p]));};
    if(state.type==builtin::DigitalFx)writePro("DigitalFxPro",state.digitalFxPro);
    if(state.type==builtin::Dynamics)writePro("DynamicsPro",state.dynamicsPro);
}
inline bool readSlot(const juce::XmlElement& row,builtin::SlotSnapshot& state,juce::String& error){
    state={};state.type=row.getIntAttribute("module");state.enabled=row.getBoolAttribute("enabled");
    const auto& metadata=builtin::metadata(state.type);
    for(int p=0;p<6;++p)state.params[static_cast<std::size_t>(p)]=static_cast<float>(row.getDoubleAttribute("p"+juce::String(p),metadata.params[static_cast<std::size_t>(p)].defaultValue));
    state.extendedEq=row.getBoolAttribute("extendedEq");state.eqInitialized=row.getBoolAttribute("eqInitialized",state.extendedEq);
    std::array<bool,builtin::eqBandCount> seen{};
    for(auto* child:row.getChildIterator())if(child->hasTagName("EqBand")){
        const int index=child->getIntAttribute("index",-1);
        if(index<0 || index>=builtin::eqBandCount || seen[static_cast<std::size_t>(index)]){error="Invalid or duplicate EQ band index.";return false;}
        seen[static_cast<std::size_t>(index)]=true;auto& band=state.eqBands[static_cast<std::size_t>(index)];
        band.enabled=child->getBoolAttribute("enabled");band.type=child->getIntAttribute("type");
        band.frequency=static_cast<float>(child->getDoubleAttribute("frequency",band.frequency));
        band.gainDb=static_cast<float>(child->getDoubleAttribute("gainDb",band.gainDb));band.q=static_cast<float>(child->getDoubleAttribute("q",band.q));
    }
    auto readPro=[&](const char* tag,int module,auto& pro){
        const juce::XmlElement* found=nullptr;
        for(const auto* node:row.getChildIterator())if(node->hasTagName(tag)){if(found){error="Duplicate advanced processor state.";return false;}found=node;}
        if(!found)return true;
        if(state.type!=module||found->getIntAttribute("schema")!=1){error="Advanced state does not match its module/schema.";return false;}
        const auto enabled=found->getStringAttribute("enabled");if(enabled!="0"&&enabled!="1"){error="Invalid advanced processor mode.";return false;}pro.enabled=enabled=="1";
        for(std::size_t p=0;p<pro.params.size();++p){const auto name="p"+juce::String(static_cast<int>(p));if(!found->hasAttribute(name)){error="Incomplete advanced processor state.";return false;}pro.params[p]=static_cast<float>(found->getDoubleAttribute(name,pro.params[p]));}
        return true;
    };
    if(!readPro("DigitalFxPro",builtin::DigitalFx,state.digitalFxPro)||!readPro("DynamicsPro",builtin::Dynamics,state.dynamicsPro))return false;
    state=builtin::boundedSnapshot(state);return true;
}
inline void writeRack(juce::XmlElement& root,const char* tag,const builtin::Controls& controls){
    auto* rack=root.createNewChildElement(tag);rack->setAttribute("schema",3);
    for(int channel=0;channel<32;++channel)for(int slot=0;slot<4;++slot){
        auto* row=rack->createNewChildElement("Slot");row->setAttribute("channel",channel);row->setAttribute("slot",slot);
        writeSlot(*row,builtin::snapshotSlot(controls.slots[static_cast<std::size_t>(channel)][static_cast<std::size_t>(slot)]));
    }
}
inline bool readRack(const juce::XmlElement& root,const char* tag,builtin::SceneSnapshot& result,juce::String& error){
    result={};const auto* rack=root.getChildByName(tag);if(!rack)return true;
    const int schema=rack->getIntAttribute("schema",1);
    if(schema<1||schema>3){error="Unsupported native rack schema.";return false;}
    std::array<bool,128> seen{};
    for(auto* row:rack->getChildIterator())if(row->hasTagName("Slot")){
        const int channel=row->getIntAttribute("channel",-1),slot=row->getIntAttribute("slot",-1);
        if(channel<0||channel>=32||slot<0||slot>=4||seen[static_cast<std::size_t>(channel*4+slot)]){error="Invalid or duplicate rack slot.";return false;}
        seen[static_cast<std::size_t>(channel*4+slot)]=true;
        if(!readSlot(*row,result[static_cast<std::size_t>(channel)][static_cast<std::size_t>(slot)],error))return false;
    }return true;
}
struct State {
    std::unique_ptr<dsp::Controls> mixer=std::make_unique<dsp::Controls>();
    builtin::SceneSnapshot input{},output{};
    juce::StringArray names;
};
inline std::unique_ptr<juce::XmlElement> capture(const dsp::Controls& controls,const builtin::Controls& input,const builtin::Controls& output,const juce::StringArray& names){
    auto root=std::make_unique<juce::XmlElement>("LiveAudioHostScene");root->setAttribute("schema",3);
    for(const auto& field:fields())for(int channel=0;channel<32;++channel)
        root->setAttribute(juce::String(field.name)+juce::String(channel),static_cast<double>((controls.*field.member)[static_cast<std::size_t>(channel)].load()));
    for(std::size_t route=0;route<controls.route.size();++route)root->setAttribute("route"+juce::String(static_cast<int>(route)),static_cast<double>(controls.route[route].load()));
    root->setAttribute("delayReturn",static_cast<double>(controls.delayReturn.load()));root->setAttribute("reverbReturn",static_cast<double>(controls.reverbReturn.load()));
    root->setAttribute("fxDelayMs",static_cast<double>(controls.fxDelayMs.load()));root->setAttribute("fxFeedback",static_cast<double>(controls.fxFeedback.load()));
    root->setAttribute("referenceChannel",controls.referenceChannel.load());root->setAttribute("measurementChannel",controls.measurementChannel.load());
    auto* labels=root->createNewChildElement("ChannelNames");
    for(int channel=0;channel<32;++channel){auto* row=labels->createNewChildElement("Channel");row->setAttribute("index",channel);
        row->setAttribute("name",cleanName(channel<names.size()?names[channel]:juce::String{},"CH"+juce::String(channel+1),32));}
    writeRack(*root,"BuiltinRack",input);writeRack(*root,"OutputBuiltinRack",output);
    ui::writeBusMaster(*root,controls.busMaster);return root;
}
inline std::unique_ptr<State> decode(const juce::XmlElement& root,juce::String& error){
    const int schema=root.getIntAttribute("schema",1);
    if(!root.hasTagName("LiveAudioHostScene") || schema<1 || schema>3){error="Unsupported or invalid scene document.";return {};}
    auto state=std::make_unique<State>();auto& controls=*state->mixer;
    for(const auto& field:fields())for(int channel=0;channel<32;++channel){auto& value=(controls.*field.member)[static_cast<std::size_t>(channel)];
        value.store(bounded(root.getDoubleAttribute(juce::String(field.name)+juce::String(channel),value.load()),field.low,field.high,value.load()));}
    for(auto& polarity:controls.outputPolarity)polarity.store(polarity.load()<0?-1.0f:1.0f);
    for(std::size_t route=0;route<controls.route.size();++route)controls.route[route].store(bounded(root.getDoubleAttribute("route"+juce::String(static_cast<int>(route)),controls.route[route].load()),-2,2,controls.route[route].load()));
    controls.delayReturn.store(bounded(root.getDoubleAttribute("delayReturn"),0,1,0));controls.reverbReturn.store(bounded(root.getDoubleAttribute("reverbReturn"),0,1,0));
    controls.fxDelayMs.store(bounded(root.getDoubleAttribute("fxDelayMs",250),10,1800,250));controls.fxFeedback.store(bounded(root.getDoubleAttribute("fxFeedback",.3),0,.85f,.3f));
    controls.referenceChannel.store(std::clamp(root.getIntAttribute("referenceChannel",0),0,31));controls.measurementChannel.store(std::clamp(root.getIntAttribute("measurementChannel",1),0,31));
    if(!readRack(root,"BuiltinRack",state->input,error)||!readRack(root,"OutputBuiltinRack",state->output,error)||!ui::readBusMaster(root,controls.busMaster,error))return {};
    for(int channel=0;channel<32;++channel)state->names.add("CH"+juce::String(channel+1));
    if(const auto* labels=root.getChildByName("ChannelNames")){std::array<bool,32> seen{};
        for(auto* row:labels->getChildIterator())if(row->hasTagName("Channel")){const int channel=row->getIntAttribute("index",-1);
            if(channel<0||channel>=32||seen[static_cast<std::size_t>(channel)]){error="Invalid or duplicate channel name index.";return {};}
            seen[static_cast<std::size_t>(channel)]=true;state->names.set(channel,cleanName(row->getStringAttribute("name"),state->names[channel],32));}}
    return state;
}
// The caller has already muted the transport. Parsing/validation finishes
// before this commit, so a corrupt scene cannot partially change the session.
inline void apply(const State& state,dsp::Controls& target,builtin::Controls& input,builtin::Controls& output){
    const auto& source=*state.mixer;
    for(const auto& field:fields())for(int channel=0;channel<32;++channel)(target.*field.member)[static_cast<std::size_t>(channel)].store((source.*field.member)[static_cast<std::size_t>(channel)].load());
    for(std::size_t route=0;route<target.route.size();++route)target.route[route].store(source.route[route].load());
    target.delayReturn.store(source.delayReturn.load());target.reverbReturn.store(source.reverbReturn.load());target.fxDelayMs.store(source.fxDelayMs.load());target.fxFeedback.store(source.fxFeedback.load());
    target.referenceChannel.store(source.referenceChannel.load());target.measurementChannel.store(source.measurementChannel.load());
    dsp::recallBusMaster(target.busMaster,dsp::snapshotBusMaster(source.busMaster));builtin::recall(input,state.input);builtin::recall(output,state.output);
}
struct NativePreset {bool chain=false;juce::String name;std::array<builtin::SlotSnapshot,4> slots{};};
inline std::unique_ptr<juce::XmlElement> capturePreset(const builtin::Controls& controls,int channel,int slot,bool chain,juce::String name){
    auto root=std::make_unique<juce::XmlElement>("LiveAudioHostPreset");root->setAttribute("schema",3);root->setAttribute("kind",chain?"chain":"plugin");root->setAttribute("name",cleanName(name,"Native preset"));
    for(int index=0;index<(chain?4:1);++index){auto* row=root->createNewChildElement("Slot");row->setAttribute("index",index);
        writeSlot(*row,builtin::snapshotSlot(controls.slots[static_cast<std::size_t>(std::clamp(channel,0,31))][static_cast<std::size_t>(chain?index:std::clamp(slot,0,3))]));}return root;
}
inline bool decodePreset(const juce::XmlElement& root,NativePreset& preset,juce::String& error){
    if(!root.hasTagName("LiveAudioHostPreset")||root.getIntAttribute("schema")<2||root.getIntAttribute("schema")>3){error="Invalid native preset schema.";return false;}
    const auto kind=root.getStringAttribute("kind");if(kind!="chain"&&kind!="plugin"){error="Unknown preset kind.";return false;}
    NativePreset staged;staged.chain=kind=="chain";staged.name=cleanName(root.getStringAttribute("name"),"Native preset");std::array<bool,4> seen{};int count=0;
    for(auto* row:root.getChildIterator())if(row->hasTagName("Slot")){const int index=row->getIntAttribute("index",-1);
        if(index<0||index>=(staged.chain?4:1)||seen[static_cast<std::size_t>(index)]){error="Invalid or duplicate preset slot.";return false;}
        seen[static_cast<std::size_t>(index)]=true;++count;if(!readSlot(*row,staged.slots[static_cast<std::size_t>(index)],error))return false;}
    if(count!=(staged.chain?4:1)){error="Preset does not contain the required slots.";return false;}preset=staged;return true;
}
inline void applyPreset(const NativePreset& preset,builtin::Controls& controls,int channel,int slot){
    channel=std::clamp(channel,0,31);slot=std::clamp(slot,0,3);
    for(int index=0;index<(preset.chain?4:1);++index)builtin::recallSlot(controls.slots[static_cast<std::size_t>(channel)][static_cast<std::size_t>(preset.chain?index:slot)],preset.slots[static_cast<std::size_t>(index)]);
}
// JUCE accepts some truncated fragments as an element. Bound and validate the
// complete outer document first, including matching tags and a single root.
inline bool completeXmlDocument(const juce::String& text){
    const auto utf8=text.toUTF8();const std::string_view data(utf8.getAddress());std::vector<std::string_view> stack;stack.reserve(32);
    std::size_t cursor=0;int roots=0,nodes=0;
    const auto whitespace=[](char c){return c==' '||c=='\t'||c=='\r'||c=='\n';};
    while(cursor<data.size()){
        if(data[cursor]!='<'){if(stack.empty()&&!whitespace(data[cursor]))return false;++cursor;continue;}
        if(data.substr(cursor,4)=="<!--"){const auto end=data.find("-->",cursor+4);if(end==std::string_view::npos)return false;cursor=end+3;continue;}
        if(data.substr(cursor,9)=="<![CDATA["){if(stack.empty())return false;const auto end=data.find("]]>",cursor+9);if(end==std::string_view::npos)return false;cursor=end+3;continue;}
        if(data.substr(cursor,2)=="<?"){const auto end=data.find("?>",cursor+2);if(end==std::string_view::npos)return false;cursor=end+2;continue;}
        if(data.substr(cursor,2)=="<!")return false;
        const bool closing=data.substr(cursor,2)=="</";std::size_t start=cursor+(closing?2:1),endName=start;
        while(endName<data.size()&&!whitespace(data[endName])&&data[endName]!='/'&&data[endName]!='>')++endName;
        if(start==endName)return false;const auto name=data.substr(start,endName-start);
        std::size_t end=endName;char quote=0;
        for(;end<data.size();++end){const char c=data[end];if(quote){if(c==quote)quote=0;}else if(c==34||c==39)quote=c;else if(c=='>')break;else if(c=='<')return false;}
        if(end==data.size()||quote)return false;
        if(closing){if(stack.empty()||stack.back()!=name)return false;for(auto n=endName;n<end;++n)if(!whitespace(data[n]))return false;stack.pop_back();}
        else{if(++nodes>8192)return false;if(stack.empty()&&++roots>1)return false;
            std::size_t last=end;while(last>endName&&whitespace(data[last-1]))--last;
            if(last==endName||data[last-1]!='/'){if(stack.size()>=64)return false;stack.push_back(name);}}
        cursor=end+1;
    }return roots==1&&stack.empty();
}
inline std::unique_ptr<juce::XmlElement> readFile(const juce::File& file,juce::String& error){
    if(!file.existsAsFile()||file.getSize()<1||file.getSize()>4*1024*1024){error="Missing document or file exceeds 4 MiB.";return {};}
    const auto text=file.loadFileAsString();if(text.containsIgnoreCase("<!DOCTYPE")||text.containsIgnoreCase("<!ENTITY")){error="DTD/entity declarations are not accepted.";return {};}
    if(!completeXmlDocument(text)){error="Truncated, unbalanced or oversized XML document.";return {};}
    auto xml=juce::XmlDocument::parse(text);if(!xml)error="Malformed XML document.";return xml;
}
inline bool writeFile(const juce::File& file,const juce::XmlElement& xml,juce::String& error){
    if(!file.getParentDirectory().createDirectory().wasOk()){error="Cannot create library directory.";return false;}
    juce::TemporaryFile temporary(file);if(!xml.writeTo(temporary.getFile())||!temporary.overwriteTargetFileWithTemporary()){error="Cannot atomically save document.";return false;}return true;
}
} // namespace lah::session
