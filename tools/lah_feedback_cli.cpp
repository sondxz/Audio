// Device-free fixture bridge. The source signal and numerical reference are owned by Python.
// stdin/stdout: mono little-endian IEEE float32; JSON telemetry is written only to stderr.
#include "../src/dsp/FeedbackSuppressionCore.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
namespace {
constexpr double pi=3.14159265358979323846;
double number(const char* text) {
    std::size_t used=0;const auto value=std::stod(text,&used);
    if(used!=std::strlen(text)||!std::isfinite(value))throw std::invalid_argument("Supply a finite numeric argument");
    return value;
}
int integer(const char* text,int minimum,int maximum) {
    const auto value=number(text);
    if(value!=std::floor(value)||value<minimum||value>maximum)throw std::invalid_argument("Integer outside argument range");
    return static_cast<int>(value);
}
// Test plant only: unity-peak-gain second-order bandpass. This is not an AFC model.
struct PlantBandpass {
    double b0=0,b2=0,a1=0,a2=0,z1=0,z2=0;
    void prepare(double rate,double hz,double q) {
        if(hz<20||hz>=rate*.45||q<.1||q>100)throw std::invalid_argument("Loop plant frequency/Q outside range");
        const double w=2*pi*hz/rate,alpha=std::sin(w)/(2*q),scale=1+alpha;
        b0=alpha/scale;b2=-b0;a1=-2*std::cos(w)/scale;a2=(1-alpha)/scale;
    }
    double tick(double x) noexcept {const double y=b0*x+z1;z1=-a1*y+z2;z2=b2*x-a2*y;return y;}
};
}
int main(int argc,char** argv) {
    try {
        if(argc<8||argc>14){
            std::cerr<<"Usage: lah_feedback_cli rate armed useModel persistenceMs modelThreshold maxDepthDb q [resetFrame=-1] [disarmFrame=-1] [loopDelay=0] [loopGain=0] [loopHz=1000] [loopQ=20]\n"
                       "stdin/stdout mono float32. Optional plant: input[n]=source[n]+loopGain*bandpass(output[n-loopDelay]); clamp plant input to +/-8. No audio device.\n";
            return 2;
        }
        const double rate=number(argv[1]);if(rate<8000||rate>192000)throw std::invalid_argument("Rate must be 8000..192000 Hz");
        lah::feedback::Settings settings;
        settings.armed=integer(argv[2],0,1)!=0;settings.useModel=integer(argv[3],0,1)!=0;
        settings.persistenceMs=static_cast<float>(number(argv[4]));settings.modelThreshold=static_cast<float>(number(argv[5]));
        settings.maxDepthDb=static_cast<float>(number(argv[6]));settings.q=static_cast<float>(number(argv[7]));
        const int reset=argc>8?integer(argv[8],-1,10000000):-1;
        const int disarm=argc>9?integer(argv[9],-1,10000000):-1;
        const int loopDelay=argc>10?integer(argv[10],0,192000):0;
        const double loopGain=argc>11?number(argv[11]):0;
        const double loopHz=argc>12?number(argv[12]):1000;
        const double loopQ=argc>13?number(argv[13]):20;
        if(loopGain<0||loopGain>2)throw std::invalid_argument("Loop gain must be 0..2");
#ifdef _WIN32
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
#endif
        static_assert(sizeof(float)==4,"Fixture protocol requires float32");
        const std::vector<char> bytes((std::istreambuf_iterator<char>(std::cin)),std::istreambuf_iterator<char>());
        if(bytes.empty()||bytes.size()%sizeof(float)!=0||bytes.size()>40000000)
            throw std::invalid_argument("Supply 1..10000000 mono float32 samples");
        std::vector<float> input(bytes.size()/sizeof(float)),output(input.size());std::memcpy(input.data(),bytes.data(),bytes.size());
        const auto frames=static_cast<int>(input.size());
        if(reset>=frames||disarm>=frames)throw std::invalid_argument("Event must precede end of fixture");
        auto processor=std::make_unique<lah::feedback::Processor>();processor->prepare(rate);processor->configure(settings);
        PlantBandpass plant;plant.prepare(rate,loopHz,loopQ);std::vector<float> ring(static_cast<std::size_t>(std::max(1,loopDelay)),0);
        int position=0,firstNotchFrame=-1,maximumNotchCount=0;float maximumScore=0,maximumInput=0,maximumOutput=0;
        for(int n=0;n<frames;++n){
            if(n==reset){processor->reset();processor->configure(settings);}
            if(n==disarm){settings.armed=false;processor->configure(settings);}
            float x=input[static_cast<std::size_t>(n)];
            if(loopDelay>0)x=static_cast<float>(std::clamp(static_cast<double>(x)+loopGain*plant.tick(ring[static_cast<std::size_t>(position)]),-8.0,8.0));
            const auto y=processor->processSample(x);output[static_cast<std::size_t>(n)]=y;
            if(loopDelay>0){ring[static_cast<std::size_t>(position)]=y;if(++position==loopDelay)position=0;}
            if(std::isfinite(x))maximumInput=std::max(maximumInput,std::abs(x));
            if(std::isfinite(y))maximumOutput=std::max(maximumOutput,std::abs(y));
            const auto status=processor->status();
            if(firstNotchFrame<0&&status.notchCount>0)firstNotchFrame=n;
            maximumNotchCount=std::max(maximumNotchCount,status.notchCount);maximumScore=std::max(maximumScore,status.modelScore);
        }
        const auto status=processor->status();std::cerr<<std::setprecision(10);
        std::cerr<<"{\"firstNotchFrame\":"<<firstNotchFrame<<",\"maximumNotchCount\":"<<maximumNotchCount
                 <<",\"notchCount\":"<<status.notchCount<<",\"candidateHz\":"<<status.candidateHz
                 <<",\"modelScore\":"<<status.modelScore<<",\"maximumScore\":"<<maximumScore
                 <<",\"armed\":"<<(status.armed?"true":"false")<<",\"maximumInput\":"<<maximumInput
                 <<",\"maximumOutput\":"<<maximumOutput<<",\"notchHz\":[";
        for(std::size_t i=0;i<status.notchHz.size();++i){if(i)std::cerr<<',';std::cerr<<status.notchHz[i];}
        std::cerr<<"],\"depthDb\":[";
        for(std::size_t i=0;i<status.depthDb.size();++i){if(i)std::cerr<<',';std::cerr<<status.depthDb[i];}
        std::cerr<<"]}\n";
        std::cout.write(reinterpret_cast<const char*>(output.data()),static_cast<std::streamsize>(output.size()*sizeof(float)));
        return std::cout.good()?0:2;
    } catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 2;}
}
