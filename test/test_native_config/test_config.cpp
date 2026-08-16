#include <unity.h>
#include <cstring>
#include "config_codec.h"

void test_config_roundtrip() {
  Config c;
  strcpy(c.wifiSsid, "MiRed"); c.pollMinutes = 10; c.brightness = 55;
  char buf[768];
  TEST_ASSERT_TRUE(configToJson(c, buf, sizeof buf));
  Config d;
  TEST_ASSERT_TRUE(configFromJson(buf, d));
  TEST_ASSERT_EQUAL_STRING("MiRed", d.wifiSsid);
  TEST_ASSERT_EQUAL(10, d.pollMinutes);
  TEST_ASSERT_EQUAL(55, d.brightness);
}
void test_config_bad_json() {
  Config d;
  TEST_ASSERT_FALSE(configFromJson("{noesjson", d));
}
void test_config_defaults_on_missing_fields() {
  Config d;
  TEST_ASSERT_TRUE(configFromJson("{}", d));
  TEST_ASSERT_EQUAL(5, d.pollMinutes);   // defaults sobreviven
}
void test_tokens_roundtrip() {
  Tokens t; strcpy(t.access, "at"); strcpy(t.refresh, "rt"); t.expiresAt = 123;
  char buf[3072];
  TEST_ASSERT_TRUE(tokensToJson(t, buf, sizeof buf));
  Tokens u;
  TEST_ASSERT_TRUE(tokensFromJson(buf, u));
  TEST_ASSERT_EQUAL_STRING("rt", u.refresh);
  TEST_ASSERT_EQUAL_UINT32(123, u.expiresAt);
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(test_config_roundtrip);
  RUN_TEST(test_config_bad_json);
  RUN_TEST(test_config_defaults_on_missing_fields);
  RUN_TEST(test_tokens_roundtrip);
  return UNITY_END();
}
