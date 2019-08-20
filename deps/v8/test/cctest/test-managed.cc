// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "src/objects/managed.h"

#include "src/objects/objects-inl.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {

class DeleteCounter {
 public:
  explicit DeleteCounter(int* deleted) : deleted_(deleted) { *deleted_ = 0; }
  ~DeleteCounter() { (*deleted_)++; }
  static void Deleter(void* arg) {
    delete reinterpret_cast<DeleteCounter*>(arg);
  }

 private:
  int* deleted_;
};

TEST(GCCausesDestruction) {
  Isolate* isolate = CcTest::InitIsolateOnce();
  int deleted1 = 0;
  int deleted2 = 0;
  DeleteCounter* d1 = new DeleteCounter(&deleted1);
  DeleteCounter* d2 = new DeleteCounter(&deleted2);
  {
    HandleScope scope(isolate);
    auto handle = Managed<DeleteCounter>::FromRawPtr(isolate, 0, d1);
    USE(handle);
  }

  CcTest::CollectAllAvailableGarbage();

  CHECK_EQ(1, deleted1);
  CHECK_EQ(0, deleted2);
  delete d2;
  CHECK_EQ(1, deleted2);
}

TEST(DisposeCausesDestruction1) {
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator =
      CcTest::InitIsolateOnce()->array_buffer_allocator();

  v8::Isolate* isolate = v8::Isolate::New(create_params);
  Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  isolate->Enter();
  int deleted1 = 0;
  DeleteCounter* d1 = new DeleteCounter(&deleted1);
  {
    HandleScope scope(i_isolate);
    auto handle = Managed<DeleteCounter>::FromRawPtr(i_isolate, 0, d1);
    USE(handle);
  }
  isolate->Exit();
  isolate->Dispose();
  CHECK_EQ(1, deleted1);
}

TEST(DisposeCausesDestruction2) {
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator =
      CcTest::InitIsolateOnce()->array_buffer_allocator();

  v8::Isolate* isolate = v8::Isolate::New(create_params);
  Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  isolate->Enter();
  int deleted1 = 0;
  int deleted2 = 0;
  DeleteCounter* d1 = new DeleteCounter(&deleted1);
  DeleteCounter* d2 = new DeleteCounter(&deleted2);
  {
    HandleScope scope(i_isolate);
    auto handle = Managed<DeleteCounter>::FromRawPtr(i_isolate, 0, d1);
    USE(handle);
  }
  ManagedPtrDestructor* destructor =
      new ManagedPtrDestructor(0, d2, DeleteCounter::Deleter);
  i_isolate->RegisterManagedPtrDestructor(destructor);

  isolate->Exit();
  isolate->Dispose();
  CHECK_EQ(1, deleted1);
  CHECK_EQ(1, deleted2);
}

TEST(DisposeWithAnotherSharedPtr) {
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator =
      CcTest::InitIsolateOnce()->array_buffer_allocator();

  v8::Isolate* isolate = v8::Isolate::New(create_params);
  Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  isolate->Enter();
  int deleted1 = 0;
  DeleteCounter* d1 = new DeleteCounter(&deleted1);
  {
    std::shared_ptr<DeleteCounter> shared1(d1);
    {
      HandleScope scope(i_isolate);
      auto handle =
          Managed<DeleteCounter>::FromSharedPtr(i_isolate, 0, shared1);
      USE(handle);
    }
    isolate->Exit();
    isolate->Dispose();
    CHECK_EQ(0, deleted1);
  }
  // Should be deleted after the second shared pointer is destroyed.
  CHECK_EQ(1, deleted1);
}

TEST(DisposeAcrossIsolates) {
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator =
      CcTest::InitIsolateOnce()->array_buffer_allocator();

  int deleted = 0;
  DeleteCounter* delete_counter = new DeleteCounter(&deleted);

  v8::Isolate* isolate1 = v8::Isolate::New(create_params);
  Isolate* i_isolate1 = reinterpret_cast<i::Isolate*>(isolate1);
  isolate1->Enter();
  {
    HandleScope scope1(i_isolate1);
    auto handle1 =
        Managed<DeleteCounter>::FromRawPtr(i_isolate1, 0, delete_counter);

    v8::Isolate* isolate2 = v8::Isolate::New(create_params);
    Isolate* i_isolate2 = reinterpret_cast<i::Isolate*>(isolate2);
    isolate2->Enter();
    {
      HandleScope scope(i_isolate2);
      auto handle2 =
          Managed<DeleteCounter>::FromSharedPtr(i_isolate2, 0, handle1->get());
      USE(handle2);
    }
    isolate2->Exit();
    isolate2->Dispose();
    CHECK_EQ(0, deleted);
  }
  // Should be deleted after the first isolate is destroyed.
  isolate1->Exit();
  isolate1->Dispose();
  CHECK_EQ(1, deleted);
}

TEST(CollectAcrossIsolates) {
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator =
      CcTest::InitIsolateOnce()->array_buffer_allocator();

  int deleted = 0;
  DeleteCounter* delete_counter = new DeleteCounter(&deleted);

  v8::Isolate* isolate1 = v8::Isolate::New(create_params);
  Isolate* i_isolate1 = reinterpret_cast<i::Isolate*>(isolate1);
  isolate1->Enter();
  {
    HandleScope scope1(i_isolate1);
    auto handle1 =
        Managed<DeleteCounter>::FromRawPtr(i_isolate1, 0, delete_counter);

    v8::Isolate* isolate2 = v8::Isolate::New(create_params);
    Isolate* i_isolate2 = reinterpret_cast<i::Isolate*>(isolate2);
    isolate2->Enter();
    {
      HandleScope scope(i_isolate2);
      auto handle2 =
          Managed<DeleteCounter>::FromSharedPtr(i_isolate2, 0, handle1->get());
      USE(handle2);
    }
    i_isolate2->heap()->CollectAllAvailableGarbage(
        i::GarbageCollectionReason::kTesting);
    CHECK_EQ(0, deleted);
    isolate2->Exit();
    isolate2->Dispose();
    CHECK_EQ(0, deleted);
  }
  // Should be deleted after the first isolate is destroyed.
  i_isolate1->heap()->CollectAllAvailableGarbage(
      i::GarbageCollectionReason::kTesting);
  CHECK_EQ(1, deleted);
  isolate1->Exit();
  isolate1->Dispose();
  CHECK_EQ(1, deleted);
}

}  // namespace internal
}  // namespace v8
