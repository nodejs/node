// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/opcodes.h"
#include "testing/gtest-support.h"

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

char const* const kMnemonics[] = {
#define OPCODE(Opcode) #Opcode,
    ALL_OP_LIST(OPCODE)
#undef OPCODE
};

const IrOpcode::Value kOpcodes[] = {
#define OPCODE(Opcode) IrOpcode::k##Opcode,
    ALL_OP_LIST(OPCODE)
#undef OPCODE
};

}  // namespace

TEST(IrOpcodeTest, IsCommonOpcode) {
  EXPECT_FALSE(IrOpcode::IsCommonOpcode(kInvalidOpcode));
  TRACED_FOREACH(IrOpcode::Value, opcode, kOpcodes) {
    EXPECT_EQ(IsCommonOpcode(opcode), IrOpcode::IsCommonOpcode(opcode));
  }
}

TEST(IrOpcodeTest, IsControlOpcode) {
  EXPECT_FALSE(IrOpcode::IsControlOpcode(kInvalidOpcode));
  TRACED_FOREACH(IrOpcode::Value, opcode, kOpcodes) {
    EXPECT_EQ(IsControlOpcode(opcode), IrOpcode::IsControlOpcode(opcode));
  }
}

TEST(IrOpcodeTest, IsJsOpcode) {
  EXPECT_FALSE(IrOpcode::IsJsOpcode(kInvalidOpcode));
  TRACED_FOREACH(IrOpcode::Value, opcode, kOpcodes) {
    EXPECT_EQ(IsJsOpcode(opcode), IrOpcode::IsJsOpcode(opcode));
  }
}

TEST(IrOpcodeTest, IsConstantOpcode) {
  EXPECT_FALSE(IrOpcode::IsConstantOpcode(kInvalidOpcode));
  TRACED_FOREACH(IrOpcode::Value, opcode, kOpcodes) {
    EXPECT_EQ(IsConstantOpcode(opcode), IrOpcode::IsConstantOpcode(opcode));
  }
}

TEST(IrOpcodeTest, IsComparisonOpcode) {
  EXPECT_FALSE(IrOpcode::IsComparisonOpcode(kInvalidOpcode));
  TRACED_FOREACH(IrOpcode::Value, opcode, kOpcodes) {
    EXPECT_EQ(IsComparisonOpcode(opcode), IrOpcode::IsComparisonOpcode(opcode));
  }
}

TEST(IrOpcodeTest, Mnemonic) {
  EXPECT_STREQ("UnknownOpcode", IrOpcode::Mnemonic(kInvalidOpcode));
  TRACED_FOREACH(IrOpcode::Value, opcode, kOpcodes) {
    EXPECT_STREQ(kMnemonics[opcode], IrOpcode::Mnemonic(opcode));
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
