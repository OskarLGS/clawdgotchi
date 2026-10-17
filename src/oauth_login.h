#pragma once
#include <Arduino.h>
#include <WiFiClientSecureBearSSL.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <bearssl/bearssl_hash.h>
#include "store.h"
#include "certs.h"
#include "netlog.h"
#include "oauth_codec.h"
#include "claude_client.h"  // tlsHeapOk(), OAUTH_CLIENT_ID, OAUTH_TOKEN_URL

// Login OAuth PKCE dentro del cubo (Task 16): el usuario abre /oauth/start en
// el movil, hace login en claude.ai y pega el codigo que le devuelven en
// /oauth/finish. Sin ordenador ni CLI.
static const char* OAUTH_REDIRECT_URI = "https://platform.claude.com/oauth/code/callback";
static const char* OAUTH_AUTHORIZE_BASE = "https://claude.ai/oauth/authorize";
// Minimo privilegio: el cubo solo hace inferencia (el poll) y nada mas. Sin
// org:create_api_key, que permitiria crear API keys con este token.
static const char* OAUTH_SCOPE = "user:profile user:inference";

// code_verifier y state de la sesion de login en curso (43 chars base64url de
// 32 bytes aleatorios cada uno; buffers globales, no en la pila "cont" de los
// handlers). Son valores DISTINTOS: el state viaja por la URL y vuelve en el
// "code#state" que se pega; el verifier es secreto y solo sale en el canje.
static char g_verifier[48];
static char g_state[48];

// 32 bytes de ESP.random() (RNG hardware real con WiFi activo) en base64url.
inline void oauthRandom43(char* out, size_t n) {
  uint8_t raw[32];
  for (int i = 0; i < 32; i += 4) {
    uint32_t r = ESP.random();
    raw[i] = r; raw[i+1] = r >> 8; raw[i+2] = r >> 16; raw[i+3] = r >> 24;
  }
  base64UrlEncode(raw, sizeof raw, out, n);
}

// Genera verifier + state nuevos y devuelve la URL de autorizacion completa
// con el code_challenge PKCE (S256) derivado del verifier.
inline String oauthBuildUrl() {
  oauthRandom43(g_verifier, sizeof g_verifier);
  oauthRandom43(g_state, sizeof g_state);

  br_sha256_context ctx;
  uint8_t digest[32];
  br_sha256_init(&ctx);
  br_sha256_update(&ctx, g_verifier, strlen(g_verifier));
  br_sha256_out(&ctx, digest);
  char challenge[48];
  base64UrlEncode(digest, sizeof digest, challenge, sizeof challenge);

  String scope = OAUTH_SCOPE;
  scope.replace(" ", "+");
  char redirect[96];
  urlEncode(OAUTH_REDIRECT_URI, redirect, sizeof redirect);

  return String(OAUTH_AUTHORIZE_BASE) + "?code=true&client_id=" + OAUTH_CLIENT_ID +
         "&response_type=code&redirect_uri=" + redirect + "&scope=" + scope +
         "&code_challenge=" + challenge + "&code_challenge_method=S256" +
         "&state=" + g_state;
}

// El usuario pega "code#state" (formato que devuelve claude.ai) o solo el
// code si no hay '#'. Si trae state debe coincidir con el de esta sesion
// (rechaza codigos de otra sesion de login). Canjea el code por tokens via
// TLS y rellena out solo si el canje sale bien. NO llamar desde un handler
// HTTP -solo desde loop()-, ver oauthPending en main.cpp.
inline bool oauthExchange(const String& codeInput, Tokens& out) {
  if (!g_verifier[0]) { LOGN("oauth: sin sesion de login (reinicio?), vuelve a abrir el login"); return false; }
  char code[256], state[64];
  if (!oauthSplitCode(codeInput.c_str(), code, sizeof code, state, sizeof state)) {
    LOGN("oauth: code vacio o demasiado largo"); return false;
  }
  if (state[0] && strcmp(state, g_state) != 0) { LOGN("oauth: state no coincide"); return false; }
  if (!tlsHeapOk("oauth")) return false;

  JsonDocument req;
  req["grant_type"] = "authorization_code";
  req["code"] = code;
  req["state"] = g_state;
  req["client_id"] = OAUTH_CLIENT_ID;
  req["redirect_uri"] = OAUTH_REDIRECT_URI;
  req["code_verifier"] = g_verifier;
  String body;
  serializeJson(req, body);

  BearSSL::WiFiClientSecure client;
  BearSSL::X509List roots(CA_ROOTS);
  client.setTrustAnchors(&roots);
  // Mismo buffer RX 8192 que poll/refresh (ver claude_client.cpp): con 16384
  // el handshake no cabe en el heap de este chip.
  client.setBufferSizes(8192, 512);
  HTTPClient http;
  http.begin(client, OAUTH_TOKEN_URL);
  http.addHeader("Content-Type", "application/json");
  int code_ = http.POST(body);
  // Lee el cuerpo completo a String (mas robusto que parsear el stream con el
  // buffer TLS pequeno). Se lee pase lo que pase el codigo HTTP: en un 400 el
  // cuerpo trae {"error":...} con el motivo exacto (no sensible).
  String resp = http.getString();
  http.end();
  if (code_ != 200 || !parseTokenResponse(resp.c_str(), (uint32_t)time(nullptr), false, out)) {
    JsonDocument d;
    deserializeJson(d, resp);
    LOGN("oauth fallo canje: code=%d err=%s", code_, (const char*)(d["error"] | ""));
    return false;
  }
  // Un solo uso: el verifier no se reutiliza para otro canje.
  g_verifier[0] = 0; g_state[0] = 0;
  LOGN("oauth exchange ok");
  return true;
}
