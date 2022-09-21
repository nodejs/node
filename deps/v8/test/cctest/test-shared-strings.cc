// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8-initialization.h"
#include "src/api/api-inl.h"
#include "src/base/strings.h"
#include "src/common/globals.h"
#include "src/heap/factory.h"
#include "src/heap/heap-inl.h"
#include "src/heap/memory-chunk-layout.h"
#include "src/heap/memory-chunk.h"
#include "src/heap/parked-scope.h"
#include "src/heap/remembered-set.h"
#include "src/objects/fixed-array.h"
#include "src/objects/objects-inl.h"
#include "src/objects/string-forwarding-table-inl.h"
#include "test/cctest/cctest.h"
#include "test/cctest/heap/heap-utils.h"

namespace v8 {
namespace internal {
namespace test_shared_strings {

struct V8_NODISCARD IsolateWrapper {
  explicit IsolateWrapper(v8::Isolate* isolate) : isolate(isolate) {}
  ~IsolateWrapper() { isolate->Dispose(); }
  v8::Isolate* const isolate;
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
    {
      i::ParkedScope parked(reinterpret_cast<Isolate*>(isolate_to_park)
                                ->main_thread_local_isolate());
      isolate->Dispose();
    }
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
  }

  ~MultiClientIsolateTest() { main_isolate_->Dispose(); }

  v8::Isolate* main_isolate() const { return main_isolate_; }

  Isolate* i_main_isolate() const {
    return reinterpret_cast<Isolate*>(main_isolate_);
  }

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
};

UNINITIALIZED_TEST(InPlaceInternalizableStringsAreShared) {
  if (FLAG_single_generation) return;
  if (!V8_CAN_CREATE_SHARED_HEAP_BOOL) return;

  FLAG_shared_string_table = true;

  MultiClientIsolateTest test;
  Isolate* i_isolate1 = test.i_main_isolate();
  Factory* factory1 = i_isolate1->factory();

  HandleScope handle_scope(i_isolate1);

  const char raw_one_byte[] = "foo";
  base::uc16 raw_two_byte[] = {2001, 2002, 2003};
  base::Vector<const base::uc16> two_byte(raw_two_byte, 3);

  // Old generation 1- and 2-byte seq strings are in-place internalizable.
  Handle<String> old_one_byte_seq =
      factory1->NewStringFromAsciiChecked(raw_one_byte, AllocationType::kOld);
  CHECK(old_one_byte_seq->InSharedHeap());
  Handle<String> old_two_byte_seq =
      factory1->NewStringFromTwoByte(two_byte, AllocationType::kOld)
          .ToHandleChecked();
  CHECK(old_two_byte_seq->InSharedHeap());

  // Young generation are not internalizable and not shared when sharing the
  // string table.
  Handle<String> young_one_byte_seq =
      factory1->NewStringFromAsciiChecked(raw_one_byte, AllocationType::kYoung);
  CHECK(!young_one_byte_seq->InSharedHeap());
  Handle<String> young_two_byte_seq =
      factory1->NewStringFromTwoByte(two_byte, AllocationType::kYoung)
          .ToHandleChecked();
  CHECK(!young_two_byte_seq->InSharedHeap());

  // Internalized strings are shared.
  uint64_t seed = HashSeed(i_isolate1);
  Handle<String> one_byte_intern = factory1->NewOneByteInternalizedString(
      base::OneByteVector(raw_one_byte),
      StringHasher::HashSequentialString<char>(raw_one_byte, 3, seed));
  CHECK(one_byte_intern->InSharedHeap());
  Handle<String> two_byte_intern = factory1->NewTwoByteInternalizedString(
      two_byte,
      StringHasher::HashSequentialString<uint16_t>(raw_two_byte, 3, seed));
  CHECK(two_byte_intern->InSharedHeap());
}

UNINITIALIZED_TEST(InPlaceInternalization) {
  if (!V8_CAN_CREATE_SHARED_HEAP_BOOL) return;

  FLAG_shared_string_table = true;

  MultiClientIsolateTest test;
  IsolateParkOnDisposeWrapper isolate_wrapper(test.NewClientIsolate(),
                                              test.main_isolate());
  Isolate* i_isolate1 = test.i_main_isolate();
  Factory* factory1 = i_isolate1->factory();
  Isolate* i_isolate2 = reinterpret_cast<Isolate*>(isolate_wrapper.isolate);
  Factory* factory2 = i_isolate2->factory();

  HandleScope scope1(i_isolate1);
  HandleScope scope2(i_isolate2);

  const char raw_one_byte[] = "foo";
  base::uc16 raw_two_byte[] = {2001, 2002, 2003};
  base::Vector<const base::uc16> two_byte(raw_two_byte, 3);

  // Allocate two in-place internalizable strings in isolate1 then intern
  // them.
  Handle<String> old_one_byte_seq1 =
      factory1->NewStringFromAsciiChecked(raw_one_byte, AllocationType::kOld);
  Handle<String> old_two_byte_seq1 =
      factory1->NewStringFromTwoByte(two_byte, AllocationType::kOld)
          .ToHandleChecked();
  Handle<String> one_byte_intern1 =
      factory1->InternalizeString(old_one_byte_seq1);
  Handle<String> two_byte_intern1 =
      factory1->InternalizeString(old_two_byte_seq1);
  CHECK(old_one_byte_seq1->InSharedHeap());
  CHECK(old_two_byte_seq1->InSharedHeap());
  CHECK(one_byte_intern1->InSharedHeap());
  CHECK(two_byte_intern1->InSharedHeap());
  CHECK(old_one_byte_seq1.equals(one_byte_intern1));
  CHECK(old_two_byte_seq1.equals(two_byte_intern1));
  CHECK_EQ(*old_one_byte_seq1, *one_byte_intern1);
  CHECK_EQ(*old_two_byte_seq1, *two_byte_intern1);

  // Allocate two in-place internalizable strings with the same contents in
  // isolate2 then intern them. They should be the same as the interned strings
  // from isolate1.
  Handle<String> old_one_byte_seq2 =
      factory2->NewStringFromAsciiChecked(raw_one_byte, AllocationType::kOld);
  Handle<String> old_two_byte_seq2 =
      factory2->NewStringFromTwoByte(two_byte, AllocationType::kOld)
          .ToHandleChecked();
  Handle<String> one_byte_intern2 =
      factory2->InternalizeString(old_one_byte_seq2);
  Handle<String> two_byte_intern2 =
      factory2->InternalizeString(old_two_byte_seq2);
  CHECK(old_one_byte_seq2->InSharedHeap());
  CHECK(old_two_byte_seq2->InSharedHeap());
  CHECK(one_byte_intern2->InSharedHeap());
  CHECK(two_byte_intern2->InSharedHeap());
  CHECK(!old_one_byte_seq2.equals(one_byte_intern2));
  CHECK(!old_two_byte_seq2.equals(two_byte_intern2));
  CHECK_NE(*old_one_byte_seq2, *one_byte_intern2);
  CHECK_NE(*old_two_byte_seq2, *two_byte_intern2);
  CHECK_EQ(*one_byte_intern1, *one_byte_intern2);
  CHECK_EQ(*two_byte_intern1, *two_byte_intern2);
}

UNINITIALIZED_TEST(YoungInternalization) {
  if (FLAG_single_generation) return;
  if (!V8_CAN_CREATE_SHARED_HEAP_BOOL) return;

  FLAG_shared_string_table = true;

  MultiClientIsolateTest test;
  IsolateParkOnDisposeWrapper isolate_wrapper(test.NewClientIsolate(),
                                              test.main_isolate());
  Isolate* i_isolate1 = test.i_main_isolate();
  Factory* factory1 = i_isolate1->factory();
  Isolate* i_isolate2 = reinterpret_cast<Isolate*>(isolate_wrapper.isolate);
  Factory* factory2 = i_isolate2->factory();

  HandleScope scope1(i_isolate1);
  HandleScope scope2(i_isolate2);

  const char raw_one_byte[] = "foo";
  base::uc16 raw_two_byte[] = {2001, 2002, 2003};
  base::Vector<const base::uc16> two_byte(raw_two_byte, 3);

  // Allocate two young strings in isolate1 then intern them. Young strings
  // aren't in-place internalizable and are copied when internalized.
  Handle<String> young_one_byte_seq1 =
      factory1->NewStringFromAsciiChecked(raw_one_byte, AllocationType::kYoung);
  Handle<String> young_two_byte_seq1 =
      factory1->NewStringFromTwoByte(two_byte, AllocationType::kYoung)
          .ToHandleChecked();
  Handle<String> one_byte_intern1 =
      factory1->InternalizeString(young_one_byte_seq1);
  Handle<String> two_byte_intern1 =
      factory1->InternalizeString(young_two_byte_seq1);
  CHECK(!young_one_byte_seq1->InSharedHeap());
  CHECK(!young_two_byte_seq1->InSharedHeap());
  CHECK(one_byte_intern1->InSharedHeap());
  CHECK(two_byte_intern1->InSharedHeap());
  CHECK(!young_one_byte_seq1.equals(one_byte_intern1));
  CHECK(!young_two_byte_seq1.equals(two_byte_intern1));
  CHECK_NE(*young_one_byte_seq1, *one_byte_intern1);
  CHECK_NE(*young_two_byte_seq1, *two_byte_intern1);

  // Allocate two young strings with the same contents in isolate2 then intern
  // them. They should be the same as the interned strings from isolate1.
  Handle<String> young_one_byte_seq2 =
      factory2->NewStringFromAsciiChecked(raw_one_byte, AllocationType::kYoung);
  Handle<String> young_two_byte_seq2 =
      factory2->NewStringFromTwoByte(two_byte, AllocationType::kYoung)
          .ToHandleChecked();
  Handle<String> one_byte_intern2 =
      factory2->InternalizeString(young_one_byte_seq2);
  Handle<String> two_byte_intern2 =
      factory2->InternalizeString(young_two_byte_seq2);
  CHECK(!young_one_byte_seq2.equals(one_byte_intern2));
  CHECK(!young_two_byte_seq2.equals(two_byte_intern2));
  CHECK_NE(*young_one_byte_seq2, *one_byte_intern2);
  CHECK_NE(*young_two_byte_seq2, *two_byte_intern2);
  CHECK_EQ(*one_byte_intern1, *one_byte_intern2);
  CHECK_EQ(*two_byte_intern1, *two_byte_intern2);
}

class ConcurrentStringThreadBase : public v8::base::Thread {
 public:
  ConcurrentStringThreadBase(const char* name, MultiClientIsolateTest* test,
                             Handle<FixedArray> shared_strings,
                             ParkingSemaphore* sema_ready,
                             ParkingSemaphore* sema_execute_start,
                             ParkingSemaphore* sema_execute_complete)
      : v8::base::Thread(base::Thread::Options(name)),
        test_(test),
        shared_strings_(shared_strings),
        sema_ready_(sema_ready),
        sema_execute_start_(sema_execute_start),
        sema_execute_complete_(sema_execute_complete) {}

