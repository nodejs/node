// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/embedder-tracing.h"
#include "src/heap/heap.h"
#include "test/unittests/test-utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

using LocalEmbedderHeapTracerWithIsolate = TestWithIsolate;

namespace heap {

using testing::StrictMock;
using testing::_;
using testing::Return;
using v8::EmbedderHeapTracer;
using v8::internal::LocalEmbedderHeapTracer;

namespace {

LocalEmbedderHeapTracer::WrapperInfo CreateWrapperInfo() {
  return LocalEmbedderHeapTracer::WrapperInfo(nullptr, nullptr);
}

}  // namespace

class MockEmbedderHeapTracer : public EmbedderHeapTracer {
 public:
  MOCK_METHOD1(TracePrologue, void(EmbedderHeapTracer::TraceFlags));
  MOCK_METHOD1(TraceEpilogue, void(EmbedderHeapTracer::TraceSummary*));
  MOCK_METHOD1(EnterFinalPause, void(EmbedderHeapTracer::EmbedderStackState));
  MOCK_METHOD0(IsTracingDone, bool());
  MOCK_METHOD1(RegisterV8References,
               void(const std::vector<std::pair<void*, void*> >&));
  MOCK_METHOD1(AdvanceTracing, bool(double deadline_in_ms));
};

TEST(LocalEmbedderHeapTracer, InUse) {
  MockEmbedderHeapTracer mock_remote_tracer;
  LocalEmbedderHeapTracer local_tracer(nullptr);
  local_tracer.SetRemoteTracer(&mock_remote_tracer);
  EXPECT_TRUE(local_tracer.InUse());
}

TEST(LocalEmbedderHeapTracer, NoRemoteTracer) {
  LocalEmbedderHeapTracer local_tracer(nullptr);
  // We should be able to call all functions without a remote tracer being
  // attached.
  EXPECT_FALSE(local_tracer.InUse());
  local_tracer.TracePrologue(EmbedderHeapTracer::TraceFlags::kNoFlags);
  local_tracer.EnterFinalPause();
  bool done = local_tracer.Trace(std::numeric_limits<double>::infinity());
  EXPECT_TRUE(done);
  local_tracer.TraceEpilogue();
}

TEST(LocalEmbedderHeapTracer, TracePrologueForwards) {
  StrictMock<MockEmbedderHeapTracer> remote_tracer;
  LocalEmbedderHeapTracer local_tracer(nullptr);
  local_tracer.SetRemoteTracer(&remote_tracer);
  EXPECT_CALL(remote_tracer, TracePrologue(_));
  local_tracer.TracePrologue(EmbedderHeapTracer::TraceFlags::kNoFlags);
}

TEST(LocalEmbedderHeapTracer, TracePrologueForwardsMemoryReducingFlag) {
  StrictMock<MockEmbedderHeapTracer> remote_tracer;
  LocalEmbedderHeapTracer local_tracer(nullptr);
  local_tracer.SetRemoteTracer(&remote_tracer);
  EXPECT_CALL(remote_tracer,
              TracePrologue(EmbedderHeapTracer::TraceFlags::kReduceMemory));
  local_tracer.TracePrologue(EmbedderHeapTracer::TraceFlags::kReduceMemory);
}

TEST(LocalEmbedderHeapTracer, TraceEpilogueForwards) {
  StrictMock<MockEmbedderHeapTracer> remote_tracer;
  LocalEmbedderHeapTracer local_tracer(nullptr);
  local_tracer.SetRemoteTracer(&remote_tracer);
  EXPECT_CALL(remote_tracer, TraceEpilogue(_));
  local_tracer.TraceEpilogue();
}

TEST(LocalEmbedderHeapTracer, EnterFinalPauseForwards) {
  StrictMock<MockEmbedderHeapTracer> remote_tracer;
  LocalEmbedderHeapTracer local_tracer(nullptr);
  local_tracer.SetRemoteTracer(&remote_tracer);
  EXPECT_CALL(remote_tracer, EnterFinalPause(_));
  local_tracer.EnterFinalPause();
}

TEST(LocalEmbedderHeapTracer, IsRemoteTracingDoneForwards) {
  StrictMock<MockEmbedderHeapTracer> remote_tracer;
  LocalEmbedderHeapTracer local_tracer(nullptr);
  local_tracer.SetRemoteTracer(&remote_tracer);
  EXPECT_CALL(remote_tracer, IsTracingDone());
  local_tracer.IsRemoteTracingDone();
}

TEST(LocalEmbedderHeapTracer, EnterFinalPauseDefaultStackStateUnkown) {
  StrictMock<MockEmbedderHeapTracer> remote_tracer;
  LocalEmbedderHeapTracer local_tracer(nullptr);
  local_tracer.SetRemoteTracer(&remote_tracer);
  // The default stack state is expected to be unkown.
  EXPECT_CALL(remote_tracer, EnterFinalPause(EmbedderHeapTracer::kUnknown));
  local_tracer.EnterFinalPause();
}

TEST_F(LocalEmbedderHeapTracerWithIsolate,
       EnterFinalPauseStackStateIsForwarded) {
  StrictMock<MockEmbedderHeapTracer> remote_tracer;
  LocalEmbedderHeapTracer local_tracer(isolate());
  local_tracer.SetRemoteTracer(&remote_tracer);
  local_tracer.SetEmbedderStackStateForNextFinalization(
      EmbedderHeapTracer::kEmpty);
  EXPECT_CALL(remote_tracer, EnterFinalPause(EmbedderHeapTracer::kEmpty));
  local_tracer.EnterFinalPause();
}

TEST_F(LocalEmbedderHeapTracerWithIsolate, TemporaryEmbedderStackState) {
  StrictMock<MockEmbedderHeapTracer> remote_tracer;
  LocalEmbedderHeapTracer local_tracer(isolate());
  local_tracer.SetRemoteTracer(&remote_tracer);
  // Default is unknown, see above.
  {
    EmbedderStackStateScope scope(&local_tracer, EmbedderHeapTracer::kEmpty);
    EXPECT_CALL(remote_tracer, EnterFinalPause(EmbedderHeapTracer::kEmpty));
    local_tracer.EnterFinalPause();
  }
}

TEST_F(LocalEmbedderHeapTracerWithIsolate,
       TemporaryEmbedderStackStateRestores) {
  StrictMock<MockEmbedderHeapTracer> remote_tracer;
  LocalEmbedderHeapTracer local_tracer(isolate());
  local_tracer.SetRemoteTracer(&remote_tracer);
  // Default is unknown, see above.
  {
    EmbedderStackStateScope scope(&local_tracer, EmbedderHeapTracer::kEmpty);
    {
      EmbedderStackStateScope scope(&local_tracer,
                                    EmbedderHeapTracer::kUnknown);
      EXPECT_CALL(remote_tracer, EnterFinalPause(EmbedderHeapTracer::kUnknown));
      local_tracer.EnterFinalPause();
    }
    EXPECT_CALL(remote_tracer, EnterFinalPause(EmbedderHeapTracer::kEmpty));
    local_tracer.EnterFinalPause();
  }
}

TEST_F(LocalEmbedderHeapTracerWithIsolate, EnterFinalPauseStackStateResets) {
  StrictMock<MockEmbedderHeapTracer> remote_tracer;
  LocalEmbedderHeapTracer local_tracer(isolate());
  local_tracer.SetRemoteTracer(&remote_tracer);
  local_tracer.SetEmbedderStackStateForNextFinalization(
      EmbedderHeapTracer::kEmpty);
  EXPECT_CALL(remote_tracer, EnterFinalPause(EmbedderHeapTracer::kEmpty));
  local_tracer.EnterFinalPause();
  EXPECT_CALL(remote_tracer, EnterFinalPause(EmbedderHeapTracer::kUnknown));
  local_tracer.EnterFinalPause();
}

TEST(LocalEmbedderHeapTracer, IsRemoteTracingDoneIncludesRemote) {
  StrictMock<MockEmbedderHeapTracer> remote_tracer;
  LocalEmbedderHeapTracer local_tracer(nullptr);
  local_tracer.SetRemoteTracer(&remote_tracer);
  EXPECT_CALL(remote_tracer, IsTracingDone());
  local_tracer.IsRemoteTracingDone();
}

TEST(LocalEmbedderHeapTracer, RegisterV8ReferencesWithRemoteTracer) {
  StrictMock<MockEmbedderHeapTracer> remote_tracer;
  LocalEmbedderHeapTracer local_tracer(nullptr);
  local_tracer.SetRemoteTracer(&remote_tracer);
  {
    LocalEmbedderHeapTracer::ProcessingScope scope(&local_tracer);
    scope.AddWrapperInfoForTesting(CreateWrapperInfo());
    EXPECT_CALL(remote_tracer, RegisterV8References(_));
  }
  EXPECT_CALL(remote_tracer, IsTracingDone()).WillOnce(Return(false));
  EXPECT_FALSE(local_tracer.IsRemoteTracingDone());
}

TEST_F(LocalEmbedderHeapTracerWithIsolate, SetRemoteTracerSetsIsolate) {
  StrictMock<MockEmbedderHeapTracer> remote_tracer;
  LocalEmbedderHeapTracer local_tracer(isolate());
  local_tracer.SetRemoteTracer(&remote_tracer);
  EXPECT_EQ(isolate(), reinterpret_cast<Isolate*>(remote_tracer.isolate()));
}

TEST_F(LocalEmbedderHeapTracerWithIsolate, DestructorClearsIsolate) {
  StrictMock<MockEmbedderHeapTracer> remote_tracer;
  {
    LocalEmbedderHeapTracer local_tracer(isolate());
    local_tracer.SetRemoteTracer(&remote_tracer);
    EXPECT_EQ(isolate(), reinterpret_cast<Isolate*>(remote_tracer.isolate()));
  }
  EXPECT_EQ(nullptr, remote_tracer.isolate());
}

}  // namespace heap
}  // namespace internal
}  // namespace v8
