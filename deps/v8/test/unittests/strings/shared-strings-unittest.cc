// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8-initialization.h"
#include "src/api/api-inl.h"
#include "src/api/api.h"
#include "src/base/strings.h"
#include "src/base/unique-array.h"
#include "src/common/assert-scope.h"
#include "src/common/globals.h"
#include "src/flags/flags.h"
#include "src/heap/factory.h"
#include "src/heap/gc-tracer-inl.h"
#include "src/heap/heap-inl.h"
#include "src/heap/heap-layout-inl.h"
#include "src/heap/heap.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/memory-chunk-layout.h"
#include "src/heap/mutable-page.h"
#include "src/heap/normal-page-inl.h"
#include "src/heap/parked-scope-inl.h"
#include "src/heap/remembered-set.h"
#include "src/heap/safepoint.h"
#include "src/objects/fixed-array.h"
#include "src/objects/heap-object.h"
#include "src/objects/js-weak-refs.h"
#include "src/objects/objects-inl.h"
#include "src/objects/string-forwarding-table-inl.h"
#include "test/common/flag-utils.h"
#include "test/unittests/heap/heap-utils.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

// In multi-cage mode we create one cage per isolate
// and we don't share objects between cages.
#if V8_CAN_CREATE_SHARED_HEAP_BOOL && !COMPRESS_POINTERS_IN_MULTIPLE_CAGES_BOOL

namespace v8 {
namespace internal {
namespace shared_strings_unittest {

class SharedStringsTest : public TestWithPlatform {
 private:
  SaveFlags save_flags_;
};

// Some tests in this file allocate two Isolates in the same thread to directly
// test shared string behavior. Because both are considered running, when
// disposing these Isolates, one must be parked to not cause a deadlock in the
// shared heap verification that happens on client Isolate disposal.
struct V8_NODISCARD IsolateParkOnDisposeWrapper {
  IsolateParkOnDisposeWrapper(v8::Isolate* isolate,
                              v8::Isolate* isolate_to_park)
      : isolate(isolate), isolate_to_park(isolate_to_park) {}

  ~IsolateParkOnDisposeWrapper() {
    auto main_isolate = reinterpret_cast<Isolate*>(isolate_to_park)
                            ->main_thread_local_isolate();
    main_isolate->ExecuteMainThreadWhileParked(
        [this]() { isolate->Dispose(); });
  }

  v8::Isolate* const isolate;
  v8::Isolate* const isolate_to_park;
};

class MultiClientIsolateTest {
 public:
  MultiClientIsolateTest() {
    std::unique_ptr<v8::ArrayBuffer::Allocator> allocator(
        v8::ArrayBuffer::Allocator::NewDefaultAllocator());
    v8::Isolate::CreateParams create_params;
    create_params.array_buffer_allocator = allocator.get();
    main_isolate_ = v8::Isolate::New(create_params);
    i_main_isolate()->Enter();
  }

  ~MultiClientIsolateTest() {
    i_main_isolate()->Exit();
    main_isolate_->Dispose();
  }

  v8::Isolate* main_isolate() const { return main_isolate_; }

  Isolate* i_main_isolate() const {
    return reinterpret_cast<Isolate*>(main_isolate_);
  }

  int& main_isolate_wakeup_counter() { return main_isolate_wakeup_counter_; }

  v8::Isolate* NewClientIsolate() {
    CHECK_NOT_NULL(main_isolate_);
    std::unique_ptr<v8::ArrayBuffer::Allocator> allocator(
        v8::ArrayBuffer::Allocator::NewDefaultAllocator());
    v8::Isolate::CreateParams create_params;
    create_params.array_buffer_allocator = allocator.get();
    return v8::Isolate::New(create_params);
  }

 private:
  v8::Isolate* main_isolate_;
  int main_isolate_wakeup_counter_ = 0;
};

namespace {

void CollectSharedGarbage(Heap* heap) {
  heap->CollectGarbageShared(heap->main_thread_local_heap(),
                             GarbageCollectionReason::kTesting);
}

std::pair<DirectHandle<String>, MaybeDirectHandle<String>>
CreateSharedOneByteString(Isolate* isolate, Factory* factory, int length,
                          bool internalize) {
  char* ascii = new char[length + 1];
  // Don't make single character strings, which will end up deduplicating to
  // an RO string and mess up the string table hit test.
  CHECK_GT(length, 1);
  for (int j = 0; j < length; j++) ascii[j] = 'a';
  ascii[length] = '\0';
  MaybeHandle<String> internalized;
  if (internalize) {
    // When testing concurrent string table hits, pre-internalize a string
    // of the same contents so all subsequent internalizations are hits.
    internalized =
        factory->InternalizeString(factory->NewStringFromAsciiChecked(ascii));
    CHECK(IsInternalizedString(*internalized.ToHandleChecked()));
  }
  Handle<String> string = String::Share(
      isolate, factory->NewStringFromAsciiChecked(ascii, AllocationType::kOld));
  delete[] ascii;
  CHECK(string->IsShared());
  string->EnsureHash();
  return std::make_pair(string, internalized);
}

void CheckSharedStringIsEqualCopy(DirectHandle<String> shared,
                                  DirectHandle<String> original) {
  CHECK(shared->IsShared());
  CHECK(shared->Equals(*original));
  CHECK_NE(*shared, *original);
}

Handle<String> ShareAndVerify(Isolate* isolate, Handle<String> string) {
  Handle<String> shared = String::Share(isolate, string);
  CHECK(shared->IsShared());
#ifdef VERIFY_HEAP
  Object::ObjectVerify(*shared, isolate);
  Object::ObjectVerify(*string, isolate);
#endif  // VERIFY_HEAP
  return shared;
}

class OneByteResource : public v8::String::ExternalOneByteStringResource {
 public:
  OneByteResource(const char* data, size_t length)
      : data_(data), length_(length) {}
  const char* data() const override { return data_; }
  size_t length() const override { return length_; }
  void Dispose() override {
    CHECK(!IsDisposed());
    i::DeleteArray(data_);
    data_ = nullptr;
  }
  bool IsDisposed() const { return data_ == nullptr; }

 private:
  const char* data_;
  size_t length_;
};

class TwoByteResource : public v8::String::ExternalStringResource {
 public:
  TwoByteResource(const uint16_t* data, size_t length)
      : data_(data), length_(length) {}
  const uint16_t* data() const override { return data_; }
  size_t length() const override { return length_; }
  void Dispose() override {
    i::DeleteArray(data_);
    data_ = nullptr;
  }
  bool IsDisposed() const { return data_ == nullptr; }

