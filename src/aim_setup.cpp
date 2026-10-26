/* SPDX-License-Identifier: GPL-3.0-or-later
 * Guided targeting adapted from Porpoise ui_app_controls.cpp (Project Porpoise).
 * Uses its original fusion, grip leveling and pointer geometry unchanged.
 */
#include "aim_setup.h"
#include "../third_party/porpoise_aim/porpoise_aim.hpp"
#include <cmath>
#include <cstdio>
#include <unistd.h>
namespace {
namespace aim=porpoise::aim;
constexpr int kSlots=8;
constexpr float kCorner=.8f,kDeg=57.2957795131f;
constexpr uint32_t kMove=0x00100000;
struct Slot {
    aim::Fusion fusion;
    aim::Basis basis=aim::kGripNormal;
    aim::Angles center,tl;
    aim::Scale scale=aim::scale_for(5);
    uint64_t timestamp=0,hold=0;
    uint32_t buttons=0;
    int step=0;
    float offset_yaw=0,offset_pitch=0;
};
Slot slots[kSlots];
bool valid(int i){return i>=0&&i<kSlots;}
bool fresh(const Slot&s,uint64_t now){return s.timestamp&&now>=s.timestamp&&now-s.timestamp<=500000;}
}
extern "C" void aim_setup_disconnect(int i){if(valid(i))slots[i]=Slot{};}
extern "C" int aim_setup_action(int i,int action,uint64_t now){
    if(!valid(i)||action<0||action>1)return 0;
    Slot&s=slots[i];if(action==0&&!fresh(s,now))return 0;
    s.step=action==0?1:0;s.hold=0;return 1;
}
extern "C" void aim_setup_sample(int i,const float*g,const float*a,uint32_t buttons,uint64_t now){
    if(!valid(i)||!g||!a)return;
    for(int j=0;j<3;j++)if(!std::isfinite(g[j])||!std::isfinite(a[j]))return;
    Slot&s=slots[i];
    if(s.timestamp&&now<=s.timestamp)return;
    if(s.timestamp&&now-s.timestamp>500000){s.fusion=aim::Fusion{};if(s.step)s.step=5;s.hold=0;}
    float dt=s.timestamp?float(now-s.timestamp)*.000001f:.01f;
    aim::fuse(s.fusion,g,a,dt);s.timestamp=now;
    const bool pressed=(buttons&kMove)&&!(s.buttons&kMove);
    const bool recenter=(buttons&1u)&&!(s.buttons&1u);s.buttons=buttons;
    const float turning=std::sqrt(g[0]*g[0]+g[1]*g[1]+g[2]*g[2]);
    if(s.step==4&&recenter){s.center=aim::remote_angles(s.fusion,s.basis);}
    if(s.step==1){
        if((buttons&kMove)&&turning<.6f){
            if(!s.hold)s.hold=now;
            if(now-s.hold>=500000){aim::level_grip(aim::kGripNormal,a,s.basis);s.center=aim::remote_angles(s.fusion,s.basis);s.step=2;s.hold=0;}
        }else s.hold=0;
    }else if(s.step==2&&pressed&&turning<1.f){s.tl=aim::remote_angles(s.fusion,s.basis);s.step=3;}
    else if(s.step==3&&pressed&&turning<1.f){
        auto br=aim::remote_angles(s.fusion,s.basis);
        float span=std::remainder(br.yaw-s.tl.yaw,6.28318530718f);
        float hx=span/(2*kCorner),hy=(s.tl.pitch-br.pitch)/(2*kCorner);
        if(hx*kDeg<=3||hx*kDeg>=75||hy*kDeg<=2||hy*kDeg>=60||hy/hx<=.25f||hy/hx>=1.3f){s.step=5;return;}
        float mid=s.tl.yaw+span*.5f;
        s.offset_yaw=std::remainder(mid-s.center.yaw,6.28318530718f);
        s.offset_pitch=(s.tl.pitch+br.pitch)*.5f-s.center.pitch;
        if(std::fabs(s.offset_yaw)>=1.31f||std::fabs(s.offset_pitch)>=1.05f){s.step=5;return;}
        s.center.yaw+=s.offset_yaw;s.center.pitch+=s.offset_pitch;
        s.scale={hx,hy};s.step=4;
    }
}
extern "C" aim_setup_state aim_setup_status(int i,uint64_t now){
    aim_setup_state out{};if(!valid(i))return out;
    Slot&s=slots[i];out.fresh=fresh(s,now);out.step=s.step;
    if(!out.fresh){if(s.step)s.step=out.step=5;return out;}
    if(s.step>=2&&s.step<=4){auto current=aim::remote_angles(s.fusion,s.basis);aim::pointer(current,s.center,s.scale,out.x,out.y);}
    out.half_x=s.scale.half_x;out.half_y=s.scale.half_y;return out;
}
extern "C" int aim_setup_save(int i,const char*path,uint64_t now){
    if(!valid(i)||!path||!fresh(slots[i],now)||slots[i].step!=4)return 0;
    const Slot&s=slots[i];char tmp[256];if(std::snprintf(tmp,sizeof tmp,"%s.tmp",path)>=(int)sizeof tmp)return 0;
    FILE*f=std::fopen(tmp,"w");if(!f)return 0;
    bool ok=std::fprintf(f,"MVA1 %.9g %.9g %.9g %.9g\n",s.scale.half_x,s.scale.half_y,s.offset_yaw,s.offset_pitch)>0;
    if(std::fflush(f)||fsync(fileno(f)))ok=false;
    if(std::fclose(f))ok=false;
    if(ok&&std::rename(tmp,path))ok=false;
    if(!ok)std::remove(tmp);
    return ok;
}
