// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/compiler/turboshaft/reducer-test.h"

#include "src/compiler/pipeline.h"

namespace v8::internal::compiler::turboshaft {

Handle<Code> TestInstance::CompileWithBuiltinPipeline(
    CallDescriptor* call_descriptor) {
  const testing::TestInfo* test_info =
      testing::UnitTest::GetInstance()->current_test_info();
  MaybeHandle<Code> code = Pipeline::GenerateCodeForTesting(
      data_, call_descriptor, test_info->name());
  return code.ToHandleChecked();
}

Handle<Code> TestInstance::CompileAsJSBuiltin() {
  int parameter_count = static_cast<int>(parameters_.size());
  CallDescriptor* call_descriptor = Linkage::GetJSCallDescriptor(
      zone(), false, parameter_count, CallDescriptor::kNoFlags);
  return CompileWithBuiltinPipeline(call_descriptor);
}

}  // namespace v8::internal::compiler::turboshaft
