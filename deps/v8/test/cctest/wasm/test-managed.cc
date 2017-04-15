// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "src/wasm/managed.h"

#include "test/cctest/cctest.h"
#include "test/common/wasm/test-signatures.h"

using namespace v8::base;
using namespace v8::internal;

class DeleteRecorder {
 public:
  explicit DeleteRecorder(bool* deleted) : deleted_(deleted) {
    *deleted_ = false;
  }
  ~DeleteRecorder() { *deleted_ = true; }

 private:
  bool* deleted_;
};

TEST(ManagedCollect) {
  Isolate* isolate = CcTest::InitIsolateOnce();
  bool deleted = false;
  DeleteRecorder* d = new DeleteRecorder(&deleted);

  {
    HandleScope scope(isolate);
    auto handle = Managed<DeleteRecorder>::New(isolate, d);
    USE(handle);
  }

  CcTest::CollectAllAvailableGarbage();

  CHECK(deleted);
}

TEST(ManagedCollectNoDelete) {
  Isolate* isolate = CcTest::InitIsolateOnce();
  bool deleted = false;
  DeleteRecorder* d = new DeleteRecorder(&deleted);

  {
    HandleScope scope(isolate);
    auto handle = Managed<DeleteRecorder>::New(isolate, d, false);
    USE(handle);
  }

  CcTest::CollectAllAvailableGarbage();

  CHECK(!deleted);
  delete d;
}
