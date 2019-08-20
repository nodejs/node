// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/test-utils.h"

#include "src/codegen/machine-type.h"
#include "src/codegen/signature.h"
#include "src/compiler/linkage.h"
#include "src/compiler/wasm-compiler.h"
#include "src/wasm/value-type.h"

namespace v8 {
namespace internal {
namespace wasm {

class WasmCallDescriptorTest : public TestWithZone {};

TEST_F(WasmCallDescriptorTest, TestAnyRefIsGrouped) {
  constexpr size_t kMaxCount = 30;
  ValueType params[kMaxCount];

  for (size_t i = 0; i < kMaxCount; i += 2) {
    params[i] = ValueType::kWasmAnyRef;
    CHECK_LT(i + 1, kMaxCount);
    params[i + 1] = ValueType::kWasmI32;
  }

  for (size_t count = 1; count <= kMaxCount; ++count) {
    FunctionSig sig(/*return_count=*/0, count, params);
    compiler::CallDescriptor* desc =
        compiler::GetWasmCallDescriptor(zone(), &sig);

    // The WasmInstance is the implicit first parameter.
    CHECK_EQ(count + 1, desc->ParameterCount());

    bool has_untagged_stack_param = false;
    bool has_tagged_register_param = false;
    int max_tagged_stack_location = std::numeric_limits<int>::min();
    int min_untagged_stack_location = std::numeric_limits<int>::max();
    for (size_t i = 1; i < desc->ParameterCount(); ++i) {
      // InputLocation i + 1, because target is the first input.
      compiler::LinkageLocation location = desc->GetInputLocation(i + 1);
      if (desc->GetParameterType(i).IsTagged()) {
        if (location.IsRegister()) {
          has_tagged_register_param = true;
        } else {
          CHECK(location.IsCallerFrameSlot());
          max_tagged_stack_location =
              std::max(max_tagged_stack_location, location.AsCallerFrameSlot());
        }
      } else {  // !isTagged()
        if (location.IsCallerFrameSlot()) {
          has_untagged_stack_param = true;
          min_untagged_stack_location = std::min(min_untagged_stack_location,
                                                 location.AsCallerFrameSlot());
        } else {
          CHECK(location.IsRegister());
        }
      }
    }
    // There should never be a tagged parameter in a register and an untagged
    // parameter on the stack at the same time.
    CHECK_EQ(false, has_tagged_register_param && has_untagged_stack_param);
    CHECK_LT(max_tagged_stack_location, min_untagged_stack_location);
  }
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
