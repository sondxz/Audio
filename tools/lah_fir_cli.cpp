#include "../src/fir/FIRCore.h"
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
int main(int argc,char** argv){
    if(argc!=5){std::cerr<<"Usage: lah_fir_cli lp|hp samplerate cutoff_hz odd_taps\n";return 2;}
    try {
        const std::string type(argv[1]);
        if(type!="lp"&&type!="hp"){std::cerr<<"Unknown FIR mode\n";return 2;}
        const auto filter=lah::fir::make(type=="lp"?lah::fir::Mode::Lowpass:lah::fir::Mode::Highpass,
                                          std::stod(argv[3]),std::stod(argv[2]),std::stoi(argv[4]));
        std::cout<<std::setprecision(17);
        for(std::size_t i=0;i<filter.taps.size();++i)
            std::cout<<i<<","<<filter.taps[i]<<"\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 2;}
}