 private:
  const uint16_t* data_;
  size_t length_;
};

class ExternalResourceFactory {
 public:
  ~ExternalResourceFactory() {
    for (auto* res : one_byte_resources_) {
      CHECK(res->IsDisposed());
      delete res;
    }
    for (auto* res : two_byte_resources_) {
      CHECK(res->IsDisposed());
      delete res;
    }
  }
  OneByteResource* CreateOneByte(const char* data, size_t length,
                                 bool copy = true) {
    OneByteResource* res =
        new OneByteResource(copy ? i::StrDup(data) : data, length);
    Register(res);
    return res;
  }
  OneByteResource* CreateOneByte(const char* data, bool copy = true) {
    return CreateOneByte(data, strlen(data), copy);
  }
  TwoByteResource* CreateTwoByte(const uint16_t* data, size_t length) {
    TwoByteResource* res = new TwoByteResource(data, length);
    Register(res);
    return res;
  }
  TwoByteResource* CreateTwoByte(base::Vector<base::uc16> vector,
                                 bool copy = true) {
    if (copy) {
      vector = base::VectorOf(
          base::UniqueCopyOf(vector).ReleaseData().release(), vector.size());
    }
    return CreateTwoByte(vector.data(), vector.size());
  }
  void Register(OneByteResource* res) { one_byte_resources_.push_back(res); }
  void Register(TwoByteResource* res) { two_byte_resources_.push_back(res); }

 private:
  std::vector<OneByteResource*> one_byte_resources_;
  std::vector<TwoByteResource*> two_byte_resources_;
};

}  // namespace

TEST_F(SharedStringsTest, StringShare) {
  FlagScope<bool> shared_strings(&v8_flags.shared_strings, true);
  i::FlagList::EnforceFlagImplications();

  ManualGCScope manual_gc_scope(nullptr);
  ExternalResourceFactory resource_factory;
  MultiClientIsolateTest test;
  Isolate* i_isolate = test.i_main_isolate();
  Factory* factory = i_isolate->factory();

  HandleScope scope(i_isolate);

  // A longer string so that concatenated to itself, the result is >
  // ConsString::kMinLength.
  const char raw_one_byte[] =
      "Lorem ipsum dolor sit amet, consectetur adipiscing elit";
  base::uc16 raw_two_byte[] = {2001, 2002, 2003};
  base::Vector<base::uc16> two_byte(raw_two_byte, 3);

  // Strings are flattened then copied if the flatten didn't already create a
  // new copy.

  {
    // Old strings.
    Handle<String> one_byte_seq =
        factory->NewStringFromAsciiChecked(raw_one_byte, AllocationType::kOld);
    Handle<String> two_byte_seq =
        factory->NewStringFromTwoByte(two_byte, AllocationType::kOld)
            .ToHandleChecked();
    CHECK(!one_byte_seq->IsShared());
    CHECK(!two_byte_seq->IsShared());
    DirectHandle<String> shared_one_byte =
        ShareAndVerify(i_isolate, one_byte_seq);
    DirectHandle<String> shared_two_byte =
        ShareAndVerify(i_isolate, two_byte_seq);
    CheckSharedStringIsEqualCopy(shared_one_byte, one_byte_seq);
    CheckSharedStringIsEqualCopy(shared_two_byte, two_byte_seq);
  }

  {
    // Internalized strings.
    Handle<String> one_byte_seq =
        factory->NewStringFromAsciiChecked(raw_one_byte, AllocationType::kOld);
    Handle<String> two_byte_seq =
        factory->NewStringFromTwoByte(two_byte, AllocationType::kOld)
            .ToHandleChecked();
    CHECK(!one_byte_seq->IsShared());
    CHECK(!two_byte_seq->IsShared());
    Handle<String> one_byte_intern = factory->InternalizeString(one_byte_seq);
    Handle<String> two_byte_intern = factory->InternalizeString(two_byte_seq);
    CHECK(!one_byte_intern->IsShared());
    CHECK(!two_byte_intern->IsShared());
    DirectHandle<String> shared_one_byte_intern =
        ShareAndVerify(i_isolate, one_byte_intern);
    DirectHandle<String> shared_two_byte_intern =
        ShareAndVerify(i_isolate, two_byte_intern);
    CheckSharedStringIsEqualCopy(shared_one_byte_intern, one_byte_intern);
    CheckSharedStringIsEqualCopy(shared_two_byte_intern, two_byte_intern);
  }

  {
    // External strings.
    Handle<String> one_byte_ext =
        factory->NewStringFromAsciiChecked(raw_one_byte, AllocationType::kOld);
    Handle<String> two_byte_ext =
        factory->NewStringFromTwoByte(two_byte, AllocationType::kOld)
            .ToHandleChecked();
    OneByteResource* one_byte_res =
        resource_factory.CreateOneByte(raw_one_byte);
    TwoByteResource* two_byte_res = resource_factory.CreateTwoByte(two_byte);
    CHECK(String::MakeExternal(i_isolate, one_byte_ext, one_byte_res));
    CHECK(String::MakeExternal(i_isolate, two_byte_ext, two_byte_res));
    if (v8_flags.always_use_string_forwarding_table) {
      i_isolate->heap()->CollectGarbageShared(
          i_isolate->main_thread_local_heap(),
          GarbageCollectionReason::kTesting);
    }
    CHECK(IsExternalString(*one_byte_ext));
    CHECK(IsExternalString(*two_byte_ext));
    CHECK(!one_byte_ext->IsShared());
    CHECK(!two_byte_ext->IsShared());
    DirectHandle<String> shared_one_byte =
        ShareAndVerify(i_isolate, one_byte_ext);
    DirectHandle<String> shared_two_byte =
        ShareAndVerify(i_isolate, two_byte_ext);
    CheckSharedStringIsEqualCopy(shared_one_byte, one_byte_ext);
    CheckSharedStringIsEqualCopy(shared_two_byte, two_byte_ext);
  }

  if (!v8_flags.single_generation) {
    // Young strings
    Handle<String> young_one_byte_seq = factory->NewStringFromAsciiChecked(
        raw_one_byte, AllocationType::kYoung);
    Handle<String> young_two_byte_seq =
        factory->NewStringFromTwoByte(two_byte, AllocationType::kYoung)
            .ToHandleChecked();
    CHECK(HeapLayout::InYoungGeneration(*young_one_byte_seq));
    CHECK(HeapLayout::InYoungGeneration(*young_two_byte_seq));
    CHECK(!young_one_byte_seq->IsShared());
    CHECK(!young_two_byte_seq->IsShared());
    DirectHandle<String> shared_one_byte =
        ShareAndVerify(i_isolate, young_one_byte_seq);
    DirectHandle<String> shared_two_byte =
        ShareAndVerify(i_isolate, young_two_byte_seq);
    CheckSharedStringIsEqualCopy(shared_one_byte, young_one_byte_seq);
    CheckSharedStringIsEqualCopy(shared_two_byte, young_two_byte_seq);
  }

  if (!v8_flags.always_use_string_forwarding_table) {
    // Thin strings
    Handle<String> one_byte_seq1 =
        factory->NewStringFromAsciiChecked(raw_one_byte);
    Handle<String> one_byte_seq2 =
        factory->NewStringFromAsciiChecked(raw_one_byte);
    CHECK(!one_byte_seq1->IsShared());
    CHECK(!one_byte_seq2->IsShared());
    factory->InternalizeString(one_byte_seq1);
    factory->InternalizeString(one_byte_seq2);
    CHECK(StringShape(*one_byte_seq2).IsThin());
    DirectHandle<String> shared = ShareAndVerify(i_isolate, one_byte_seq2);
    CheckSharedStringIsEqualCopy(shared, one_byte_seq2);
  }

  {
    // Cons strings
    Handle<String> one_byte_seq1 =
        factory->NewStringFromAsciiChecked(raw_one_byte);
    Handle<String> one_byte_seq2 =
        factory->NewStringFromAsciiChecked(raw_one_byte);
    CHECK(!one_byte_seq1->IsShared());
    CHECK(!one_byte_seq2->IsShared());
    Handle<String> cons =
        factory->NewConsString(one_byte_seq1, one_byte_seq2).ToHandleChecked();
    CHECK(!cons->IsShared());
    CHECK(IsConsString(*cons));
    DirectHandle<String> shared = ShareAndVerify(i_isolate, cons);
    CheckSharedStringIsEqualCopy(shared, cons);
  }

  {
    // Sliced strings
    Handle<String> one_byte_seq =
        factory->NewStringFromAsciiChecked(raw_one_byte);
    CHECK(!one_byte_seq->IsShared());
    Handle<String> sliced =
        factory->NewSubString(one_byte_seq, 1, one_byte_seq->length());
    CHECK(!sliced->IsShared());
    CHECK(IsSlicedString(*sliced));
    DirectHandle<String> shared = ShareAndVerify(i_isolate, sliced);
    CheckSharedStringIsEqualCopy(shared, sliced);
  }
}

TEST_F(SharedStringsTest, SharedStringInGlobalHandle) {
  FlagScope<bool> shared_strings(&v8_flags.shared_strings, true);
  i::FlagList::EnforceFlagImplications();

  MultiClientIsolateTest test;
  Isolate* i_isolate = test.i_main_isolate();
  Factory* factory = i_isolate->factory();

  HandleScope handle_scope(i_isolate);
  DirectHandle<String> shared_string =
      factory->NewStringFromAsciiChecked("foobar", AllocationType::kSharedOld);
  CHECK(HeapLayout::InWritableSharedSpace(*shared_string));
  v8::Local<v8::String> lh_shared_string = Utils::ToLocal(shared_string);
  v8::Global<v8::String> gh_shared_string(test.main_isolate(),
                                          lh_shared_string);
  gh_shared_string.SetWeak();

  InvokeMajorGC(i_isolate);

  CHECK(!gh_shared_string.IsEmpty());
}

class WakeupTask : public CancelableTask {
 public:
  explicit WakeupTask(Isolate* isolate, int& wakeup_counter)
      : CancelableTask(isolate), wakeup_counter_(wakeup_counter) {}

