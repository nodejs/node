// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "src/managed.h"

#include "src/objects-inl.h"
#include "test/cctest/cctest.h"

using namespace v8::base;
using namespace v8::internal;

class DeleteRecorder {
 public:
  explicit DeleteRecorder(bool* deleted) : deleted_(deleted) {
    *deleted_ = false;
  }
  ~DeleteRecorder() { *deleted_ = true; }
  static void Deleter(void* value) {
    delete reinterpret_cast<DeleteRecorder*>(value);
  }

 private:
  bool* deleted_;
};

TEST(ManagedCollect) {
  Isolate* isolate = CcTest::InitIsolateOnce();
  bool deleted1 = false;
  bool deleted2 = false;
  DeleteRecorder* d1 = new DeleteRecorder(&deleted1);
  DeleteRecorder* d2 = new DeleteRecorder(&deleted2);
  Isolate::ManagedObjectFinalizer* finalizer =
      isolate->RegisterForReleaseAtTeardown(d2, DeleteRecorder::Deleter);
  {
    HandleScope scope(isolate);
    auto handle = Managed<DeleteRecorder>::New(isolate, d1);
    USE(handle);
  }

  CcTest::CollectAllAvailableGarbage();

  CHECK(deleted1);
  CHECK(!deleted2);
  isolate->UnregisterFromReleaseAtTeardown(&finalizer);
  CHECK_NULL(finalizer);
  delete d2;
  CHECK(deleted2);
}

TEST(DisposeCollect) {
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator =
      CcTest::InitIsolateOnce()->array_buffer_allocator();

  v8::Isolate* isolate = v8::Isolate::New(create_params);
  isolate->Enter();
  Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  bool deleted1 = false;
  bool deleted2 = false;
  DeleteRecorder* d1 = new DeleteRecorder(&deleted1);
  DeleteRecorder* d2 = new DeleteRecorder(&deleted2);
  {
    HandleScope scope(i_isolate);
    auto handle = Managed<DeleteRecorder>::New(i_isolate, d1);
    USE(handle);
  }
  i_isolate->RegisterForReleaseAtTeardown(d2, DeleteRecorder::Deleter);

  isolate->Exit();
  isolate->Dispose();
  CHECK(deleted1);
  CHECK(deleted2);
}
