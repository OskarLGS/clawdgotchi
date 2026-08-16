#pragma once
#include <Arduino.h>
#include <WiFiClientSecureBearSSL.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <bearssl/bearssl_hash.h>
#include "store.h"
#include "certs.h"
#include "netlog.h"
#include "claude_client.h"  // tlsHeapOk()

// Login OAuth PKCE dentro del cubo (Task 16): el usuario abre /oauth/start en
// el movil, hace login en claude.ai y pega el codigo que le devuelven en
// /oauth/finish. Sin ordenador ni CLI.
static const char* OAUTH_CLIENT_ID = "9d1c250a-e61b-44d9-88ed-5944d1962f5e";
static const char* OAUTH_REDIRECT_URI = "https://platform.claude.com/oauth/code/callback";
static const char* OAUTH_AUTHORIZE_BASE = "https://claude.ai/oauth/authorize";
static const char* OAUTH_SCOPE = "org:create_api_key user:profile user:inference";

// code_verifier de la sesion de login en curso. String (no buffer fijo en
// pila): oauthBuildUrl()/oauthExchange() los llaman handlers HTTP con pila
// "cont" de 4KB, y ~43 bytes en el heap no la aprietan.
static String g_verifier;

// base64url sin padding: tabla estandar con +/ sustituidos por -_ y sin '='
// final (elegido en vez de post-procesar base64::encode porque el original
// ya usa +/ y hay que tocar cada caracter igualmente).
inline String base64UrlEncode(const uint8_t* data, size_t len) {
  static const char T[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
  String out;
  out.reserve((len + 2) / 3 * 4);
  size_t i = 0;
  for (; i + 3 <= len; i += 3) {
    uint32_t v = ((uint32_t)data[i] << 16) | ((uint32_t)data[i+1] << 8) | data[i+2];
    out += T[(v >> 18) & 63]; out += T[(v >> 12) & 63]; out += T[(v >> 6) & 63]; out += T[v & 63];
  }
  size_t rem = len - i;
  if (rem == 1) {
    uint32_t v = (uint32_t)data[i] << 16;
    out += T[(v >> 18) & 63]; out += T[(v >> 12) & 63];
  } else if (rem == 2) {
    uint32_t v = ((uint32_t)data[i] << 16) | ((uint32_t)data[i+1] << 8);
    out += T[(v >> 18) & 63]; out += T[(v >> 12) & 63]; out += T[(v >> 6) & 63];
  }
  return out;
}

inline String urlEncode(const String& in) {
  String out;
  out.reserve(in.length() * 3);
  const char* hex = "0123456789ABCDEF";
  for (size_t i = 0; i < in.length(); i++) {
    uint8_t c = (uint8_t)in[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out += (char)c;
    else { out += '%'; out += hex[c >> 4]; out += hex[c & 15]; }
  }
  return out;
}

// Genera un code_verifier nuevo (32 bytes de ESP.random(), el RNG hardware
// real con WiFi activo -no random()/Math-) y devuelve la URL de autorizacion
// completa con el code_challenge PKCE (S256) derivado de el.
inline String oauthBuildUrl() {
  uint8_t raw[32];
  for (int i = 0; i < 32; i += 4) {
    uint32_t r = ESP.random();
    raw[i] = r; raw[i+1] = r >> 8; raw[i+2] = r >> 16; raw[i+3] = r >> 24;
  }
  g_verifier = base64UrlEncode(raw, sizeof raw);

  br_sha256_context ctx;
  uint8_t digest[32];
  br_sha256_init(&ctx);
  br_sha256_update(&ctx, g_verifier.c_str(), g_verifier.length());
  br_sha256_out(&ctx, digest);
  String challenge = base64UrlEncode(digest, sizeof digest);

  String scope = OAUTH_SCOPE;
  scope.replace(" ", "+");

  String url = String(OAUTH_AUTHORIZE_BASE) + "?code=true&client_id=" + OAUTH_CLIENT_ID +
               "&response_type=code&redirect_uri=" + urlEncode(OAUTH_REDIRECT_URI) +
               "&scope=" + scope +
               "&code_challenge=" + challenge + "&code_challenge_method=S256" +
               "&state=" + g_verifier;
  return url;
}

// El usuario pega "code#state" (formato que devuelve claude.ai) o solo el
// code si no hay '#'. Canjea el code por tokens via TLS (patron identico a
// refreshTokens() en claude_client.cpp: BearSSL + CA_ROOTS + buffers
// 16384/512) y rellena out. NO llamar desde un handler HTTP -solo desde
// loop()-, ver oauthPending en main.cpp.
inline bool oauthExchange(const String& codeInput, Tokens& out) {
  String code = codeInput, state = g_verifier;
  int hash = codeInput.indexOf('#');
  if (hash >= 0) { code = codeInput.substring(0, hash); state = codeInput.substring(hash + 1); }
  code.trim(); state.trim();
  if (!code.length()) { LOGN("oauth: code vacio"); return false; }
  if (!tlsHeapOk("oauth")) return false;

  JsonDocument req;
  req["grant_type"] = "authorization_code";
  req["code"] = code;
  req["state"] = state;
  req["client_id"] = OAUTH_CLIENT_ID;
  req["redirect_uri"] = OAUTH_REDIRECT_URI;
  req["code_verifier"] = g_verifier;
  String body;
  serializeJson(req, body);

  BearSSL::WiFiClientSecure client;
  BearSSL::X509List roots(CA_ROOTS);
  client.setTrustAnchors(&roots);
  // platform.claude.com no soporta MFLN (probado), asi que no se puede pedir
  // fragmentos pequenos. Pero el consumo de heap del handshake escala con el
  // buffer RX: con 16384 el spike consumio ~22KB y en este chip (pantalla+
  // sprites) solo hay ~19KB contiguos -> fallaba (ssl -1000). Con RX 8192 el
  // handshake consume ~14KB y cabe. Requiere que los registros TLS del
  // servidor quepan en 8KB (lo habitual). El poll (api.anthropic.com) sigue en
  // 16384 porque ese si funciona y ahi la cadena es distinta.
  client.setBufferSizes(8192, 512);
  LOGN("oauth blk=%u", ESP.getMaxFreeBlockSize());
  HTTPClient http;
  http.begin(client, "https://platform.claude.com/v1/oauth/token");
  http.addHeader("Content-Type", "application/json");
  int code_ = http.POST(body);
  if (code_ != 200) {
    LOGN("oauth exchange code=%d ssl=%d", code_, client.getLastSSLError());
    http.end(); return false;
  }

  JsonDocument d;
  DeserializationError e = deserializeJson(d, http.getStream());
  http.end();
  if (e || !d["access_token"].is<const char*>()) { LOGN("oauth: json invalido"); return false; }

  snprintf(out.access, sizeof out.access, "%s", (const char*)d["access_token"]);
  out.refresh[0] = 0;
  if (d["refresh_token"].is<const char*>())
    snprintf(out.refresh, sizeof out.refresh, "%s", (const char*)d["refresh_token"]);
  // Guard NTP identico a refreshTokens(): sin hora real expiresAt seria
  // basura; se deja en 0 y el refresh reactivo (runPoll en main.cpp) se
  // encarga en cuanto haga falta.
  out.expiresAt = (time(nullptr) < 1600000000) ? 0 : (uint32_t)time(nullptr) + (d["expires_in"] | 28800);
  LOGN("oauth exchange ok");
  return true;
}
