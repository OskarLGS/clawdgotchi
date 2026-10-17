#include <unity.h>
#include <cstring>
#include "oauth_codec.h"

void test_base64url_rfc4648() {
  char b[16];
  const uint8_t* f = (const uint8_t*)"foobar";
  TEST_ASSERT_EQUAL(0, base64UrlEncode(f, 0, b, sizeof b)); TEST_ASSERT_EQUAL_STRING("", b);
  base64UrlEncode(f, 1, b, sizeof b); TEST_ASSERT_EQUAL_STRING("Zg", b);
  base64UrlEncode(f, 2, b, sizeof b); TEST_ASSERT_EQUAL_STRING("Zm8", b);
  base64UrlEncode(f, 3, b, sizeof b); TEST_ASSERT_EQUAL_STRING("Zm9v", b);
  base64UrlEncode(f, 6, b, sizeof b); TEST_ASSERT_EQUAL_STRING("Zm9vYmFy", b);
  const uint8_t u[] = {0xfb, 0xff};  // usa el alfabeto url: '-' y '_' en vez de '+' y '/'
  base64UrlEncode(u, 2, b, sizeof b); TEST_ASSERT_EQUAL_STRING("-_8", b);
  char small[4];
  TEST_ASSERT_EQUAL(0, base64UrlEncode(f, 3, small, sizeof small));  // "Zm9v"+NUL no cabe
}
void test_url_encode() {
  char b[64];
  TEST_ASSERT_TRUE(urlEncode("https://a.b/c d~", b, sizeof b));
  TEST_ASSERT_EQUAL_STRING("https%3A%2F%2Fa.b%2Fc%20d~", b);
  char small[4];
  TEST_ASSERT_FALSE(urlEncode("a b", small, sizeof small));
}
void test_split_code() {
  char c[32], s[32];
  TEST_ASSERT_TRUE(oauthSplitCode("  abc#xyz \n", c, sizeof c, s, sizeof s));
  TEST_ASSERT_EQUAL_STRING("abc", c); TEST_ASSERT_EQUAL_STRING("xyz", s);
  TEST_ASSERT_TRUE(oauthSplitCode("abc", c, sizeof c, s, sizeof s));
  TEST_ASSERT_EQUAL_STRING("", s);
  TEST_ASSERT_FALSE(oauthSplitCode("  #xyz", c, sizeof c, s, sizeof s));
}
void test_refresh_body_escapes() {
  char b[256];
  TEST_ASSERT_TRUE(buildRefreshBody("r\"t&=", "cid", b, sizeof b));
  TEST_ASSERT_EQUAL_STRING("{\"grant_type\":\"refresh_token\",\"refresh_token\":\"r\\\"t&=\",\"client_id\":\"cid\"}", b);
  char small[16];
  TEST_ASSERT_FALSE(buildRefreshBody("rt", "cid", small, sizeof small));
}
void test_token_response() {
  Tokens t; strcpy(t.access, "old"); strcpy(t.refresh, "oldrt");
  // refresh sin refresh_token nuevo: conserva el anterior
  TEST_ASSERT_TRUE(parseTokenResponse("{\"access_token\":\"a1\",\"expires_in\":100}", 1700000000, true, t));
  TEST_ASSERT_EQUAL_STRING("a1", t.access); TEST_ASSERT_EQUAL_STRING("oldrt", t.refresh);
  TEST_ASSERT_EQUAL_UINT32(1700000100u, t.expiresAt);
  // canje inicial sin refresh_token: lo borra; sin NTP expiresAt=0
  TEST_ASSERT_TRUE(parseTokenResponse("{\"access_token\":\"a2\"}", 5, false, t));
  TEST_ASSERT_EQUAL_STRING("", t.refresh); TEST_ASSERT_EQUAL_UINT32(0, t.expiresAt);
  // error del servidor: no toca t
  TEST_ASSERT_FALSE(parseTokenResponse("{\"error\":\"invalid_grant\"}", 1700000000, true, t));
  TEST_ASSERT_FALSE(parseTokenResponse("no json", 1700000000, true, t));
  TEST_ASSERT_EQUAL_STRING("a2", t.access);
}
void test_web_pass() {
  TEST_ASSERT_TRUE(validWebPass("x"));
  TEST_ASSERT_TRUE(validWebPass("12345678901234567890123456789012"));   // 32
  TEST_ASSERT_FALSE(validWebPass("123456789012345678901234567890123")); // 33
  TEST_ASSERT_FALSE(validWebPass(""));
  TEST_ASSERT_FALSE(validWebPass("tab\there"));
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(test_base64url_rfc4648);
  RUN_TEST(test_url_encode);
  RUN_TEST(test_split_code);
  RUN_TEST(test_refresh_body_escapes);
  RUN_TEST(test_token_response);
  RUN_TEST(test_web_pass);
  return UNITY_END();
}
