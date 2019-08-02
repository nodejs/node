// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TEST_CCTEST_TEST_API_H_
#define V8_TEST_CCTEST_TEST_API_H_

#include "src/init/v8.h"

#include "src/api/api.h"
#include "src/execution/isolate.h"
#include "src/execution/vm-state.h"
#include "test/cctest/cctest.h"

template <typename T>
static void CheckReturnValue(const T& t, i::Address callback) {
  v8::ReturnValue<v8::Value> rv = t.GetReturnValue();
  i::FullObjectSlot o(*reinterpret_cast<i::Address*>(&rv));
  CHECK_EQ(CcTest::isolate(), t.GetIsolate());
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(t.GetIsolate());
  CHECK_EQ(t.GetIsolate(), rv.GetIsolate());
  CHECK((*o).IsTheHole(isolate) || (*o).IsUndefined(isolate));
  // Verify reset
  bool is_runtime = (*o).IsTheHole(isolate);
  if (is_runtime) {
    CHECK(rv.Get()->IsUndefined());
  } else {
    i::Handle<i::Object> v = v8::Utils::OpenHandle(*rv.Get());
    CHECK_EQ(*v, *o);
  }
  rv.Set(true);
  CHECK(!(*o).IsTheHole(isolate) && !(*o).IsUndefined(isolate));
  rv.Set(v8::Local<v8::Object>());
  CHECK((*o).IsTheHole(isolate) || (*o).IsUndefined(isolate));
  CHECK_EQ(is_runtime, (*o).IsTheHole(isolate));
  // If CPU profiler is active check that when API callback is invoked
  // VMState is set to EXTERNAL.
  if (isolate->is_profiling()) {
    CHECK_EQ(v8::EXTERNAL, isolate->current_vm_state());
    CHECK(isolate->external_callback_scope());
    CHECK_EQ(callback, isolate->external_callback_scope()->callback());
  }
}

#endif  // V8_TEST_CCTEST_TEST_API_H_
