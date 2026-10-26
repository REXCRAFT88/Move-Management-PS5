#include "../integrations/move_management_client.hpp"
extern "C" {
#include "../src/move_bridge.h"
}
#include <cassert>
#include <fstream>
#include <chrono>
int main(){
 unsigned char calibration[143]{};
 auto value=[&](unsigned offset,int v){unsigned u=unsigned(v+32768);calibration[offset]=u&255;calibration[offset+1]=u>>8;};
 const unsigned low[3]={10,36,20},high[3]={22,30,8},bias[3]={42,44,46},rate[3]={70,80,90};
 for(unsigned a=0;a<3;a++){value(low[a],-1000*int(a+1));value(high[a],1000*int(a+1));value(bias[a],100);value(rate[a],1100);}
 move_management::Client consumer;assert(consumer.open(9976));assert(movebridge_start(0,calibration));
 pad_state state;pad_state_reset(&state);state.has_motion=1;state.buttons=PAD_L2|PAD_TOUCHPAD|PAD_CREATE;state.l2=255;state.accel[0]=-1000;state.accel[1]=2000;state.accel[2]=3000;state.gyro[0]=1000;state.gyro[1]=1000;state.gyro[2]=1000;
 auto now=[](){return(uint64_t)std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();};
 assert(movebridge_update(0,&state,now()));consumer.poll(now());assert(consumer.fresh(now()));auto&p=consumer.packet();assert(p.valid);assert(move_management::detail::u32(p.pad)==(PAD_L2|0x20000|PAD_CREATE));assert(p.pad[8]==255);
 for(int a=0;a<3;++a){assert(std::fabs(move_management::detail::f32(p.pad+28+4*a)-(a==1?1.f:-1.f))<0.001f);assert(std::fabs(move_management::detail::f32(p.pad+40+4*a)-(a==2?-8.37758f:8.37758f))<0.001f);}
 movebridge_disconnect(0);consumer.poll(now());assert(consumer.active(now()));assert(!consumer.packet().valid);assert(!consumer.fresh(now()));movebridge_close();
}