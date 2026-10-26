// Offline reference-vs-measured bench utility. NO live audio and NO SPL calibration.
#include "../src/measurement/MeasurementCore.h"
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

static std::vector<float> readSamples(const char* path){
    std::ifstream file(path);
    if(!file)throw std::runtime_error(std::string("Cannot open: ")+path);
    std::vector<float> data;
    std::string row;
    while(std::getline(file,row)){
        if(row.empty())continue;
        std::size_t consumed=0;
        const double v=std::stod(row,&consumed);
        if(consumed!=row.size()||!std::isfinite(v)||std::abs(v)>100)
            throw std::runtime_error("Nonfinite/invalid sample in input CSV");
        if(data.size()>=65536)throw std::runtime_error("Maximum: 65536 samples per file");
        data.push_back(static_cast<float>(v));
    }
    if(data.size()<16384)throw std::runtime_error("Need at least 16384 samples per channel");
    return data;
}
int main(int argc,char** argv){
    if(argc!=5){
        std::cerr<<"Usage: lah_tf_cli <sample_rate> <reference_one_column.csv> <measured_one_column.csv> <result.csv>\n"
                 <<"CSV: one finite float/sample, no header; same rate and samples for both input files.\n";
        return 2;
    }
    try{
        std::size_t pos=0;
        const double fs=std::stod(argv[1],&pos);
        if(pos!=std::string(argv[1]).size()||!std::isfinite(fs)||fs<8000||fs>384000)throw std::runtime_error("Bad sample rate");
        const auto ref=readSamples(argv[2]);
        const auto mic=readSamples(argv[3]);
        if(ref.size()!=mic.size())throw std::runtime_error("Both channels must have equal frame count");
        const auto result=lah::measurement::analyze(ref,mic,fs);
        std::ofstream out(argv[4]);
        if(!out)throw std::runtime_error("Cannot write CSV output");
        out<<"frequency_hz,rta_dbfs_per_bin_rms,tf_magnitude_db,tf_phase_deg_compensated,coherence,bin_valid,bin_reliable,delay_ms,delay_valid\n";
        out<<std::fixed<<std::setprecision(7);
        for(int k=1;k<=lah::measurement::fftSize/2;++k){
            const auto i=static_cast<std::size_t>(k);
            out<<k*fs/lah::measurement::fftSize<<","<<result.rtaDb[i]<<",";
            if(result.binValid[i])out<<result.magnitudeDb[i];
            out<<",";
            if(result.binValid[i])out<<result.phaseDeg[i];
            out<<",";
            if(result.binValid[i])out<<result.coherence[i];
            out<<","<<(result.binValid[i]?1:0)<<","<<(result.binReliable[i]?1:0)
               <<","<<result.delayMs<<","<<(result.delayValid?1:0)<<"\n";
        }
        std::cerr<<"TF valid="<<result.valid<<" refRms="<<result.referenceRmsDbfs
                 <<"dBFS micRms="<<result.measurementRmsDbfs
                 <<"dBFS; reliableBins="<<result.reliableBins
                 <<"; avg coherence="<<result.meanCoherence
                 <<"; delay estimate="<<(result.delayValid?std::to_string(result.delayMs):"INVALID")
                 <<"ms. Do not equate with calibrated SPL or loopback round-trip latency.\n";
        return result.valid?0:1;
    }catch(const std::exception& e){std::cerr<<"ERROR: "<<e.what()<<"\n";return 2;}
}
