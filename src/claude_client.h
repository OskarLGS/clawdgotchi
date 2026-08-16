#pragma once
#include "quota.h"
#include "config_codec.h"

enum class PollStatus { OK, AUTH_FAILED, NET_ERROR };
PollStatus pollQuota(const char* accessToken, QuotaSnapshot& out);
bool refreshTokens(Tokens& t);  // true si renovo y hay que persistir
