#include "claude_client.h"
#include <WiFiClientSecureBearSSL.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include "certs.h"
#include "netlog.h"

// Spike (Task 5) midio MFLN 1024 RECHAZADO por el servidor (mfln1024=0),
// asi que usamos los buffers grandes que si funcionaron: {16384, 512}.
static const int RX_BUF = 16384, TX_BUF = 512;

static void tlsClient(BearSSL::WiFiClientSecure& c, BearSSL::X509List& roots) {
  c.setTrustAnchors(&roots);
  c.setBufferSizes(RX_BUF, TX_BUF);
}

PollStatus pollQuota(const char* accessToken, QuotaSnapshot& out) {
  BearSSL::WiFiClientSecure client;
  BearSSL::X509List roots(CA_ROOTS);
  tlsClient(client, roots);
  HTTPClient http;
  http.begin(client, "https://api.anthropic.com/v1/messages");
  http.addHeader("anthropic-version", "2023-06-01");
  http.addHeader("anthropic-beta", "oauth-2025-04-20");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", String("Bearer ") + accessToken);
  const char* keep[] = {"anthropic-ratelimit-unified-5h-utilization",
                        "anthropic-ratelimit-unified-5h-reset",
                        "anthropic-ratelimit-unified-7d-utilization",
                        "anthropic-ratelimit-unified-7d-reset",
                        "anthropic-ratelimit-unified-5h-status"};
  http.collectHeaders(keep, 5);
  int code = http.POST("{\"model\":\"claude-haiku-4-5-20251001\",\"max_tokens\":1,"
                       "\"messages\":[{\"role\":\"user\",\"content\":\"hi\"}]}");
  PollStatus st;
  if (code == 200 || code == 429) {   // 429 = cuota agotada: las cabeceras vienen igual
    bool ok = parseRateHeaders(http.header(keep[0]).c_str(), http.header(keep[1]).c_str(),
                               http.header(keep[2]).c_str(), http.header(keep[3]).c_str(),
                               http.header(keep[4]).c_str(), out);
    st = ok ? PollStatus::OK : PollStatus::NET_ERROR;
  } else if (code == 401 || code == 403) st = PollStatus::AUTH_FAILED;
  else st = PollStatus::NET_ERROR;
  LOGN("poll code=%d 5h=%u%% 7d=%u%%", code, out.pct5h, out.pctWeek);
  http.end();
  return st;
}

bool refreshTokens(Tokens& t) {
  if (!t.refresh[0]) return false;
  // Guard NTP: sin hora real, expiresAt = time(nullptr)+ttl saldria basura
  // (epoca ~1970) y el chequeo de "quedan <5 min" de loop() dispararia sin
  // parar. No reintenta: loop() ya evita llamar aqui hasta que NTP sincronice.
  if (time(nullptr) < 1600000000) { LOGN("refresh omitido: sin NTP"); return false; }
  BearSSL::WiFiClientSecure client;
  BearSSL::X509List roots(CA_ROOTS);
  tlsClient(client, roots);
  HTTPClient http;
  http.begin(client, "https://platform.claude.com/v1/oauth/token");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  int code = http.POST(String("grant_type=refresh_token&refresh_token=") + t.refresh);
  if (code != 200) { LOGN("refresh code=%d", code); http.end(); return false; }
  JsonDocument d;
  DeserializationError e = deserializeJson(d, http.getStream());
  http.end();
  if (e || !d["access_token"].is<const char*>()) return false;
  snprintf(t.access, sizeof t.access, "%s", (const char*)d["access_token"]);
  if (d["refresh_token"].is<const char*>())
    snprintf(t.refresh, sizeof t.refresh, "%s", (const char*)d["refresh_token"]);
  t.expiresAt = (uint32_t)time(nullptr) + (d["expires_in"] | 28800);
  LOGN("token renovado");
  return true;
}
