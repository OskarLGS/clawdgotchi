#pragma once
#include <Arduino.h>
// 3 crashes seguidos -> modo seguro (solo web UI). Un arranque sano resetea el contador.
struct RtcGuard { uint32_t magic; uint32_t crashes; };
inline bool safeModeRequested() {
  RtcGuard g;
  ESP.rtcUserMemoryRead(0, (uint32_t*)&g, sizeof g);
  rst_info* ri = ESP.getResetInfoPtr();
  bool crash = ri && (ri->reason == REASON_EXCEPTION_RST || ri->reason == REASON_WDT_RST ||
                      ri->reason == REASON_SOFT_WDT_RST);
  if (g.magic != 0xC1A3D077) { g.magic = 0xC1A3D077; g.crashes = 0; }
  g.crashes = crash ? g.crashes + 1 : 0;
  ESP.rtcUserMemoryWrite(0, (uint32_t*)&g, sizeof g);
  return g.crashes >= 3;
}
inline void markBootHealthy() {   // llamar tras 60 s de uptime estable
  RtcGuard g{0xC1A3D077, 0};
  ESP.rtcUserMemoryWrite(0, (uint32_t*)&g, sizeof g);
}
