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
#include "src/heap/parked-scope.h"
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
                         base::Semaphore* sema_started)
      : v8::base::Thread(base::Thread::Options("ThreadWithLocalHeap")),
        heap_(heap),
        handles_(std::move(handles)),
        ph_(std::move(ph)),
        sema_started_(sema_started) {}

  void Run() override {
    LocalHeap local_heap(heap_, ThreadKind::kBackground, std::move(ph_));
    UnparkedScope unparked_scope(&local_heap);
    LocalHandleScope scope(&local_heap);

    for (int i = 0; i < kNumHandles; i++) {
      handles_.push_back(local_heap.NewPersistentHandle(handles_[0]));
    }

    sema_started_->Signal();

    for (Handle<JSObject> js_obj : handles_) {
      // Walk up the prototype chain all the way to the top.
      Handle<Map> map(js_obj->synchronized_map(), &local_heap);
      while (!map->prototype().IsNull()) {
        Handle<Map> map_prototype_map(map->prototype().synchronized_map(),
                                      &local_heap);
        if (!map_prototype_map->IsJSObjectMap()) {
          break;
        }
        map = map_prototype_map;
      }
    }

    CHECK_EQ(handles_.size(), kNumHandles * 2);
  }

 private:
  Heap* heap_;
  std::vector<Handle<JSObject>> handles_;
  std::unique_ptr<PersistentHandles> ph_;
  base::Semaphore* sema_started_;
};

// Test to search on a background thread, while the main thread is idle.
TEST(ProtoWalkBackground) {
  heap::EnsureFlagLocalHeapsEnabled();
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();

  std::unique_ptr<PersistentHandles> ph = isolate->NewPersistentHandles();
  std::vector<Handle<JSObject>> handles;

  auto factory = isolate->factory();
  HandleScope handle_scope(isolate);

  Handle<JSFunction> function =
      factory->NewFunctionForTesting(factory->empty_string());
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

  base::Semaphore sema_started(0);

  // Pass persistent handles to background thread.
  std::unique_ptr<ConcurrentSearchThread> thread(new ConcurrentSearchThread(
      isolate->heap(), std::move(handles), std::move(ph), &sema_started));
  CHECK(thread->Start());

  sema_started.Wait();

  thread->Join();
}

// Test to search on a background thread, while the main thread modifies the
// descriptor array.
TEST(ProtoWalkBackground_DescriptorArrayWrite) {
  heap::EnsureFlagLocalHeapsEnabled();
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();

  std::unique_ptr<PersistentHandles> ph = isolate->NewPersistentHandles();
  std::vector<Handle<JSObject>> handles;

  auto factory = isolate->factory();
  HandleScope handle_scope(isolate);

  Handle<JSFunction> function =
      factory->NewFunctionForTesting(factory->empty_string());
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

  base::Semaphore sema_started(0);

  // Pass persistent handles to background thread.
  std::unique_ptr<ConcurrentSearchThread> thread(new ConcurrentSearchThread(
      isolate->heap(), std::move(handles), std::move(ph), &sema_started));
  CHECK(thread->Start());

  sema_started.Wait();

  // Exercise descriptor array.
  for (int i = 0; i < 20; ++i) {
    Handle<String> filler_name = CcTest::MakeName("filler_property_", i);
    Handle<Object> filler_value = CcTest::MakeString("dummy_value");
    JSObject::DefinePropertyOrElementIgnoreAttributes(js_object, filler_name,
                                                      filler_value, NONE)
        .Check();
  }

  thread->Join();
}

TEST(ProtoWalkBackground_PrototypeChainWrite) {
  heap::EnsureFlagLocalHeapsEnabled();
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();

  std::unique_ptr<PersistentHandles> ph = isolate->NewPersistentHandles();
  std::vector<Handle<JSObject>> handles;

  auto factory = isolate->factory();
  HandleScope handle_scope(isolate);

  Handle<JSFunction> function =
      factory->NewFunctionForTesting(factory->empty_string());
  Handle<JSObject> js_object = factory->NewJSObject(function);

  for (int i = 0; i < kNumHandles; i++) {
    handles.push_back(ph->NewHandle(js_object));
  }

  base::Semaphore sema_started(0);

  // Pass persistent handles to background thread.
  std::unique_ptr<ConcurrentSearchThread> thread(new ConcurrentSearchThread(
      isolate->heap(), std::move(handles), std::move(ph), &sema_started));
  CHECK(thread->Start());

  // The prototype chain looks like this JSObject -> Object -> null. Change the
  // prototype of the js_object to be JSObject -> null, and then back a bunch of
  // times.
  Handle<Map> map(js_object->map(), isolate);
  Handle<HeapObject> old_proto(map->prototype(), isolate);
  DCHECK(!old_proto->IsNull());
  Handle<HeapObject> new_proto(old_proto->map().prototype(), isolate);

  sema_started.Wait();

  for (int i = 0; i < 20; ++i) {
    CHECK(JSReceiver::SetPrototype(
              js_object, i % 2 == 0 ? new_proto : old_proto, false, kDontThrow)
              .FromJust());
  }

  thread->Join();
}

}  // anonymous namespace

}  // namespace internal
}  // namespace v8
