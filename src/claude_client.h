#pragma once
#include "quota.h"         // QuotaSnapshot, PollStatus
#include "config_codec.h"

// Client ID publico de Claude Code (OAuth PKCE). Lo comparten el canje
// inicial (oauth_login.h) y el refresh.
static const char* const OAUTH_CLIENT_ID = "9d1c250a-e61b-44d9-88ed-5944d1962f5e";
static const char* const OAUTH_TOKEN_URL = "https://platform.claude.com/v1/oauth/token";

// httpCode: codigo HTTP (o error negativo de HTTPClient) del ultimo intento,
// para /status y el aviso de la web UI.
PollStatus pollQuota(const char* accessToken, const char* model, QuotaSnapshot& out, int& httpCode);
bool refreshTokens(Tokens& t);  // true si renovo y hay que persistir
// true si hay bloque de heap suficiente para un handshake TLS; si no, LOGN y false.
bool tlsHeapOk(const char* who);
