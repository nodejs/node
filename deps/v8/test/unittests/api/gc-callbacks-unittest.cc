// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/heap/heap-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace {

namespace {

class GCCallbacksTest : public internal::TestWithHeapInternalsAndContext {
 public:
  static void PrologueCallbackAlloc(v8::Isolate* isolate, v8::GCType,
                                    v8::GCCallbackFlags flags) {
    v8::HandleScope scope(isolate);

    CHECK_EQ(flags, v8::kNoGCCallbackFlags);
    CHECK_EQ(current_test_->gc_callbacks_isolate_, isolate);
    ++current_test_->prologue_call_count_alloc_;

    if (!i::v8_flags.single_generation) {
      // Simulate full heap to see if we will reenter this callback
      current_test_->SimulateFullSpace(current_test_->heap()->new_space());
    }

    Local<Object> obj = Object::New(isolate);
    CHECK(!obj.IsEmpty());

    current_test_->InvokeAtomicMajorGC();
  }

  static void EpilogueCallbackAlloc(v8::Isolate* isolate, v8::GCType,
                                    v8::GCCallbackFlags flags) {
    v8::HandleScope scope(isolate);

    CHECK_EQ(flags, v8::kNoGCCallbackFlags);
    CHECK_EQ(current_test_->gc_callbacks_isolate_, isolate);
    ++current_test_->epilogue_call_count_alloc_;

    if (!i::v8_flags.single_generation) {
      // Simulate full heap to see if we will reenter this callback
      current_test_->SimulateFullSpace(current_test_->heap()->new_space());
    }

    Local<Object> obj = Object::New(isolate);
    CHECK(!obj.IsEmpty());

    current_test_->InvokeAtomicMajorGC();
  }

  static void PrologueCallback(v8::Isolate* isolate, v8::GCType,
                               v8::GCCallbackFlags flags) {
    CHECK_EQ(flags, v8::kNoGCCallbackFlags);
    CHECK_EQ(current_test_->gc_callbacks_isolate_, isolate);
    ++current_test_->prologue_call_count_;
  }

  static void EpilogueCallback(v8::Isolate* isolate, v8::GCType,
                               v8::GCCallbackFlags flags) {
    CHECK_EQ(flags, v8::kNoGCCallbackFlags);
    CHECK_EQ(current_test_->gc_callbacks_isolate_, isolate);
    ++current_test_->epilogue_call_count_;
  }

  static void PrologueCallbackSecond(v8::Isolate* isolate, v8::GCType,
                                     v8::GCCallbackFlags flags) {
    CHECK_EQ(flags, v8::kNoGCCallbackFlags);
    CHECK_EQ(current_test_->gc_callbacks_isolate_, isolate);
    ++current_test_->prologue_call_count_second_;
  }

  static void EpilogueCallbackSecond(v8::Isolate* isolate, v8::GCType,
                                     v8::GCCallbackFlags flags) {
    CHECK_EQ(flags, v8::kNoGCCallbackFlags);
    CHECK_EQ(current_test_->gc_callbacks_isolate_, isolate);
    ++current_test_->epilogue_call_count_second_;
  }

  static void PrologueCallbackNew(v8::Isolate* isolate, v8::GCType,
                                  v8::GCCallbackFlags flags, void* data) {
    CHECK_EQ(flags, v8::kNoGCCallbackFlags);
    CHECK_EQ(current_test_->gc_callbacks_isolate_, isolate);
    ++*static_cast<int*>(data);
  }

  static void EpilogueCallbackNew(v8::Isolate* isolate, v8::GCType,
                                  v8::GCCallbackFlags flags, void* data) {
    CHECK_EQ(flags, v8::kNoGCCallbackFlags);
    CHECK_EQ(current_test_->gc_callbacks_isolate_, isolate);
    ++*static_cast<int*>(data);
  }

 protected:
  void SetUp() override {
    internal::TestWithHeapInternalsAndContext::SetUp();
    DCHECK_NULL(current_test_);
    current_test_ = this;
  }
  void TearDown() override {
    DCHECK_NOT_NULL(current_test_);
    current_test_ = nullptr;
    internal::TestWithHeapInternalsAndContext::TearDown();
  }
  static GCCallbacksTest* current_test_;

  v8::Isolate* gc_callbacks_isolate_ = nullptr;
  int prologue_call_count_ = 0;
  int epilogue_call_count_ = 0;
  int prologue_call_count_second_ = 0;
  int epilogue_call_count_second_ = 0;
  int prologue_call_count_alloc_ = 0;
  int epilogue_call_count_alloc_ = 0;
};

GCCallbacksTest* GCCallbacksTest::current_test_ = nullptr;

}  // namespace

TEST_F(GCCallbacksTest, GCCallbacks) {
  // For SimulateFullSpace in PrologueCallbackAlloc and EpilogueCallbackAlloc.
  i::v8_flags.stress_concurrent_allocation = false;
  v8::Isolate* isolate = context()->GetIsolate();
  gc_callbacks_isolate_ = isolate;
  isolate->AddGCPrologueCallback(PrologueCallback);
  isolate->AddGCEpilogueCallback(EpilogueCallback);
  CHECK_EQ(0, prologue_call_count_);
  CHECK_EQ(0, epilogue_call_count_);
  InvokeMajorGC();
  CHECK_EQ(1, prologue_call_count_);
  CHECK_EQ(1, epilogue_call_count_);
  isolate->AddGCPrologueCallback(PrologueCallbackSecond);
  isolate->AddGCEpilogueCallback(EpilogueCallbackSecond);
  InvokeMajorGC();
  CHECK_EQ(2, prologue_call_count_);
  CHECK_EQ(2, epilogue_call_count_);
  CHECK_EQ(1, prologue_call_count_second_);
  CHECK_EQ(1, epilogue_call_count_second_);
  isolate->RemoveGCPrologueCallback(PrologueCallback);
  isolate->RemoveGCEpilogueCallback(EpilogueCallback);
  InvokeMajorGC();
  CHECK_EQ(2, prologue_call_count_);
  CHECK_EQ(2, epilogue_call_count_);
  CHECK_EQ(2, prologue_call_count_second_);
  CHECK_EQ(2, epilogue_call_count_second_);
  isolate->RemoveGCPrologueCallback(PrologueCallbackSecond);
  isolate->RemoveGCEpilogueCallback(EpilogueCallbackSecond);
  InvokeMajorGC();
  CHECK_EQ(2, prologue_call_count_);
  CHECK_EQ(2, epilogue_call_count_);
  CHECK_EQ(2, prologue_call_count_second_);
  CHECK_EQ(2, epilogue_call_count_second_);

  CHECK_EQ(0, prologue_call_count_alloc_);
  CHECK_EQ(0, epilogue_call_count_alloc_);
  isolate->AddGCPrologueCallback(PrologueCallbackAlloc);
  isolate->AddGCEpilogueCallback(EpilogueCallbackAlloc);
  InvokeAtomicMajorGC();
  CHECK_EQ(1, prologue_call_count_alloc_);
  CHECK_EQ(1, epilogue_call_count_alloc_);
  isolate->RemoveGCPrologueCallback(PrologueCallbackAlloc);
  isolate->RemoveGCEpilogueCallback(EpilogueCallbackAlloc);
}

}  // namespace
}  // namespace v8
