// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/execution/isolate.h"
#include "src/objects/bytecode-array-inl.h"
#include "src/objects/bytecode-array.h"
#include "src/objects/shared-function-info.h"
#include "src/sandbox/testing.h"
#include "test/unittests/test-utils.h"

#ifdef V8_ENABLE_SANDBOX

namespace v8 {
namespace internal {

using SandboxViolationTest = TestWithNativeContext;

TEST_F(SandboxViolationTest, SandboxViolationOnInvalidBytecode) {
  // We expect the process to abort_with_sandbox_violation when executing
  // invalid bytecode in the interpreter as that can be a symptom of bytecode
  // corruption (or similar), which can allow breaking out of the sandbox.
  //
  // For this test we create a function with valid bytecode, then corrupt it to
  // test that we detect this correctly.
  const char* script = "function f() { return 42; }; f(); f";
  Handle<JSFunction> func = RunJS<JSFunction>(script);

  Isolate* i_isolate = isolate();
  Handle<SharedFunctionInfo> shared(func->shared(), i_isolate);
  CHECK(shared->HasBytecodeArray());
  Handle<BytecodeArray> bytecode_array(shared->GetBytecodeArray(i_isolate),
                                       i_isolate);

  uint8_t* bytecode =
      reinterpret_cast<uint8_t*>(bytecode_array->GetFirstBytecodeAddress());
  bytecode[0] = 0xFF;

  ASSERT_DEATH_IF_SUPPORTED(RunJS("f()"), "V8 sandbox violation detected!");
}

}  // namespace internal
}  // namespace v8

#endif  // V8_ENABLE_SANDBOX
