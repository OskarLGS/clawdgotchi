#include <unity.h>
#include "quota.h"

void test_parse_ok() {
  QuotaSnapshot q;
  TEST_ASSERT_TRUE(parseRateHeaders("0.42", "1755350000", "0.07", "1755600000", "allowed", q));
  TEST_ASSERT_EQUAL(42, q.pct5h);
  TEST_ASSERT_EQUAL(7, q.pctWeek);
  TEST_ASSERT_EQUAL_UINT32(1755350000u, q.reset5h);
  TEST_ASSERT_FALSE(q.exhausted);
  TEST_ASSERT_TRUE(q.valid);
}
void test_parse_garbage_fails() {
  QuotaSnapshot q;
  TEST_ASSERT_FALSE(parseRateHeaders("banana", "0", "0.1", "0", "allowed", q));
  TEST_ASSERT_FALSE(q.valid);
}
void test_parse_clamps_and_flags_exhausted() {
  QuotaSnapshot q;
  parseRateHeaders("1.30", "1", "0.5", "1", "allowed", q);
  TEST_ASSERT_EQUAL(100, q.pct5h);
  TEST_ASSERT_TRUE(q.exhausted);
  parseRateHeaders("0.5", "1", "0.5", "1", "rejected", q);
  TEST_ASSERT_TRUE(q.exhausted);
}
void test_moods() {
  QuotaSnapshot q{};
  q.valid = true;
  q.pct5h = 10; q.pctWeek = 20;
  TEST_ASSERT_EQUAL((int)Mood::HAPPY, (int)moodFor(q, true));
  q.pct5h = 50;  TEST_ASSERT_EQUAL((int)Mood::UNEASY,   (int)moodFor(q, true));
  q.pct5h = 75;  TEST_ASSERT_EQUAL((int)Mood::STRESSED, (int)moodFor(q, true));
  q.pct5h = 90;  TEST_ASSERT_EQUAL((int)Mood::PANIC,    (int)moodFor(q, true));
  q.pct5h = 30; q.pctWeek = 91; // manda la peor cuota
  TEST_ASSERT_EQUAL((int)Mood::PANIC, (int)moodFor(q, true));
  q.exhausted = true;
  TEST_ASSERT_EQUAL((int)Mood::SLEEPING, (int)moodFor(q, true));
  TEST_ASSERT_EQUAL((int)Mood::OFFLINE, (int)moodFor(q, false));
  QuotaSnapshot inv{}; // sin datos válidos -> OFFLINE aunque haya red
  TEST_ASSERT_EQUAL((int)Mood::OFFLINE, (int)moodFor(inv, true));
}
void test_countdown_format() {
  char b[16];
  formatCountdown(1000, 1000 + 3*3600 + 12*60, b, sizeof b);
  TEST_ASSERT_EQUAL_STRING("3h 12m", b);
  formatCountdown(1000, 1000 + 2*86400 + 5*3600, b, sizeof b);
  TEST_ASSERT_EQUAL_STRING("2d 5h", b);
  formatCountdown(2000, 1000, b, sizeof b);   // reset en el pasado
  TEST_ASSERT_EQUAL_STRING("0m", b);
  formatCountdown(1000, 1000 + 59, b, sizeof b);
  TEST_ASSERT_EQUAL_STRING("0m", b);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_parse_ok);
  RUN_TEST(test_parse_garbage_fails);
  RUN_TEST(test_parse_clamps_and_flags_exhausted);
  RUN_TEST(test_moods);
  RUN_TEST(test_countdown_format);
  return UNITY_END();
}
