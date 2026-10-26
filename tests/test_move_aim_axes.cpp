#include "../../porpoise/src/porpoise_aim.hpp"
#include <cassert>
#include <cmath>
using namespace porpoise::aim;
int main(){
 const float up[3]={0,1,0};Fusion seed{};float zero[3]={0,0,0};for(int n=0;n<100;n++)fuse(seed,zero,up,.01f);
 Basis basis{};assert(level_grip(kGripUprightRight,up,basis));auto centre=remote_angles(seed,basis);
 // Move handle twist (native Y) becomes pad -Z: it rotates about pointing direction.
 Fusion roll=seed;for(int n=1;n<=30;n++){float t=n*.01f;float a[3]={std::sin(-t),std::cos(t),0};float g[3]={0,0,-1};fuse(roll,g,a,.01f);}auto r=remote_angles(roll,basis);assert(std::fabs(r.yaw-centre.yaw)<.02f);assert(std::fabs(r.pitch-centre.pitch)<.02f);
 // Pitch around native X remains pad X and must move vertical aim.
 Fusion pitch=seed;for(int n=1;n<=30;n++){float t=n*.01f;float a[3]={0,std::cos(t),-std::sin(t)};float g[3]={1,0,0};fuse(pitch,g,a,.01f);}auto p=remote_angles(pitch,basis);assert(std::fabs(p.pitch-centre.pitch)>.2f);
}