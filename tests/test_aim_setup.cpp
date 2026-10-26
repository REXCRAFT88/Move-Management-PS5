/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../src/aim_setup.h"
#include "../integrations/aim_screen_profile.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
static uint64_t t=1000000;
static float pitch=0;
static void sample(float x,float y,uint32_t buttons=0){
    float gyro[3]={x,y,0},accel[3]={0,std::cos(pitch),-std::sin(pitch)};
    t+=10000;aim_setup_sample(0,gyro,accel,buttons,t);pitch+=x*.01f;
}
int main(){
    assert(!aim_setup_action(0,0,t));
    sample(0,0);assert(aim_setup_action(0,0,t));
    for(int i=0;i<60;i++)sample(0,0,0x100000);
    assert(aim_setup_status(0,t).step==2);sample(0,0);
    for(int i=0;i<80;i++)sample(0,.35f);
    for(int i=0;i<40;i++)sample(.35f,0);
    auto tl=aim_setup_status(0,t);std::printf("TL x=%f y=%f\n",tl.x,tl.y);
    sample(0,0,0x100000);assert(aim_setup_status(0,t).step==3);sample(0,0);
    for(int i=0;i<40;i++)sample(-.35f,0);
    for(int i=0;i<160;i++)sample(0,-.35f);
    for(int i=0;i<40;i++)sample(-.35f,0);
    sample(0,0,0x100000);
    auto done=aim_setup_status(0,t);std::printf("step=%d half=%f,%f\n",done.step,done.half_x,done.half_y);
    assert(done.step==4&&done.fresh);
    assert(aim_setup_save(0,"build/aim-screen-test.ini",t));
    auto saved=move_studio::read_screen_profile("build/aim-screen-test.ini");assert(saved.valid);
    assert(std::fabs(saved.half_x-done.half_x)<.00001f);
    assert(aim_setup_status(0,t+600000).step==5);
    assert(!aim_setup_save(0,"build/aim-screen-test.ini",t+600000));
    aim_setup_disconnect(0);assert(!aim_setup_status(0,t).fresh);
    std::remove("build/aim-screen-test.ini");
    std::puts("Aim target, profile and stale-input tests passed");
}
