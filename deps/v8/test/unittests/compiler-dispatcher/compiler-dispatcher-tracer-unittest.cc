// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler-dispatcher/compiler-dispatcher-tracer.h"
#include "testing/gtest-support.h"

namespace v8 {
namespace internal {

TEST(CompilerDispatcherTracerTest, EstimateWithoutSamples) {
  CompilerDispatcherTracer tracer(nullptr);

  EXPECT_EQ(0.0, tracer.EstimatePrepareInMs());
  EXPECT_EQ(1.0, tracer.EstimateCompileInMs(1));
  EXPECT_EQ(1.0, tracer.EstimateCompileInMs(42));
  EXPECT_EQ(0.0, tracer.EstimateFinalizeInMs());
}

TEST(CompilerDispatcherTracerTest, Average) {
  CompilerDispatcherTracer tracer(nullptr);

  EXPECT_EQ(0.0, tracer.EstimatePrepareInMs());

  tracer.RecordPrepare(1.0);
  tracer.RecordPrepare(2.0);
  tracer.RecordPrepare(3.0);

  EXPECT_EQ((1.0 + 2.0 + 3.0) / 3, tracer.EstimatePrepareInMs());
}

TEST(CompilerDispatcherTracerTest, SizeBasedAverage) {
  CompilerDispatcherTracer tracer(nullptr);

  EXPECT_EQ(1.0, tracer.EstimateCompileInMs(100));

  // All three samples parse 100 units/ms.
  tracer.RecordCompile(1.0, 100);
  tracer.RecordCompile(2.0, 200);
  tracer.RecordCompile(3.0, 300);

  EXPECT_EQ(1.0, tracer.EstimateCompileInMs(100));
  EXPECT_EQ(5.0, tracer.EstimateCompileInMs(500));
}

}  // namespace internal
}  // namespace v8
