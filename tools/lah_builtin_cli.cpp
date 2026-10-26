// Offline bridge for independently generated DSP vectors. No device or JUCE.
// Input/output: native little-endian interleaved stereo IEEE float32 samples.
// Channel 0 has one selected processor; channel 1 remains exactly bypassed.
#include "../src/dsp/BuiltinRackCore.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

namespace {
double number(const char* text) {
    std::size_t used=0;
    const auto value=std::stod(text,&used);
    if(used!=std::strlen(text)||!std::isfinite(value))throw std::invalid_argument("Invalid finite numeric argument");
    return value;
}
int integer(const char* text,int minimum,int maximum) {
    const double value=number(text);
    if(value!=std::floor(value)||value<minimum||value>maximum)throw std::invalid_argument("Integer argument outside range");
    return static_cast<int>(value);
}
std::string quoted(const char* text) {
    std::string result="\"";
    for(const char* p=text;*p;++p){
        if(*p=='"'||*p=='\\')result+='\\';
        if(*p=='\n')result+="\\n";else result+=*p;
    }
    return result+'"';
}
void catalog() {
    std::cout<<"["<<std::setprecision(10);
    for(int type=1;type<=lah::builtin::moduleCount;++type){
        const auto& info=lah::builtin::metadata(type);
        if(type!=1)std::cout<<',';
        std::cout<<"{\"type\":"<<type<<",\"name\":"<<quoted(info.name)
                 <<",\"description\":"<<quoted(info.description)<<",\"parameters\":[";
        for(int i=0;i<info.parameterCount;++i){
            const auto& parameter=info.params[static_cast<std::size_t>(i)];
            if(i)std::cout<<',';
            std::cout<<"{\"name\":"<<quoted(parameter.name)<<",\"unit\":"<<quoted(parameter.unit)
                     <<",\"min\":"<<parameter.minimum<<",\"max\":"<<parameter.maximum
                     <<",\"default\":"<<parameter.defaultValue<<'}';
        }
        std::cout<<"]}";
    }
    std::cout<<"]\n";
}
}

int main(int argc,char** argv) {
    try {
        if(argc==2&&std::string(argv[1])=="--catalog"){catalog();return 0;}
        if(argc<9||argc>13){
            std::cerr<<"Usage: lah_builtin_cli type rate p0 p1 p2 p3 p4 p5 [enabled=1] [resetFrame=-1] [disableFrame=-1] [blockSize=0]\n"
                       "stdin/stdout: interleaved stereo float32; blockSize 0 uses 1/17/64/127/513 frames.\n";
            return 2;
        }
        const int type=integer(argv[1],1,lah::builtin::moduleCount);
        const double rate=number(argv[2]);
        if(rate<8000||rate>192000)throw std::invalid_argument("Rate must be 8000..192000 Hz");
        const int enabled=argc>9?integer(argv[9],0,1):1;
        const int resetFrame=argc>10?integer(argv[10],-1,1920000):-1;
        const int disableFrame=argc>11?integer(argv[11],-1,1920000):-1;
        const int blockSize=argc>12?integer(argv[12],0,4096):0;
#ifdef _WIN32
        _setmode(_fileno(stdin),_O_BINARY);
        _setmode(_fileno(stdout),_O_BINARY);
#endif
        static_assert(sizeof(float)==4,"Fixture protocol requires IEEE float32");
        const std::vector<char> bytes((std::istreambuf_iterator<char>(std::cin)),std::istreambuf_iterator<char>());
        if(bytes.empty()||bytes.size()%(2*sizeof(float))!=0||bytes.size()>1920000u*2u*sizeof(float))
            throw std::invalid_argument("Supply 1..1920000 interleaved stereo float32 frames");
        std::vector<float> input(bytes.size()/sizeof(float));
        std::memcpy(input.data(),bytes.data(),bytes.size());
        std::vector<float> output(input.size());
        const int frames=static_cast<int>(input.size()/2);
        if(resetFrame>=frames||disableFrame>=frames)throw std::invalid_argument("Event frame must precede end of input");
        lah::builtin::Controls controls;
        lah::builtin::configureSlot(controls.slots[0][0],type,enabled!=0);
        for(int i=0;i<6;++i)controls.slots[0][0].params[static_cast<std::size_t>(i)].store(static_cast<float>(number(argv[3+i])));
        lah::builtin::Rack rack;
        rack.prepare(rate,std::max(513,blockSize));
        constexpr std::array<int,5> blocks{1,17,64,127,513};
        for(int offset=0,block=0;offset<frames;++block){
            if(offset==resetFrame)rack.reset();
            if(offset==disableFrame)controls.slots[0][0].enabled.store(false);
            int count=std::min(blockSize>0?blockSize:blocks[static_cast<std::size_t>(block)%blocks.size()],frames-offset);
            for(const int event:{resetFrame,disableFrame})if(event>offset&&event<offset+count)count=event-offset;
            rack.beginBlock(controls);
            for(int frame=offset;frame<offset+count;++frame){
                const auto sample=static_cast<std::size_t>(2*frame);
                output[sample]=rack.processSample(0,input[sample]);
                output[sample+1]=rack.processSample(1,input[sample+1]);
            }
            offset+=count;
        }
        if(type==lah::builtin::AutoFeedback)
            std::cerr<<std::setprecision(10)<<"{\"candidateHz\":"<<rack.feedbackCandidateHz(0,0)<<"}\n";
        std::cout.write(reinterpret_cast<const char*>(output.data()),static_cast<std::streamsize>(output.size()*sizeof(float)));
        return std::cout.good()?0:2;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 2;}
}
