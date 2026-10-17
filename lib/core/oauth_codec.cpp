#include "oauth_codec.h"
#include <ArduinoJson.h>
#include <cctype>
#include <cstdio>
#include <cstring>

size_t base64UrlEncode(const uint8_t* data, size_t len, char* out, size_t n) {
  static const char T[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
  size_t need = len / 3 * 4 + (len % 3 ? len % 3 + 1 : 0);
  if (!out || n < need + 1) return 0;
  size_t o = 0, i = 0;
  for (; i + 3 <= len; i += 3) {
    uint32_t v = ((uint32_t)data[i] << 16) | ((uint32_t)data[i+1] << 8) | data[i+2];
    out[o++] = T[(v >> 18) & 63]; out[o++] = T[(v >> 12) & 63];
    out[o++] = T[(v >> 6) & 63];  out[o++] = T[v & 63];
  }
  size_t rem = len - i;
  if (rem) {
    uint32_t v = (uint32_t)data[i] << 16;
    if (rem == 2) v |= (uint32_t)data[i+1] << 8;
    out[o++] = T[(v >> 18) & 63]; out[o++] = T[(v >> 12) & 63];
    if (rem == 2) out[o++] = T[(v >> 6) & 63];
  }
  out[o] = 0;
  return o;
}

bool urlEncode(const char* in, char* out, size_t n) {
  static const char hex[] = "0123456789ABCDEF";
  size_t o = 0;
  for (; *in; in++) {
    uint8_t c = (uint8_t)*in;
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      if (o + 1 >= n) return false;
      out[o++] = (char)c;
    } else {
      if (o + 3 >= n) return false;
      out[o++] = '%'; out[o++] = hex[c >> 4]; out[o++] = hex[c & 15];
    }
  }
  if (o >= n) return false;
  out[o] = 0;
  return true;
}

// Copia [b, e) recortando espacios/saltos en ambos extremos.
static bool copyTrim(const char* b, const char* e, char* out, size_t n) {
  while (b < e && isspace((uint8_t)*b)) b++;
  while (e > b && isspace((uint8_t)e[-1])) e--;
  size_t len = (size_t)(e - b);
  if (len + 1 > n) return false;
  memcpy(out, b, len);
  out[len] = 0;
  return true;
}

bool oauthSplitCode(const char* input, char* code, size_t cn, char* state, size_t sn) {
  if (!input) return false;
  const char* end = input + strlen(input);
  const char* hash = strchr(input, '#');
  if (!copyTrim(input, hash ? hash : end, code, cn)) return false;
  if (hash) { if (!copyTrim(hash + 1, end, state, sn)) return false; }
  else if (sn) state[0] = 0;
  return code[0] != 0;
}

bool buildRefreshBody(const char* refresh, const char* clientId, char* out, size_t n) {
  JsonDocument d;
  d["grant_type"] = "refresh_token";
  d["refresh_token"] = refresh;
  d["client_id"] = clientId;
  // measureJson: serializeJson trunca en silencio si el buffer no basta
  return measureJson(d) < n && serializeJson(d, out, n) > 0;
}

bool parseTokenResponse(const char* json, uint32_t nowEpoch, bool keepRefresh, Tokens& t) {
  JsonDocument d;
  if (deserializeJson(d, json)) return false;
  const char* at = d["access_token"];
  if (!at || !*at || strlen(at) >= sizeof t.access) return false;
  const char* rt = d["refresh_token"];
  if (rt && strlen(rt) >= sizeof t.refresh) return false;
  snprintf(t.access, sizeof t.access, "%s", at);
  if (rt) snprintf(t.refresh, sizeof t.refresh, "%s", rt);
  else if (!keepRefresh) t.refresh[0] = 0;
  uint32_t ttl = d["expires_in"] | 28800u;
  t.expiresAt = nowEpoch < 1600000000u ? 0 : nowEpoch + ttl;
  return true;
}

bool validWebPass(const char* p) {
  if (!p) return false;
  size_t len = strlen(p);
  if (len < 1 || len > 32) return false;
  for (; *p; p++) if ((uint8_t)*p < 0x20 || (uint8_t)*p > 0x7e) return false;
  return true;
}
