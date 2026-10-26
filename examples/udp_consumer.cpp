/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../integrations/move_management_client.hpp"
#include <cstdio>
#include <thread>
static std::uint64_t now_us(){return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
int main(){move_management::Client input;if(!input.open(9976))return 1;const auto end=now_us()+5000000;std::uint64_t last=~std::uint64_t(0);while(now_us()<end){const auto t=now_us();input.poll(t);if(input.fresh(t)&&input.packet().sequence!=last){const auto&p=input.packet();last=p.sequence;std::printf("seq=%llu buttons=%08x T=%u optical=%d\n",(unsigned long long)p.sequence,move_management::detail::u32(p.pad),p.pad[8],input.optical_fresh(t));}std::this_thread::sleep_for(std::chrono::milliseconds(8));}input.close();}
