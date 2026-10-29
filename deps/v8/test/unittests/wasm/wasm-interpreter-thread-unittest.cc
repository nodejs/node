// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef V8_ENABLE_DRUMBRAKE

#include "src/wasm/interpreter/wasm-interpreter.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8::internal::wasm {

using WasmInterpreterThreadMapTest = TestWithIsolate;

TEST_F(WasmInterpreterThreadMapTest, PerIsolateThread) {
  Isolate* isolate_a = isolate();

  IsolateWrapper wrapper_b(kNoCounters, false);
  Isolate* isolate_b = wrapper_b.i_isolate();

  WasmInterpreterThreadMap map;

  WasmInterpreterThread* thread_a = map.GetCurrentInterpreterThread(isolate_a);
  ASSERT_NE(nullptr, thread_a);
  EXPECT_EQ(isolate_a, thread_a->GetIsolate());

  WasmInterpreterThread* thread_b;
  {
    v8::Isolate::Scope scope_b(wrapper_b.isolate());
    thread_b = map.GetCurrentInterpreterThread(isolate_b);
    ASSERT_NE(nullptr, thread_b);
    EXPECT_EQ(isolate_b, thread_b->GetIsolate());
    EXPECT_NE(thread_a, thread_b);
  }

  EXPECT_EQ(thread_a, map.GetCurrentInterpreterThread(isolate_a));

  // Removing entries for one isolate must not affect entries owned by another
  // isolate on the same OS thread.
  map.NotifyIsolateDisposal(isolate_a);
  {
    v8::Isolate::Scope scope_b(wrapper_b.isolate());
    EXPECT_EQ(thread_b, map.GetCurrentInterpreterThread(isolate_b));
    map.NotifyIsolateDisposal(isolate_b);
  }
}

}  // namespace v8::internal::wasm

#endif  // V8_ENABLE_DRUMBRAKE
