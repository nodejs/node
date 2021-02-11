// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/api/api.h"
#include "src/base/platform/semaphore.h"
#include "src/handles/handles-inl.h"
#include "src/handles/local-handles-inl.h"
#include "src/handles/persistent-handles.h"
#include "src/heap/heap.h"
#include "src/heap/local-heap-inl.h"
#include "src/heap/local-heap.h"
#include "test/cctest/cctest.h"
#include "test/cctest/heap/heap-utils.h"

namespace v8 {
namespace internal {

static constexpr int kNumHandles = kHandleBlockSize * 2 + kHandleBlockSize / 2;

namespace {

class ConcurrentSearchThread final : public v8::base::Thread {
 public:
  ConcurrentSearchThread(Heap* heap, std::vector<Handle<JSObject>> handles,
                         std::unique_ptr<PersistentHandles> ph,
                         Handle<Name> name, base::Semaphore* sema_started)
      : v8::base::Thread(base::Thread::Options("ThreadWithLocalHeap")),
        heap_(heap),
        handles_(std::move(handles)),
        ph_(std::move(ph)),
        name_(name),
        sema_started_(sema_started) {}

  void Run() override {
    LocalHeap local_heap(heap_, ThreadKind::kBackground, std::move(ph_));
    UnparkedScope unparked_scope(&local_heap);
    LocalHandleScope scope(&local_heap);

    for (int i = 0; i < kNumHandles; i++) {
      handles_.push_back(local_heap.NewPersistentHandle(handles_[0]));
    }

    sema_started_->Signal();

    for (Handle<JSObject> handle : handles_) {
      // Lookup the named property on the {map}.
      CHECK(name_->IsUniqueName());
      Handle<Map> map(handle->map(), &local_heap);

      Handle<DescriptorArray> descriptors(
          map->instance_descriptors(kAcquireLoad), &local_heap);
      bool is_background_thread = true;
      InternalIndex const number =
          descriptors->Search(*name_, *map, is_background_thread);
      CHECK(number.is_found());
    }

    CHECK_EQ(handles_.size(), kNumHandles * 2);
  }

 private:
  Heap* heap_;
  std::vector<Handle<JSObject>> handles_;
  std::unique_ptr<PersistentHandles> ph_;
  Handle<Name> name_;
  base::Semaphore* sema_started_;
};

// Uses linear search on a flat object, with up to 8 elements.
TEST(LinearSearchFlatObject) {
  heap::EnsureFlagLocalHeapsEnabled();
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();

  std::unique_ptr<PersistentHandles> ph = isolate->NewPersistentHandles();
  std::vector<Handle<JSObject>> handles;

  auto factory = isolate->factory();
  HandleScope handle_scope(isolate);

  Handle<JSFunction> function =
      factory->NewFunctionForTest(factory->empty_string());
  Handle<JSObject> js_object = factory->NewJSObject(function);
  Handle<String> name = CcTest::MakeString("property");
  Handle<Object> value = CcTest::MakeString("dummy_value");
  // For the default constructor function no in-object properties are reserved
  // hence adding a single property will initialize the property-array.
  JSObject::DefinePropertyOrElementIgnoreAttributes(js_object, name, value,
                                                    NONE)
      .Check();

  for (int i = 0; i < kNumHandles; i++) {
    handles.push_back(ph->NewHandle(js_object));
  }

  Handle<Name> persistent_name = ph->NewHandle(name);

  base::Semaphore sema_started(0);

  // Pass persistent handles to background thread.
  std::unique_ptr<ConcurrentSearchThread> thread(new ConcurrentSearchThread(
      isolate->heap(), std::move(handles), std::move(ph), persistent_name,
      &sema_started));
  CHECK(thread->Start());

  sema_started.Wait();

  // Exercise descriptor in main thread too.
  for (int i = 0; i < 7; ++i) {
    Handle<String> filler_name = CcTest::MakeName("filler_property_", i);
    Handle<Object> filler_value = CcTest::MakeString("dummy_value");
    JSObject::DefinePropertyOrElementIgnoreAttributes(js_object, filler_name,
                                                      filler_value, NONE)
        .Check();
  }
  CHECK_EQ(js_object->map().NumberOfOwnDescriptors(), 8);

  thread->Join();
}

// Uses linear search on a flat object, which has more than 8 elements.
TEST(LinearSearchFlatObject_ManyElements) {
  heap::EnsureFlagLocalHeapsEnabled();
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();

  std::unique_ptr<PersistentHandles> ph = isolate->NewPersistentHandles();
  std::vector<Handle<JSObject>> handles;

  auto factory = isolate->factory();
  HandleScope handle_scope(isolate);

  Handle<JSFunction> function =
      factory->NewFunctionForTest(factory->empty_string());
  Handle<JSObject> js_object = factory->NewJSObject(function);
  Handle<String> name = CcTest::MakeString("property");
  Handle<Object> value = CcTest::MakeString("dummy_value");
  // For the default constructor function no in-object properties are reserved
  // hence adding a single property will initialize the property-array.
  JSObject::DefinePropertyOrElementIgnoreAttributes(js_object, name, value,
                                                    NONE)
      .Check();

  // If we have more than 8 properties we would do a binary search. However,
  // since we are going search in a background thread, we force a linear search
  // that is safe to do in the background.
  for (int i = 0; i < 10; ++i) {
    Handle<String> filler_name = CcTest::MakeName("filler_property_", i);
    Handle<Object> filler_value = CcTest::MakeString("dummy_value");
    JSObject::DefinePropertyOrElementIgnoreAttributes(js_object, filler_name,
                                                      filler_value, NONE)
        .Check();
  }
  CHECK_GT(js_object->map().NumberOfOwnDescriptors(), 8);

  for (int i = 0; i < kNumHandles; i++) {
    handles.push_back(ph->NewHandle(js_object));
  }

  Handle<Name> persistent_name = ph->NewHandle(name);

  base::Semaphore sema_started(0);

  // Pass persistent handles to background thread.
  std::unique_ptr<ConcurrentSearchThread> thread(new ConcurrentSearchThread(
      isolate->heap(), std::move(handles), std::move(ph), persistent_name,
      &sema_started));
  CHECK(thread->Start());

  sema_started.Wait();

  // Exercise descriptor in main thread too.
  for (int i = 10; i < 20; ++i) {
    Handle<String> filler_name = CcTest::MakeName("filler_property_", i);
    Handle<Object> filler_value = CcTest::MakeString("dummy_value");
    JSObject::DefinePropertyOrElementIgnoreAttributes(js_object, filler_name,
                                                      filler_value, NONE)
        .Check();
  }

  thread->Join();
}

}  // anonymous namespace

}  // namespace internal
}  // namespace v8
