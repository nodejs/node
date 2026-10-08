// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8-script.h"
#include "src/execution/isolate.h"
#include "src/flags/flags.h"
#include "src/heap/local-heap.h"
#include "test/unittests/heap/heap-utils.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8::internal {

using MinimalStackTest = TestWithHeapInternals;

TEST_F(MinimalStackTest, MinimalStackInTurbofanAllocate) {
  CrashKeyStore crash_key_store(i_isolate());
  v8::HandleScope handle_scope(v8_isolate());
  v8_flags.allow_natives_syntax = true;
  v8_flags.lazy_feedback_allocation = false;
  v8_flags.stress_concurrent_allocation = false;
  v8_flags.enable_lazy_source_positions = true;

  v8::Local<v8::Context> context = v8::Context::New(v8_isolate());
  v8::Context::Scope context_scope(context);

  // Set up the run() method and ensure it is compiled by Turbofan.
  const char* script_src_part1 =
      "function run(x) {\n"
      "  return x + 1.1;\n"
      "}\n"
      "%PrepareFunctionForOptimization(run);\n"
      "run(1);\n"
      "run(2);\n"
      "%OptimizeFunctionOnNextCall(run);\n"
      "run(3);\n";

  // This code should have only one allocation: the HeapNumber returned from `x
  // + 1.1`.
  const char* script_src_part2 = "run(4);\n";

  v8::Local<v8::String> source_part1 =
      v8::String::NewFromUtf8(v8_isolate(), script_src_part1).ToLocalChecked();
  v8::ScriptOrigin origin_part1(
      v8::String::NewFromUtf8(v8_isolate(), "test_part1.js").ToLocalChecked());
  v8::Local<v8::Script> script_part1 =
      v8::Script::Compile(context, source_part1, &origin_part1)
          .ToLocalChecked();
  script_part1->Run(context).ToLocalChecked();

  v8::Local<v8::String> source_part2 =
      v8::String::NewFromUtf8(v8_isolate(), script_src_part2).ToLocalChecked();
  v8::ScriptOrigin origin_part2(
      v8::String::NewFromUtf8(v8_isolate(), "test_part2.js").ToLocalChecked());
  v8::Local<v8::Script> script_part2 =
      v8::Script::Compile(context, source_part2, &origin_part2)
          .ToLocalChecked();

  // Disable inline allocation so that allocating the HeapNumber triggers the
  // slow path.
  v8_flags.inline_new = false;
  i_isolate()->heap()->DisableInlineAllocation();

  // Register a GC epilogue callback to trigger Isolate::BuildMinimalStack().
  auto gc_epilogue_callback = [](void* data) {
    auto* isolate = static_cast<i::Isolate*>(data);
    isolate->heap()->set_force_gc_on_next_allocation(false);
    isolate->ReportStackAsCrashKey();
  };
  auto* local_heap = i_isolate()->heap()->main_thread_local_heap();
  local_heap->AddGCEpilogueCallback(gc_epilogue_callback, i_isolate());

  // Force a GC on the HeapNumber allocation to trigger the GC epilogue
  // callback.
  i_isolate()->heap()->set_force_gc_on_next_allocation(true);
  if (i_isolate()->heap()->new_space()) {
    SimulateFullSpace(i_isolate()->heap()->new_space());
  } else {
    SimulateFullSpace(i_isolate()->heap()->old_space());
  }

  v8::Local<v8::Value> result = script_part2->Run(context).ToLocalChecked();
  local_heap->RemoveGCEpilogueCallback(gc_epilogue_callback, i_isolate());

  // Just for sanity.
  ASSERT_TRUE(result->IsNumber());
  EXPECT_DOUBLE_EQ(5.1, result.As<v8::Number>()->Value());

  // Assert that the run() method is reported on the stack trace.
  EXPECT_TRUE(crash_key_store.HasKey("v8-oom-stack"));
  std::string output = crash_key_store.ValueForKey("v8-oom-stack");
  const char* expected_output =
      "run in test_part1.js\n"
      "<none> in test_part2.js\n"
      "$\n";
  EXPECT_EQ(output, expected_output);
}

}  // namespace v8::internal
