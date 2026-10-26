#include "../../porpoise/src/porpoise_move_bridge.hpp"
extern "C" {
#include "../src/porpoise_move.h"
}
#include <cassert>
#include <fstream>
#include <chrono>
int main(){
 const unsigned char calibration[143] = {16,0,98,7,113,127,7,127,44,144,121,110,116,127,229,126,171,127,106,127,3,110,154,144,49,127,63,127,117,127,93,144,126,127,120,127,63,110,221,126,253,8,148,127,9,128,100,128,231,7,149,127,10,128,107,128,0,0,0,0,0,0,0,1,0,1,251,8,224,1,227,147,10,128,8,128,224,1,248,127,85,146,88,128,224,1,194,127,40,128,131,148,3,8,192,160,205,65,80,171,189,67,73,208,143,195,125,66,116,63,68,232,125,63,199,157,113,63,230,135,138,63,12,125,196,61,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
 porpoise::move_bridge::Client consumer;assert(consumer.open(9976));assert(movebridge_start(0,calibration));
 pad_state state;pad_state_reset(&state);state.has_motion=1;state.buttons=PAD_L2|PAD_TOUCHPAD|PAD_CREATE;state.l2=255;state.accel[0]=-4487;state.accel[1]=4189;state.accel[2]=4140;state.gyro[0]=5199;state.gyro[1]=4684;state.gyro[2]=5151;
 auto now=[](){return(uint64_t)std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();};
 assert(movebridge_update(0,&state,now()));consumer.poll(now());assert(consumer.fresh(now()));auto&p=consumer.packet();assert(p.valid);assert(porpoise::move_bridge::detail::u32(p.pad)==(PAD_L2|0x20000|PAD_CREATE));assert(p.pad[8]==255);
 for(int a=0;a<3;++a){assert(std::fabs(porpoise::move_bridge::detail::f32(p.pad+28+4*a)-(a==1?1.f:-1.f))<0.001f);assert(std::fabs(porpoise::move_bridge::detail::f32(p.pad+40+4*a)-(a==2?-8.37758f:8.37758f))<0.001f);}
 movebridge_disconnect(0);consumer.poll(now());assert(consumer.active(now()));assert(!consumer.packet().valid);assert(!consumer.fresh(now()));movebridge_close();
}