 private:
  // v8::internal::CancelableTask overrides.
  void RunInternal() override { (wakeup_counter_)++; }

  int& wakeup_counter_;
};

class WorkerIsolateThread : public v8::base::Thread {
 public:
  WorkerIsolateThread(const char* name, MultiClientIsolateTest* test)
      : v8::base::Thread(base::Thread::Options(name)), test_(test) {}

  void Run() override {
    v8::Isolate* client = test_->NewClientIsolate();
    Isolate* i_client = reinterpret_cast<Isolate*>(client);
    Factory* factory = i_client->factory();

    v8::Global<v8::String> gh_shared_string;

    {
      v8::Isolate::Scope isolate_scope(client);

      {
        HandleScope handle_scope(i_client);
        DirectHandle<String> shared_string = factory->NewStringFromAsciiChecked(
            "foobar", AllocationType::kSharedOld);
        CHECK(HeapLayout::InWritableSharedSpace(*shared_string));
        v8::Local<v8::String> lh_shared_string = Utils::ToLocal(shared_string);
        gh_shared_string.Reset(test_->main_isolate(), lh_shared_string);
        gh_shared_string.SetWeak();
      }

      {
        // We need to invoke GC without stack, otherwise some objects may
        // survive.
        DisableConservativeStackScanningScopeForTesting no_stack_scanning(
            i_client->heap());
        i_client->heap()->CollectGarbageShared(
            i_client->main_thread_local_heap(),
            GarbageCollectionReason::kTesting);
      }
    }

    CHECK(gh_shared_string.IsEmpty());
    client->Dispose();

    V8::GetCurrentPlatform()
        ->GetForegroundTaskRunner(test_->main_isolate())
        ->PostTask(std::make_unique<WakeupTask>(
            test_->i_main_isolate(), test_->main_isolate_wakeup_counter()));
  }

