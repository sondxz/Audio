#pragma once
#include <juce_core/juce_core.h>
#include "../dsp/BusMasterCore.h"
#include <cstdlib>

namespace lah::ui {
inline void writeBusMaster(juce::XmlElement& root,const dsp::BusMasterControls& controls){
    const auto state=dsp::snapshotBusMaster(controls);auto* node=root.createNewChildElement("BusMaster");
    node->setAttribute("schema",1);node->setAttribute("enabled",state.enabled);node->setAttribute("masterMute",state.masterMute);
    node->setAttribute("left",state.masterLeft);node->setAttribute("right",state.masterRight);
    node->setAttribute("gainL",state.masterGainDb[0]);node->setAttribute("gainR",state.masterGainDb[1]);
    for(int i=0;i<32;++i){auto* source=node->createNewChildElement("Source");source->setAttribute("index",i);
        source->setAttribute("master",state.masterSends[i]);source->setAttribute("pan",state.inputPan[i]);
        for(int b=0;b<8;++b)source->setAttribute("b"+juce::String(b+1),state.sends[b*32+i]);
    }
    for(int b=0;b<8;++b){auto* bus=node->createNewChildElement("Bus");bus->setAttribute("index",b);
        bus->setAttribute("gain",state.gainDb[b]);bus->setAttribute("mute",state.mute[b]);bus->setAttribute("pan",state.pan[b]);
        bus->setAttribute("master",state.toMaster[b]);bus->setAttribute("out",state.output[b]);
    }
}
// Parse the whole node before committing, so corrupt files never partially route audio.
inline bool readBusMaster(const juce::XmlElement& root,dsp::BusMasterControls& controls,juce::String& error){
    dsp::BusMasterSnapshot state;const auto* node=root.getChildByName("BusMaster");
    if(!node){dsp::recallBusMaster(controls,state);return true;}
    auto number=[&error](const juce::XmlElement& e,const juce::String& key,double lo,double hi,double& result){
        if(!e.hasAttribute(key)){error="Missing bus/master field: "+key;return false;}
        const auto text=e.getStringAttribute(key).trim();const auto utf=text.toRawUTF8();char* end=nullptr;
        result=std::strtod(utf,&end);
        if(end==utf||*end!='\0'||!std::isfinite(result)||result<lo||result>hi){error="Invalid bus/master field: "+key;return false;}return true;
    };
    auto field=[&](const juce::XmlElement& e,const juce::String& key,float& target,double lo,double hi){double value=0;if(!number(e,key,lo,hi,value))return false;target=static_cast<float>(value);return true;};
    auto integer=[&](const juce::XmlElement& e,const juce::String& key,int& target,int lo,int hi){double value=0;if(!number(e,key,lo,hi,value)||std::floor(value)!=value){if(error.isEmpty())error="Invalid bus/master integer: "+key;return false;}target=static_cast<int>(value);return true;};
    int schema=0,enabled=0,mute=0;
    if(!integer(*node,"schema",schema,1,1)||!integer(*node,"enabled",enabled,0,1)||!integer(*node,"masterMute",mute,0,1)||
       !integer(*node,"left",state.masterLeft,-1,31)||!integer(*node,"right",state.masterRight,-1,31)||
       !field(*node,"gainL",state.masterGainDb[0],-60,12)||!field(*node,"gainR",state.masterGainDb[1],-60,12))return false;
    if(state.masterLeft>=0&&state.masterLeft==state.masterRight){error="Master L/R must use different USB outputs.";return false;}
    state.enabled=enabled!=0;state.masterMute=mute!=0;
    std::array<bool,32> sources{};std::array<bool,8> buses{};
    for(const auto* child:node->getChildIterator()){
        int index=0;
        if(child->hasTagName("Source")){
            if(!integer(*child,"index",index,0,31))return false;
            if(sources[index]){error="Duplicate bus source.";return false;}sources[index]=true;
            if(!field(*child,"master",state.masterSends[index],0,2)||!field(*child,"pan",state.inputPan[index],-1,1))return false;
            for(int b=0;b<8;++b)if(!field(*child,"b"+juce::String(b+1),state.sends[b*32+index],0,2))return false;
        }else if(child->hasTagName("Bus")){
            if(!integer(*child,"index",index,0,7))return false;
            if(buses[index]){error="Duplicate software bus.";return false;}buses[index]=true;
            if(!field(*child,"gain",state.gainDb[index],-60,12)||!field(*child,"mute",state.mute[index],0,1)||
               !field(*child,"pan",state.pan[index],-1,1)||!field(*child,"master",state.toMaster[index],0,2)||!integer(*child,"out",state.output[index],-1,31))return false;
        }else{error="Unknown bus/master node.";return false;}
    }
    if(std::find(sources.begin(),sources.end(),false)!=sources.end()||std::find(buses.begin(),buses.end(),false)!=buses.end()){
        error="Incomplete bus/master scene.";return false;
    }
    dsp::recallBusMaster(controls,state);return true;
}
}
