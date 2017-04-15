// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler-dispatcher/compiler-dispatcher-tracer.h"
#include "testing/gtest-support.h"

namespace v8 {
namespace internal {

TEST(CompilerDispatcherTracerTest, EstimateWithoutSamples) {
  CompilerDispatcherTracer tracer(nullptr);

  EXPECT_EQ(0.0, tracer.EstimatePrepareToParseInMs());
  EXPECT_EQ(1.0, tracer.EstimateParseInMs(0));
  EXPECT_EQ(1.0, tracer.EstimateParseInMs(42));
  EXPECT_EQ(0.0, tracer.EstimateFinalizeParsingInMs());
  EXPECT_EQ(0.0, tracer.EstimatePrepareToCompileInMs());
  EXPECT_EQ(1.0, tracer.EstimateCompileInMs(0));
  EXPECT_EQ(1.0, tracer.EstimateCompileInMs(42));
  EXPECT_EQ(0.0, tracer.EstimateFinalizeCompilingInMs());
}

TEST(CompilerDispatcherTracerTest, Average) {
  CompilerDispatcherTracer tracer(nullptr);

  EXPECT_EQ(0.0, tracer.EstimatePrepareToParseInMs());

  tracer.RecordPrepareToParse(1.0);
  tracer.RecordPrepareToParse(2.0);
  tracer.RecordPrepareToParse(3.0);

  EXPECT_EQ((1.0 + 2.0 + 3.0) / 3, tracer.EstimatePrepareToParseInMs());
}

TEST(CompilerDispatcherTracerTest, SizeBasedAverage) {
  CompilerDispatcherTracer tracer(nullptr);

  EXPECT_EQ(1.0, tracer.EstimateParseInMs(100));

  // All three samples parse 100 units/ms.
  tracer.RecordParse(1.0, 100);
  tracer.RecordParse(2.0, 200);
  tracer.RecordParse(3.0, 300);

  EXPECT_EQ(1.0, tracer.EstimateParseInMs(100));
  EXPECT_EQ(5.0, tracer.EstimateParseInMs(500));
}

}  // namespace internal
}  // namespace v8
