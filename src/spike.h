#pragma once
#include <ESP8266WebServer.h>
#include <WiFiClientSecureBearSSL.h>
#include <ESP8266HTTPClient.h>
#include <time.h>
#include "certs.h"
#include "netlog.h"

// GET /spike?token=<token de `claude setup-token`>
// Prueba: NTP -> MFLN probe -> TLS -> POST minimo -> cabeceras de cuota.
inline void spikeRoutes(ESP8266WebServer& server) {
  server.on("/spike", [&server] {
    String token = server.arg("token");
    if (token.isEmpty()) { server.send(400, "text/plain", "falta ?token="); return; }
    configTime(0, 0, "pool.ntp.org");
    time_t now = 0;
    for (int i = 0; i < 40 && (now = time(nullptr)) < 1600000000; i++) delay(250);
    LOGN("ntp: %ld", (long)now);

    bool mfln = BearSSL::WiFiClientSecure::probeMaxFragmentLength("api.anthropic.com", 443, 1024);
    LOGN("mfln1024=%d maxblock=%u", mfln, ESP.getMaxFreeBlockSize());

    BearSSL::WiFiClientSecure client;
    BearSSL::X509List roots(CA_ROOTS);
    client.setTrustAnchors(&roots);
    if (mfln) client.setBufferSizes(1024, 512); else client.setBufferSizes(16384, 512);

    HTTPClient http;
    http.begin(client, "https://api.anthropic.com/v1/messages");
    http.addHeader("anthropic-version", "2023-06-01");
    http.addHeader("anthropic-beta", "oauth-2025-04-20");
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Bearer " + token);
    const char* keep[] = {"anthropic-ratelimit-unified-5h-utilization",
                          "anthropic-ratelimit-unified-5h-reset",
                          "anthropic-ratelimit-unified-7d-utilization",
                          "anthropic-ratelimit-unified-7d-reset",
                          "anthropic-ratelimit-unified-5h-status"};
    http.collectHeaders(keep, 5);
    uint32_t before = ESP.getMaxFreeBlockSize();
    int code = http.POST("{\"model\":\"claude-haiku-4-5-20251001\",\"max_tokens\":1,"
                         "\"messages\":[{\"role\":\"user\",\"content\":\"hi\"}]}");
    String out = "code=" + String(code) + " maxblock antes=" + String(before) +
                 " despues=" + String(ESP.getMaxFreeBlockSize()) + "\n";
    for (int i = 0; i < http.headers(); i++)
      out += http.headerName(i) + ": " + http.header(i) + "\n";
    http.end();
    LOGN("spike code=%d", code);
    server.send(200, "text/plain", out);
  });
}
