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
  i::v8_flags.stress_concurrent_allocation.Set(false);
  v8::Isolate* isolate = reinterpret_cast<v8::Isolate*>(i_isolate());
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

TEST_F(GCCallbacksTest, DisallowJSObjectModification) {
  // `v8_unittests` runs death tests in "threadsafe" mode, which re-spawns the
  // process and re-runs the test from `main()` for every
  // `EXPECT_DEATH_IF_SUPPORTED` check. Ensure `i::v8_flags.hard_abort = true`
  // so each expected `CHECK` failure terminates immediately without `abort()` /
  // core-dump overhead.
  i::v8_flags.hard_abort.Set(true);

  v8::Isolate* isolate = v8_isolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  v8::Local<v8::String> existing_key = NewString("existing");
  v8::Local<v8::String> new_key = NewString("new_prop");
  v8::Local<v8::Value> value = v8::Integer::New(isolate, 42);

  v8::Local<v8::Object> fast_obj = v8::Object::New(isolate);
  CHECK(fast_obj->CreateDataProperty(context, existing_key, value).FromJust());
  CHECK(Utils::OpenDirectHandle(*fast_obj)->HasFastProperties());

  v8::Local<v8::Object> dict_obj = v8::Object::New(isolate);
  CHECK(dict_obj->CreateDataProperty(context, existing_key, value).FromJust());
  i::JSObject::NormalizeProperties(
      i_isolate(), i::Cast<i::JSObject>(Utils::OpenDirectHandle(*dict_obj)),
      i::KEEP_INOBJECT_PROPERTIES, 0, "test");
  CHECK(!Utils::OpenDirectHandle(*dict_obj)->HasFastProperties());

#if !defined(DEBUG) && defined(OFFICIAL_BUILD)
  constexpr const char* kExpectedDeathMessage = "";
#else
  constexpr const char* kExpectedDeathMessage =
      "Check failed: AllowJSObjectModification::IsAllowed";
#endif

  size_t prologue_count = 0;
  size_t epilogue_count = 0;
  using CallbackFn =
      std::function<void(v8::Isolate*, v8::GCType, v8::GCCallbackFlags)>;
  CallbackFn on_prologue = [&](v8::Isolate*, v8::GCType, v8::GCCallbackFlags) {
    // Adding a new property to a fast-mode JSObject.
    EXPECT_DEATH_IF_SUPPORTED(
        fast_obj->CreateDataProperty(context, new_key, value).Check(),
        kExpectedDeathMessage);
    // Adding an indexed element to a fast-mode JSObject.
    EXPECT_DEATH_IF_SUPPORTED(
        fast_obj->CreateDataProperty(context, 0, value).Check(),
        kExpectedDeathMessage);
    // Reconfiguring an existing property on a fast-mode JSObject.
    EXPECT_DEATH_IF_SUPPORTED(
        fast_obj->DefineOwnProperty(context, existing_key, value, v8::ReadOnly)
            .Check(),
        kExpectedDeathMessage);
    // Deleting a property from a fast-mode JSObject.
    EXPECT_DEATH_IF_SUPPORTED(fast_obj->Delete(context, existing_key).Check(),
                              kExpectedDeathMessage);
    // Adding a property to a dictionary-mode JSObject.
    EXPECT_DEATH_IF_SUPPORTED(
        dict_obj->CreateDataProperty(context, new_key, value).Check(),
        kExpectedDeathMessage);
    // Deleting a property from a dictionary-mode JSObject.
    EXPECT_DEATH_IF_SUPPORTED(dict_obj->Delete(context, existing_key).Check(),
                              kExpectedDeathMessage);
    prologue_count++;
  };
  CallbackFn on_epilogue = [&](v8::Isolate*, v8::GCType, v8::GCCallbackFlags) {
    // Run a single representative check in epilogue to avoid spawning 6 more
    // "threadsafe" death-test subprocesses.
    EXPECT_DEATH_IF_SUPPORTED(
        fast_obj->CreateDataProperty(context, new_key, value).Check(),
        kExpectedDeathMessage);
    epilogue_count++;
  };

  auto callback_thunk = [](v8::Isolate* isolate, v8::GCType type,
                           v8::GCCallbackFlags flags, void* data) {
    auto* fn = static_cast<CallbackFn*>(data);
    (*fn)(isolate, type, flags);
  };

  isolate->AddGCPrologueCallback(callback_thunk, &on_prologue);
  isolate->AddGCEpilogueCallback(callback_thunk, &on_epilogue);
  InvokeMajorGC();
  isolate->RemoveGCPrologueCallback(callback_thunk, &on_prologue);
  isolate->RemoveGCEpilogueCallback(callback_thunk, &on_epilogue);

  EXPECT_EQ(1u, prologue_count);
  EXPECT_EQ(1u, epilogue_count);
}

}  // namespace
}  // namespace v8

