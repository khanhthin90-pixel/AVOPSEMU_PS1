#include "audio_stretcher.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
namespace ps1 {
unsigned AudioStretcher::required(unsigned frames,double ratio)const{
 unsigned available=Hop-ready_offset;if(frames<=available)return 0;
 if(!std::isfinite(ratio)||ratio<0.25||ratio>2)return std::numeric_limits<unsigned>::max();
 unsigned blocks=(frames-available+Hop-1)/Hop;
 double last=position+(blocks-1)*Hop*ratio;
 if(!primed&&blocks==1)return Window;
 double limit=std::ceil(last)+Search+Window;
 if(limit>=std::numeric_limits<unsigned>::max())return std::numeric_limits<unsigned>::max();
 return unsigned(limit);
}
void AudioStretcher::generate(std::deque<int16_t>& input,double ratio){
 unsigned chosen=0;
 if(primed){
  unsigned nominal=unsigned(std::lround(position));
  unsigned low=nominal>Search?nominal-Search:0;
  unsigned high=std::min(unsigned(input.size()/2)-Window,nominal+Search);
  chosen=std::clamp(nominal,low,high);
  if(std::abs(ratio-1)>0.0001){
   // Read the deque once; the search repeatedly accesses overlapping windows.
   std::array<int16_t,(Hop+2*Search+2)*2> samples;
   std::copy_n(input.begin(),(high+Hop)*2,samples.begin());
   // Four interleaved prefix sums reproduce the stride-4 energy exactly.
   std::array<int64_t,Hop+2*Search+6> energy_prefix{};
   for(unsigned i=0;i<high+Hop;i++){
    const int64_t l=samples[2*i],r=samples[2*i+1];
    energy_prefix[i+4]=energy_prefix[i]+l*l+r*r;
   }
   int64_t energy_a=1;
   for(unsigned i=0;i<Hop;i+=4)for(unsigned ch=0;ch<2;ch++){int64_t a=tail[2*i+ch];energy_a+=a*a;}
   auto similarity=[&](unsigned candidate){
    int64_t dot=0;const int64_t energy_b=1+energy_prefix[candidate+Hop]-energy_prefix[candidate];
    for(unsigned i=0;i<Hop;i+=4)for(unsigned ch=0;ch<2;ch++){
     int64_t a=tail[2*i+ch],b=samples[2*(candidate+i)+ch];dot+=a*b;
    }
    return double(dot)/std::sqrt(double(energy_a)*double(energy_b))-1e-9*std::abs(double(candidate)-position);
   };
   double best=similarity(chosen);
   // Coarse search followed by sample-resolution refinement; both channels share the offset.
   for(unsigned n=low;n<=high;n+=4){double score=similarity(n);if(score>best){best=score;chosen=n;}}
   unsigned fine_low=chosen>3?std::max(low,chosen-3):low,fine_high=std::min(high,chosen+3);
   for(unsigned n=fine_low;n<=fine_high;n++){double score=similarity(n);if(score>best){best=score;chosen=n;}}
  }
 }
 for(unsigned i=0;i<Hop;i++)for(unsigned ch=0;ch<2;ch++){
  const int current=input[2*(chosen+i)+ch];
  // Complementary linear windows. At 1x this reproduces the source exactly.
  ready[2*i+ch]=primed?int16_t((int64_t(tail[2*i+ch])*(Hop-i)+int64_t(current)*i)/Hop):int16_t(current);
  tail[2*i+ch]=input[2*(chosen+Hop+i)+ch];
 }
 primed=true;ready_offset=0;position+=Hop*ratio;
 // Keep search history, and advance the nominal timeline independently of matching offsets.
 unsigned consumed=position>Search?unsigned(position-Search):0;
 input.erase(input.begin(),input.begin()+consumed*2);
 position-=consumed;
}
bool AudioStretcher::fill(std::deque<int16_t>& input,int16_t* output,unsigned frames,double ratio){
 if(!frames)return true;
 if(!std::isfinite(ratio)||ratio<0.25||ratio>2||input.size()/2<required(frames,ratio))return false;
 unsigned written=0;
 while(written<frames){
  if(ready_offset==Hop)generate(input,ratio);
  unsigned take=std::min(frames-written,Hop-ready_offset);
  std::copy_n(ready.begin()+ready_offset*2,take*2,output+written*2);
  ready_offset+=take;written+=take;
 }
 return true;
}
}
