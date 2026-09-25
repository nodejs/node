#if HAVE_OPENSSL && HAVE_QUIC
#include "quic/guard.h"
#ifndef OPENSSL_NO_QUIC
#include <env-inl.h>
#include <gtest/gtest.h>
#include <quic/defs.h>
#include <uv.h>

namespace node::quic {
namespace {

// Helper: nanoseconds from seconds
static constexpr uint64_t secs(double s) {
  return static_cast<uint64_t>(s * 1e9);
}

TEST(QuicTokenBucket, AllowsBurst) {
  TokenBucket bucket(10, 5);  // 10/sec rate, burst of 5
  uint64_t now = secs(1000);

  // Should allow up to burst count immediately
  for (int i = 0; i < 5; i++) {
    EXPECT_TRUE(bucket.consume(now)) << "consume " << i << " should succeed";
  }

  // Bucket should be empty now
  EXPECT_FALSE(bucket.consume(now));
}

TEST(QuicTokenBucket, RefillsOverTime) {
  TokenBucket bucket(1000, 1);  // 1000/sec rate, burst of 1
  uint64_t now = secs(1000);

  // Drain the bucket
  EXPECT_TRUE(bucket.consume(now));
  EXPECT_FALSE(bucket.consume(now));

  // Advance 2ms — at 1000/sec that's ~2 tokens, but burst is 1
  now += secs(0.002);

  // Should have refilled to 1 (capped by burst)
  EXPECT_TRUE(bucket.consume(now));
  EXPECT_FALSE(bucket.consume(now));
}

TEST(QuicTokenBucket, BurstCapacity) {
  TokenBucket bucket(10000, 3);  // high rate, burst of 3
  uint64_t now = secs(1000);

  // Drain
  EXPECT_TRUE(bucket.consume(now));
  EXPECT_TRUE(bucket.consume(now));
  EXPECT_TRUE(bucket.consume(now));
  EXPECT_FALSE(bucket.consume(now));

  // Advance enough to fully refill
  now += secs(1);

  // Should be capped at burst (3), not more
  int count = 0;
  while (bucket.consume(now)) count++;
  EXPECT_EQ(count, 3);
}

TEST(QuicTokenBucket, ZeroRateAlwaysDenies) {
  TokenBucket bucket(0, 0);
  uint64_t now = secs(1000);
  EXPECT_FALSE(bucket.consume(now));
  now += secs(10);
  EXPECT_FALSE(bucket.consume(now));
}

TEST(QuicTokenBucket, HighRateAllowsRapidConsume) {
  TokenBucket bucket(1000000, 1000);  // 1M/sec, burst 1000
  uint64_t now = secs(1000);

  // Should be able to consume the full burst
  int count = 0;
  for (int i = 0; i < 1000; i++) {
    if (bucket.consume(now)) count++;
  }
  EXPECT_EQ(count, 1000);
}

TEST(QuicTokenBucket, InitOnce) {
  TokenBucket bucket;  // default: rate=0, burst=0, last_ts=0
  uint64_t now = secs(1000);

  // Init with rate=100, burst=5
  bucket.InitOnce(100, 5, now);
  EXPECT_TRUE(bucket.consume(now));

  // InitOnce is idempotent — second call is a no-op
  bucket.InitOnce(0, 0, now);        // would make it deny-all if applied
  EXPECT_TRUE(bucket.consume(now));  // still has tokens from first init
}

TEST(QuicTokenBucket, DefaultConstructorDenies) {
  TokenBucket bucket;  // default: rate=0, burst=0, last_ts=0
  uint64_t now = secs(1000);
  EXPECT_FALSE(bucket.consume(now));
}

TEST(QuicTokenBucket, GradualRefill) {
  TokenBucket bucket(10, 10);  // 10/sec rate, burst of 10
  uint64_t now = secs(1000);

  // Drain fully
  for (int i = 0; i < 10; i++) {
    EXPECT_TRUE(bucket.consume(now));
  }
  EXPECT_FALSE(bucket.consume(now));

  // Advance 500ms — should get 5 tokens
  now += secs(0.5);
  int count = 0;
  while (bucket.consume(now)) count++;
  EXPECT_EQ(count, 5);
}

}  // namespace
}  // namespace node::quic

#endif  // OPENSSL_NO_QUIC
#endif  // HAVE_OPENSSL && HAVE_QUIC