namespace v8::internal {
using GCCallbackFlagsTest = TestWithHeapInternalsAndContext;

using GCCallbackFunction = std::function<void(v8::Isolate* isolate, v8::GCType,
                                              v8::GCCallbackFlags flags)>;

class CallbackScope {
 public:
  CallbackScope(v8::Isolate* isolate, GCCallbackFunction prologue,
                GCCallbackFunction epilogue)
      : isolate_(isolate), prologue_(prologue), epilogue_(epilogue) {
    isolate_->AddGCPrologueCallback(PrologueCallback, this);
    isolate_->AddGCEpilogueCallback(EpilogueCallback, this);
  }

  ~CallbackScope() {
    isolate_->RemoveGCPrologueCallback(PrologueCallback, this);
    isolate_->RemoveGCEpilogueCallback(EpilogueCallback, this);
  }

 private:
  static void PrologueCallback(v8::Isolate* isolate, v8::GCType gc_type,
                               v8::GCCallbackFlags flags, void* data) {
    reinterpret_cast<CallbackScope*>(data)->prologue_(isolate, gc_type, flags);
  }

  static void EpilogueCallback(v8::Isolate* isolate, v8::GCType gc_type,
                               v8::GCCallbackFlags flags, void* data) {
    reinterpret_cast<CallbackScope*>(data)->epilogue_(isolate, gc_type, flags);
  }

  v8::Isolate* isolate_;
  GCCallbackFunction prologue_;
  GCCallbackFunction epilogue_;
};

TEST_F(GCCallbackFlagsTest, CollectAllAvailableGarbage) {
  ManualGCScope gc_scope(i_isolate());

  size_t prologue_count = 0;
  size_t epilogue_count = 0;

  CallbackScope callback_scope(
      v8_isolate(),
      [&](v8::Isolate* isolate, v8::GCType, v8::GCCallbackFlags flags) {
        ASSERT_TRUE(flags &
                    GCCallbackFlags::kGCCallbackFlagCollectAllAvailableGarbage);
        ASSERT_FALSE(flags & GCCallbackFlags::kGCCallbackFlagLastResort);
        prologue_count++;
      },
      [&](v8::Isolate* isolate, v8::GCType, v8::GCCallbackFlags flags) {
        ASSERT_TRUE(flags &
                    GCCallbackFlags::kGCCallbackFlagCollectAllAvailableGarbage);
        ASSERT_FALSE(flags & GCCallbackFlags::kGCCallbackFlagLastResort);
        epilogue_count++;
      });

  i_isolate()->heap()->CollectAllAvailableGarbage(
      GarbageCollectionReason::kHeapProfiler);
  EXPECT_GT(prologue_count, 0u);
  EXPECT_EQ(prologue_count, epilogue_count);
}

TEST_F(GCCallbackFlagsTest, CollectAllAvailableGarbageAndLastResort) {
  ManualGCScope gc_scope(i_isolate());

  size_t prologue_count = 0;
  size_t epilogue_count = 0;

  CallbackScope callback_scope(
      v8_isolate(),
      [&](v8::Isolate* isolate, v8::GCType, v8::GCCallbackFlags flags) {
        ASSERT_TRUE(flags &
                    GCCallbackFlags::kGCCallbackFlagCollectAllAvailableGarbage);
        ASSERT_TRUE(flags & GCCallbackFlags::kGCCallbackFlagLastResort);
        prologue_count++;
      },
      [&](v8::Isolate* isolate, v8::GCType, v8::GCCallbackFlags flags) {
        ASSERT_TRUE(flags &
                    GCCallbackFlags::kGCCallbackFlagCollectAllAvailableGarbage);
        ASSERT_TRUE(flags & GCCallbackFlags::kGCCallbackFlagLastResort);
        epilogue_count++;
      });

  i_isolate()->heap()->CollectAllAvailableGarbage(
      GarbageCollectionReason::kLastResort);
  EXPECT_GT(prologue_count, 0u);
  EXPECT_EQ(prologue_count, epilogue_count);
}
}  // namespace v8::internal
