#pragma once
#include <cstdint>
#include <cstddef>
#include "config_codec.h"

// Utilidades puras (native-testables) del flujo OAuth y de la web UI.

// base64url sin padding (RFC 4648 §5). Devuelve la longitud escrita (sin el
// NUL final) o 0 si `n` no basta.
size_t base64UrlEncode(const uint8_t* data, size_t len, char* out, size_t n);

// Percent-encoding de todo salvo los no reservados de RFC 3986. false si no cabe.
bool urlEncode(const char* in, char* out, size_t n);

// Separa lo que pega el usuario ("code#state" o solo "code"), recortando
// espacios. state queda "" si no venia. false si el code queda vacio o no cabe.
bool oauthSplitCode(const char* input, char* code, size_t cn, char* state, size_t sn);

// Cuerpo JSON del refresh: {"grant_type":"refresh_token","refresh_token":..,"client_id":..}.
bool buildRefreshBody(const char* refresh, const char* clientId, char* out, size_t n);

// Parsea la respuesta del endpoint de tokens y rellena t. Si la respuesta no
// trae refresh_token: keepRefresh=true conserva el de t (refresh), false lo
// borra (canje inicial). nowEpoch < 1600000000 (sin NTP) deja expiresAt=0.
// false si no hay access_token o no cabe; en ese caso t no se modifica.
bool parseTokenResponse(const char* json, uint32_t nowEpoch, bool keepRefresh, Tokens& t);

// Contraseña web aceptable: 1..32 caracteres imprimibles ASCII (cabe en
// Config::webPass sin truncarse en silencio).
bool validWebPass(const char* p);
