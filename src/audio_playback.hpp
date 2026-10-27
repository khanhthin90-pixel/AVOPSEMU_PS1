#pragma once
#include "audio_stretcher.hpp"
#include <cstdint>
#include <deque>
namespace ps1 {
// Owned by the playback thread. Receives PCM via a short queue swap under the machine lock.
struct AudioPlayback {
 std::deque<int16_t> input;
 AudioStretcher stretcher;
 std::array<int16_t,2> last_sample{},transition_from{};
 unsigned transition_left=0;
 static constexpr unsigned MaxInputFrames=8192,TrimInputFrames=6144;
 double rate=1,ratio=1,last_time=0,gain=0;
 uint64_t last_frames=0,dropped=0;
 bool measured=false;
 void append(std::deque<int16_t>& incoming);
 void observe(double seconds,uint64_t generated,unsigned device_frames);
 bool preroll()const;
 bool render(int16_t* output,unsigned frames,bool muted);
 void conceal(int16_t* output,unsigned frames);
 void restart(bool from_silence=false){stretcher.reset();gain=0;transition_from=from_silence?std::array<int16_t,2>{}:last_sample;transition_left=128;}
 void fade_in(){gain=0;}
};
}
