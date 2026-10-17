#include "claude_client.h"
#include "oauth_codec.h"
#include <WiFiClientSecureBearSSL.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include "certs.h"
#include "netlog.h"

// El handshake TLS consume heap proporcional al buffer RX. Con 16384 el
// consumo (~22KB, medido en el spike) NO cabe de forma fiable en este chip
// (pantalla+sprites dejan ~19-20KB contiguos) y el poll crasheaba a veces al
// arrancar. Con 8192 el consumo baja a ~11-12KB y cabe con margen. Requiere
// que los registros TLS del servidor quepan en 8KB (la respuesta es diminuta y
// las cadenas de certificados de api.anthropic.com / platform.claude.com
// caben). Sin MFLN (el servidor lo rechaza), 8192 es el minimo seguro. Lo
// comparten poll, refresh y canje OAuth.
static const int RX_BUF = 8192, TX_BUF = 512;

static void tlsClient(BearSSL::WiFiClientSecure& c, BearSSL::X509List& roots) {
  c.setTrustAnchors(&roots);
  c.setBufferSizes(RX_BUF, TX_BUF);
}

// Si el heap fragmentado no tiene un bloque contiguo para el handshake,
// intentarlo revienta el firmware (crash-loop). Mejor abortar limpiamente y
// reintentar en el siguiente ciclo. Con RX 8192 el handshake necesita
// ~11-12KB; 13000 deja margen.
bool tlsHeapOk(const char* who) {
  uint32_t blk = ESP.getMaxFreeBlockSize();
  if (blk >= 13000) return true;
  LOGN("%s: heap bajo (%u), salto TLS", who, blk);
  return false;
}

PollStatus pollQuota(const char* accessToken, const char* model, QuotaSnapshot& out, int& httpCode) {
  httpCode = 0;
  if (!tlsHeapOk("poll")) return PollStatus::NET_ERROR;
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
  // Peticion minima: solo interesan las cabeceras de cuota. El modelo sale de
  // la config (serializado con ArduinoJson: es texto de usuario).
  JsonDocument req;
  req["model"] = model;
  req["max_tokens"] = 1;
  req["messages"][0]["role"] = "user";
  req["messages"][0]["content"] = "hi";
  String body;
  serializeJson(req, body);
  int code = http.POST(body);
  httpCode = code;
  PollStatus st = classifyPollCode(code);
  if (st == PollStatus::OK) {   // 429 = cuota agotada: las cabeceras vienen igual
    bool ok = parseRateHeaders(http.header(keep[0]).c_str(), http.header(keep[1]).c_str(),
                               http.header(keep[2]).c_str(), http.header(keep[3]).c_str(),
                               http.header(keep[4]).c_str(), out);
    if (!ok) st = PollStatus::API_ERROR;  // 200 sin cabeceras: la API cambio, no es la red
  }
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
  if (!tlsHeapOk("refresh")) return false;
  // Mismo formato que el canje inicial (oauthExchange): JSON con client_id. El
  // refresh_token viaja escapado por ArduinoJson, no concatenado a mano.
  static char body[700];  // static: refresh[512] + campos; fuera de la pila
  if (!buildRefreshBody(t.refresh, OAUTH_CLIENT_ID, body, sizeof body)) return false;
  BearSSL::WiFiClientSecure client;
  BearSSL::X509List roots(CA_ROOTS);
  tlsClient(client, roots);
  HTTPClient http;
  http.begin(client, OAUTH_TOKEN_URL);
  http.addHeader("Content-Type", "application/json");
  int code = http.POST((const uint8_t*)body, strlen(body));
  // getString() antes de parsear, nunca deserializeJson(stream): con el buffer
  // TLS de 8KB el stream llega troceado y el parseo falla (ver AGENTS.md).
  String resp = http.getString();
  http.end();
  if (code != 200 || !parseTokenResponse(resp.c_str(), (uint32_t)time(nullptr), true, t)) {
    JsonDocument d;
    deserializeJson(d, resp);
    LOGN("refresh fallo: code=%d err=%s", code, (const char*)(d["error"] | ""));
    return false;
  }
  LOGN("token renovado");
  return true;
}
