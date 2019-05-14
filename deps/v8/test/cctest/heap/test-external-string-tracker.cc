// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/api-inl.h"
#include "src/api.h"
#include "src/heap/heap-inl.h"
#include "src/heap/spaces.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "test/cctest/cctest.h"
#include "test/cctest/heap/heap-tester.h"
#include "test/cctest/heap/heap-utils.h"

#define TEST_STR "tests are great!"

namespace v8 {
namespace internal {
namespace heap {

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

TEST(ExternalString_ExternalBackingStoreSizeIncreases) {
  CcTest::InitializeVM();
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  Heap* heap = reinterpret_cast<Isolate*>(isolate)->heap();
  ExternalBackingStoreType type = ExternalBackingStoreType::kExternalString;

  const size_t backing_store_before =
      heap->old_space()->ExternalBackingStoreBytes(type);

  {
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::String> es = v8::String::NewExternalOneByte(
        isolate, new TestOneByteResource(i::StrDup(TEST_STR))).ToLocalChecked();
    USE(es);

    const size_t backing_store_after =
        heap->old_space()->ExternalBackingStoreBytes(type);

    CHECK_EQ(es->Length(), backing_store_after - backing_store_before);
  }
}

TEST(ExternalString_ExternalBackingStoreSizeDecreases) {
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  Heap* heap = reinterpret_cast<Isolate*>(isolate)->heap();
  ExternalBackingStoreType type = ExternalBackingStoreType::kExternalString;

  const size_t backing_store_before =
      heap->old_space()->ExternalBackingStoreBytes(type);

  {
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::String> es = v8::String::NewExternalOneByte(
        isolate, new TestOneByteResource(i::StrDup(TEST_STR))).ToLocalChecked();
    USE(es);
  }

  heap::GcAndSweep(heap, OLD_SPACE);

  const size_t backing_store_after =
      heap->old_space()->ExternalBackingStoreBytes(type);

  CHECK_EQ(0, backing_store_after - backing_store_before);
}

TEST(ExternalString_ExternalBackingStoreSizeIncreasesMarkCompact) {
  if (FLAG_never_compact) return;
  ManualGCScope manual_gc_scope;
  FLAG_manual_evacuation_candidates_selection = true;
  CcTest::InitializeVM();
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  Heap* heap = reinterpret_cast<Isolate*>(isolate)->heap();
  heap::AbandonCurrentlyFreeMemory(heap->old_space());
  ExternalBackingStoreType type = ExternalBackingStoreType::kExternalString;

  const size_t backing_store_before =
      heap->old_space()->ExternalBackingStoreBytes(type);

  {
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::String> es = v8::String::NewExternalOneByte(
        isolate, new TestOneByteResource(i::StrDup(TEST_STR))).ToLocalChecked();
    v8::internal::Handle<v8::internal::String> esh = v8::Utils::OpenHandle(*es);

    Page* page_before_gc = Page::FromHeapObject(*esh);
    heap::ForceEvacuationCandidate(page_before_gc);

    CcTest::CollectAllGarbage();

    const size_t backing_store_after =
        heap->old_space()->ExternalBackingStoreBytes(type);
    CHECK_EQ(es->Length(), backing_store_after - backing_store_before);
  }

  heap::GcAndSweep(heap, OLD_SPACE);
  const size_t backing_store_after =
      heap->old_space()->ExternalBackingStoreBytes(type);
  CHECK_EQ(0, backing_store_after - backing_store_before);
}

TEST(ExternalString_ExternalBackingStoreSizeIncreasesAfterExternalization) {
  CcTest::InitializeVM();
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  Heap* heap = reinterpret_cast<Isolate*>(isolate)->heap();
  ExternalBackingStoreType type = ExternalBackingStoreType::kExternalString;
  size_t old_backing_store_before = 0, new_backing_store_before = 0;

  {
    v8::HandleScope handle_scope(isolate);

    new_backing_store_before =
        heap->new_space()->ExternalBackingStoreBytes(type);
    old_backing_store_before =
        heap->old_space()->ExternalBackingStoreBytes(type);

    // Allocate normal string in the new gen.
    v8::Local<v8::String> str =
        v8::String::NewFromUtf8(isolate, TEST_STR, v8::NewStringType::kNormal)
            .ToLocalChecked();

    CHECK_EQ(0, heap->new_space()->ExternalBackingStoreBytes(type) -
                    new_backing_store_before);

    // Trigger GCs so that the newly allocated string moves to old gen.
    heap::GcAndSweep(heap, NEW_SPACE);  // in survivor space now
    heap::GcAndSweep(heap, NEW_SPACE);  // in old gen now

    bool success =
        str->MakeExternal(new TestOneByteResource(i::StrDup(TEST_STR)));
    CHECK(success);

    CHECK_EQ(str->Length(), heap->old_space()->ExternalBackingStoreBytes(type) -
                                old_backing_store_before);
  }

  heap::GcAndSweep(heap, OLD_SPACE);

  CHECK_EQ(0, heap->old_space()->ExternalBackingStoreBytes(type) -
                  old_backing_store_before);
}

TEST(ExternalString_PromotedThinString) {
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  i::Isolate* i_isolate = CcTest::i_isolate();
  i::Factory* factory = i_isolate->factory();
  Heap* heap = i_isolate->heap();

  {
    v8::HandleScope handle_scope(isolate);

    // New external string in the old space.
    v8::internal::Handle<v8::internal::String> string1 =
        factory
            ->NewExternalStringFromOneByte(
                new TestOneByteResource(i::StrDup(TEST_STR)))
            .ToHandleChecked();

    // Internalize external string.
    i::Handle<i::String> isymbol1 = factory->InternalizeString(string1);
    CHECK(isymbol1->IsInternalizedString());
    CHECK(string1->IsExternalString());
    CHECK(!heap->InYoungGeneration(*isymbol1));

    // New external string in the young space. This string has the same content
    // as the previous one (that was already internalized).
    v8::Local<v8::String> string2 =
        v8::String::NewFromUtf8(isolate, TEST_STR, v8::NewStringType::kNormal)
            .ToLocalChecked();
    bool success =
        string2->MakeExternal(new TestOneByteResource(i::StrDup(TEST_STR)));
    CHECK(success);

    // Internalize (it will create a thin string in the new space).
    i::Handle<i::String> istring = v8::Utils::OpenHandle(*string2);
    i::Handle<i::String> isymbol2 = factory->InternalizeString(istring);
    CHECK(isymbol2->IsInternalizedString());
    CHECK(istring->IsThinString());
    CHECK(heap->InYoungGeneration(*istring));

    // Collect thin string. References to the thin string will be updated to
    // point to the actual external string in the old space.
    heap::GcAndSweep(heap, NEW_SPACE);

    USE(isymbol1);
    USE(isymbol2);
  }
}
}  // namespace heap
}  // namespace internal
}  // namespace v8

#undef TEST_STR