 private:
  MultiClientIsolateTest* test_;
};

TEST_F(SharedStringsTest, SharedStringInClientGlobalHandle) {
  FlagScope<bool> shared_strings(&v8_flags.shared_strings, true);
  i::FlagList::EnforceFlagImplications();

  MultiClientIsolateTest test;
  ManualGCScope manual_gc_scope(test.i_main_isolate());
  WorkerIsolateThread thread("worker", &test);
  CHECK(thread.Start());

  while (test.main_isolate_wakeup_counter() < 1) {
    v8::platform::PumpMessageLoop(
        i::V8::GetCurrentPlatform(), test.main_isolate(),
        v8::platform::MessageLoopBehavior::kWaitForWork);
  }

  thread.Join();
}

class ClientIsolateThreadForPagePromotions : public v8::base::Thread,
                                             protected HeapInternalsBase {
 public:
  // Expects a ManualGCScope to be in scope while `Run()` is executed.
  ClientIsolateThreadForPagePromotions(const char* name,
                                       MultiClientIsolateTest* test,
                                       Handle<String>* shared_string,
                                       const ManualGCScope& witness)
      : v8::base::Thread(base::Thread::Options(name)),
        test_(test),
        shared_string_(shared_string) {}

  void Run() override {
    CHECK(v8_flags.minor_ms);
    v8::Isolate* client = test_->NewClientIsolate();
    Isolate* i_client = reinterpret_cast<Isolate*>(client);
    Factory* factory = i_client->factory();
    Heap* heap = i_client->heap();

    {
      v8::Isolate::Scope isolate_scope(client);
      HandleScope handle_scope(i_client);

      DirectHandle<FixedArray> young_object =
          factory->NewFixedArray(1, AllocationType::kYoung);
      CHECK(HeapLayout::InYoungGeneration(*young_object));
      Address young_object_address = young_object->address();

      std::vector<Handle<FixedArray>> handles;
      // Make the whole page transition from new->old, getting the buffers
      // processed in the sweeper (relying on marking information) instead of
      // processing during newspace evacuation.
      FillCurrentPage(heap->new_space(), &handles);

      CHECK(!heap->Contains(**shared_string_));
      CHECK(heap->SharedHeapContains(**shared_string_));
      young_object->set(0, **shared_string_);

      InvokeMajorGC(i_client);
      heap->CompleteSweepingFull(CompleteSweepingReason::kTesting);

      // Object should get promoted using page promotion, so address should
      // remain the same.
      CHECK(!HeapLayout::InYoungGeneration(*young_object));
      CHECK(heap->Contains(*young_object));
      CHECK_EQ(young_object_address, young_object->address());

      // Since the GC promoted that string into shared heap, it also needs to
      // create an OLD_TO_SHARED slot.
      ObjectSlot slot = young_object->RawFieldOfFirstElement();
      CHECK(RememberedSet<OLD_TO_SHARED>::Contains(
          MutablePage::FromHeapObject(i_client, *young_object),
          slot.address()));
    }

    client->Dispose();

    V8::GetCurrentPlatform()
        ->GetForegroundTaskRunner(test_->main_isolate())
        ->PostTask(std::make_unique<WakeupTask>(
            test_->i_main_isolate(), test_->main_isolate_wakeup_counter()));
  }

 private:
  MultiClientIsolateTest* test_;
  Handle<String>* shared_string_;
};

TEST_F(SharedStringsTest, RegisterOldToSharedForPromotedPageFromClient) {
  if (v8_flags.single_generation) return;
  if (!v8_flags.minor_ms) return;

  FlagScope<bool> no_stress_concurrent_allocation(
      &v8_flags.stress_concurrent_allocation,
      false);  // For SealCurrentObjects.
  FlagScope<bool> shared_strings(&v8_flags.shared_strings, true);
  i::FlagList::EnforceFlagImplications();

  ManualGCScope manual_gc_scope(nullptr);
  ManualEvacuationCandidatesSelectionScope
      manual_evacuation_candidate_selection_scope(manual_gc_scope);

  MultiClientIsolateTest test;

  Isolate* i_isolate = test.i_main_isolate();
  Isolate* shared_isolate = i_isolate->shared_space_isolate();
  Heap* shared_heap = shared_isolate->heap();

  HandleScope scope(i_isolate);

  const char raw_one_byte[] = "foo";
  Handle<String> shared_string =
      i_isolate->factory()->NewStringFromAsciiChecked(
          raw_one_byte, AllocationType::kSharedOld);
  CHECK(shared_heap->Contains(*shared_string));

  ClientIsolateThreadForPagePromotions thread("worker", &test, &shared_string,
                                              manual_gc_scope);
  CHECK(thread.Start());

  while (test.main_isolate_wakeup_counter() < 1) {
    v8::platform::PumpMessageLoop(
        i::V8::GetCurrentPlatform(), test.main_isolate(),
        v8::platform::MessageLoopBehavior::kWaitForWork);
  }

  thread.Join();
}

TEST_F(SharedStringsTest,
       RegisterOldToSharedForPromotedPageFromClientDuringIncrementalMarking) {
  if (v8_flags.single_generation) return;
  if (!v8_flags.minor_ms) return;

  FlagScope<bool> no_stress_concurrent_allocation(
      &v8_flags.stress_concurrent_allocation,
      false);  // For SealCurrentObjects.
  FlagScope<bool> shared_strings(&v8_flags.shared_strings, true);
  i::FlagList::EnforceFlagImplications();

  ManualGCScope manual_gc_scope(nullptr);
  ManualEvacuationCandidatesSelectionScope
      manual_evacuation_candidate_selection_scope(manual_gc_scope);
  FlagScope<bool> no_incremental_marking_task(
      &v8_flags.incremental_marking_task,
      false);  // Prevent the incremental GC from finishing and finalizing in a
               // task.

  MultiClientIsolateTest test;

  Isolate* i_isolate = test.i_main_isolate();
  Isolate* shared_isolate = i_isolate->shared_space_isolate();
  Heap* shared_heap = shared_isolate->heap();

  HandleScope scope(i_isolate);

  const char raw_one_byte[] = "foo";
  Handle<String> shared_string =
      i_isolate->factory()->NewStringFromAsciiChecked(
          raw_one_byte, AllocationType::kSharedOld);
  CHECK(shared_heap->Contains(*shared_string));

  // Start an incremental shared GC such that shared_string resides on an
  // evacuation candidate.
  ForceEvacuationCandidate(NormalPage::FromHeapObject(*shared_string));
  i::IncrementalMarking* marking = shared_heap->incremental_marking();
  CHECK(marking->IsStopped());
  {
    SafepointScope safepoint_scope(shared_isolate,
                                   kGlobalSafepointForSharedSpaceIsolate);
    shared_heap->tracer()->StartCycle(
        GarbageCollector::MARK_COMPACTOR, GarbageCollectionReason::kTesting,
        "collector cctest", GCTracer::MarkingType::kIncremental);
    marking->Start(GarbageCollector::MARK_COMPACTOR,
                   i::GarbageCollectionReason::kTesting, "testing");
  }

  ClientIsolateThreadForPagePromotions thread("worker", &test, &shared_string,
                                              manual_gc_scope);
  CHECK(thread.Start());

  while (test.main_isolate_wakeup_counter() < 1) {
    v8::platform::PumpMessageLoop(
        i::V8::GetCurrentPlatform(), test.main_isolate(),
        v8::platform::MessageLoopBehavior::kWaitForWork);
  }

  thread.Join();
}

class ClientIsolateThreadForRetainingByRememberedSet
    : public v8::base::Thread,
      protected HeapInternalsBase {
 public:
  // Expects a ManualGCScope to be in scope while `Run()` is executed.
  ClientIsolateThreadForRetainingByRememberedSet(
      const char* name, MultiClientIsolateTest* test,
      Persistent<v8::String>* weak_ref, const ManualGCScope& witness)
      : v8::base::Thread(base::Thread::Options(name)),
        test_(test),
        weak_ref_(weak_ref) {}

  void Run() override {
    CHECK(v8_flags.minor_ms);
    client_isolate_ = test_->NewClientIsolate();
    Isolate* i_client = reinterpret_cast<Isolate*>(client_isolate_);
    Factory* factory = i_client->factory();
    Heap* heap = i_client->heap();

    {
      v8::Isolate::Scope isolate_scope(client_isolate_);
      HandleScope scope(i_client);

      IndirectHandle<FixedArray> young_object =
          factory->NewFixedArray(1, AllocationType::kYoung);
      CHECK(HeapLayout::InYoungGeneration(*young_object));
      Address young_object_address = young_object->address();

      std::vector<Handle<FixedArray>> handles;
      // Make the whole page transition from new->old, getting the buffers
      // processed in the sweeper (relying on marking information) instead of
      // processing during newspace evacuation.
      FillCurrentPage(heap->new_space(), &handles);

      // Create a new to shared reference.
      CHECK(!weak_ref_->IsEmpty());
      IndirectHandle<String> shared_string =
          Utils::OpenHandle<v8::String, String>(
              weak_ref_->Get(client_isolate_));
      CHECK(!heap->Contains(*shared_string));
      CHECK(heap->SharedHeapContains(*shared_string));
      young_object->set(0, *shared_string);

      InvokeMajorGC(i_client);

      // Object should get promoted using page promotion, so address should
      // remain the same.
      CHECK(!HeapLayout::InYoungGeneration(*young_object));
      CHECK(heap->Contains(*young_object));
      CHECK_EQ(young_object_address, young_object->address());

      // GC should still be in progress (unless heap verification is enabled).
      CHECK_IMPLIES(!v8_flags.verify_heap, heap->sweeping_in_progress());

      // Inform main thread that the client is set up and is doing a GC.
      V8::GetCurrentPlatform()
          ->GetForegroundTaskRunner(test_->main_isolate())
          ->PostTask(std::make_unique<WakeupTask>(
              test_->i_main_isolate(), test_->main_isolate_wakeup_counter()));

      // We need to ensure that the shared GC does not scan the stack for this
      // client, otherwise some objects may survive.
      DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap);

      // Wait for main thread to do a shared GC.
      while (wakeup_counter_ < 1) {
        v8::platform::PumpMessageLoop(
            i::V8::GetCurrentPlatform(), isolate(),
            v8::platform::MessageLoopBehavior::kWaitForWork);
      }

      // Since the GC promoted that string into shared heap, it also needs to
      // create an OLD_TO_SHARED slot.
      ObjectSlot slot = young_object->RawFieldOfFirstElement();
      CHECK(RememberedSet<OLD_TO_SHARED>::Contains(
          MutablePage::FromHeapObject(i_client, *young_object),
          slot.address()));
    }

    client_isolate_->Dispose();

    // Inform main thread that client is finished.
    V8::GetCurrentPlatform()
        ->GetForegroundTaskRunner(test_->main_isolate())
        ->PostTask(std::make_unique<WakeupTask>(
            test_->i_main_isolate(), test_->main_isolate_wakeup_counter()));
  }

