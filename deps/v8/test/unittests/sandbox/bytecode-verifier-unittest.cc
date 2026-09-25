// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/sandbox/bytecode-verifier.h"

#include "src/codegen/handler-table.h"
#include "src/interpreter/bytecode-register.h"
#include "src/interpreter/bytecodes.h"
#include "src/objects/bytecode-array.h"
#include "src/objects/fixed-array.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

#ifdef V8_ENABLE_SANDBOX

namespace v8 {
namespace internal {

// In official builds we drop the message in CHECK_WITH_MSG so we cannot check
// for it in these tests.
#ifdef OFFICIAL_BUILD
#define EXPECTED_CHECK_WITH_MSG(msg) ""
#else
#define EXPECTED_CHECK_WITH_MSG(msg) msg
#endif

class BytecodeVerifierTest : public TestWithIsolateAndZone {
 public:
  void VerifyLight(IsolateForSandbox isolate, Handle<BytecodeArray> bytecode) {
    BytecodeVerifier::VerifyLight(isolate, bytecode, zone());
  }
  void VerifyFull(IsolateForSandbox isolate, Handle<BytecodeArray> bytecode) {
    BytecodeVerifier::VerifyFull(isolate, bytecode, zone());
  }

  Handle<BytecodeArray> MakeBytecodeArray(
      Isolate* isolate, std::vector<uint8_t> raw_bytes,
      Handle<TrustedFixedArray> constant_pool,
      Handle<TrustedByteArray> handler_table, int32_t frame_size = 32,
      uint16_t parameter_count = 2, uint16_t max_arguments = 0) {
    return isolate->factory()->NewBytecodeArray(
        static_cast<int>(raw_bytes.size()), raw_bytes.data(), frame_size,
        parameter_count, max_arguments, constant_pool, handler_table);
  }
};

TEST_F(BytecodeVerifierTest, UnverifiedBytecodeIsUnsusable) {
  Isolate* isolate = i_isolate();
  Factory* factory = isolate->factory();

  Handle<TrustedFixedArray> constant_pool = factory->NewTrustedFixedArray(0);
  Handle<TrustedByteArray> handler_table = factory->NewTrustedByteArray(0);

  std::vector<uint8_t> kRawBytes = {
      static_cast<uint8_t>(interpreter::Bytecode::kReturn)};

  Handle<BytecodeArray> bc =
      MakeBytecodeArray(isolate, kRawBytes, constant_pool, handler_table);

  CHECK(!bc->IsPublished(isolate));

  BytecodeVerifier::Verify(isolate, bc, zone());

  CHECK(bc->IsPublished(isolate));
}

TEST_F(BytecodeVerifierTest, JumpToInvalidOffset) {
  Isolate* isolate = i_isolate();
  Factory* factory = isolate->factory();

  Handle<TrustedFixedArray> constant_pool = factory->NewTrustedFixedArray(0);
  Handle<TrustedByteArray> handler_table = factory->NewTrustedByteArray(0);

  std::vector<uint8_t> kRawBytes = {
      static_cast<uint8_t>(interpreter::Bytecode::kJump), 0xff};

  Handle<BytecodeArray> bc =
      MakeBytecodeArray(isolate, kRawBytes, constant_pool, handler_table);
  ASSERT_DEATH_IF_SUPPORTED(VerifyLight(isolate, bc), "Invalid jump offset");
}

TEST_F(BytecodeVerifierTest, JumpToNegativeOffset) {
  Isolate* isolate = i_isolate();
  Factory* factory = isolate->factory();

  Handle<TrustedFixedArray> constant_pool = factory->NewTrustedFixedArray(0);
  Handle<TrustedByteArray> handler_table = factory->NewTrustedByteArray(0);

  // kJumpLoop takes an unsigned operand, but the jump is backwards.
  // JumpLoop 0x80 -> target = current - 0x80.
  std::vector<uint8_t> kRawBytes = {
      static_cast<uint8_t>(interpreter::Bytecode::kJumpLoop), 0x80};

  Handle<BytecodeArray> bc =
      MakeBytecodeArray(isolate, kRawBytes, constant_pool, handler_table);
  ASSERT_DEATH_IF_SUPPORTED(VerifyLight(isolate, bc), "Invalid jump offset");
}

TEST_F(BytecodeVerifierTest, JumpToMisalignedOffset) {
  Isolate* isolate = i_isolate();
  Factory* factory = isolate->factory();

  Handle<TrustedFixedArray> constant_pool = factory->NewTrustedFixedArray(0);
  Handle<TrustedByteArray> handler_table = factory->NewTrustedByteArray(0);

  std::vector<uint8_t> kRawBytes = {
      static_cast<uint8_t>(interpreter::Bytecode::kJump), 1,
      static_cast<uint8_t>(interpreter::Bytecode::kReturn)};

  Handle<BytecodeArray> bc =
      MakeBytecodeArray(isolate, kRawBytes, constant_pool, handler_table);
  ASSERT_DEATH_IF_SUPPORTED(VerifyLight(isolate, bc), "Invalid control-flow");
}

TEST_F(BytecodeVerifierTest, ForwardJumpToCurrentInstruction) {
  Isolate* isolate = i_isolate();
  Factory* factory = isolate->factory();

  Handle<TrustedFixedArray> constant_pool = factory->NewTrustedFixedArray(0);
  Handle<TrustedByteArray> handler_table = factory->NewTrustedByteArray(0);

  // We assume that forward jumps go forward and not (back) to the current
  // instruction. Some of our compilers (e.g. the baseline compiler) may get
  // confused if they ever see such jumps as they will initially emit a
  // placeholder jump instruction (assuming the instruction will be patched
  // again later on), but then fail to update the instruction as they never
  // bind the label (as it's for an instruction that has already been
  // processed). This would then lead to invalid machine code. As such, we rely
  // on the bytecode verifier to forbid such jumps.
  std::vector<uint8_t> kRawBytes = {
      static_cast<uint8_t>(interpreter::Bytecode::kJump), 0,
      static_cast<uint8_t>(interpreter::Bytecode::kReturn)};

  Handle<BytecodeArray> bc =
      MakeBytecodeArray(isolate, kRawBytes, constant_pool, handler_table);
  ASSERT_DEATH_IF_SUPPORTED(VerifyLight(isolate, bc), "Invalid jump offset");
}

TEST_F(BytecodeVerifierTest, SwitchToInvalidOffset) {
  Isolate* isolate = i_isolate();
  Factory* factory = isolate->factory();

  Handle<TrustedFixedArray> constant_pool = factory->NewTrustedFixedArray(2);
  constant_pool->set(0, Smi::FromInt(0));
  constant_pool->set(1, Smi::FromInt(0xff));  // Invalid target

  auto handler_table = factory->NewTrustedByteArray(0);

  std::vector<uint8_t> kRawBytes = {
      static_cast<uint8_t>(interpreter::Bytecode::kSwitchOnSmiNoFeedback),
      0,  // jump_table_index
      2,  // jump_table_size
      0,  // case_value_base
      static_cast<uint8_t>(interpreter::Bytecode::kReturn),
  };

  Handle<BytecodeArray> bc =
      MakeBytecodeArray(isolate, kRawBytes, constant_pool, handler_table);
  ASSERT_DEATH_IF_SUPPORTED(VerifyLight(isolate, bc), "Invalid switch offset");
}

TEST_F(BytecodeVerifierTest, SwitchToMisalignedOffset) {
  Isolate* isolate = i_isolate();
  Factory* factory = isolate->factory();

  Handle<TrustedFixedArray> constant_pool = factory->NewTrustedFixedArray(1);
  constant_pool->set(0, Smi::FromInt(1));  // Misaligned target

  auto handler_table = factory->NewTrustedByteArray(0);

  std::vector<uint8_t> kRawBytes = {
      static_cast<uint8_t>(interpreter::Bytecode::kSwitchOnSmiNoFeedback),
      0,  // jump_table_index
      1,  // jump_table_size
      0,  // case_value_base
      static_cast<uint8_t>(interpreter::Bytecode::kReturn),
  };

  Handle<BytecodeArray> bc =
      MakeBytecodeArray(isolate, kRawBytes, constant_pool, handler_table);
  ASSERT_DEATH_IF_SUPPORTED(VerifyLight(isolate, bc), "Invalid control-flow");
}

TEST_F(BytecodeVerifierTest, JumpConstantToInvalidIndex) {
  Isolate* isolate = i_isolate();
  Factory* factory = isolate->factory();

  Handle<TrustedFixedArray> constant_pool = factory->NewTrustedFixedArray(0);
  Handle<TrustedByteArray> handler_table = factory->NewTrustedByteArray(0);

  std::vector<uint8_t> kRawBytes = {
      static_cast<uint8_t>(interpreter::Bytecode::kJumpConstant), 0xff};

  Handle<BytecodeArray> bc =
      MakeBytecodeArray(isolate, kRawBytes, constant_pool, handler_table);
  ASSERT_DEATH_IF_SUPPORTED(
      VerifyLight(isolate, bc),
      EXPECTED_CHECK_WITH_MSG("Constant pool index out of bounds"));
}

TEST_F(BytecodeVerifierTest, JumpConstantToNonSmi) {
  Isolate* isolate = i_isolate();
  Factory* factory = isolate->factory();

  Handle<TrustedFixedArray> constant_pool = factory->NewTrustedFixedArray(1);
  constant_pool->set(0, *factory->NewFixedArray(0));

  Handle<TrustedByteArray> handler_table = factory->NewTrustedByteArray(0);

  std::vector<uint8_t> kRawBytes = {
      static_cast<uint8_t>(interpreter::Bytecode::kJumpConstant), 0};

  Handle<BytecodeArray> bc =
      MakeBytecodeArray(isolate, kRawBytes, constant_pool, handler_table);
  ASSERT_DEATH_IF_SUPPORTED(
      VerifyLight(isolate, bc),
      EXPECTED_CHECK_WITH_MSG("Constant pool entry is not a Smi"));
}

TEST_F(BytecodeVerifierTest, SwitchToInvalidIndex) {
  Isolate* isolate = i_isolate();
  Factory* factory = isolate->factory();

  Handle<TrustedFixedArray> constant_pool = factory->NewTrustedFixedArray(0);
  Handle<TrustedByteArray> handler_table = factory->NewTrustedByteArray(0);

  std::vector<uint8_t> kRawBytes = {
      static_cast<uint8_t>(interpreter::Bytecode::kSwitchOnSmiNoFeedback),
      0,     // jump_table_index
      0xff,  // jump_table_size
      0,     // case_value_base
      static_cast<uint8_t>(interpreter::Bytecode::kReturn),
  };

  Handle<BytecodeArray> bc =
      MakeBytecodeArray(isolate, kRawBytes, constant_pool, handler_table);
  ASSERT_DEATH_IF_SUPPORTED(
      VerifyLight(isolate, bc),
      EXPECTED_CHECK_WITH_MSG("Constant pool index out of bounds"));
}

TEST_F(BytecodeVerifierTest, SwitchToNonSmi) {
  Isolate* isolate = i_isolate();
  Factory* factory = isolate->factory();

  Handle<TrustedFixedArray> constant_pool = factory->NewTrustedFixedArray(1);
  constant_pool->set(0, *factory->NewFixedArray(0));

  Handle<TrustedByteArray> handler_table = factory->NewTrustedByteArray(0);

  std::vector<uint8_t> kRawBytes = {
      static_cast<uint8_t>(interpreter::Bytecode::kSwitchOnSmiNoFeedback),
      0,  // jump_table_index
      1,  // jump_table_size
      0,  // case_value_base
      static_cast<uint8_t>(interpreter::Bytecode::kReturn),
  };

  Handle<BytecodeArray> bc =
      MakeBytecodeArray(isolate, kRawBytes, constant_pool, handler_table);
  ASSERT_DEATH_IF_SUPPORTED(
      VerifyLight(isolate, bc),
      EXPECTED_CHECK_WITH_MSG("Constant pool entry is not a Smi"));
}

TEST_F(BytecodeVerifierTest, HandlerTableEntryWithInvalidRange) {
  Isolate* isolate = i_isolate();
  Factory* factory = isolate->factory();

  Handle<TrustedFixedArray> constant_pool = factory->NewTrustedFixedArray(0);

  Handle<TrustedByteArray> handler_table = factory->NewTrustedByteArray(
      HandlerTable::LengthForRange(1), AllocationType::kTrusted);
  {
    HandlerTable table(*handler_table);
    table.SetRangeStart(0, 1);
    table.SetRangeEnd(0, 0);  // Invalid range: start > end
    table.SetRangeHandler(0, 0, HandlerTable::CAUGHT);
    table.SetRangeData(0, 0);
  }

  std::vector<uint8_t> kRawBytes = {
      static_cast<uint8_t>(interpreter::Bytecode::kReturn)};

  Handle<BytecodeArray> bc =
      MakeBytecodeArray(isolate, kRawBytes, constant_pool, handler_table);

  ASSERT_DEATH_IF_SUPPORTED(VerifyLight(isolate, bc),
                            "Invalid exception handler range");
}

TEST_F(BytecodeVerifierTest, HandlerTableEntryWithNegativeRange) {
  Isolate* isolate = i_isolate();
  Factory* factory = isolate->factory();

  Handle<TrustedFixedArray> constant_pool = factory->NewTrustedFixedArray(0);

  Handle<TrustedByteArray> handler_table = factory->NewTrustedByteArray(
      HandlerTable::LengthForRange(1), AllocationType::kTrusted);
  {
    HandlerTable table(*handler_table);
    table.SetRangeStart(0, -1);
    table.SetRangeEnd(0, 0);
    table.SetRangeHandler(0, 0, HandlerTable::CAUGHT);
    table.SetRangeData(0, 0);
  }

  std::vector<uint8_t> kRawBytes = {
      static_cast<uint8_t>(interpreter::Bytecode::kReturn)};

  Handle<BytecodeArray> bc =
      MakeBytecodeArray(isolate, kRawBytes, constant_pool, handler_table);

  ASSERT_DEATH_IF_SUPPORTED(VerifyLight(isolate, bc),
                            "Invalid exception handler range");
}

TEST_F(BytecodeVerifierTest, HandlerTableEntryWithInvalidHandler) {
  Isolate* isolate = i_isolate();
  Factory* factory = isolate->factory();

  Handle<TrustedFixedArray> constant_pool = factory->NewTrustedFixedArray(0);

  Handle<TrustedByteArray> handler_table = factory->NewTrustedByteArray(
      HandlerTable::LengthForRange(1), AllocationType::kTrusted);
  {
    HandlerTable table(*handler_table);
    table.SetRangeStart(0, 0);
    table.SetRangeEnd(0, 1);
    table.SetRangeHandler(0, 0xff, HandlerTable::CAUGHT);  // Invalid handler
    table.SetRangeData(0, 0);
  }

  std::vector<uint8_t> kRawBytes = {
      static_cast<uint8_t>(interpreter::Bytecode::kReturn)};

  Handle<BytecodeArray> bc =
      MakeBytecodeArray(isolate, kRawBytes, constant_pool, handler_table);

  ASSERT_DEATH_IF_SUPPORTED(VerifyLight(isolate, bc),
                            "Invalid exception handler offset");
}

TEST_F(BytecodeVerifierTest, HandlerTableEntryWithMisalignedHandler) {
  Isolate* isolate = i_isolate();
  Factory* factory = isolate->factory();

  Handle<TrustedFixedArray> constant_pool = factory->NewTrustedFixedArray(0);

  Handle<TrustedByteArray> handler_table = factory->NewTrustedByteArray(
      HandlerTable::LengthForRange(1), AllocationType::kTrusted);
  {
    HandlerTable table(*handler_table);
    table.SetRangeStart(0, 0);
    table.SetRangeEnd(0, 1);
    table.SetRangeHandler(0, 1, HandlerTable::CAUGHT);  // Misaligned handler
    table.SetRangeData(0, 0);
  }

  // kLdaConstant takes 1 operand (1 byte), so it is 2 bytes long.
  // Offset 1 is inside the instruction.
  std::vector<uint8_t> kRawBytes = {
      static_cast<uint8_t>(interpreter::Bytecode::kLdaConstant), 0,
      static_cast<uint8_t>(interpreter::Bytecode::kReturn)};

  Handle<BytecodeArray> bc =
      MakeBytecodeArray(isolate, kRawBytes, constant_pool, handler_table);

  ASSERT_DEATH_IF_SUPPORTED(VerifyLight(isolate, bc),
                            "Invalid exception handler offset");
}

TEST_F(BytecodeVerifierTest, StackStoreOutOfBounds) {
  Isolate* isolate = i_isolate();
  Factory* factory = isolate->factory();

  Handle<TrustedFixedArray> constant_pool = factory->NewTrustedFixedArray(0);
  Handle<TrustedByteArray> handler_table = factory->NewTrustedByteArray(0);

  // Star0 writes to register 0.
  std::vector<uint8_t> kRawBytes = {
      static_cast<uint8_t>(interpreter::Bytecode::kStar0),
      static_cast<uint8_t>(interpreter::Bytecode::kReturn)};
  // Frame size 0 means 0 locals. Register 0 is out of bounds.
  constexpr int32_t kFrameSize = 0;

  Handle<BytecodeArray> bc = MakeBytecodeArray(
      isolate, kRawBytes, constant_pool, handler_table, kFrameSize);
  ASSERT_DEATH_IF_SUPPORTED(VerifyFull(isolate, bc),
                            "Register index out of bounds");
}

TEST_F(BytecodeVerifierTest, RegisterLoadOutOfBounds) {
  Isolate* isolate = i_isolate();
  Factory* factory = isolate->factory();
  Handle<TrustedFixedArray> constant_pool = factory->NewTrustedFixedArray(0);
  Handle<TrustedByteArray> handler_table = factory->NewTrustedByteArray(0);

  // Ldar register 100. (Frame size 0 means 0 registers).
  uint8_t reg_operand =
      static_cast<uint8_t>(interpreter::Register(100).ToOperand());
  std::vector<uint8_t> kRawBytes = {
      static_cast<uint8_t>(interpreter::Bytecode::kLdar), reg_operand,
      static_cast<uint8_t>(interpreter::Bytecode::kReturn)};
  constexpr int32_t kFrameSize = 0;

  Handle<BytecodeArray> bc = MakeBytecodeArray(
      isolate, kRawBytes, constant_pool, handler_table, kFrameSize);

  ASSERT_DEATH_IF_SUPPORTED(VerifyFull(isolate, bc),
                            "Register index out of bounds");
}

TEST_F(BytecodeVerifierTest, ParameterLoadOutOfBounds) {
  Isolate* isolate = i_isolate();
  Factory* factory = isolate->factory();
  Handle<TrustedFixedArray> constant_pool = factory->NewTrustedFixedArray(0);
  Handle<TrustedByteArray> handler_table = factory->NewTrustedByteArray(0);

  // Ldar parameter 2. (Parameter count 2 means 0,1 valid).
  uint8_t param_operand = static_cast<uint8_t>(
      interpreter::Register::FromParameterIndex(2).ToOperand());
  std::vector<uint8_t> kRawBytes = {
      static_cast<uint8_t>(interpreter::Bytecode::kLdar), param_operand,
      static_cast<uint8_t>(interpreter::Bytecode::kReturn)};
  constexpr int32_t kFrameSize = 32;
  constexpr int32_t kParameterCount = 0;

  Handle<BytecodeArray> bc =
      MakeBytecodeArray(isolate, kRawBytes, constant_pool, handler_table,
                        kFrameSize, kParameterCount);

  ASSERT_DEATH_IF_SUPPORTED(VerifyFull(isolate, bc),
                            "Parameter index out of bounds");
}

TEST_F(BytecodeVerifierTest, ParameterLoadOutOfBoundsNegative) {
  Isolate* isolate = i_isolate();
  Factory* factory = isolate->factory();

  Handle<TrustedFixedArray> constant_pool = factory->NewTrustedFixedArray(0);
  Handle<TrustedByteArray> handler_table = factory->NewTrustedByteArray(0);

  // Register(-1) (and some others) are special registers referring to e.g. the
  // current context. Here we test that those are validated correctly.
  uint8_t param_operand =
      static_cast<uint8_t>(interpreter::Register(-1).ToOperand());

  std::vector<uint8_t> kRawBytes = {
      static_cast<uint8_t>(interpreter::Bytecode::kPushContext), param_operand,
      static_cast<uint8_t>(interpreter::Bytecode::kReturn)};

  Handle<BytecodeArray> bc =
      MakeBytecodeArray(isolate, kRawBytes, constant_pool, handler_table);

  ASSERT_DEATH_IF_SUPPORTED(VerifyFull(isolate, bc),
                            "Parameter index out of bounds");
}

TEST_F(BytecodeVerifierTest, InvalidWideBytecode) {
  Isolate* isolate = i_isolate();
  Factory* factory = isolate->factory();

  Handle<TrustedFixedArray> constant_pool = factory->NewTrustedFixedArray(0);
  Handle<TrustedByteArray> handler_table = factory->NewTrustedByteArray(0);

  // Star0 cannot be scaled.
  std::vector<uint8_t> kRawBytes = {
      static_cast<uint8_t>(interpreter::Bytecode::kWide),
      static_cast<uint8_t>(interpreter::Bytecode::kStar0),
      static_cast<uint8_t>(interpreter::Bytecode::kReturn)};

  Handle<BytecodeArray> bc =
      MakeBytecodeArray(isolate, kRawBytes, constant_pool, handler_table);
  ASSERT_DEATH_IF_SUPPORTED(VerifyFull(isolate, bc), "Invalid scaled bytecode");
}

TEST_F(BytecodeVerifierTest, InvalidExtraWideBytecode) {
  Isolate* isolate = i_isolate();
  Factory* factory = isolate->factory();

  Handle<TrustedFixedArray> constant_pool = factory->NewTrustedFixedArray(0);
  Handle<TrustedByteArray> handler_table = factory->NewTrustedByteArray(0);

  // Star0 cannot be scaled.
  std::vector<uint8_t> kRawBytes = {
      static_cast<uint8_t>(interpreter::Bytecode::kExtraWide),
      static_cast<uint8_t>(interpreter::Bytecode::kStar0),
      static_cast<uint8_t>(interpreter::Bytecode::kReturn)};

  Handle<BytecodeArray> bc =
      MakeBytecodeArray(isolate, kRawBytes, constant_pool, handler_table);
  ASSERT_DEATH_IF_SUPPORTED(VerifyFull(isolate, bc), "Invalid scaled bytecode");
}

TEST_F(BytecodeVerifierTest, ConstantPoolAccessOutOfBounds) {
  Isolate* isolate = i_isolate();
  Factory* factory = isolate->factory();

  Handle<TrustedFixedArray> constant_pool = factory->NewTrustedFixedArray(0);
  Handle<TrustedByteArray> handler_table = factory->NewTrustedByteArray(0);

  // StaGlobal, index 0, slot 0.
  std::vector<uint8_t> kRawBytes = {
      static_cast<uint8_t>(interpreter::Bytecode::kStaGlobal),
      0,  // Name Index (OOB for size 0)
      0,  // Slot Index
      static_cast<uint8_t>(interpreter::Bytecode::kReturn)};

  Handle<BytecodeArray> bc =
      MakeBytecodeArray(isolate, kRawBytes, constant_pool, handler_table);

  ASSERT_DEATH_IF_SUPPORTED(VerifyFull(isolate, bc),
                            "Constant pool index out of bounds");
}

TEST_F(BytecodeVerifierTest, FinalInstructionPartiallyOutOfBounds) {
  Isolate* isolate = i_isolate();
  Factory* factory = isolate->factory();

  Handle<TrustedFixedArray> constant_pool = factory->NewTrustedFixedArray(0);
  Handle<TrustedByteArray> handler_table = factory->NewTrustedByteArray(0);

  // kJump offset being read out-of-bounds.
  std::vector<uint8_t> kRawBytes = {
      static_cast<uint8_t>(interpreter::Bytecode::kJump)};

  Handle<BytecodeArray> bc =
      MakeBytecodeArray(isolate, kRawBytes, constant_pool, handler_table);
  ASSERT_DEATH_IF_SUPPORTED(
      VerifyFull(isolate, bc),
      "Final instruction does not end at the end of the bytecode array");
}

TEST_F(BytecodeVerifierTest, InvalidFinalInstruction) {
  Isolate* isolate = i_isolate();
  Factory* factory = isolate->factory();

  Handle<TrustedFixedArray> constant_pool = factory->NewTrustedFixedArray(0);
  Handle<TrustedByteArray> handler_table = factory->NewTrustedByteArray(0);

  std::vector<uint8_t> kRawBytes = {
      static_cast<uint8_t>(interpreter::Bytecode::kLdaZero)};

  Handle<BytecodeArray> bc =
      MakeBytecodeArray(isolate, kRawBytes, constant_pool, handler_table);
  ASSERT_DEATH_IF_SUPPORTED(
      VerifyFull(isolate, bc),
      "Bytecode does not end with a control-flow terminating instruction");
}

TEST_F(BytecodeVerifierTest, WritingToSpecialRegister) {
  Isolate* isolate = i_isolate();
  Factory* factory = isolate->factory();

  Handle<TrustedFixedArray> constant_pool = factory->NewTrustedFixedArray(0);
  Handle<TrustedByteArray> handler_table = factory->NewTrustedByteArray(0);

  uint8_t closure_reg_operand = static_cast<uint8_t>(
      interpreter::Register::function_closure().ToOperand());

  std::vector<uint8_t> kRawBytes = {
      static_cast<uint8_t>(interpreter::Bytecode::kLdaZero),
      static_cast<uint8_t>(interpreter::Bytecode::kStar), closure_reg_operand,
      static_cast<uint8_t>(interpreter::Bytecode::kReturn)};

  Handle<BytecodeArray> bc =
      MakeBytecodeArray(isolate, kRawBytes, constant_pool, handler_table);

  ASSERT_DEATH_IF_SUPPORTED(VerifyFull(isolate, bc),
                            "Invalid write to special register");
}

}  // namespace internal
}  // namespace v8

#endif  // V8_ENABLE_SANDBOX
