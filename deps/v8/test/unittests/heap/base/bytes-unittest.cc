// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/base/bytes.h"

#include <optional>

#include "testing/gtest/include/gtest/gtest.h"

namespace heap::base {

TEST(BytesAndDurationTest, MakeBytesAndDuration) {
  const auto bad =
      BytesAndDuration(17, v8::base::TimeDelta::FromMilliseconds(35));
  EXPECT_EQ(bad.bytes, 17u);
  EXPECT_EQ(bad.duration.InMilliseconds(), 35);
}

TEST(BytesAndDurationTest, InitialAsAverage) {
  BytesAndDurationBuffer buffer;
  EXPECT_DOUBLE_EQ(
      100.0 / 2,
      *AverageSpeed(
          buffer,
          BytesAndDuration(100, v8::base::TimeDelta::FromMilliseconds(2)),
          std::nullopt));
}

TEST(BytesAndDurationTest, SelectedDuration) {
  BytesAndDurationBuffer buffer;
  // The entry will be ignored because of the selected duration below filtering
  // for the last 2ms.
  buffer.Push(BytesAndDuration(100, v8::base::TimeDelta::FromMilliseconds(8)));
  EXPECT_DOUBLE_EQ(
      100.0 / 2,
      *AverageSpeed(
          buffer,
          BytesAndDuration(100, v8::base::TimeDelta::FromMilliseconds(2)),
          v8::base::TimeDelta::FromMilliseconds(2)));
}

TEST(BytesAndDurationTest, Empty) {
  BytesAndDurationBuffer buffer;
  EXPECT_EQ(std::nullopt,
            AverageSpeed(buffer, BytesAndDuration(), std::nullopt));
}

TEST(BytesAndDurationTest, Clear) {
  BytesAndDurationBuffer buffer;
  buffer.Push(BytesAndDuration(100, v8::base::TimeDelta::FromMilliseconds(2)));
  EXPECT_DOUBLE_EQ(100.0 / 2,
                   *AverageSpeed(buffer, BytesAndDuration(), std::nullopt));
  buffer.Clear();
  EXPECT_EQ(std::nullopt,
            AverageSpeed(buffer, BytesAndDuration(), std::nullopt));
}

TEST(BytesAndDurationTest, MaxSpeed) {
  BytesAndDurationBuffer buffer;
  static constexpr size_t kMaxBytesPerMs = 1024;
  buffer.Push(BytesAndDuration(kMaxBytesPerMs,
                               v8::base::TimeDelta::FromMillisecondsD(0.5)));
  const double bounded_speed = *AverageSpeed(buffer, BytesAndDuration(),
                                             std::nullopt, 0, kMaxBytesPerMs);
  EXPECT_DOUBLE_EQ(double{kMaxBytesPerMs}, bounded_speed);
}

TEST(BytesAndDurationTest, MinSpeed) {
  BytesAndDurationBuffer buffer;
  static constexpr size_t kMinBytesPerMs = 1;
  buffer.Push(BytesAndDuration(kMinBytesPerMs,
                               v8::base::TimeDelta::FromMillisecondsD(2)));
  const double bounded_speed =
      *AverageSpeed(buffer, BytesAndDuration(), std::nullopt, kMinBytesPerMs);
  EXPECT_DOUBLE_EQ(double{kMinBytesPerMs}, bounded_speed);
}

TEST(BytesAndDurationTest, RingBufferAverage) {
  BytesAndDurationBuffer buffer;
  size_t sum = 0;
  for (size_t i = 0; i < BytesAndDurationBuffer::kSize; ++i) {
    sum += i + 1;
    buffer.Push(
        BytesAndDuration(i + 1, v8::base::TimeDelta::FromMillisecondsD(1)));
    EXPECT_DOUBLE_EQ(static_cast<double>(sum) / (i + 1),
                     *AverageSpeed(buffer, BytesAndDuration(), std::nullopt));
  }
  EXPECT_DOUBLE_EQ(static_cast<double>(sum) / BytesAndDurationBuffer::kSize,
                   *AverageSpeed(buffer, BytesAndDuration(), std::nullopt));
  // Overflow the ring buffer.
  buffer.Push(BytesAndDuration(100, v8::base::TimeDelta::FromMilliseconds(1)));
  EXPECT_DOUBLE_EQ(
      static_cast<double>(sum + 100 - 1) / BytesAndDurationBuffer::kSize,
      *AverageSpeed(buffer, BytesAndDuration(), std::nullopt));
}

TEST(SmoothedBytesAndDuration, ZeroDelta) {
  SmoothedBytesAndDuration smoothed_throughput(
      v8::base::TimeDelta::FromSeconds(1));

  EXPECT_EQ(smoothed_throughput.GetThroughput(), 0);

  // NaN rate is ignored.
  smoothed_throughput.Update(BytesAndDuration(10, v8::base::TimeDelta()));
  EXPECT_EQ(smoothed_throughput.GetThroughput(), 0);
}

TEST(SmoothedBytesAndDuration, Update) {
  SmoothedBytesAndDuration smoothed_throughput(
      v8::base::TimeDelta::FromMilliseconds(1));

  EXPECT_EQ(smoothed_throughput.GetThroughput(), 0);

  // Smoothed update from the original throughput, with 1ms half-life.
  smoothed_throughput.Update(
      BytesAndDuration(10, v8::base::TimeDelta::FromMilliseconds(1)));
  EXPECT_EQ(smoothed_throughput.GetThroughput(), 5.0);

  // After long enough, the throughput will converge.
  smoothed_throughput.Update(
      BytesAndDuration(1000, v8::base::TimeDelta::FromMilliseconds(1000)));
  EXPECT_EQ(smoothed_throughput.GetThroughput(), 1.0);

  // The throughput decays with a half-life of 1ms.
  EXPECT_EQ(smoothed_throughput.GetThroughput(
                v8::base::TimeDelta::FromMilliseconds(1)),
            0.5);
  smoothed_throughput.Update(
      BytesAndDuration(0, v8::base::TimeDelta::FromMilliseconds(1)));
  EXPECT_EQ(smoothed_throughput.GetThroughput(), 0.5);
}

}  // namespace heap::base