  v8::Isolate* isolate() const {
    DCHECK_NOT_NULL(client_isolate_);
    return client_isolate_;
  }

  int& wakeup_counter() { return wakeup_counter_; }

 private:
  MultiClientIsolateTest* test_;
  Persistent<v8::String>* weak_ref_;
  v8::Isolate* client_isolate_;
  int wakeup_counter_ = 0;
};

TEST_F(SharedStringsTest, SharedObjectRetainedByClientRememberedSet) {
  if (v8_flags.single_generation) return;
  if (!v8_flags.minor_ms) return;

  FlagScope<bool> no_stress_concurrent_allocation(
      &v8_flags.stress_concurrent_allocation,
      false);  // For SealCurrentObjects.
  FlagScope<bool> shared_strings(&v8_flags.shared_strings, true);
  i::FlagList::EnforceFlagImplications();

  ManualGCScope manual_gc_scope(nullptr);
  ManualEvacuationCandidatesSelectionScope
      manual_evacuation_candidate_selection_scope(manual_gc_scope);

  MultiClientIsolateTest test;

  v8::Isolate* isolate = test.main_isolate();
  Isolate* i_isolate = test.i_main_isolate();
  Isolate* shared_isolate = i_isolate->shared_space_isolate();
  Heap* shared_heap = shared_isolate->heap();

  // We need to invoke GC without stack, otherwise some objects may survive.
  DisableConservativeStackScanningScopeForTesting no_stack_scanning(
      shared_heap);

  // Create two weak references to Strings. One should die, the other should be
  // kept alive by the client isolate.
  Persistent<v8::String> live_weak_ref;
  Persistent<v8::String> dead_weak_ref;
  {
    HandleScope scope(i_isolate);
    const char raw_one_byte[] = "foo";

    DirectHandle<String> live_shared_string =
        i_isolate->factory()->NewStringFromAsciiChecked(
            raw_one_byte, AllocationType::kSharedOld);
    CHECK(shared_heap->Contains(*live_shared_string));
    live_weak_ref.Reset(isolate, Utils::ToLocal(live_shared_string));
    live_weak_ref.SetWeak();

    DirectHandle<String> dead_shared_string =
        i_isolate->factory()->NewStringFromAsciiChecked(
            raw_one_byte, AllocationType::kSharedOld);
    CHECK(shared_heap->Contains(*dead_shared_string));
    dead_weak_ref.Reset(isolate, Utils::ToLocal(dead_shared_string));
    dead_weak_ref.SetWeak();
  }

  ClientIsolateThreadForRetainingByRememberedSet thread(
      "worker", &test, &live_weak_ref, manual_gc_scope);
  CHECK(thread.Start());

  // Wait for client isolate to allocate objects and start a GC.
  while (test.main_isolate_wakeup_counter() < 1) {
    v8::platform::PumpMessageLoop(
        i::V8::GetCurrentPlatform(), test.main_isolate(),
        v8::platform::MessageLoopBehavior::kWaitForWork);
  }

  // Do shared GC. The live weak ref should be kept alive via an OLD_TO_SHARED
  // slot in the client isolate.
  CHECK(!live_weak_ref.IsEmpty());
  CHECK(!dead_weak_ref.IsEmpty());
  CollectSharedGarbage(i_isolate->heap());
  CHECK(!live_weak_ref.IsEmpty());
  CHECK(dead_weak_ref.IsEmpty());

  // Inform client that shared GC is finished.
  auto thread_wakeup_task = std::make_unique<WakeupTask>(
      reinterpret_cast<Isolate*>(thread.isolate()), thread.wakeup_counter());
  V8::GetCurrentPlatform()
      ->GetForegroundTaskRunner(thread.isolate())
      ->PostTask(std::move(thread_wakeup_task));

  while (test.main_isolate_wakeup_counter() < 2) {
    v8::platform::PumpMessageLoop(
        i::V8::GetCurrentPlatform(), test.main_isolate(),
        v8::platform::MessageLoopBehavior::kWaitForWork);
  }

  thread.Join();
}

class Regress1424955ClientIsolateThread : public v8::base::Thread {
 public:
  Regress1424955ClientIsolateThread(const char* name,
                                    MultiClientIsolateTest* test)
      : v8::base::Thread(base::Thread::Options(name)), test_(test) {}

