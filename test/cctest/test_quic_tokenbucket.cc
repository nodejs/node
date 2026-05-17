#if HAVE_OPENSSL && HAVE_QUIC
#include "quic/guard.h"
#ifndef OPENSSL_NO_QUIC
#include <env-inl.h>
#include <gtest/gtest.h>
#include <quic/defs.h>
#include <uv.h>

namespace node::quic {
namespace {

TEST(QuicTokenBucket, AllowsBurst) {
  TokenBucket bucket(10, 5);  // 10/sec rate, burst of 5

  // Should allow up to burst count immediately
  for (int i = 0; i < 5; i++) {
    EXPECT_TRUE(bucket.consume()) << "consume " << i << " should succeed";
  }

  // Bucket should be empty now
  EXPECT_FALSE(bucket.consume());
}

TEST(QuicTokenBucket, RefillsOverTime) {
  TokenBucket bucket(1000, 1);  // 1000/sec rate, burst of 1

  // Drain the bucket
  EXPECT_TRUE(bucket.consume());
  EXPECT_FALSE(bucket.consume());

  // Sleep 2ms — at 1000/sec that's ~2 tokens, but burst is 1
  uv_sleep(2);

  // Should have refilled to at least 1 (capped by burst)
  EXPECT_TRUE(bucket.consume());
}

TEST(QuicTokenBucket, BurstCapacity) {
  TokenBucket bucket(10000, 3);  // high rate, burst of 3

  // Drain
  EXPECT_TRUE(bucket.consume());
  EXPECT_TRUE(bucket.consume());
  EXPECT_TRUE(bucket.consume());
  EXPECT_FALSE(bucket.consume());

  // Sleep enough to fully refill
  uv_sleep(10);

  // Should be capped at burst (3), not more
  int count = 0;
  while (bucket.consume()) count++;
  EXPECT_EQ(count, 3);
}

TEST(QuicTokenBucket, ZeroRateAlwaysDenies) {
  TokenBucket bucket(0, 0);
  EXPECT_FALSE(bucket.consume());
  EXPECT_FALSE(bucket.consume());
}

TEST(QuicTokenBucket, HighRateAllowsRapidConsume) {
  TokenBucket bucket(1000000, 1000);  // 1M/sec, burst 1000

  // Should be able to consume many quickly
  int count = 0;
  for (int i = 0; i < 1000; i++) {
    if (bucket.consume()) count++;
  }
  EXPECT_EQ(count, 1000);
}

}  // namespace
}  // namespace node::quic

#endif  // OPENSSL_NO_QUIC
#endif  // HAVE_OPENSSL && HAVE_QUIC
