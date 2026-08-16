// quota.cpp
#include "quota.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

static bool parsePct(const char* s, uint8_t& outPct) {
  if (!s || !*s) return false;
  char* end = nullptr;
  double v = strtod(s, &end);
  if (end == s || v < 0) return false;
  double pct = round(v * 100.0);
  outPct = (uint8_t)(pct > 100 ? 100 : pct);
  return true;
}
static uint32_t parseEpoch(const char* s) {
  if (!s) return 0;
  return (uint32_t)strtoul(s, nullptr, 10);
}
bool parseRateHeaders(const char* u5, const char* r5, const char* u7,
                      const char* r7, const char* st, QuotaSnapshot& out) {
  out = QuotaSnapshot{};
  uint8_t p5, p7;
  if (!parsePct(u5, p5) || !parsePct(u7, p7)) return false;
  out.pct5h = p5; out.pctWeek = p7;
  out.reset5h = parseEpoch(r5); out.resetWeek = parseEpoch(r7);
  double raw5 = strtod(u5, nullptr), raw7 = strtod(u7, nullptr);
  out.exhausted = raw5 >= 1.0 || raw7 >= 1.0 ||
                  (st && strcmp(st, "rejected") == 0);
  out.valid = true;
  return true;
}
Mood moodFor(const QuotaSnapshot& q, bool online) {
  if (!online || !q.valid) return Mood::OFFLINE;
  if (q.exhausted) return Mood::SLEEPING;
  uint8_t worst = q.pct5h > q.pctWeek ? q.pct5h : q.pctWeek;
  if (worst >= 90) return Mood::PANIC;
  if (worst >= 75) return Mood::STRESSED;
  if (worst >= 50) return Mood::UNEASY;
  return Mood::HAPPY;
}
void formatCountdown(uint32_t now, uint32_t reset, char* buf, size_t n) {
  uint32_t s = reset > now ? reset - now : 0;
  uint32_t d = s / 86400, h = (s % 86400) / 3600, m = (s % 3600) / 60;
  if (d > 0)      snprintf(buf, n, "%ud %uh", d, h);
  else if (h > 0) snprintf(buf, n, "%uh %um", h, m);
  else            snprintf(buf, n, "%um", m);
}