  void Run() override {
    client_isolate_ = test_->NewClientIsolate();
    Isolate* i_client = reinterpret_cast<Isolate*>(client_isolate_);
    Heap* i_client_heap = i_client->heap();
    Factory* factory = i_client->factory();

    {
      // Allocate an object so that there is work for the sweeper. Otherwise,
      // starting a minor GC after a full GC may finalize sweeping since it is
      // out of work.
      v8::Isolate::Scope isolate_scope(client_isolate_);
      HandleScope handle_scope(i_client);
      Handle<FixedArray> array =
          factory->NewFixedArray(64, AllocationType::kOld);
      USE(array);

      // Start sweeping.
      InvokeMajorGC(i_client);
      CHECK(i_client_heap->sweeping_in_progress());

      // Inform the initiator thread it's time to request a global safepoint.
      V8::GetCurrentPlatform()
          ->GetForegroundTaskRunner(test_->main_isolate())
          ->PostTask(std::make_unique<WakeupTask>(
              test_->i_main_isolate(), test_->main_isolate_wakeup_counter()));

      // Wait for the initiator thread to request a global safepoint.
      while (!i_client->shared_space_isolate()
                  ->global_safepoint()
                  ->IsRequestedForTesting()) {
        v8::base::OS::Sleep(v8::base::TimeDelta::FromMilliseconds(1));
      }

      // Start a minor GC. This will cause this client isolate to join the
      // global safepoint. At which point, the initiator isolate will try to
      // finalize sweeping on behalf of this client isolate.
      InvokeMinorGC(i_client);
    }

    // Wait for the initiator isolate to finish the shared GC.
    while (wakeup_counter_ < 1) {
      v8::platform::PumpMessageLoop(
          i::V8::GetCurrentPlatform(), client_isolate_,
          v8::platform::MessageLoopBehavior::kWaitForWork);
    }

    client_isolate_->Dispose();

    V8::GetCurrentPlatform()
        ->GetForegroundTaskRunner(test_->main_isolate())
        ->PostTask(std::make_unique<WakeupTask>(
            test_->i_main_isolate(), test_->main_isolate_wakeup_counter()));
  }

  v8::Isolate* isolate() const {
    DCHECK_NOT_NULL(client_isolate_);
    return client_isolate_;
  }

  int& wakeup_counter() { return wakeup_counter_; }

 private:
  MultiClientIsolateTest* test_;
  v8::Isolate* client_isolate_;
  int wakeup_counter_ = 0;
};

// TOOD(pthier): This test doesn't belong here. It should be moved to
// shared-heap testing.
TEST_F(SharedStringsTest, Regress1424955) {
  if (v8_flags.single_generation) return;
  // When heap verification is enabled, sweeping is finalized in the atomic
  // pause. This issue requires that sweeping is still in progress after the
  // atomic pause is finished.
  if (v8_flags.verify_heap) return;
  FlagScope<bool> shared_heap(&v8_flags.shared_heap, true);
  i::FlagList::EnforceFlagImplications();

  ManualGCScope manual_gc_scope(nullptr);

  MultiClientIsolateTest test;
  Regress1424955ClientIsolateThread thread("worker", &test);
  CHECK(thread.Start());

  // Wait for client thread to start sweeping.
  while (test.main_isolate_wakeup_counter() < 1) {
    v8::platform::PumpMessageLoop(
        i::V8::GetCurrentPlatform(), test.main_isolate(),
        v8::platform::MessageLoopBehavior::kWaitForWork);
  }

  // Client isolate waits for this isolate to request a global safepoint and
  // then triggers a minor GC.
  CollectSharedGarbage(test.i_main_isolate()->heap());
  V8::GetCurrentPlatform()
      ->GetForegroundTaskRunner(thread.isolate())
      ->PostTask(std::make_unique<WakeupTask>(
          reinterpret_cast<Isolate*>(thread.isolate()),
          thread.wakeup_counter()));

  // Wait for client isolate to finish the minor GC and dispose of its isolate.
  while (test.main_isolate_wakeup_counter() < 2) {
    v8::platform::PumpMessageLoop(
        i::V8::GetCurrentPlatform(), test.main_isolate(),
        v8::platform::MessageLoopBehavior::kWaitForWork);
  }

  thread.Join();
}

// This client isolate thread and the following test are equivalent to
// `HeapTest.ConservativePinningScopeMarkCompactRetainsObjectReachableFromStack`
// in heap-unittest.cc but for client isolates and shared heap GCs.
class ClientIsolateThreadForConservativePinningScope : public v8::base::Thread {
 public:
  // Expects a ManualGCScope to be in scope while `Run()` is executed.
  ClientIsolateThreadForConservativePinningScope(const char* name,
                                                 MultiClientIsolateTest* test,
                                                 const ManualGCScope& witness)
      : v8::base::Thread(base::Thread::Options(name)), test_(test) {}

