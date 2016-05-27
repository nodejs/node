// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/isolate.h"
#include "src/profiler/cpu-profiler.h"
#include "src/vm-state.h"
#include "test/cctest/cctest.h"

template <typename T>
static void CheckReturnValue(const T& t, i::Address callback) {
  v8::ReturnValue<v8::Value> rv = t.GetReturnValue();
  i::Object** o = *reinterpret_cast<i::Object***>(&rv);
  CHECK_EQ(CcTest::isolate(), t.GetIsolate());
  CHECK_EQ(t.GetIsolate(), rv.GetIsolate());
  CHECK((*o)->IsTheHole() || (*o)->IsUndefined());
  // Verify reset
  bool is_runtime = (*o)->IsTheHole();
  if (is_runtime) {
    CHECK(rv.Get()->IsUndefined());
  } else {
    i::Handle<i::Object> v = v8::Utils::OpenHandle(*rv.Get());
    CHECK_EQ(*v, *o);
  }
  rv.Set(true);
  CHECK(!(*o)->IsTheHole() && !(*o)->IsUndefined());
  rv.Set(v8::Local<v8::Object>());
  CHECK((*o)->IsTheHole() || (*o)->IsUndefined());
  CHECK_EQ(is_runtime, (*o)->IsTheHole());
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(t.GetIsolate());
  // If CPU profiler is active check that when API callback is invoked
  // VMState is set to EXTERNAL.
  if (isolate->cpu_profiler()->is_profiling()) {
    CHECK_EQ(v8::EXTERNAL, isolate->current_vm_state());
    CHECK(isolate->external_callback_scope());
    CHECK_EQ(callback, isolate->external_callback_scope()->callback());
  }
}
