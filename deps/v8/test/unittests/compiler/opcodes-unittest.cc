// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/opcodes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {

bool IsCommonOpcode(IrOpcode::Value opcode) {
  switch (opcode) {
#define OPCODE(Opcode)      \
  case IrOpcode::k##Opcode: \
    return true;
    COMMON_OP_LIST(OPCODE)
    CONTROL_OP_LIST(OPCODE)
#undef OPCODE
    default:
      return false;
  }
}


bool IsControlOpcode(IrOpcode::Value opcode) {
  switch (opcode) {
#define OPCODE(Opcode)      \
  case IrOpcode::k##Opcode: \
    return true;
    CONTROL_OP_LIST(OPCODE)
#undef OPCODE
    default:
      return false;
  }
}


bool IsJsOpcode(IrOpcode::Value opcode) {
  switch (opcode) {
#define OPCODE(Opcode)      \
  case IrOpcode::k##Opcode: \
    return true;
    JS_OP_LIST(OPCODE)
#undef OPCODE
    default:
      return false;
  }
}


bool IsConstantOpcode(IrOpcode::Value opcode) {
  switch (opcode) {
#define OPCODE(Opcode)      \
  case IrOpcode::k##Opcode: \
    return true;
    CONSTANT_OP_LIST(OPCODE)
#undef OPCODE
    default:
      return false;
  }
}


bool IsComparisonOpcode(IrOpcode::Value opcode) {
  switch (opcode) {
#define OPCODE(Opcode)      \
  case IrOpcode::k##Opcode: \
    return true;
    JS_COMPARE_BINOP_LIST(OPCODE)
    SIMPLIFIED_COMPARE_BINOP_LIST(OPCODE)
    MACHINE_COMPARE_BINOP_LIST(OPCODE)
#undef OPCODE
    default:
      return false;
  }
}


const IrOpcode::Value kInvalidOpcode = static_cast<IrOpcode::Value>(123456789);

}  // namespace


TEST(IrOpcodeTest, IsCommonOpcode) {
  EXPECT_FALSE(IrOpcode::IsCommonOpcode(kInvalidOpcode));
#define OPCODE(Opcode)                           \
  EXPECT_EQ(IsCommonOpcode(IrOpcode::k##Opcode), \
            IrOpcode::IsCommonOpcode(IrOpcode::k##Opcode));
  ALL_OP_LIST(OPCODE)
#undef OPCODE
}


TEST(IrOpcodeTest, IsControlOpcode) {
  EXPECT_FALSE(IrOpcode::IsControlOpcode(kInvalidOpcode));
#define OPCODE(Opcode)                            \
  EXPECT_EQ(IsControlOpcode(IrOpcode::k##Opcode), \
            IrOpcode::IsControlOpcode(IrOpcode::k##Opcode));
  ALL_OP_LIST(OPCODE)
#undef OPCODE
}


TEST(IrOpcodeTest, IsJsOpcode) {
  EXPECT_FALSE(IrOpcode::IsJsOpcode(kInvalidOpcode));
#define OPCODE(Opcode)                       \
  EXPECT_EQ(IsJsOpcode(IrOpcode::k##Opcode), \
            IrOpcode::IsJsOpcode(IrOpcode::k##Opcode));
  ALL_OP_LIST(OPCODE)
#undef OPCODE
}


TEST(IrOpcodeTest, IsConstantOpcode) {
  EXPECT_FALSE(IrOpcode::IsConstantOpcode(kInvalidOpcode));
#define OPCODE(Opcode)                             \
  EXPECT_EQ(IsConstantOpcode(IrOpcode::k##Opcode), \
            IrOpcode::IsConstantOpcode(IrOpcode::k##Opcode));
  ALL_OP_LIST(OPCODE)
#undef OPCODE
}


TEST(IrOpcodeTest, IsComparisonOpcode) {
  EXPECT_FALSE(IrOpcode::IsComparisonOpcode(kInvalidOpcode));
#define OPCODE(Opcode)                               \
  EXPECT_EQ(IsComparisonOpcode(IrOpcode::k##Opcode), \
            IrOpcode::IsComparisonOpcode(IrOpcode::k##Opcode));
  ALL_OP_LIST(OPCODE)
#undef OPCODE
}


TEST(IrOpcodeTest, Mnemonic) {
  EXPECT_STREQ("UnknownOpcode", IrOpcode::Mnemonic(kInvalidOpcode));
#define OPCODE(Opcode) \
  EXPECT_STREQ(#Opcode, IrOpcode::Mnemonic(IrOpcode::k##Opcode));
  ALL_OP_LIST(OPCODE)
#undef OPCODE
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