  virtual void Setup() {}
  virtual void RunForString(Handle<String> string, int counter) = 0;
  virtual void Teardown() {}
  void Run() override {
    IsolateWrapper isolate_wrapper(test_->NewClientIsolate());
    i_isolate = reinterpret_cast<Isolate*>(isolate_wrapper.isolate);

    Setup();

    sema_ready_->Signal();
    sema_execute_start_->ParkedWait(i_isolate->main_thread_local_isolate());

    {
      HandleScope scope(i_isolate);
      for (int i = 0; i < shared_strings_->length(); i++) {
        Handle<String> input_string(String::cast(shared_strings_->get(i)),
                                    i_isolate);
        RunForString(input_string, i);
      }
    }

    sema_execute_complete_->Signal();

    Teardown();

    i_isolate = nullptr;
  }

  void ParkedJoin(const ParkedScope& scope) {
    USE(scope);
    Join();
  }

 protected:
  using base::Thread::Join;

  Isolate* i_isolate;
  MultiClientIsolateTest* test_;
  Handle<FixedArray> shared_strings_;
  ParkingSemaphore* sema_ready_;
  ParkingSemaphore* sema_execute_start_;
  ParkingSemaphore* sema_execute_complete_;
};

enum TestHitOrMiss { kTestMiss, kTestHit };

class ConcurrentInternalizationThread final
    : public ConcurrentStringThreadBase {
 public:
  ConcurrentInternalizationThread(MultiClientIsolateTest* test,
                                  Handle<FixedArray> shared_strings,
                                  TestHitOrMiss hit_or_miss,
                                  ParkingSemaphore* sema_ready,
                                  ParkingSemaphore* sema_execute_start,
                                  ParkingSemaphore* sema_execute_complete)
      : ConcurrentStringThreadBase("ConcurrentInternalizationThread", test,
                                   shared_strings, sema_ready,
                                   sema_execute_start, sema_execute_complete),
        hit_or_miss_(hit_or_miss) {}

  void Setup() override { factory = i_isolate->factory(); }

  void RunForString(Handle<String> input_string, int counter) override {
    CHECK(input_string->IsShared());
    Handle<String> interned = factory->InternalizeString(input_string);
    CHECK(interned->IsShared());
    CHECK(interned->IsInternalizedString());
    if (hit_or_miss_ == kTestMiss) {
      CHECK_EQ(*input_string, *interned);
    } else {
      CHECK(input_string->HasForwardingIndex(kAcquireLoad));
      CHECK(String::Equals(i_isolate, input_string, interned));
    }
  }

 private:
  TestHitOrMiss hit_or_miss_;
  Factory* factory;
};

namespace {

Handle<FixedArray> CreateSharedOneByteStrings(Isolate* isolate,
                                              Factory* factory, int count,
                                              int min_length = 2,
                                              bool internalize = false) {
  Handle<FixedArray> shared_strings =
      factory->NewFixedArray(count, AllocationType::kSharedOld);
  for (int i = 0; i < count; i++) {
    char* ascii = new char[i + min_length + 1];
    // Don't make single character strings, which will end up deduplicating to
    // an RO string and mess up the string table hit test.
    for (int j = 0; j < i + min_length; j++) ascii[j] = 'a';
    ascii[i + min_length] = '\0';
    if (internalize) {
      // When testing concurrent string table hits, pre-internalize a string of
      // the same contents so all subsequent internalizations are hits.
      factory->InternalizeString(factory->NewStringFromAsciiChecked(ascii));
    }
    Handle<String> string = String::Share(
        isolate,
        factory->NewStringFromAsciiChecked(ascii, AllocationType::kOld));
    CHECK(string->IsShared());
    string->EnsureHash();
    shared_strings->set(i, *string);
    delete[] ascii;
  }
  return shared_strings;
}

void TestConcurrentInternalization(TestHitOrMiss hit_or_miss) {
  if (!V8_CAN_CREATE_SHARED_HEAP_BOOL) return;

  FLAG_shared_string_table = true;

  constexpr int kThreads = 4;
  constexpr int kStrings = 4096;

  MultiClientIsolateTest test;
  Isolate* i_isolate = test.i_main_isolate();
  Factory* factory = i_isolate->factory();

  HandleScope scope(i_isolate);

  Handle<FixedArray> shared_strings = CreateSharedOneByteStrings(
      i_isolate, factory, kStrings, 2, hit_or_miss == kTestHit);

  ParkingSemaphore sema_ready(0);
  ParkingSemaphore sema_execute_start(0);
  ParkingSemaphore sema_execute_complete(0);
  std::vector<std::unique_ptr<ConcurrentInternalizationThread>> threads;
  for (int i = 0; i < kThreads; i++) {
    auto thread = std::make_unique<ConcurrentInternalizationThread>(
        &test, shared_strings, hit_or_miss, &sema_ready, &sema_execute_start,
        &sema_execute_complete);
    CHECK(thread->Start());
    threads.push_back(std::move(thread));
  }

  LocalIsolate* local_isolate = i_isolate->main_thread_local_isolate();
  for (int i = 0; i < kThreads; i++) {
    sema_ready.ParkedWait(local_isolate);
  }
  for (int i = 0; i < kThreads; i++) {
    sema_execute_start.Signal();
  }
  for (int i = 0; i < kThreads; i++) {
    sema_execute_complete.ParkedWait(local_isolate);
  }

  ParkedScope parked(local_isolate);
  for (auto& thread : threads) {
    thread->ParkedJoin(parked);
  }
}
}  // namespace

UNINITIALIZED_TEST(ConcurrentInternalizationMiss) {
  TestConcurrentInternalization(kTestMiss);
}

UNINITIALIZED_TEST(ConcurrentInternalizationHit) {
  TestConcurrentInternalization(kTestHit);
}

class ConcurrentStringTableLookupThread final
    : public ConcurrentStringThreadBase {
 public:
  ConcurrentStringTableLookupThread(MultiClientIsolateTest* test,
                                    Handle<FixedArray> shared_strings,
                                    ParkingSemaphore* sema_ready,
                                    ParkingSemaphore* sema_execute_start,
                                    ParkingSemaphore* sema_execute_complete)
      : ConcurrentStringThreadBase("ConcurrentStringTableLookup", test,
                                   shared_strings, sema_ready,
                                   sema_execute_start, sema_execute_complete) {}

  void RunForString(Handle<String> input_string, int counter) override {
    CHECK(input_string->IsShared());
    Object result = Object(StringTable::TryStringToIndexOrLookupExisting(
        i_isolate, input_string->ptr()));
    if (result.IsString()) {
      String internalized = String::cast(result);
      CHECK(internalized.IsInternalizedString());
      CHECK_IMPLIES(input_string->IsInternalizedString(),
                    *input_string == internalized);
    } else {
      CHECK_EQ(Smi::cast(result).value(), ResultSentinel::kNotFound);
    }
  }
};

UNINITIALIZED_TEST(ConcurrentStringTableLookup) {
  if (!V8_CAN_CREATE_SHARED_HEAP_BOOL) return;

  FLAG_shared_string_table = true;

  constexpr int kTotalThreads = 4;
  constexpr int kInternalizationThreads = 1;
  constexpr int kStrings = 4096;

  MultiClientIsolateTest test;
  Isolate* i_isolate = test.i_main_isolate();
  Factory* factory = i_isolate->factory();

  HandleScope scope(i_isolate);

  Handle<FixedArray> shared_strings =
      CreateSharedOneByteStrings(i_isolate, factory, kStrings, 2, false);

  ParkingSemaphore sema_ready(0);
  ParkingSemaphore sema_execute_start(0);
  ParkingSemaphore sema_execute_complete(0);
  std::vector<std::unique_ptr<ConcurrentStringThreadBase>> threads;
  for (int i = 0; i < kInternalizationThreads; i++) {
    auto thread = std::make_unique<ConcurrentInternalizationThread>(
        &test, shared_strings, kTestMiss, &sema_ready, &sema_execute_start,
        &sema_execute_complete);
    CHECK(thread->Start());
    threads.push_back(std::move(thread));
  }
  for (int i = 0; i < kTotalThreads - kInternalizationThreads; i++) {
    auto thread = std::make_unique<ConcurrentStringTableLookupThread>(
        &test, shared_strings, &sema_ready, &sema_execute_start,
        &sema_execute_complete);
    CHECK(thread->Start());
    threads.push_back(std::move(thread));
  }

  LocalIsolate* local_isolate = i_isolate->main_thread_local_isolate();
  for (int i = 0; i < kTotalThreads; i++) {
    sema_ready.ParkedWait(local_isolate);
  }
  for (int i = 0; i < kTotalThreads; i++) {
    sema_execute_start.Signal();
  }
  for (int i = 0; i < kTotalThreads; i++) {
    sema_execute_complete.ParkedWait(local_isolate);
  }

  ParkedScope parked(local_isolate);
  for (auto& thread : threads) {
    thread->ParkedJoin(parked);
  }
}

namespace {

void CheckSharedStringIsEqualCopy(Handle<String> shared,
                                  Handle<String> original) {
  CHECK(shared->IsShared());
  CHECK(shared->Equals(*original));
  CHECK_NE(*shared, *original);
}

Handle<String> ShareAndVerify(Isolate* isolate, Handle<String> string) {
  Handle<String> shared = String::Share(isolate, string);
  CHECK(shared->IsShared());
#ifdef VERIFY_HEAP
  shared->ObjectVerify(isolate);
  string->ObjectVerify(isolate);
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
  TwoByteResource* CreateTwoByte(const uint16_t* data, size_t length,
                                 bool copy = true) {
    TwoByteResource* res = new TwoByteResource(data, length);
    Register(res);
    return res;
  }
  TwoByteResource* CreateTwoByte(base::Vector<base::uc16> vector,
                                 bool copy = true) {
    auto vec = copy ? vector.Clone() : vector;
    return CreateTwoByte(vec.begin(), vec.size(), copy);
  }
  void Register(OneByteResource* res) { one_byte_resources_.push_back(res); }
  void Register(TwoByteResource* res) { two_byte_resources_.push_back(res); }

 private:
  std::vector<OneByteResource*> one_byte_resources_;
  std::vector<TwoByteResource*> two_byte_resources_;
};

}  // namespace

UNINITIALIZED_TEST(StringShare) {
  if (!V8_CAN_CREATE_SHARED_HEAP_BOOL) return;

  FLAG_shared_string_table = true;

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

  {
    // Old-generation sequential strings are shared in-place.
    Handle<String> one_byte_seq =
        factory->NewStringFromAsciiChecked(raw_one_byte, AllocationType::kOld);
    Handle<String> two_byte_seq =
        factory->NewStringFromTwoByte(two_byte, AllocationType::kOld)
            .ToHandleChecked();
    CHECK(!one_byte_seq->IsShared());
    CHECK(!two_byte_seq->IsShared());
    Handle<String> shared_one_byte = ShareAndVerify(i_isolate, one_byte_seq);
    Handle<String> shared_two_byte = ShareAndVerify(i_isolate, two_byte_seq);
    CHECK_EQ(*one_byte_seq, *shared_one_byte);
    CHECK_EQ(*two_byte_seq, *shared_two_byte);
  }

  {
    // Internalized strings are always shared.
    Handle<String> one_byte_seq =
        factory->NewStringFromAsciiChecked(raw_one_byte, AllocationType::kOld);
    Handle<String> two_byte_seq =
        factory->NewStringFromTwoByte(two_byte, AllocationType::kOld)
            .ToHandleChecked();
    CHECK(!one_byte_seq->IsShared());
    CHECK(!two_byte_seq->IsShared());
    Handle<String> one_byte_intern = factory->InternalizeString(one_byte_seq);
    Handle<String> two_byte_intern = factory->InternalizeString(two_byte_seq);
    CHECK(one_byte_intern->IsShared());
    CHECK(two_byte_intern->IsShared());
    Handle<String> shared_one_byte_intern =
        ShareAndVerify(i_isolate, one_byte_intern);
    Handle<String> shared_two_byte_intern =
        ShareAndVerify(i_isolate, two_byte_intern);
    CHECK_EQ(*one_byte_intern, *shared_one_byte_intern);
    CHECK_EQ(*two_byte_intern, *shared_two_byte_intern);
  }

  {
    // Old-generation external strings are shared in-place.
    Handle<String> one_byte_ext =
        factory->NewStringFromAsciiChecked(raw_one_byte, AllocationType::kOld);
    Handle<String> two_byte_ext =
        factory->NewStringFromTwoByte(two_byte, AllocationType::kOld)
            .ToHandleChecked();
    OneByteResource* one_byte_res =
        resource_factory.CreateOneByte(raw_one_byte);
    TwoByteResource* two_byte_res = resource_factory.CreateTwoByte(two_byte);
    CHECK(one_byte_ext->MakeExternal(one_byte_res));
    CHECK(two_byte_ext->MakeExternal(two_byte_res));
    if (FLAG_always_use_string_forwarding_table) {
      i_isolate->heap()->CollectSharedGarbage(
          GarbageCollectionReason::kTesting);
    }
    CHECK(one_byte_ext->IsExternalString());
    CHECK(two_byte_ext->IsExternalString());
    CHECK(!one_byte_ext->IsShared());
    CHECK(!two_byte_ext->IsShared());
    Handle<String> shared_one_byte = ShareAndVerify(i_isolate, one_byte_ext);
    Handle<String> shared_two_byte = ShareAndVerify(i_isolate, two_byte_ext);
    CHECK_EQ(*one_byte_ext, *shared_one_byte);
    CHECK_EQ(*two_byte_ext, *shared_two_byte);
  }

  // All other strings are flattened then copied if the flatten didn't already
  // create a new copy.

  if (!FLAG_single_generation) {
    // Young strings
    Handle<String> young_one_byte_seq = factory->NewStringFromAsciiChecked(
        raw_one_byte, AllocationType::kYoung);
    Handle<String> young_two_byte_seq =
        factory->NewStringFromTwoByte(two_byte, AllocationType::kYoung)
            .ToHandleChecked();
    CHECK(Heap::InYoungGeneration(*young_one_byte_seq));
    CHECK(Heap::InYoungGeneration(*young_two_byte_seq));
    CHECK(!young_one_byte_seq->IsShared());
    CHECK(!young_two_byte_seq->IsShared());
    Handle<String> shared_one_byte =
        ShareAndVerify(i_isolate, young_one_byte_seq);
    Handle<String> shared_two_byte =
        ShareAndVerify(i_isolate, young_two_byte_seq);
    CheckSharedStringIsEqualCopy(shared_one_byte, young_one_byte_seq);
    CheckSharedStringIsEqualCopy(shared_two_byte, young_two_byte_seq);
  }

  if (!FLAG_always_use_string_forwarding_table) {
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
    Handle<String> shared = ShareAndVerify(i_isolate, one_byte_seq2);
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
    CHECK(cons->IsConsString());
    Handle<String> shared = ShareAndVerify(i_isolate, cons);
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
    CHECK(sliced->IsSlicedString());
    Handle<String> shared = ShareAndVerify(i_isolate, sliced);
    CheckSharedStringIsEqualCopy(shared, sliced);
  }
}

UNINITIALIZED_TEST(PromotionMarkCompact) {
  if (FLAG_single_generation) return;
  if (!V8_CAN_CREATE_SHARED_HEAP_BOOL) return;

  FLAG_stress_concurrent_allocation = false;  // For SealCurrentObjects.
  FLAG_shared_string_table = true;
  FLAG_manual_evacuation_candidates_selection = true;

  MultiClientIsolateTest test;
  Isolate* i_isolate = test.i_main_isolate();
  Factory* factory = i_isolate->factory();
  Heap* heap = i_isolate->heap();
  // Heap* shared_heap = test.i_shared_isolate()->heap();

  const char raw_one_byte[] = "foo";

  {
    HandleScope scope(i_isolate);

    // heap::SealCurrentObjects(heap);
    // heap::SealCurrentObjects(shared_heap);

    Handle<String> one_byte_seq = factory->NewStringFromAsciiChecked(
        raw_one_byte, AllocationType::kYoung);

    CHECK(String::IsInPlaceInternalizable(*one_byte_seq));
    CHECK(heap->InSpace(*one_byte_seq, NEW_SPACE));

    // 1st GC moves `one_byte_seq` to old space and 2nd GC evacuates it within
    // old space.
    CcTest::CollectAllGarbage(i_isolate);
    heap::ForceEvacuationCandidate(i::Page::FromHeapObject(*one_byte_seq));
    CcTest::CollectAllGarbage(i_isolate);

    // In-place-internalizable strings are promoted into the shared heap when
    // sharing.
    CHECK(!heap->Contains(*one_byte_seq));
    CHECK(heap->SharedHeapContains(*one_byte_seq));
  }
}

UNINITIALIZED_TEST(PromotionScavenge) {
  if (FLAG_minor_mc) return;
  if (FLAG_single_generation) return;
  if (!V8_CAN_CREATE_SHARED_HEAP_BOOL) return;

  FLAG_stress_concurrent_allocation = false;  // For SealCurrentObjects.
  FLAG_shared_string_table = true;

  MultiClientIsolateTest test;
  Isolate* i_isolate = test.i_main_isolate();
  Factory* factory = i_isolate->factory();
  Heap* heap = i_isolate->heap();
  // Heap* shared_heap = test.i_shared_isolate()->heap();

  const char raw_one_byte[] = "foo";

  {
    HandleScope scope(i_isolate);

    // heap::SealCurrentObjects(heap);
    // heap::SealCurrentObjects(shared_heap);

    Handle<String> one_byte_seq = factory->NewStringFromAsciiChecked(
        raw_one_byte, AllocationType::kYoung);

    CHECK(String::IsInPlaceInternalizable(*one_byte_seq));
    CHECK(heap->InSpace(*one_byte_seq, NEW_SPACE));

    for (int i = 0; i < 2; i++) {
      CcTest::CollectGarbage(NEW_SPACE, i_isolate);
    }

    // In-place-internalizable strings are promoted into the shared heap when
    // sharing.
    CHECK(!heap->Contains(*one_byte_seq));
    CHECK(heap->SharedHeapContains(*one_byte_seq));
  }
}

UNINITIALIZED_TEST(PromotionScavengeOldToShared) {
  if (FLAG_minor_mc) {
    // Promoting from new space directly to shared heap is not implemented in
    // MinorMC.
    return;
  }
  if (FLAG_single_generation) return;
  if (!V8_CAN_CREATE_SHARED_HEAP_BOOL) return;
  if (FLAG_stress_concurrent_allocation) return;

  FLAG_shared_string_table = true;

  MultiClientIsolateTest test;
  Isolate* i_isolate = test.i_main_isolate();
  Factory* factory = i_isolate->factory();
  Heap* heap = i_isolate->heap();
  ManualGCScope manual_gc(i_isolate);

  const char raw_one_byte[] = "foo";

  {
    HandleScope scope(i_isolate);

    Handle<FixedArray> old_object =
        factory->NewFixedArray(1, AllocationType::kOld);
    MemoryChunk* old_object_chunk = MemoryChunk::FromHeapObject(*old_object);
    CHECK(!old_object_chunk->InYoungGeneration());

    Handle<String> one_byte_seq = factory->NewStringFromAsciiChecked(
        raw_one_byte, AllocationType::kYoung);
    CHECK(String::IsInPlaceInternalizable(*one_byte_seq));
    CHECK(MemoryChunk::FromHeapObject(*one_byte_seq)->InYoungGeneration());

    old_object->set(0, *one_byte_seq);
    ObjectSlot slot = old_object->GetFirstElementAddress();
    CHECK(
        RememberedSet<OLD_TO_NEW>::Contains(old_object_chunk, slot.address()));

    for (int i = 0; i < 2; i++) {
      CcTest::CollectGarbage(NEW_SPACE, i_isolate);
    }

    // In-place-internalizable strings are promoted into the shared heap when
    // sharing.
    CHECK(!heap->Contains(*one_byte_seq));
    CHECK(heap->SharedHeapContains(*one_byte_seq));

    // Since the GC promoted that string into shared heap, it also needs to
    // create an OLD_TO_SHARED slot.
    CHECK(RememberedSet<OLD_TO_SHARED>::Contains(old_object_chunk,
                                                 slot.address()));
  }
}

UNINITIALIZED_TEST(PromotionMarkCompactNewToShared) {
  if (FLAG_single_generation) return;
  if (!V8_CAN_CREATE_SHARED_HEAP_BOOL) return;
  if (FLAG_stress_concurrent_allocation) return;

  FLAG_shared_string_table = true;
  FLAG_manual_evacuation_candidates_selection = true;
  FLAG_page_promotion = false;

  MultiClientIsolateTest test;
  Isolate* i_isolate = test.i_main_isolate();
  Factory* factory = i_isolate->factory();
  Heap* heap = i_isolate->heap();
  ManualGCScope manual_gc(i_isolate);

  const char raw_one_byte[] = "foo";

  {
    HandleScope scope(i_isolate);

    Handle<FixedArray> old_object =
        factory->NewFixedArray(1, AllocationType::kOld);
    MemoryChunk* old_object_chunk = MemoryChunk::FromHeapObject(*old_object);
    CHECK(!old_object_chunk->InYoungGeneration());

    Handle<String> one_byte_seq = factory->NewStringFromAsciiChecked(
        raw_one_byte, AllocationType::kYoung);
    CHECK(String::IsInPlaceInternalizable(*one_byte_seq));
    CHECK(MemoryChunk::FromHeapObject(*one_byte_seq)->InYoungGeneration());

    old_object->set(0, *one_byte_seq);
    ObjectSlot slot = old_object->GetFirstElementAddress();
    CHECK(
        RememberedSet<OLD_TO_NEW>::Contains(old_object_chunk, slot.address()));

    CcTest::CollectGarbage(OLD_SPACE, i_isolate);

    // In-place-internalizable strings are promoted into the shared heap when
    // sharing.
    CHECK(!heap->Contains(*one_byte_seq));
    CHECK(heap->SharedHeapContains(*one_byte_seq));

    // Since the GC promoted that string into shared heap, it also needs to
    // create an OLD_TO_SHARED slot.
    CHECK(RememberedSet<OLD_TO_SHARED>::Contains(old_object_chunk,
                                                 slot.address()));
  }
}

UNINITIALIZED_TEST(PromotionMarkCompactOldToShared) {
  if (!V8_CAN_CREATE_SHARED_HEAP_BOOL) return;
  if (FLAG_stress_concurrent_allocation) return;
  if (!FLAG_page_promotion) return;

  FLAG_shared_string_table = true;
  FLAG_manual_evacuation_candidates_selection = true;

  MultiClientIsolateTest test;
  Isolate* i_isolate = test.i_main_isolate();
  Factory* factory = i_isolate->factory();
  Heap* heap = i_isolate->heap();
  ManualGCScope manual_gc(i_isolate);

  const char raw_one_byte[] = "foo";

  {
    HandleScope scope(i_isolate);

    Handle<FixedArray> old_object =
        factory->NewFixedArray(1, AllocationType::kOld);
    MemoryChunk* old_object_chunk = MemoryChunk::FromHeapObject(*old_object);
    CHECK(!old_object_chunk->InYoungGeneration());

    Handle<String> one_byte_seq = factory->NewStringFromAsciiChecked(
        raw_one_byte, AllocationType::kYoung);
    CHECK(String::IsInPlaceInternalizable(*one_byte_seq));
    CHECK(MemoryChunk::FromHeapObject(*one_byte_seq)->InYoungGeneration());

    std::vector<Handle<FixedArray>> handles;
    // Fill the page and do a full GC. Page promotion should kick in and promote
    // the page as is to old space.
    heap::FillCurrentPage(heap->new_space(), &handles);
    heap->CollectGarbage(OLD_SPACE, GarbageCollectionReason::kTesting);
    // Make sure 'one_byte_seq' is in old space.
    CHECK(!MemoryChunk::FromHeapObject(*one_byte_seq)->InYoungGeneration());
    CHECK(heap->Contains(*one_byte_seq));

    old_object->set(0, *one_byte_seq);
    ObjectSlot slot = old_object->GetFirstElementAddress();
    CHECK(
        !RememberedSet<OLD_TO_NEW>::Contains(old_object_chunk, slot.address()));

    heap::ForceEvacuationCandidate(Page::FromHeapObject(*one_byte_seq));
    heap->CollectGarbage(OLD_SPACE, GarbageCollectionReason::kTesting);

    // In-place-internalizable strings are promoted into the shared heap when
    // sharing.
    CHECK(!heap->Contains(*one_byte_seq));
    CHECK(heap->SharedHeapContains(*one_byte_seq));

    // Since the GC promoted that string into shared heap, it also needs to
    // create an OLD_TO_SHARED slot.
    CHECK(RememberedSet<OLD_TO_SHARED>::Contains(old_object_chunk,
                                                 slot.address()));
  }
}

UNINITIALIZED_TEST(PagePromotionRecordingOldToShared) {
  if (FLAG_single_generation) return;
  if (!V8_CAN_CREATE_SHARED_HEAP_BOOL) return;
  if (FLAG_stress_concurrent_allocation) return;

  FLAG_shared_string_table = true;
  FLAG_manual_evacuation_candidates_selection = true;

  MultiClientIsolateTest test;
  Isolate* i_isolate = test.i_main_isolate();
  Factory* factory = i_isolate->factory();
  Heap* heap = i_isolate->heap();
  ManualGCScope manual_gc(i_isolate);

  const char raw_one_byte[] = "foo";

  {
    HandleScope scope(i_isolate);

    Handle<FixedArray> young_object =
        factory->NewFixedArray(1, AllocationType::kYoung);
    CHECK(Heap::InYoungGeneration(*young_object));
    Address young_object_address = young_object->address();

    std::vector<Handle<FixedArray>> handles;
    // Make the whole page transition from new->old, getting the buffers
    // processed in the sweeper (relying on marking information) instead of
    // processing during newspace evacuation.
    heap::FillCurrentPage(heap->new_space(), &handles);

    Handle<String> shared_string = factory->NewStringFromAsciiChecked(
        raw_one_byte, AllocationType::kSharedOld);
    CHECK(shared_string->InSharedWritableHeap());

    young_object->set(0, *shared_string);

    CcTest::CollectGarbage(OLD_SPACE, i_isolate);

    // Object should get promoted using page promotion, so address should remain
    // the same.
    CHECK(!Heap::InYoungGeneration(*shared_string));
    CHECK_EQ(young_object_address, young_object->address());

    // Since the GC promoted that string into shared heap, it also needs to
    // create an OLD_TO_SHARED slot.
    ObjectSlot slot = young_object->GetFirstElementAddress();
    CHECK(RememberedSet<OLD_TO_SHARED>::Contains(
        MemoryChunk::FromHeapObject(*young_object), slot.address()));
  }
}

UNINITIALIZED_TEST(InternalizedSharedStringsTransitionDuringGC) {
  if (!V8_CAN_CREATE_SHARED_HEAP_BOOL) return;

  FLAG_shared_string_table = true;

  constexpr int kStrings = 4096;

  MultiClientIsolateTest test;
  Isolate* i_isolate = test.i_main_isolate();
  Factory* factory = i_isolate->factory();

  HandleScope scope(i_isolate);

  // Run two times to test that everything is reset correctly during GC.
  for (int run = 0; run < 2; run++) {
    Handle<FixedArray> shared_strings =
        CreateSharedOneByteStrings(i_isolate, factory, kStrings, 2, run == 0);

    // Check strings are in the forwarding table after internalization.
    for (int i = 0; i < shared_strings->length(); i++) {
      Handle<String> input_string(String::cast(shared_strings->get(i)),
                                  i_isolate);
      Handle<String> interned = factory->InternalizeString(input_string);
      CHECK(input_string->IsShared());
      CHECK(!input_string->IsThinString());
      CHECK(input_string->HasForwardingIndex(kAcquireLoad));
      CHECK(String::Equals(i_isolate, input_string, interned));
    }

    // Trigger garbage collection on the shared isolate.
    CcTest::CollectSharedGarbage(i_isolate);

    // Check that GC cleared the forwarding table.
    CHECK_EQ(i_isolate->string_forwarding_table()->size(), 0);

    // Check all strings are transitioned to ThinStrings
    for (int i = 0; i < shared_strings->length(); i++) {
      Handle<String> input_string(String::cast(shared_strings->get(i)),
                                  i_isolate);
      CHECK(input_string->IsThinString());
    }
  }
}

UNINITIALIZED_TEST(ShareExternalString) {
  if (FLAG_single_generation) return;
  if (!V8_CAN_CREATE_SHARED_HEAP_BOOL) return;

  FLAG_shared_string_table = true;

  ExternalResourceFactory resource_factory;
  MultiClientIsolateTest test;
  Isolate* i_isolate1 = test.i_main_isolate();
  Factory* factory1 = i_isolate1->factory();

  HandleScope handle_scope(i_isolate1);

  const char raw_one_byte[] = "external string";

  // External strings in old space can be shared in-place.
  Handle<String> one_byte =
      factory1->NewStringFromAsciiChecked(raw_one_byte, AllocationType::kOld);
  CHECK(!one_byte->IsShared());

  OneByteResource* resource = resource_factory.CreateOneByte(raw_one_byte);
  one_byte->MakeExternal(resource);
  if (FLAG_always_use_string_forwarding_table) {
    i_isolate1->heap()->CollectSharedGarbage(GarbageCollectionReason::kTesting);
  }
  CHECK(one_byte->IsExternalString());
  Handle<ExternalOneByteString> one_byte_external =
      Handle<ExternalOneByteString>::cast(one_byte);
  Handle<String> shared_one_byte =
      ShareAndVerify(i_isolate1, one_byte_external);
  CHECK_EQ(*shared_one_byte, *one_byte);
}

namespace {

void CheckExternalStringResource(
    Handle<String> string, v8::String::ExternalStringResourceBase* resource) {
  const bool is_one_byte = string->IsOneByteRepresentation();
  Local<v8::String> api_string = Utils::ToLocal(string);
  v8::String::Encoding encoding;
  CHECK_EQ(resource, api_string->GetExternalStringResourceBase(&encoding));
  if (is_one_byte) {
    CHECK_EQ(encoding, v8::String::Encoding::ONE_BYTE_ENCODING);
    CHECK_EQ(resource, api_string->GetExternalOneByteStringResource());
  } else {
    CHECK(string->IsTwoByteRepresentation());
    CHECK_EQ(encoding, v8::String::Encoding::TWO_BYTE_ENCODING);
    CHECK_EQ(resource, api_string->GetExternalStringResource());
  }
}

}  // namespace

UNINITIALIZED_TEST(ExternalizeSharedString) {
  if (FLAG_single_generation) return;
  if (!V8_CAN_CREATE_SHARED_HEAP_BOOL) return;

  FLAG_shared_string_table = true;

  ExternalResourceFactory resource_factory;
  MultiClientIsolateTest test;
  Isolate* i_isolate1 = test.i_main_isolate();
  Factory* factory1 = i_isolate1->factory();

  HandleScope handle_scope(i_isolate1);

  const char raw_one_byte[] = "external string";
  base::uc16 raw_two_byte[] = {2001, 2002, 2003};
  base::Vector<base::uc16> two_byte_vec(raw_two_byte, 3);

  Handle<String> one_byte =
      factory1->NewStringFromAsciiChecked(raw_one_byte, AllocationType::kOld);
  Handle<String> two_byte =
      factory1->NewStringFromTwoByte(two_byte_vec, AllocationType::kOld)
          .ToHandleChecked();
  CHECK(one_byte->IsOneByteRepresentation());
  CHECK(two_byte->IsTwoByteRepresentation());
  CHECK(!one_byte->IsShared());
  CHECK(!two_byte->IsShared());

  Handle<String> shared_one_byte = ShareAndVerify(i_isolate1, one_byte);
  Handle<String> shared_two_byte = ShareAndVerify(i_isolate1, two_byte);

  OneByteResource* one_byte_res = resource_factory.CreateOneByte(raw_one_byte);
  TwoByteResource* two_byte_res = resource_factory.CreateTwoByte(two_byte_vec);
  shared_one_byte->MakeExternal(one_byte_res);
  shared_two_byte->MakeExternal(two_byte_res);
  CHECK(!shared_one_byte->IsExternalString());
  CHECK(!shared_two_byte->IsExternalString());
  CHECK(shared_one_byte->HasExternalForwardingIndex(kAcquireLoad));
  CHECK(shared_two_byte->HasExternalForwardingIndex(kAcquireLoad));

  // Check that API calls return the resource from the forwarding table.
  CheckExternalStringResource(shared_one_byte, one_byte_res);
  CheckExternalStringResource(shared_two_byte, two_byte_res);
}

UNINITIALIZED_TEST(ExternalizedSharedStringsTransitionDuringGC) {
  if (!V8_CAN_CREATE_SHARED_HEAP_BOOL) return;

  FLAG_shared_string_table = true;

  ExternalResourceFactory resource_factory;
  MultiClientIsolateTest test;

  constexpr int kStrings = 4096;

  Isolate* i_isolate = test.i_main_isolate();
  Factory* factory = i_isolate->factory();

  HandleScope scope(i_isolate);

  // Run two times to test that everything is reset correctly during GC.
  for (int run = 0; run < 2; run++) {
    Handle<FixedArray> shared_strings = CreateSharedOneByteStrings(
        i_isolate, factory, kStrings, ExternalString::kUncachedSize, run == 0);

    // Check strings are in the forwarding table after internalization.
    for (int i = 0; i < shared_strings->length(); i++) {
      Handle<String> input_string(String::cast(shared_strings->get(i)),
                                  i_isolate);
      const int length = input_string->length();
      char* buffer = new char[length + 1];
      String::WriteToFlat(*input_string, reinterpret_cast<uint8_t*>(buffer), 0,
                          length);
      OneByteResource* resource =
          resource_factory.CreateOneByte(buffer, length, false);
      CHECK(input_string->MakeExternal(resource));
      CHECK(input_string->IsShared());
      CHECK(!input_string->IsExternalString());
      CHECK(input_string->HasExternalForwardingIndex(kAcquireLoad));
    }

    // Trigger garbage collection on the shared isolate.
    i_isolate->heap()->CollectSharedGarbage(GarbageCollectionReason::kTesting);

    // Check that GC cleared the forwarding table.
    CHECK_EQ(i_isolate->string_forwarding_table()->size(), 0);

    // Check all strings are transitioned to ExternalStrings
    for (int i = 0; i < shared_strings->length(); i++) {
      Handle<String> input_string(String::cast(shared_strings->get(i)),
                                  i_isolate);
      CHECK(input_string->IsExternalString());
    }
  }
}

UNINITIALIZED_TEST(ExternalizeInternalizedString) {
  if (FLAG_single_generation) return;
  if (!V8_CAN_CREATE_SHARED_HEAP_BOOL) return;

  FLAG_shared_string_table = true;

  ExternalResourceFactory resource_factory;
  MultiClientIsolateTest test;
  Isolate* i_isolate1 = test.i_main_isolate();
  Factory* factory1 = i_isolate1->factory();

  HandleScope handle_scope(i_isolate1);

  const char raw_one_byte[] = "external string";
  base::uc16 raw_two_byte[] = {2001, 2002, 2003};
  base::Vector<base::uc16> two_byte_vec(raw_two_byte, 3);

  Handle<String> one_byte =
      factory1->NewStringFromAsciiChecked(raw_one_byte, AllocationType::kOld);
  Handle<String> two_byte =
      factory1->NewStringFromTwoByte(two_byte_vec, AllocationType::kOld)
          .ToHandleChecked();
  // Internalize copies, s.t. internalizing the original strings creates a
  // forwarding entry.
  factory1->InternalizeString(
      factory1->NewStringFromAsciiChecked(raw_one_byte));
  factory1->InternalizeString(
      factory1->NewStringFromTwoByte(two_byte_vec).ToHandleChecked());
  Handle<String> one_byte_intern = factory1->InternalizeString(one_byte);
  Handle<String> two_byte_intern = factory1->InternalizeString(two_byte);
  if (FLAG_always_use_string_forwarding_table) {
    i_isolate1->heap()->CollectSharedGarbage(GarbageCollectionReason::kTesting);
  }
  CHECK(one_byte->IsThinString());
  CHECK(two_byte->IsThinString());
  CHECK(one_byte_intern->IsOneByteRepresentation());
  CHECK(two_byte_intern->IsTwoByteRepresentation());
  CHECK(one_byte_intern->IsShared());
  CHECK(two_byte_intern->IsShared());

  uint32_t one_byte_hash = one_byte_intern->hash();
  uint32_t two_byte_hash = two_byte_intern->hash();

  OneByteResource* one_byte_res = resource_factory.CreateOneByte(raw_one_byte);
  TwoByteResource* two_byte_res = resource_factory.CreateTwoByte(two_byte_vec);
  CHECK(one_byte_intern->MakeExternal(one_byte_res));
  CHECK(two_byte_intern->MakeExternal(two_byte_res));
  CHECK(!one_byte_intern->IsExternalString());
  CHECK(!two_byte_intern->IsExternalString());
  CHECK(one_byte_intern->HasExternalForwardingIndex(kAcquireLoad));
  CHECK(two_byte_intern->HasExternalForwardingIndex(kAcquireLoad));
  // The hash of internalized strings is stored in the forwarding table.
  CHECK_EQ(one_byte_intern->hash(), one_byte_hash);
  CHECK_EQ(two_byte_intern->hash(), two_byte_hash);

  // Check that API calls return the resource from the forwarding table.
  CheckExternalStringResource(one_byte_intern, one_byte_res);
  CheckExternalStringResource(two_byte_intern, two_byte_res);

  // API calls to the ThinStrings should also return the correct resource.
  CheckExternalStringResource(one_byte, one_byte_res);
  CheckExternalStringResource(two_byte, two_byte_res);
}

UNINITIALIZED_TEST(InternalizeSharedExternalString) {
  if (FLAG_single_generation) return;
  if (!V8_CAN_CREATE_SHARED_HEAP_BOOL) return;

  FLAG_shared_string_table = true;

  ExternalResourceFactory resource_factory;
  MultiClientIsolateTest test;
  Isolate* i_isolate1 = test.i_main_isolate();
  Factory* factory1 = i_isolate1->factory();

  HandleScope handle_scope(i_isolate1);

  const char raw_one_byte[] = "external string";
  base::uc16 raw_two_byte[] = {2001, 2002, 2003};
  base::Vector<base::uc16> two_byte_vec(raw_two_byte, 3);

  Handle<String> one_byte =
      factory1->NewStringFromAsciiChecked(raw_one_byte, AllocationType::kOld);
  Handle<String> two_byte =
      factory1->NewStringFromTwoByte(two_byte_vec, AllocationType::kOld)
          .ToHandleChecked();

  Handle<String> shared_one_byte = ShareAndVerify(i_isolate1, one_byte);
  Handle<String> shared_two_byte = ShareAndVerify(i_isolate1, two_byte);

  OneByteResource* one_byte_res = resource_factory.CreateOneByte(raw_one_byte);
  TwoByteResource* two_byte_res = resource_factory.CreateTwoByte(two_byte_vec);
  CHECK(shared_one_byte->MakeExternal(one_byte_res));
  CHECK(shared_two_byte->MakeExternal(two_byte_res));
  CHECK(shared_one_byte->HasExternalForwardingIndex(kAcquireLoad));
  CHECK(shared_two_byte->HasExternalForwardingIndex(kAcquireLoad));

  // Trigger GC to externalize the shared string.
  i_isolate1->heap()->CollectSharedGarbage(GarbageCollectionReason::kTesting);
  CHECK(shared_one_byte->IsShared());
  CHECK(shared_one_byte->IsExternalString());
  CHECK(shared_two_byte->IsShared());
  CHECK(shared_two_byte->IsExternalString());

  // Shared cached external strings are in-place internalizable.
  Handle<String> one_byte_intern = factory1->InternalizeString(shared_one_byte);
  CHECK_EQ(*one_byte_intern, *shared_one_byte);
  CHECK(shared_one_byte->IsExternalString());
  CHECK(shared_one_byte->IsInternalizedString());

  // Depending on the architecture/build options the two byte string might be
  // cached or uncached.
  const bool is_uncached =
      two_byte->Size() < ExternalString::kSizeOfAllExternalStrings;

  if (is_uncached) {
    // Shared uncached external strings are not internalizable. A new internal
    // copy will be created.
    Handle<String> two_byte_intern = factory1->InternalizeString(two_byte);
    CHECK_NE(*two_byte_intern, *shared_two_byte);
    CHECK(shared_two_byte->HasInternalizedForwardingIndex(kAcquireLoad));
    CHECK(two_byte_intern->IsInternalizedString());
    CHECK(!two_byte_intern->IsExternalString());
  } else {
    Handle<String> two_byte_intern = factory1->InternalizeString(two_byte);
    CHECK_EQ(*two_byte_intern, *shared_two_byte);
    CHECK(shared_two_byte->IsExternalString());
    CHECK(shared_two_byte->IsInternalizedString());
  }

  // Another GC should create an externalized internalized string of the cached
  // (one byte) string and turn the uncached (two byte) string into a
  // ThinString, disposing the external resource.
  i_isolate1->heap()->CollectSharedGarbage(GarbageCollectionReason::kTesting);
  CHECK_EQ(shared_one_byte->map().instance_type(),
           InstanceType::EXTERNAL_ONE_BYTE_INTERNALIZED_STRING_TYPE);
  if (is_uncached) {
    CHECK(shared_two_byte->IsThinString());
    CHECK(two_byte_res->IsDisposed());
  } else {
    CHECK_EQ(shared_two_byte->map().instance_type(),
             InstanceType::EXTERNAL_INTERNALIZED_STRING_TYPE);
  }
}

UNINITIALIZED_TEST(ExternalizeAndInternalizeMissSharedString) {
  if (FLAG_single_generation) return;
  if (!V8_CAN_CREATE_SHARED_HEAP_BOOL) return;

  FLAG_shared_string_table = true;

  ExternalResourceFactory resource_factory;
  MultiClientIsolateTest test;
  Isolate* i_isolate1 = test.i_main_isolate();
  Factory* factory1 = i_isolate1->factory();

  HandleScope handle_scope(i_isolate1);

  const char raw_one_byte[] = "external string";

  Handle<String> one_byte =
      factory1->NewStringFromAsciiChecked(raw_one_byte, AllocationType::kOld);
  uint32_t one_byte_hash = one_byte->EnsureHash();

  Handle<String> shared_one_byte = ShareAndVerify(i_isolate1, one_byte);

  OneByteResource* one_byte_res = resource_factory.CreateOneByte(raw_one_byte);

  CHECK(shared_one_byte->MakeExternal(one_byte_res));
  CHECK(shared_one_byte->HasExternalForwardingIndex(kAcquireLoad));

  Handle<String> one_byte_intern = factory1->InternalizeString(shared_one_byte);
  CHECK_EQ(*one_byte_intern, *shared_one_byte);
  CHECK(shared_one_byte->IsInternalizedString());
  // Check that we have both, a forwarding index and an accessable hash.
  CHECK(shared_one_byte->HasExternalForwardingIndex(kAcquireLoad));
  CHECK(shared_one_byte->HasHashCode());
  CHECK_EQ(shared_one_byte->hash(), one_byte_hash);
}

UNINITIALIZED_TEST(InternalizeHitAndExternalizeSharedString) {
  if (FLAG_single_generation) return;
  if (!V8_CAN_CREATE_SHARED_HEAP_BOOL) return;

  FLAG_shared_string_table = true;

  ExternalResourceFactory resource_factory;
  MultiClientIsolateTest test;
  Isolate* i_isolate1 = test.i_main_isolate();
  Factory* factory1 = i_isolate1->factory();

  HandleScope handle_scope(i_isolate1);

  const char raw_one_byte[] = "external string";
  base::uc16 raw_two_byte[] = {2001, 2002, 2003};
  base::Vector<base::uc16> two_byte_vec(raw_two_byte, 3);

  Handle<String> one_byte =
      factory1->NewStringFromAsciiChecked(raw_one_byte, AllocationType::kOld);
  Handle<String> two_byte =
      factory1->NewStringFromTwoByte(two_byte_vec, AllocationType::kOld)
          .ToHandleChecked();
  Handle<String> shared_one_byte = ShareAndVerify(i_isolate1, one_byte);
  Handle<String> shared_two_byte = ShareAndVerify(i_isolate1, two_byte);
  // Internalize copies, s.t. internalizing the original strings creates a
  // forwarding entry.
  factory1->InternalizeString(
      factory1->NewStringFromAsciiChecked(raw_one_byte));
  factory1->InternalizeString(
      factory1->NewStringFromTwoByte(two_byte_vec).ToHandleChecked());
  Handle<String> one_byte_intern = factory1->InternalizeString(shared_one_byte);
  Handle<String> two_byte_intern = factory1->InternalizeString(shared_two_byte);
  CHECK_NE(*one_byte_intern, *shared_one_byte);
  CHECK_NE(*two_byte_intern, *shared_two_byte);
  CHECK(String::IsHashFieldComputed(one_byte_intern->raw_hash_field()));
  CHECK(String::IsHashFieldComputed(two_byte_intern->raw_hash_field()));
  CHECK(shared_one_byte->HasInternalizedForwardingIndex(kAcquireLoad));
  CHECK(shared_two_byte->HasInternalizedForwardingIndex(kAcquireLoad));

  OneByteResource* one_byte_res = resource_factory.CreateOneByte(raw_one_byte);
  TwoByteResource* two_byte_res = resource_factory.CreateTwoByte(two_byte_vec);
  CHECK(shared_one_byte->MakeExternal(one_byte_res));
  CHECK(shared_two_byte->MakeExternal(two_byte_res));
  CHECK(shared_one_byte->HasExternalForwardingIndex(kAcquireLoad));
  CHECK(shared_two_byte->HasExternalForwardingIndex(kAcquireLoad));
  CHECK(shared_one_byte->HasInternalizedForwardingIndex(kAcquireLoad));
  CHECK(shared_two_byte->HasInternalizedForwardingIndex(kAcquireLoad));

  // Check that API calls return the resource from the forwarding table.
  CheckExternalStringResource(shared_one_byte, one_byte_res);
  CheckExternalStringResource(shared_two_byte, two_byte_res);
}

UNINITIALIZED_TEST(InternalizeMissAndExternalizeSharedString) {
  if (FLAG_single_generation) return;
  if (!V8_CAN_CREATE_SHARED_HEAP_BOOL) return;

  FLAG_shared_string_table = true;

  ExternalResourceFactory resource_factory;
  MultiClientIsolateTest test;
  Isolate* i_isolate1 = test.i_main_isolate();
  Factory* factory1 = i_isolate1->factory();

  HandleScope handle_scope(i_isolate1);

  const char raw_one_byte[] = "external string";
  base::uc16 raw_two_byte[] = {2001, 2002, 2003};
  base::Vector<base::uc16> two_byte_vec(raw_two_byte, 3);

  Handle<String> one_byte =
      factory1->NewStringFromAsciiChecked(raw_one_byte, AllocationType::kOld);
  Handle<String> two_byte =
      factory1->NewStringFromTwoByte(two_byte_vec, AllocationType::kOld)
          .ToHandleChecked();
  Handle<String> shared_one_byte = ShareAndVerify(i_isolate1, one_byte);
  Handle<String> shared_two_byte = ShareAndVerify(i_isolate1, two_byte);
  Handle<String> one_byte_intern = factory1->InternalizeString(shared_one_byte);
  Handle<String> two_byte_intern = factory1->InternalizeString(shared_two_byte);
  CHECK_EQ(*one_byte_intern, *shared_one_byte);
  CHECK_EQ(*two_byte_intern, *shared_two_byte);
  CHECK(!shared_one_byte->HasInternalizedForwardingIndex(kAcquireLoad));
  CHECK(!shared_two_byte->HasInternalizedForwardingIndex(kAcquireLoad));

  OneByteResource* one_byte_res = resource_factory.CreateOneByte(raw_one_byte);
  TwoByteResource* two_byte_res = resource_factory.CreateTwoByte(two_byte_vec);
  CHECK(shared_one_byte->MakeExternal(one_byte_res));
  CHECK(shared_two_byte->MakeExternal(two_byte_res));
  CHECK(shared_one_byte->HasExternalForwardingIndex(kAcquireLoad));
  CHECK(shared_two_byte->HasExternalForwardingIndex(kAcquireLoad));
  CHECK(one_byte_intern->HasExternalForwardingIndex(kAcquireLoad));
  CHECK(two_byte_intern->HasExternalForwardingIndex(kAcquireLoad));

  // Check that API calls return the resource from the forwarding table.
  CheckExternalStringResource(shared_one_byte, one_byte_res);
  CheckExternalStringResource(shared_two_byte, two_byte_res);
}

class ConcurrentExternalizationThread final
    : public ConcurrentStringThreadBase {
 public:
  ConcurrentExternalizationThread(MultiClientIsolateTest* test,
                                  Handle<FixedArray> shared_strings,
                                  std::vector<OneByteResource*> resources,
                                  bool share_resources,
                                  ParkingSemaphore* sema_ready,
                                  ParkingSemaphore* sema_execute_start,
                                  ParkingSemaphore* sema_execute_complete)
      : ConcurrentStringThreadBase("ConcurrentExternalizationThread", test,
                                   shared_strings, sema_ready,
                                   sema_execute_start, sema_execute_complete),
        resources_(resources),
        share_resources_(share_resources) {}

  void RunForString(Handle<String> input_string, int counter) override {
    CHECK(input_string->IsShared());
    OneByteResource* resource = Resource(counter);
    if (!input_string->MakeExternal(resource)) {
      if (!share_resources_) resource->Dispose();
    }
    CHECK(input_string->HasForwardingIndex(kAcquireLoad));
  }

  OneByteResource* Resource(int index) const { return resources_[index]; }

 private:
  std::vector<OneByteResource*> resources_;
  const bool share_resources_;
};

namespace {

void CreateExternalResources(Isolate* i_isolate, Handle<FixedArray> strings,
                             std::vector<OneByteResource*>& resources,
                             ExternalResourceFactory& resource_factory) {
  resources.reserve(strings->length());
  for (int i = 0; i < strings->length(); i++) {
    Handle<String> input_string(String::cast(strings->get(i)), i_isolate);
    CHECK(Utils::ToLocal(input_string)->CanMakeExternal());
    const int length = input_string->length();
    char* buffer = new char[length + 1];
    String::WriteToFlat(*input_string, reinterpret_cast<uint8_t*>(buffer), 0,
                        length);
    resources.push_back(resource_factory.CreateOneByte(buffer, length, false));
  }
}

}  // namespace

void TestConcurrentExternalization(bool share_resources) {
  if (!V8_CAN_CREATE_SHARED_HEAP_BOOL) return;

  FLAG_shared_string_table = true;

  ExternalResourceFactory resource_factory;
  MultiClientIsolateTest test;

  constexpr int kThreads = 4;
  constexpr int kStrings = 4096;

  Isolate* i_isolate = test.i_main_isolate();
  Factory* factory = i_isolate->factory();

  HandleScope scope(i_isolate);

  Handle<FixedArray> shared_strings = CreateSharedOneByteStrings(
      i_isolate, factory, kStrings, ExternalString::kUncachedSize, false);

  ParkingSemaphore sema_ready(0);
  ParkingSemaphore sema_execute_start(0);
  ParkingSemaphore sema_execute_complete(0);
  std::vector<std::unique_ptr<ConcurrentExternalizationThread>> threads;
  std::vector<OneByteResource*> shared_resources;

  if (share_resources) {
    CreateExternalResources(i_isolate, shared_strings, shared_resources,
                            resource_factory);
  }

  for (int i = 0; i < kThreads; i++) {
    std::vector<OneByteResource*> local_resources;
    if (share_resources) {
      local_resources = shared_resources;
    } else {
      CreateExternalResources(i_isolate, shared_strings, local_resources,
                              resource_factory);
    }
    auto thread = std::make_unique<ConcurrentExternalizationThread>(
        &test, shared_strings, local_resources, share_resources, &sema_ready,
        &sema_execute_start, &sema_execute_complete);
    CHECK(thread->Start());
    threads.push_back(std::move(thread));
  }

  LocalIsolate* local_isolate = i_isolate->main_thread_local_isolate();
  for (int i = 0; i < kThreads; i++) {
    sema_ready.ParkedWait(local_isolate);
  }
  for (int i = 0; i < kThreads; i++) {
    sema_execute_start.Signal();
  }
  for (int i = 0; i < kThreads; i++) {
    sema_execute_complete.ParkedWait(local_isolate);
  }

  i_isolate->heap()->CollectSharedGarbage(GarbageCollectionReason::kTesting);

  for (int i = 0; i < shared_strings->length(); i++) {
    Handle<String> input_string(String::cast(shared_strings->get(i)),
                                i_isolate);
    String string = *input_string;
    CHECK(string.IsExternalString());
    int alive_resources = 0;
    for (int t = 0; t < kThreads; t++) {
      ConcurrentExternalizationThread* thread = threads[t].get();
      if (!thread->Resource(i)->IsDisposed()) {
        alive_resources++;
      }
    }

    if (share_resources) {
      // Since we share the same resource for all threads, we accounted for it
      // in every thread.
      CHECK_EQ(alive_resources, kThreads);
    } else {
      // Check that exaclty one resource is alive.
      CHECK_EQ(alive_resources, 1);
    }
  }

  ParkedScope parked(local_isolate);
  for (auto& thread : threads) {
    thread->ParkedJoin(parked);
  }
}

UNINITIALIZED_TEST(ConcurrentExternalizationWithUniqueResources) {
  TestConcurrentExternalization(false);
}

UNINITIALIZED_TEST(ConcurrentExternalizationWithSharedResources) {
  TestConcurrentExternalization(true);
}

void TestConcurrentExternalizationAndInternalization(
    TestHitOrMiss hit_or_miss) {
  if (!V8_CAN_CREATE_SHARED_HEAP_BOOL) return;

  FLAG_shared_string_table = true;

  ExternalResourceFactory resource_factory;
  MultiClientIsolateTest test;

  constexpr int kInternalizationThreads = 4;
  constexpr int kExternalizationThreads = 4;
  constexpr int kTotalThreads =
      kInternalizationThreads + kExternalizationThreads;
  constexpr int kStrings = 4096;

  Isolate* i_isolate = test.i_main_isolate();
  Factory* factory = i_isolate->factory();

  HandleScope scope(i_isolate);

  Handle<FixedArray> shared_strings = CreateSharedOneByteStrings(
      i_isolate, factory, kStrings, ExternalString::kUncachedSize,
      hit_or_miss == kTestHit);

  ParkingSemaphore sema_ready(0);
  ParkingSemaphore sema_execute_start(0);
  ParkingSemaphore sema_execute_complete(0);
  std::vector<std::unique_ptr<ConcurrentStringThreadBase>> threads;
  for (int i = 0; i < kInternalizationThreads; i++) {
    auto thread = std::make_unique<ConcurrentInternalizationThread>(
        &test, shared_strings, hit_or_miss, &sema_ready, &sema_execute_start,
        &sema_execute_complete);
    CHECK(thread->Start());
    threads.push_back(std::move(thread));
  }
  for (int i = 0; i < kExternalizationThreads; i++) {
    std::vector<OneByteResource*> resources;
    CreateExternalResources(i_isolate, shared_strings, resources,
                            resource_factory);
    auto thread = std::make_unique<ConcurrentExternalizationThread>(
        &test, shared_strings, resources, false, &sema_ready,
        &sema_execute_start, &sema_execute_complete);
    CHECK(thread->Start());
    threads.push_back(std::move(thread));
  }

  LocalIsolate* local_isolate = i_isolate->main_thread_local_isolate();
  for (int i = 0; i < kTotalThreads; i++) {
    sema_ready.ParkedWait(local_isolate);
  }
  for (int i = 0; i < kTotalThreads; i++) {
    sema_execute_start.Signal();
  }
  for (int i = 0; i < kTotalThreads; i++) {
    sema_execute_complete.ParkedWait(local_isolate);
  }

  i_isolate->heap()->CollectSharedGarbage(GarbageCollectionReason::kTesting);

  for (int i = 0; i < shared_strings->length(); i++) {
    Handle<String> input_string(String::cast(shared_strings->get(i)),
                                i_isolate);
    String string = *input_string;
    if (hit_or_miss == kTestHit) {
      CHECK(string.IsThinString());
      string = ThinString::cast(string).actual();
    }
    int alive_resources = 0;
    for (int t = kInternalizationThreads; t < kTotalThreads; t++) {
      ConcurrentExternalizationThread* thread =
          reinterpret_cast<ConcurrentExternalizationThread*>(threads[t].get());
      if (!thread->Resource(i)->IsDisposed()) {
        alive_resources++;
      }
    }

    StringShape shape(string);
    CHECK(shape.IsInternalized());
    // Check at most one external resource is alive.
    // If internalization happens on an external string and we already have an
    // internalized string with the same content, we turn it into a ThinString
    // and dispose the resource.
    CHECK_LE(alive_resources, 1);
    CHECK_EQ(shape.IsExternal(), alive_resources);
    CHECK(string.HasHashCode());
  }

  ParkedScope parked(local_isolate);
  for (auto& thread : threads) {
    thread->ParkedJoin(parked);
  }
}

UNINITIALIZED_TEST(ConcurrentExternalizationAndInternalizationMiss) {
  TestConcurrentExternalizationAndInternalization(kTestMiss);
}

UNINITIALIZED_TEST(ConcurrentExternalizationAndInternalizationHit) {
  TestConcurrentExternalizationAndInternalization(kTestHit);
}

}  // namespace test_shared_strings
}  // namespace internal
}  // namespace v8
