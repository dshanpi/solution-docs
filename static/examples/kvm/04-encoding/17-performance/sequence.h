#pragma once
#include <cstring>
#include <cstdint>
constexpr int W=1920,H=1080,N=600,BYTES=W*H*3/2;
// Exact same integer NV12 transform on x86 reference and ARM hardware input.
inline void make_frame(uint8_t* out,const uint8_t* base,int frame) {
 std::memcpy(out,base,BYTES);
 if(frame<120)return;
 int dy=frame<360 ? ((frame-120)*8)%1000 : ((frame-360)*6)%1000;
 int dx=frame<360 ? 0 : ((frame-360)*12)%W;
 for(int plane=0;plane<2;plane++) {
  int start=plane?40:80,rows=plane?500:1000,shift=plane?dy/2:dy;
  int offset=plane?W*H:0;
  for(int y=0;y<rows;y++) {
   const uint8_t* src=base+offset+(start+(y+shift)%rows)*W;
   uint8_t* dst=out+offset+(start+y)*W;
   std::memcpy(dst,src+dx,W-dx);if(dx)std::memcpy(dst+W-dx,src,dx);
  }
 }
}