  void Run() override {
    v8::Isolate* client = test_->NewClientIsolate();
    Isolate* i_client = reinterpret_cast<Isolate*>(client);
    Isolate* shared_isolate = i_client->shared_space_isolate();
    Heap* shared_heap = shared_isolate->heap();
    Factory* factory = i_client->factory();
    Heap* heap = i_client->heap();

    {
      v8::Isolate::Scope isolate_scope(client);
      HandleScope handle_scope(i_client);

      ConservativePinningScope conservative_pinning_scope(heap);

      // The main isolate's conservative stack visitor will find this on the
      // client isolate's stack, so `object` will be retained and not move
      // during GC.
      Address object_address;
      // Use a `Global` to check whether `object` is actually retained. There
      // should be no other references to it.
      v8::Global<v8::String> global;
      {
        HandleScope nested_handle_scope(i_client);

        const char raw_one_byte[] = "foo";
        Handle<String> shared_string = factory->NewStringFromAsciiChecked(
            raw_one_byte, AllocationType::kSharedOld);
        CHECK(shared_heap->Contains(*shared_string));
        // Set a weak Global reference to `str` to check that it isn't
        // reclaimed. Scavenger should not strongify weak Globals.
        global.Reset(client, Utils::ToLocal(shared_string));
        global.SetWeak();
        object_address = shared_string->address();
      }

      CHECK(!global.IsEmpty());
      CHECK(global.IsWeak());
      i_client->heap()->CollectGarbageShared(i_client->main_thread_local_heap(),
                                             GarbageCollectionReason::kTesting);
      CHECK(global.IsWeak());
      CHECK(!global.IsEmpty());
      // Make sure `object_address` isn't optimized away.
      CHECK_EQ((*Utils::OpenDirectHandle(*global.Get(client)))->address(),
               object_address);
    }

    client->Dispose();

    V8::GetCurrentPlatform()
        ->GetForegroundTaskRunner(test_->main_isolate())
        ->PostTask(std::make_unique<WakeupTask>(
            test_->i_main_isolate(), test_->main_isolate_wakeup_counter()));
  }

