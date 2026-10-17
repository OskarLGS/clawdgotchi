// quota.h
#pragma once
#include <stdint.h>
#include <stddef.h>
struct QuotaSnapshot {
  uint8_t pct5h = 0, pctWeek = 0;
  uint32_t reset5h = 0, resetWeek = 0;
  bool exhausted = false, valid = false;
};
enum class Mood { HAPPY, UNEASY, STRESSED, PANIC, SLEEPING, OFFLINE };
// API_ERROR: la API rechazo la peticion (400/404...: modelo retirado, cambio de
// formato). No es transitorio, asi que no entra en el backoff de red.
enum class PollStatus { OK, AUTH_FAILED, NET_ERROR, API_ERROR };
bool parseRateHeaders(const char* util5h, const char* reset5h,
                      const char* util7d, const char* reset7d,
                      const char* status5h, QuotaSnapshot& out);
Mood moodFor(const QuotaSnapshot& q, bool online);
// Clasifica el codigo HTTP del poll (negativo = error de HTTPClient/red).
// 200 y 429 traen cabeceras de cuota -> OK (pendiente de parsearlas).
PollStatus classifyPollCode(int code);
void formatCountdown(uint32_t nowEpoch, uint32_t resetEpoch, char* buf, size_t n);
