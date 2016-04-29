// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <limits>

#include "src/heap/gc-tracer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

TEST(GCTracer, AverageSpeed) {
  RingBuffer<BytesAndDuration> buffer;
  EXPECT_EQ(100 / 2,
            GCTracer::AverageSpeed(buffer, MakeBytesAndDuration(100, 2), 0));
  buffer.Push(MakeBytesAndDuration(100, 8));
  EXPECT_EQ(100 / 2,
            GCTracer::AverageSpeed(buffer, MakeBytesAndDuration(100, 2), 2));
  EXPECT_EQ(200 / 10,
            GCTracer::AverageSpeed(buffer, MakeBytesAndDuration(100, 2), 3));
  const int max_speed = 1024 * MB;
  buffer.Reset();
  buffer.Push(MakeBytesAndDuration(max_speed, 0.5));
  EXPECT_EQ(max_speed,
            GCTracer::AverageSpeed(buffer, MakeBytesAndDuration(0, 0), 1));
  const int min_speed = 1;
  buffer.Reset();
  buffer.Push(MakeBytesAndDuration(1, 10000));
  EXPECT_EQ(min_speed,
            GCTracer::AverageSpeed(buffer, MakeBytesAndDuration(0, 0), 1));
  buffer.Reset();
  int sum = 0;
  for (int i = 0; i < buffer.kSize; i++) {
    sum += i + 1;
    buffer.Push(MakeBytesAndDuration(i + 1, 1));
  }
  EXPECT_EQ(
      sum * 1.0 / buffer.kSize,
      GCTracer::AverageSpeed(buffer, MakeBytesAndDuration(0, 0), buffer.kSize));
  buffer.Push(MakeBytesAndDuration(100, 1));
  EXPECT_EQ(
      (sum * 1.0 - 1 + 100) / buffer.kSize,
      GCTracer::AverageSpeed(buffer, MakeBytesAndDuration(0, 0), buffer.kSize));
}

}  // namespace internal
}  // namespace v8