 private:
  MultiClientIsolateTest* test_;
};

TEST_F(SharedStringsTest, ConservativePinningScopeInClientIsolate) {
  // `v8_flags.conservative_stack_scanning` is constexpr in some builds.
  if (v8_flags.conservative_stack_scanning) {
    return;
  }
  FlagScope<bool> shared_strings(&v8_flags.shared_strings, true);
  FlagScope<bool> no_scavenger_conservative_object_pinning(
      &v8_flags.scavenger_conservative_object_pinning, false);
  FlagScope<bool> no_precise_object_pinning(&v8_flags.precise_object_pinning,
                                            false);
  FlagScope<bool> no_scavenger_precise_object_pinning(
      &v8_flags.scavenger_precise_object_pinning, false);
  i::FlagList::EnforceFlagImplications();

  MultiClientIsolateTest test;
  ManualGCScope manual_gc_scope(test.i_main_isolate());

  ClientIsolateThreadForConservativePinningScope thread("worker", &test,
                                                        manual_gc_scope);
  CHECK(thread.Start());

  while (test.main_isolate_wakeup_counter() < 1) {
    v8::platform::PumpMessageLoop(
        i::V8::GetCurrentPlatform(), test.main_isolate(),
        v8::platform::MessageLoopBehavior::kWaitForWork);
  }

  thread.Join();
}

TEST_F(SharedStringsTest, InternalizeWithoutSharedStringTable) {
  if (v8_flags.single_generation) return;

  FlagScope<bool> shared_strings(&v8_flags.shared_strings, true);
  i::FlagList::EnforceFlagImplications();

  MultiClientIsolateTest test;
  Isolate* main_isolate = test.i_main_isolate();
  Factory* main_factory = main_isolate->factory();

  HandleScope handle_scope(main_isolate);

  const char raw_one_byte[] = "foo";
  base::uc16 raw_two_byte[] = {2001, 2002, 2003};
  base::Vector<const base::uc16> two_byte(raw_two_byte, 3);

  // Old generation 1- and 2-byte seq strings are in-place internalizable, but
  // not shared without shared string table.
  DirectHandle<String> old_one_byte_seq =
      main_factory->NewStringFromAsciiChecked(raw_one_byte,
                                              AllocationType::kOld);
  CHECK(!old_one_byte_seq->IsShared());
  CHECK(!HeapLayout::InAnySharedSpace(*old_one_byte_seq));
  DirectHandle<String> old_two_byte_seq =
      main_factory->NewStringFromTwoByte(two_byte, AllocationType::kOld)
          .ToHandleChecked();
  CHECK(!old_two_byte_seq->IsShared());
  CHECK(!HeapLayout::InAnySharedSpace(*old_two_byte_seq));

  // Internalized strings are not shared without shared string table.
  HashSeed seed = HashSeed(main_isolate);
  DirectHandle<String> one_byte_intern =
      main_factory->NewOneByteInternalizedString(
          base::OneByteVector(raw_one_byte),
          StringHasher::HashSequentialString<char>(raw_one_byte, 3, seed));
  CHECK(!one_byte_intern->IsShared());
  CHECK(!HeapLayout::InAnySharedSpace(*one_byte_intern));
  DirectHandle<String> two_byte_intern =
      main_factory->NewTwoByteInternalizedString(
          two_byte,
          StringHasher::HashSequentialString<uint16_t>(raw_two_byte, 3, seed));
  CHECK(!two_byte_intern->IsShared());
  CHECK(!HeapLayout::InAnySharedSpace(*two_byte_intern));
}

TEST_F(SharedStringsTest, InternalizeSharedWithoutSharedStringTable) {
  if (v8_flags.single_generation) return;

  FlagScope<bool> shared_strings(&v8_flags.shared_strings, true);
  i::FlagList::EnforceFlagImplications();

  MultiClientIsolateTest test;
  IsolateParkOnDisposeWrapper isolate_wrapper(test.NewClientIsolate(),
                                              test.main_isolate());
  Isolate* main_isolate = test.i_main_isolate();
  Factory* main_factory = main_isolate->factory();
  Isolate* client_isolate = reinterpret_cast<Isolate*>(isolate_wrapper.isolate);
  Factory* client_factory = client_isolate->factory();

  HandleScope main_scope(main_isolate);
  HandleScope client_scope(client_isolate);

  DirectHandle<String> shared_string;
  DirectHandle<String> main_internalized;

  // Park the client isolate while we do work on the main isolate. Otherwise we
  // could get a deadlock when a global safepoint is requested.
  client_isolate->main_thread_local_isolate()->ExecuteMainThreadWhileParked(
      [&]() {
        // Create a new shared string on the main isolate.
        constexpr int kStringLength = 14;
        constexpr bool kInternalize = false;
        shared_string = CreateSharedOneByteString(main_isolate, main_factory,
                                                  kStringLength, kInternalize)
                            .first;
        CHECK(shared_string->IsShared());
        CHECK(HeapLayout::InAnySharedSpace(*shared_string));

        // Internalize shared string on the main isolate.
        main_internalized = main_factory->InternalizeString(shared_string);
        // Internalized string is not shared.
        CHECK(!main_internalized->IsShared());
        CHECK(!HeapLayout::InAnySharedSpace(*main_internalized));
        // Shared string is still shared.
        CHECK(shared_string->IsShared());
        CHECK(HeapLayout::InAnySharedSpace(*shared_string));
        CheckSharedStringIsEqualCopy(shared_string, main_internalized);
        // Both have the same, valid hash.
        CHECK(String::IsHashFieldComputed(shared_string->raw_hash_field()));
        CHECK(String::IsHashFieldComputed(main_internalized->raw_hash_field()));
        CHECK_EQ(shared_string->hash(), main_internalized->hash());
      });

  // Internalize shared string on client isolate.
  {
    v8::Isolate::Scope isolate_scope(isolate_wrapper.isolate);
    DirectHandle<String> client_internalized =
        client_factory->InternalizeString(shared_string);
    // Internalized string is not shared.
    CHECK(!client_internalized->IsShared());
    CHECK(!HeapLayout::InAnySharedSpace(*client_internalized));
    // Shared string is still shared.
    CHECK(shared_string->IsShared());
    CHECK(HeapLayout::InAnySharedSpace(*shared_string));
    // Both have the same, valid hash.
    CHECK(String::IsHashFieldComputed(shared_string->raw_hash_field()));
    CHECK(String::IsHashFieldComputed(client_internalized->raw_hash_field()));
    CHECK_EQ(shared_string->hash(), client_internalized->hash());
    // Internalized strings on client and main isolate are not the same object.
    CHECK(!client_internalized.equals(main_internalized));
    CHECK_NE(*client_internalized, *main_internalized);
    // Internalized strings on client and main isolate are not comparable.
    CHECK(!client_internalized->Equals(*main_internalized));
  }
}

TEST_F(SharedStringsTest, LookupSharedWithoutSharedStringTable) {
  if (v8_flags.single_generation) return;

  FlagScope<bool> shared_strings(&v8_flags.shared_strings, true);
  i::FlagList::EnforceFlagImplications();

  MultiClientIsolateTest test;
  Isolate* main_isolate = test.i_main_isolate();
  Factory* main_factory = main_isolate->factory();

  HandleScope main_scope(main_isolate);

  // Create a new string on the main isolate and internalize it.
  constexpr int kStringLength = 14;
  constexpr bool kInternalize = true;
  auto [shared_string, maybe_internalized_string] = CreateSharedOneByteString(
      main_isolate, main_factory, kStringLength, kInternalize);
  DirectHandle<String> main_pre_internalized =
      maybe_internalized_string.ToHandleChecked();
  CHECK(shared_string->IsShared());
  CHECK(HeapLayout::InAnySharedSpace(*shared_string));
  CHECK(!main_pre_internalized->IsShared());
  CHECK(!HeapLayout::InAnySharedSpace(*main_pre_internalized));

  // Internalize shared string on the main isolate.
  DirectHandle<String> main_internalized =
      main_factory->InternalizeString(shared_string);
  // Internalized string is not shared and equal to |main_pre_internalized|
  CHECK(!main_internalized->IsShared());
  CHECK(!HeapLayout::InAnySharedSpace(*main_internalized));
  CHECK_EQ(*main_internalized, *main_pre_internalized);

  // Shared string is still a sequential one-byte string.
  CHECK_EQ(shared_string->map()->instance_type(),
           InstanceType::SHARED_SEQ_ONE_BYTE_STRING_TYPE);
  // Shared string is still shared.
  CHECK(shared_string->IsShared());
  CHECK(HeapLayout::InAnySharedSpace(*shared_string));
  CheckSharedStringIsEqualCopy(shared_string, main_internalized);
  // All strings have the same, valid hash.
  CHECK(String::IsHashFieldComputed(shared_string->raw_hash_field()));
  CHECK(String::IsHashFieldComputed(main_internalized->raw_hash_field()));
  CHECK(String::IsHashFieldComputed(main_pre_internalized->raw_hash_field()));
  CHECK_EQ(shared_string->hash(), main_internalized->hash());
  CHECK_EQ(main_internalized->hash(), main_pre_internalized->hash());
}

TEST_F(SharedStringsTest, ExternalizeSharedWithoutSharedStringTable) {
  if (v8_flags.single_generation) return;

  FlagScope<bool> shared_strings(&v8_flags.shared_strings, true);
  i::FlagList::EnforceFlagImplications();

  MultiClientIsolateTest test;
  Isolate* main_isolate = test.i_main_isolate();
  Factory* main_factory = main_isolate->factory();

  HandleScope main_scope(main_isolate);

  // Create a new shared string on the main isolate.
  const char raw_one_byte[] = "foobarstringtest";

  DirectHandle<String> string = main_factory->NewStringFromAsciiChecked(
      raw_one_byte, AllocationType::kOld);
  DirectHandle<String> shared_string = String::Share(main_isolate, string);
  CHECK(!string->IsShared());
  CHECK(!HeapLayout::InAnySharedSpace(*string));
  CHECK(shared_string->IsShared());
  CHECK(HeapLayout::InAnySharedSpace(*shared_string));

  // Original string can be externalized.
  CHECK(
      string->SupportsExternalization(v8::String::Encoding::ONE_BYTE_ENCODING));
  // Shared string can't be externalized.
  CHECK(!shared_string->SupportsExternalization(
      v8::String::Encoding::ONE_BYTE_ENCODING));
}

}  // namespace shared_strings_unittest
}  // namespace internal
}  // namespace v8

#endif  // V8_CAN_CREATE_SHARED_HEAP_BOOL &&
        // !COMPRESS_POINTERS_IN_MULTIPLE_CAGES_BOOL
