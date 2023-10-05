// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/wasm-compiler.h"

#include "src/codegen/machine-type.h"
#include "src/codegen/signature.h"
#include "src/compiler/linkage.h"
#include "src/wasm/value-type.h"
#include "src/wasm/wasm-linkage.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace wasm {

class WasmCallDescriptorTest : public TestWithZone {};

TEST_F(WasmCallDescriptorTest, TestExternRefIsGrouped) {
  constexpr size_t kMaxCount = 30;
  ValueType params[kMaxCount];

  for (size_t i = 0; i < kMaxCount; i += 2) {
    params[i] = kWasmExternRef;
    EXPECT_TRUE(i + 1 < kMaxCount);
    params[i + 1] = kWasmI32;
  }

  for (size_t count = 1; count <= kMaxCount; ++count) {
    FunctionSig sig(/*return_count=*/0, count, params);
    compiler::CallDescriptor* desc =
        compiler::GetWasmCallDescriptor(zone(), &sig);

    // The WasmInstance is the implicit first parameter.
    EXPECT_EQ(count + 1, desc->ParameterCount());

    bool has_untagged_stack_param = false;
    bool has_tagged_register_param = false;
    int max_tagged_stack_location = std::numeric_limits<int>::min();
    int min_untagged_stack_location = std::numeric_limits<int>::max();
    for (size_t i = 1; i < desc->ParameterCount(); ++i) {
      // InputLocation i + 1, because target is the first input.
      LinkageLocation location = desc->GetInputLocation(i + 1);
      if (desc->GetParameterType(i).IsTagged()) {
        if (location.IsRegister()) {
          has_tagged_register_param = true;
        } else {
          EXPECT_TRUE(location.IsCallerFrameSlot());
          max_tagged_stack_location =
              std::max(max_tagged_stack_location, location.AsCallerFrameSlot());
        }
      } else {  // !isTagged()
        if (location.IsCallerFrameSlot()) {
          has_untagged_stack_param = true;
          min_untagged_stack_location = std::min(min_untagged_stack_location,
                                                 location.AsCallerFrameSlot());
        } else {
          EXPECT_TRUE(location.IsRegister());
        }
      }
    }
    // There should never be a tagged parameter in a register and an untagged
    // parameter on the stack at the same time.
    EXPECT_EQ(false, has_tagged_register_param && has_untagged_stack_param);
    EXPECT_TRUE(max_tagged_stack_location < min_untagged_stack_location);
  }
}

TEST_F(WasmCallDescriptorTest, Regress_1174500) {
  // Our test signature should have just enough params and returns to force
  // 1 param and 1 return to be allocated as stack slots. Use FP registers to
  // avoid interference with implicit parameters, like the Wasm Instance.
  constexpr int kParamRegisters = arraysize(kFpParamRegisters);
  constexpr int kParams = kParamRegisters + 1;
  constexpr int kReturnRegisters = arraysize(kFpReturnRegisters);
  constexpr int kReturns = kReturnRegisters + 1;
  ValueType types[kReturns + kParams];
  // One S128 return slot which shouldn't be padded unless the arguments area
  // of the frame requires it.
  for (int i = 0; i < kReturnRegisters; ++i) types[i] = kWasmF32;
  types[kReturnRegisters] = kWasmS128;
  // One F32 parameter slot to misalign the parameter area.
  for (int i = 0; i < kParamRegisters; ++i) types[kReturns + i] = kWasmF32;
  types[kReturns + kParamRegisters] = kWasmF32;

  FunctionSig sig(kReturns, kParams, types);
  compiler::CallDescriptor* desc =
      compiler::GetWasmCallDescriptor(zone(), &sig);

  // Get the location of our stack parameter slot. Skip the implicit Wasm
  // instance parameter.
  LinkageLocation last_param = desc->GetInputLocation(kParams + 1);
  EXPECT_TRUE(last_param.IsCallerFrameSlot());
  EXPECT_EQ(MachineType::Float32(), last_param.GetType());
  EXPECT_EQ(-1, last_param.GetLocation());

  // The stack return slot should be right above our last parameter, and any
  // argument padding slots. The return slot itself should not be padded.
  const int padding = ShouldPadArguments(1);
  const int first_return_slot = -1 - (padding + 1);
  LinkageLocation return_location = desc->GetReturnLocation(kReturns - 1);
  EXPECT_TRUE(return_location.IsCallerFrameSlot());
  EXPECT_EQ(MachineType::Simd128(), return_location.GetType());
  EXPECT_EQ(first_return_slot, return_location.GetLocation());
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
