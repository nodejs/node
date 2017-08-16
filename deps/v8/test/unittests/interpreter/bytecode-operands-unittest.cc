// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/interpreter/bytecode-operands.h"
#include "src/isolate.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace interpreter {

using BytecodeOperandsTest = TestWithIsolateAndZone;

TEST(BytecodeOperandsTest, IsScalableSignedByte) {
#define SCALABLE_SIGNED_OPERAND(Name, ...) \
  CHECK(BytecodeOperands::IsScalableSignedByte(OperandType::k##Name));
  REGISTER_OPERAND_TYPE_LIST(SCALABLE_SIGNED_OPERAND)
  SIGNED_SCALABLE_SCALAR_OPERAND_TYPE_LIST(SCALABLE_SIGNED_OPERAND)
#undef SCALABLE_SIGNED_OPERAND
#define NOT_SCALABLE_SIGNED_OPERAND(Name, ...) \
  CHECK(!BytecodeOperands::IsScalableSignedByte(OperandType::k##Name));
  INVALID_OPERAND_TYPE_LIST(NOT_SCALABLE_SIGNED_OPERAND)
  UNSIGNED_FIXED_SCALAR_OPERAND_TYPE_LIST(NOT_SCALABLE_SIGNED_OPERAND)
  UNSIGNED_SCALABLE_SCALAR_OPERAND_TYPE_LIST(NOT_SCALABLE_SIGNED_OPERAND)
#undef NOT_SCALABLE_SIGNED_OPERAND
}

TEST(BytecodeOperandsTest, IsScalableUnsignedByte) {
#define SCALABLE_UNSIGNED_OPERAND(Name, ...) \
  CHECK(BytecodeOperands::IsScalableUnsignedByte(OperandType::k##Name));
  UNSIGNED_SCALABLE_SCALAR_OPERAND_TYPE_LIST(SCALABLE_UNSIGNED_OPERAND)
#undef SCALABLE_SIGNED_OPERAND
#define NOT_SCALABLE_UNSIGNED_OPERAND(Name, ...) \
  CHECK(!BytecodeOperands::IsScalableUnsignedByte(OperandType::k##Name));
  INVALID_OPERAND_TYPE_LIST(NOT_SCALABLE_UNSIGNED_OPERAND)
  REGISTER_OPERAND_TYPE_LIST(NOT_SCALABLE_UNSIGNED_OPERAND)
  SIGNED_SCALABLE_SCALAR_OPERAND_TYPE_LIST(NOT_SCALABLE_UNSIGNED_OPERAND)
  UNSIGNED_FIXED_SCALAR_OPERAND_TYPE_LIST(NOT_SCALABLE_UNSIGNED_OPERAND)
#undef NOT_SCALABLE_SIGNED_OPERAND
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
