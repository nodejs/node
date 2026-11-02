// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/api/api-inl.h"
#include "src/api/api.h"
#include "src/execution/isolate.h"
#include "src/heap/heap-inl.h"
#include "src/heap/heap-layout-inl.h"
#include "src/heap/normal-page-inl.h"
#include "src/heap/spaces.h"
#include "src/objects/objects-inl.h"
#include "test/unittests/heap/heap-utils.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

#define TEST_STR "tests are great!"

namespace v8 {
namespace internal {

namespace {

using ExternalStringTrackerTest = TestWithHeapInternalsAndContext;

// Adapted from cctest/test-api.cc
class TestOneByteResource : public v8::String::ExternalOneByteStringResource {
 public:
  explicit TestOneByteResource(const char* data, int* counter = nullptr,
                               size_t offset = 0)
      : orig_data_(data),
        data_(data + offset),
        length_(strlen(data) - offset),
        counter_(counter) {}

  ~TestOneByteResource() override {
    i::DeleteArray(orig_data_);
    if (counter_ != nullptr) ++*counter_;
  }

  const char* data() const override { return data_; }

  size_t length() const override { return length_; }

 private:
  const char* orig_data_;
  const char* data_;
  size_t length_;
  int* counter_;
};

}  // namespace

TEST_F(ExternalStringTrackerTest,
       ExternalString_ExternalBackingStoreSizeIncreases) {
  v8::Isolate* isolate = v8_isolate();
  Heap* heap = this->heap();

  const size_t backing_store_before = heap->GetExternalStrinBytesForTesting();

  {
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::String> es = v8::String::NewExternalOneByte(
                                   isolate, new TestOneByteResource(
                                                i::StrDup(TEST_STR)))
                                   .ToLocalChecked();
    USE(es);

    const size_t backing_store_after = heap->GetExternalStrinBytesForTesting();

    EXPECT_EQ(static_cast<size_t>(es->Length()),
              backing_store_after - backing_store_before);
  }
}

TEST_F(ExternalStringTrackerTest,
       ExternalString_ExternalBackingStoreSizeDecreases) {
  ManualGCScope manual_gc_scope(i_isolate());
  v8::Isolate* isolate = v8_isolate();
  Heap* heap = this->heap();

  const size_t backing_store_before = heap->GetExternalStrinBytesForTesting();

  {
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::String> es = v8::String::NewExternalOneByte(
                                   isolate, new TestOneByteResource(
                                                i::StrDup(TEST_STR)))
                                   .ToLocalChecked();
    USE(es);
  }

  {
    // We need to invoke GC without stack, otherwise some objects may not be
    // reclaimed because of conservative stack scanning.
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap);
    InvokeAtomicMajorGC();
  }

  const size_t backing_store_after = heap->GetExternalStrinBytesForTesting();
  EXPECT_EQ(backing_store_after, backing_store_before);
}

TEST_F(ExternalStringTrackerTest,
       ExternalString_ExternalBackingStoreSizeIncreasesMarkCompact) {
  if (!v8_flags.compact) return;
  ManualGCScope manual_gc_scope(i_isolate());
  ManualEvacuationCandidatesSelectionScope
      manual_evacuation_candidate_selection_scope(manual_gc_scope);
  v8::Isolate* isolate = v8_isolate();
  Heap* heap = this->heap();
  AbandonCurrentlyFreeMemory(heap->old_space());

  const size_t backing_store_before = heap->GetExternalStrinBytesForTesting();

  {
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::String> es = v8::String::NewExternalOneByte(
                                   isolate, new TestOneByteResource(
                                                i::StrDup(TEST_STR)))
                                   .ToLocalChecked();
    DirectHandle<String> esh = v8::Utils::OpenDirectHandle(*es);

    NormalPage* page_before_gc = NormalPage::FromHeapObject(*esh);
    ForceEvacuationCandidate(page_before_gc);

    InvokeMajorGC();

    const size_t backing_store_after = heap->GetExternalStrinBytesForTesting();
    EXPECT_EQ(static_cast<size_t>(es->Length()),
              backing_store_after - backing_store_before);
  }

  {
    // We need to invoke GC without stack, otherwise some objects may not be
    // reclaimed because of conservative stack scanning.
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap);
    InvokeAtomicMajorGC();
  }

  const size_t backing_store_after = heap->GetExternalStrinBytesForTesting();
  EXPECT_EQ(0u, backing_store_after - backing_store_before);
}

TEST_F(ExternalStringTrackerTest,
       ExternalString_ExternalBackingStoreSizeIncreasesAfterExternalization) {
  if (v8_flags.single_generation) return;
  ManualGCScope manual_gc_scope(i_isolate());
  v8::Isolate* isolate = v8_isolate();
  Heap* heap = this->heap();
  const size_t backing_store_before = heap->GetExternalStrinBytesForTesting();

  {
    v8::HandleScope handle_scope(isolate);

    // Allocate normal string in the new gen.
    v8::Local<v8::String> str =
        v8::String::NewFromUtf8Literal(isolate, TEST_STR);

    EXPECT_EQ(heap->GetExternalStrinBytesForTesting(), backing_store_before);

    // Trigger full GC so that the newly allocated string moves to old gen.
    InvokeAtomicMajorGC();

    bool success = str->MakeExternal(
        isolate, new TestOneByteResource(i::StrDup(TEST_STR)));
    EXPECT_TRUE(success);

    EXPECT_EQ(static_cast<size_t>(str->Length()),
              heap->GetExternalStrinBytesForTesting() - backing_store_before);
  }

  {
    // We need to invoke GC without stack, otherwise some objects may not be
    // reclaimed because of conservative stack scanning.
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap);
    InvokeAtomicMajorGC();
  }

  const size_t backing_store_after = heap->GetExternalStrinBytesForTesting();
  EXPECT_EQ(backing_store_after, backing_store_before);
}

TEST_F(ExternalStringTrackerTest, ExternalString_PromotedThinString) {
  if (v8_flags.single_generation) return;
  ManualGCScope manual_gc_scope(i_isolate());
  v8::Isolate* isolate = v8_isolate();
  Factory* factory = i_isolate()->factory();

  {
    v8::HandleScope handle_scope(isolate);

    // New external string in the old space.
    Handle<String> string1 =
        factory
            ->NewExternalStringFromOneByte(
                new TestOneByteResource(i::StrDup(TEST_STR)))
            .ToHandleChecked();

    // Internalize external string.
    Handle<String> isymbol1 = factory->InternalizeString(string1);
    EXPECT_TRUE(IsInternalizedString(*isymbol1));
    EXPECT_TRUE(IsExternalString(*string1));
    EXPECT_FALSE(HeapLayout::InYoungGeneration(*isymbol1));

    // Collect thin string. References to the thin string will be updated to
    // point to the actual external string in the old space.
    InvokeAtomicMinorGC();

    USE(isymbol1);
  }
}

}  // namespace internal
}  // namespace v8

#undef TEST_STR
