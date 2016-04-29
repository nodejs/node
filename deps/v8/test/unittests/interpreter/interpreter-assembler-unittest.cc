// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/interpreter/interpreter-assembler-unittest.h"

#include "src/code-factory.h"
#include "src/compiler/graph.h"
#include "src/compiler/node.h"
#include "src/interface-descriptors.h"
#include "src/isolate.h"
#include "test/unittests/compiler/compiler-test-utils.h"
#include "test/unittests/compiler/node-test-utils.h"

using ::testing::_;

namespace v8 {
namespace internal {

using namespace compiler;

namespace interpreter {

const interpreter::Bytecode kBytecodes[] = {
#define DEFINE_BYTECODE(Name, ...) interpreter::Bytecode::k##Name,
    BYTECODE_LIST(DEFINE_BYTECODE)
#undef DEFINE_BYTECODE
};

Matcher<Node*> IsIntPtrConstant(const intptr_t value) {
  return kPointerSize == 8 ? IsInt64Constant(static_cast<int64_t>(value))
                           : IsInt32Constant(static_cast<int32_t>(value));
}

Matcher<Node*> IsIntPtrAdd(const Matcher<Node*>& lhs_matcher,
                           const Matcher<Node*>& rhs_matcher) {
  return kPointerSize == 8 ? IsInt64Add(lhs_matcher, rhs_matcher)
                           : IsInt32Add(lhs_matcher, rhs_matcher);
}

Matcher<Node*> IsIntPtrSub(const Matcher<Node*>& lhs_matcher,
                           const Matcher<Node*>& rhs_matcher) {
  return kPointerSize == 8 ? IsInt64Sub(lhs_matcher, rhs_matcher)
                           : IsInt32Sub(lhs_matcher, rhs_matcher);
}

Matcher<Node*> IsWordShl(const Matcher<Node*>& lhs_matcher,
                         const Matcher<Node*>& rhs_matcher) {
  return kPointerSize == 8 ? IsWord64Shl(lhs_matcher, rhs_matcher)
                           : IsWord32Shl(lhs_matcher, rhs_matcher);
}

Matcher<Node*> IsWordSar(const Matcher<Node*>& lhs_matcher,
                         const Matcher<Node*>& rhs_matcher) {
  return kPointerSize == 8 ? IsWord64Sar(lhs_matcher, rhs_matcher)
                           : IsWord32Sar(lhs_matcher, rhs_matcher);
}

Matcher<Node*> IsWordOr(const Matcher<Node*>& lhs_matcher,
                        const Matcher<Node*>& rhs_matcher) {
  return kPointerSize == 8 ? IsWord64Or(lhs_matcher, rhs_matcher)
                           : IsWord32Or(lhs_matcher, rhs_matcher);
}

InterpreterAssemblerTest::InterpreterAssemblerForTest::
    ~InterpreterAssemblerForTest() {
  // Tests don't necessarily read and write accumulator but
  // InterpreterAssembler checks accumulator uses.
  if (Bytecodes::ReadsAccumulator(bytecode())) {
    GetAccumulator();
  }
  if (Bytecodes::WritesAccumulator(bytecode())) {
    SetAccumulator(nullptr);
  }
}

Matcher<Node*> InterpreterAssemblerTest::InterpreterAssemblerForTest::IsLoad(
    const Matcher<LoadRepresentation>& rep_matcher,
    const Matcher<Node*>& base_matcher, const Matcher<Node*>& index_matcher) {
  return ::i::compiler::IsLoad(rep_matcher, base_matcher, index_matcher, _, _);
}

Matcher<Node*> InterpreterAssemblerTest::InterpreterAssemblerForTest::IsStore(
    const Matcher<StoreRepresentation>& rep_matcher,
    const Matcher<Node*>& base_matcher, const Matcher<Node*>& index_matcher,
    const Matcher<Node*>& value_matcher) {
  return ::i::compiler::IsStore(rep_matcher, base_matcher, index_matcher,
                                value_matcher, _, _);
}

Matcher<Node*>
InterpreterAssemblerTest::InterpreterAssemblerForTest::IsUnsignedByteOperand(
    int offset) {
  return IsLoad(
      MachineType::Uint8(),
      IsParameter(InterpreterDispatchDescriptor::kBytecodeArrayParameter),
      IsIntPtrAdd(
          IsParameter(InterpreterDispatchDescriptor::kBytecodeOffsetParameter),
          IsIntPtrConstant(offset)));
}

Matcher<Node*>
InterpreterAssemblerTest::InterpreterAssemblerForTest::IsSignedByteOperand(
    int offset) {
  Matcher<Node*> load_matcher = IsLoad(
      MachineType::Int8(),
      IsParameter(InterpreterDispatchDescriptor::kBytecodeArrayParameter),
      IsIntPtrAdd(
          IsParameter(InterpreterDispatchDescriptor::kBytecodeOffsetParameter),
          IsIntPtrConstant(offset)));
  if (kPointerSize == 8) {
    load_matcher = IsChangeInt32ToInt64(load_matcher);
  }
  return load_matcher;
}

Matcher<Node*>
InterpreterAssemblerTest::InterpreterAssemblerForTest::IsUnsignedShortOperand(
    int offset) {
  if (TargetSupportsUnalignedAccess()) {
    return IsLoad(
        MachineType::Uint16(),
        IsParameter(InterpreterDispatchDescriptor::kBytecodeArrayParameter),
        IsIntPtrAdd(
            IsParameter(
                InterpreterDispatchDescriptor::kBytecodeOffsetParameter),
            IsIntPtrConstant(offset)));
  } else {
#if V8_TARGET_LITTLE_ENDIAN
    const int kStep = -1;
    const int kMsbOffset = 1;
#elif V8_TARGET_BIG_ENDIAN
    const int kStep = 1;
    const int kMsbOffset = 0;
#else
#error "Unknown Architecture"
#endif
    Matcher<Node*> bytes[2];
    for (int i = 0; i < static_cast<int>(arraysize(bytes)); i++) {
      bytes[i] = IsLoad(
          MachineType::Uint8(),
          IsParameter(InterpreterDispatchDescriptor::kBytecodeArrayParameter),
          IsIntPtrAdd(
              IsParameter(
                  InterpreterDispatchDescriptor::kBytecodeOffsetParameter),
              IsIntPtrConstant(offset + kMsbOffset + kStep * i)));
    }
    return IsWord32Or(IsWord32Shl(bytes[0], IsInt32Constant(kBitsPerByte)),
                      bytes[1]);
  }
}

Matcher<Node*>
InterpreterAssemblerTest::InterpreterAssemblerForTest::IsSignedShortOperand(
    int offset) {
  Matcher<Node*> load_matcher;
  if (TargetSupportsUnalignedAccess()) {
    load_matcher = IsLoad(
        MachineType::Int16(),
        IsParameter(InterpreterDispatchDescriptor::kBytecodeArrayParameter),
        IsIntPtrAdd(
            IsParameter(
                InterpreterDispatchDescriptor::kBytecodeOffsetParameter),
            IsIntPtrConstant(offset)));
  } else {
#if V8_TARGET_LITTLE_ENDIAN
    const int kStep = -1;
    const int kMsbOffset = 1;
#elif V8_TARGET_BIG_ENDIAN
    const int kStep = 1;
    const int kMsbOffset = 0;
#else
#error "Unknown Architecture"
#endif
    Matcher<Node*> bytes[2];
    for (int i = 0; i < static_cast<int>(arraysize(bytes)); i++) {
      bytes[i] = IsLoad(
          (i == 0) ? MachineType::Int8() : MachineType::Uint8(),
          IsParameter(InterpreterDispatchDescriptor::kBytecodeArrayParameter),
          IsIntPtrAdd(
              IsParameter(
                  InterpreterDispatchDescriptor::kBytecodeOffsetParameter),
              IsIntPtrConstant(offset + kMsbOffset + kStep * i)));
    }
    load_matcher = IsWord32Or(
        IsWord32Shl(bytes[0], IsInt32Constant(kBitsPerByte)), bytes[1]);
  }

  if (kPointerSize == 8) {
    load_matcher = IsChangeInt32ToInt64(load_matcher);
  }
  return load_matcher;
}

Matcher<Node*>
InterpreterAssemblerTest::InterpreterAssemblerForTest::IsUnsignedQuadOperand(
    int offset) {
  if (TargetSupportsUnalignedAccess()) {
    return IsLoad(
        MachineType::Uint32(),
        IsParameter(InterpreterDispatchDescriptor::kBytecodeArrayParameter),
        IsIntPtrAdd(
            IsParameter(
                InterpreterDispatchDescriptor::kBytecodeOffsetParameter),
            IsIntPtrConstant(offset)));
  } else {
#if V8_TARGET_LITTLE_ENDIAN
    const int kStep = -1;
    const int kMsbOffset = 3;
#elif V8_TARGET_BIG_ENDIAN
    const int kStep = 1;
    const int kMsbOffset = 0;
#else
#error "Unknown Architecture"
#endif
    Matcher<Node*> bytes[4];
    for (int i = 0; i < static_cast<int>(arraysize(bytes)); i++) {
      bytes[i] = IsLoad(
          MachineType::Uint8(),
          IsParameter(InterpreterDispatchDescriptor::kBytecodeArrayParameter),
          IsIntPtrAdd(
              IsParameter(
                  InterpreterDispatchDescriptor::kBytecodeOffsetParameter),
              IsIntPtrConstant(offset + kMsbOffset + kStep * i)));
    }
    return IsWord32Or(
        IsWord32Shl(bytes[0], IsInt32Constant(3 * kBitsPerByte)),
        IsWord32Or(
            IsWord32Shl(bytes[1], IsInt32Constant(2 * kBitsPerByte)),
            IsWord32Or(IsWord32Shl(bytes[2], IsInt32Constant(1 * kBitsPerByte)),
                       bytes[3])));
  }
}

Matcher<Node*>
InterpreterAssemblerTest::InterpreterAssemblerForTest::IsSignedQuadOperand(
    int offset) {
  Matcher<Node*> load_matcher;
  if (TargetSupportsUnalignedAccess()) {
    load_matcher = IsLoad(
        MachineType::Int32(),
        IsParameter(InterpreterDispatchDescriptor::kBytecodeArrayParameter),
        IsIntPtrAdd(
            IsParameter(
                InterpreterDispatchDescriptor::kBytecodeOffsetParameter),
            IsIntPtrConstant(offset)));
  } else {
#if V8_TARGET_LITTLE_ENDIAN
    const int kStep = -1;
    int kMsbOffset = 3;
#elif V8_TARGET_BIG_ENDIAN
    const int kStep = 1;
    int kMsbOffset = 0;
#else
#error "Unknown Architecture"
#endif
    Matcher<Node*> bytes[4];
    for (int i = 0; i < static_cast<int>(arraysize(bytes)); i++) {
      bytes[i] = IsLoad(
          (i == 0) ? MachineType::Int8() : MachineType::Uint8(),
          IsParameter(InterpreterDispatchDescriptor::kBytecodeArrayParameter),
          IsIntPtrAdd(
              IsParameter(
                  InterpreterDispatchDescriptor::kBytecodeOffsetParameter),
              IsIntPtrConstant(offset + kMsbOffset + kStep * i)));
    }
    load_matcher = IsWord32Or(
        IsWord32Shl(bytes[0], IsInt32Constant(3 * kBitsPerByte)),
        IsWord32Or(
            IsWord32Shl(bytes[1], IsInt32Constant(2 * kBitsPerByte)),
            IsWord32Or(IsWord32Shl(bytes[2], IsInt32Constant(1 * kBitsPerByte)),
                       bytes[3])));
  }

  if (kPointerSize == 8) {
    load_matcher = IsChangeInt32ToInt64(load_matcher);
  }
  return load_matcher;
}

Matcher<Node*>
InterpreterAssemblerTest::InterpreterAssemblerForTest::IsSignedOperand(
    int offset, OperandSize operand_size) {
  switch (operand_size) {
    case OperandSize::kByte:
      return IsSignedByteOperand(offset);
    case OperandSize::kShort:
      return IsSignedShortOperand(offset);
    case OperandSize::kQuad:
      return IsSignedQuadOperand(offset);
    case OperandSize::kNone:
      UNREACHABLE();
  }
  return nullptr;
}

Matcher<Node*>
InterpreterAssemblerTest::InterpreterAssemblerForTest::IsUnsignedOperand(
    int offset, OperandSize operand_size) {
  switch (operand_size) {
    case OperandSize::kByte:
      return IsUnsignedByteOperand(offset);
    case OperandSize::kShort:
      return IsUnsignedShortOperand(offset);
    case OperandSize::kQuad:
      return IsUnsignedQuadOperand(offset);
    case OperandSize::kNone:
      UNREACHABLE();
  }
  return nullptr;
}

TARGET_TEST_F(InterpreterAssemblerTest, Dispatch) {
  TRACED_FOREACH(interpreter::Bytecode, bytecode, kBytecodes) {
    InterpreterAssemblerForTest m(this, bytecode);
    m.Dispatch();
    Graph* graph = m.graph();

    Node* end = graph->end();
    EXPECT_EQ(1, end->InputCount());
    Node* tail_call_node = end->InputAt(0);

    OperandScale operand_scale = OperandScale::kSingle;
    Matcher<Node*> next_bytecode_offset_matcher = IsIntPtrAdd(
        IsParameter(InterpreterDispatchDescriptor::kBytecodeOffsetParameter),
        IsIntPtrConstant(
            interpreter::Bytecodes::Size(bytecode, operand_scale)));
    Matcher<Node*> target_bytecode_matcher = m.IsLoad(
        MachineType::Uint8(),
        IsParameter(InterpreterDispatchDescriptor::kBytecodeArrayParameter),
        next_bytecode_offset_matcher);
    if (kPointerSize == 8) {
      target_bytecode_matcher = IsChangeUint32ToUint64(target_bytecode_matcher);
    }
    Matcher<Node*> code_target_matcher = m.IsLoad(
        MachineType::Pointer(),
        IsParameter(InterpreterDispatchDescriptor::kDispatchTableParameter),
        IsWordShl(target_bytecode_matcher, IsIntPtrConstant(kPointerSizeLog2)));

    EXPECT_THAT(
        tail_call_node,
        IsTailCall(
            _, code_target_matcher,
            IsParameter(InterpreterDispatchDescriptor::kAccumulatorParameter),
            IsParameter(InterpreterDispatchDescriptor::kRegisterFileParameter),
            next_bytecode_offset_matcher,
            IsParameter(InterpreterDispatchDescriptor::kBytecodeArrayParameter),
            IsParameter(InterpreterDispatchDescriptor::kDispatchTableParameter),
            IsParameter(InterpreterDispatchDescriptor::kContextParameter), _,
            _));
  }
}

TARGET_TEST_F(InterpreterAssemblerTest, Jump) {
  // If debug code is enabled we emit extra code in Jump.
  if (FLAG_debug_code) return;

  int jump_offsets[] = {-9710, -77, 0, +3, +97109};
  TRACED_FOREACH(int, jump_offset, jump_offsets) {
    TRACED_FOREACH(interpreter::Bytecode, bytecode, kBytecodes) {
      InterpreterAssemblerForTest m(this, bytecode);
      m.Jump(m.IntPtrConstant(jump_offset));
      Graph* graph = m.graph();
      Node* end = graph->end();
      EXPECT_EQ(1, end->InputCount());
      Node* tail_call_node = end->InputAt(0);

      Matcher<Node*> next_bytecode_offset_matcher = IsIntPtrAdd(
          IsParameter(InterpreterDispatchDescriptor::kBytecodeOffsetParameter),
          IsIntPtrConstant(jump_offset));
      Matcher<Node*> target_bytecode_matcher =
          m.IsLoad(MachineType::Uint8(), _, next_bytecode_offset_matcher);
      if (kPointerSize == 8) {
        target_bytecode_matcher =
            IsChangeUint32ToUint64(target_bytecode_matcher);
      }
      Matcher<Node*> code_target_matcher = m.IsLoad(
          MachineType::Pointer(),
          IsParameter(InterpreterDispatchDescriptor::kDispatchTableParameter),
          IsWordShl(target_bytecode_matcher,
                    IsIntPtrConstant(kPointerSizeLog2)));

      EXPECT_THAT(
          tail_call_node,
          IsTailCall(
              _, code_target_matcher,
              IsParameter(InterpreterDispatchDescriptor::kAccumulatorParameter),
              IsParameter(
                  InterpreterDispatchDescriptor::kRegisterFileParameter),
              next_bytecode_offset_matcher, _,
              IsParameter(
                  InterpreterDispatchDescriptor::kDispatchTableParameter),
              IsParameter(InterpreterDispatchDescriptor::kContextParameter), _,
              _));
    }
  }
}

TARGET_TEST_F(InterpreterAssemblerTest, JumpIfWordEqual) {
  static const int kJumpIfTrueOffset = 73;

  // If debug code is enabled we emit extra code in Jump.
  if (FLAG_debug_code) return;

  MachineOperatorBuilder machine(zone());

  TRACED_FOREACH(interpreter::Bytecode, bytecode, kBytecodes) {
    InterpreterAssemblerForTest m(this, bytecode);
    Node* lhs = m.IntPtrConstant(0);
    Node* rhs = m.IntPtrConstant(1);
    m.JumpIfWordEqual(lhs, rhs, m.IntPtrConstant(kJumpIfTrueOffset));
    Graph* graph = m.graph();
    Node* end = graph->end();
    EXPECT_EQ(2, end->InputCount());

    OperandScale operand_scale = OperandScale::kSingle;
    int jump_offsets[] = {kJumpIfTrueOffset, interpreter::Bytecodes::Size(
                                                 bytecode, operand_scale)};
    for (int i = 0; i < static_cast<int>(arraysize(jump_offsets)); i++) {
      Matcher<Node*> next_bytecode_offset_matcher = IsIntPtrAdd(
          IsParameter(InterpreterDispatchDescriptor::kBytecodeOffsetParameter),
          IsIntPtrConstant(jump_offsets[i]));
      Matcher<Node*> target_bytecode_matcher =
          m.IsLoad(MachineType::Uint8(), _, next_bytecode_offset_matcher);
      if (kPointerSize == 8) {
        target_bytecode_matcher =
            IsChangeUint32ToUint64(target_bytecode_matcher);
      }
      Matcher<Node*> code_target_matcher = m.IsLoad(
          MachineType::Pointer(),
          IsParameter(InterpreterDispatchDescriptor::kDispatchTableParameter),
          IsWordShl(target_bytecode_matcher,
                    IsIntPtrConstant(kPointerSizeLog2)));
      EXPECT_THAT(
          end->InputAt(i),
          IsTailCall(
              _, code_target_matcher,
              IsParameter(InterpreterDispatchDescriptor::kAccumulatorParameter),
              IsParameter(
                  InterpreterDispatchDescriptor::kRegisterFileParameter),
              next_bytecode_offset_matcher, _,
              IsParameter(
                  InterpreterDispatchDescriptor::kDispatchTableParameter),
              IsParameter(InterpreterDispatchDescriptor::kContextParameter), _,
              _));
    }

    // TODO(oth): test control flow paths.
  }
}

TARGET_TEST_F(InterpreterAssemblerTest, InterpreterReturn) {
  // If debug code is enabled we emit extra code in InterpreterReturn.
  if (FLAG_debug_code) return;

  TRACED_FOREACH(interpreter::Bytecode, bytecode, kBytecodes) {
    InterpreterAssemblerForTest m(this, bytecode);
    m.InterpreterReturn();
    Graph* graph = m.graph();

    Node* end = graph->end();
    EXPECT_EQ(1, end->InputCount());
    Node* tail_call_node = end->InputAt(0);

    Handle<HeapObject> exit_trampoline =
        isolate()->builtins()->InterpreterExitTrampoline();
    EXPECT_THAT(
        tail_call_node,
        IsTailCall(
            _, IsHeapConstant(exit_trampoline),
            IsParameter(InterpreterDispatchDescriptor::kAccumulatorParameter),
            IsParameter(InterpreterDispatchDescriptor::kRegisterFileParameter),
            IsParameter(
                InterpreterDispatchDescriptor::kBytecodeOffsetParameter),
            _,
            IsParameter(InterpreterDispatchDescriptor::kDispatchTableParameter),
            IsParameter(InterpreterDispatchDescriptor::kContextParameter), _,
            _));
  }
}

TARGET_TEST_F(InterpreterAssemblerTest, BytecodeOperand) {
  static const OperandScale kOperandScales[] = {
      OperandScale::kSingle, OperandScale::kDouble, OperandScale::kQuadruple};
  TRACED_FOREACH(interpreter::Bytecode, bytecode, kBytecodes) {
    TRACED_FOREACH(interpreter::OperandScale, operand_scale, kOperandScales) {
      InterpreterAssemblerForTest m(this, bytecode, operand_scale);
      int number_of_operands =
          interpreter::Bytecodes::NumberOfOperands(bytecode);
      for (int i = 0; i < number_of_operands; i++) {
        int offset = interpreter::Bytecodes::GetOperandOffset(bytecode, i,
                                                              operand_scale);
        OperandType operand_type =
            interpreter::Bytecodes::GetOperandType(bytecode, i);
        OperandSize operand_size =
            Bytecodes::SizeOfOperand(operand_type, operand_scale);
        switch (interpreter::Bytecodes::GetOperandType(bytecode, i)) {
          case interpreter::OperandType::kRegCount:
            EXPECT_THAT(m.BytecodeOperandCount(i),
                        m.IsUnsignedOperand(offset, operand_size));
            break;
          case interpreter::OperandType::kFlag8:
            EXPECT_THAT(m.BytecodeOperandFlag(i),
                        m.IsUnsignedOperand(offset, operand_size));
            break;
          case interpreter::OperandType::kIdx:
            EXPECT_THAT(m.BytecodeOperandIdx(i),
                        m.IsUnsignedOperand(offset, operand_size));
            break;
          case interpreter::OperandType::kImm: {
            EXPECT_THAT(m.BytecodeOperandImm(i),
                        m.IsSignedOperand(offset, operand_size));
            break;
          }
          case interpreter::OperandType::kMaybeReg:
          case interpreter::OperandType::kReg:
          case interpreter::OperandType::kRegOut:
          case interpreter::OperandType::kRegOutPair:
          case interpreter::OperandType::kRegOutTriple:
          case interpreter::OperandType::kRegPair:
            EXPECT_THAT(m.BytecodeOperandReg(i),
                        m.IsSignedOperand(offset, operand_size));
            break;
          case interpreter::OperandType::kRuntimeId:
            EXPECT_THAT(m.BytecodeOperandRuntimeId(i),
                        m.IsUnsignedOperand(offset, operand_size));
            break;
          case interpreter::OperandType::kNone:
            UNREACHABLE();
            break;
        }
      }
    }
  }
}

TARGET_TEST_F(InterpreterAssemblerTest, GetSetAccumulator) {
  TRACED_FOREACH(interpreter::Bytecode, bytecode, kBytecodes) {
    if (!interpreter::Bytecodes::ReadsAccumulator(bytecode) ||
        !interpreter::Bytecodes::WritesAccumulator(bytecode)) {
      continue;
    }

    InterpreterAssemblerForTest m(this, bytecode);
    // Should be incoming accumulator if not set.
    EXPECT_THAT(
        m.GetAccumulator(),
        IsParameter(InterpreterDispatchDescriptor::kAccumulatorParameter));
    // Should be set by SetAccumulator.
    Node* accumulator_value_1 = m.Int32Constant(0xdeadbeef);
    m.SetAccumulator(accumulator_value_1);
    EXPECT_THAT(m.GetAccumulator(), accumulator_value_1);
    Node* accumulator_value_2 = m.Int32Constant(42);
    m.SetAccumulator(accumulator_value_2);
    EXPECT_THAT(m.GetAccumulator(), accumulator_value_2);

    // Should be passed to next bytecode handler on dispatch.
    m.Dispatch();
    Graph* graph = m.graph();

    Node* end = graph->end();
    EXPECT_EQ(1, end->InputCount());
    Node* tail_call_node = end->InputAt(0);

    EXPECT_THAT(tail_call_node,
                IsTailCall(_, _, accumulator_value_2, _, _, _, _, _, _));
  }
}

TARGET_TEST_F(InterpreterAssemblerTest, GetSetContext) {
  TRACED_FOREACH(interpreter::Bytecode, bytecode, kBytecodes) {
    InterpreterAssemblerForTest m(this, bytecode);
    Node* context_node = m.Int32Constant(100);
    m.SetContext(context_node);
    EXPECT_THAT(m.GetContext(), context_node);
  }
}

TARGET_TEST_F(InterpreterAssemblerTest, RegisterLocation) {
  TRACED_FOREACH(interpreter::Bytecode, bytecode, kBytecodes) {
    InterpreterAssemblerForTest m(this, bytecode);
    Node* reg_index_node = m.IntPtrConstant(44);
    Node* reg_location_node = m.RegisterLocation(reg_index_node);
    EXPECT_THAT(
        reg_location_node,
        IsIntPtrAdd(
            IsParameter(InterpreterDispatchDescriptor::kRegisterFileParameter),
            IsWordShl(reg_index_node, IsIntPtrConstant(kPointerSizeLog2))));
  }
}

TARGET_TEST_F(InterpreterAssemblerTest, LoadRegister) {
  TRACED_FOREACH(interpreter::Bytecode, bytecode, kBytecodes) {
    InterpreterAssemblerForTest m(this, bytecode);
    Node* reg_index_node = m.IntPtrConstant(44);
    Node* load_reg_node = m.LoadRegister(reg_index_node);
    EXPECT_THAT(
        load_reg_node,
        m.IsLoad(
            MachineType::AnyTagged(),
            IsParameter(InterpreterDispatchDescriptor::kRegisterFileParameter),
            IsWordShl(reg_index_node, IsIntPtrConstant(kPointerSizeLog2))));
  }
}

TARGET_TEST_F(InterpreterAssemblerTest, StoreRegister) {
  TRACED_FOREACH(interpreter::Bytecode, bytecode, kBytecodes) {
    InterpreterAssemblerForTest m(this, bytecode);
    Node* store_value = m.Int32Constant(0xdeadbeef);
    Node* reg_index_node = m.IntPtrConstant(44);
    Node* store_reg_node = m.StoreRegister(store_value, reg_index_node);
    EXPECT_THAT(
        store_reg_node,
        m.IsStore(
            StoreRepresentation(MachineRepresentation::kTagged,
                                kNoWriteBarrier),
            IsParameter(InterpreterDispatchDescriptor::kRegisterFileParameter),
            IsWordShl(reg_index_node, IsIntPtrConstant(kPointerSizeLog2)),
            store_value));
  }
}

TARGET_TEST_F(InterpreterAssemblerTest, SmiTag) {
  TRACED_FOREACH(interpreter::Bytecode, bytecode, kBytecodes) {
    InterpreterAssemblerForTest m(this, bytecode);
    Node* value = m.Int32Constant(44);
    EXPECT_THAT(
        m.SmiTag(value),
        IsWordShl(value, IsIntPtrConstant(kSmiShiftSize + kSmiTagSize)));
    EXPECT_THAT(
        m.SmiUntag(value),
        IsWordSar(value, IsIntPtrConstant(kSmiShiftSize + kSmiTagSize)));
  }
}

TARGET_TEST_F(InterpreterAssemblerTest, IntPtrAdd) {
  TRACED_FOREACH(interpreter::Bytecode, bytecode, kBytecodes) {
    InterpreterAssemblerForTest m(this, bytecode);
    Node* a = m.Int32Constant(0);
    Node* b = m.Int32Constant(1);
    Node* add = m.IntPtrAdd(a, b);
    EXPECT_THAT(add, IsIntPtrAdd(a, b));
  }
}

TARGET_TEST_F(InterpreterAssemblerTest, IntPtrSub) {
  TRACED_FOREACH(interpreter::Bytecode, bytecode, kBytecodes) {
    InterpreterAssemblerForTest m(this, bytecode);
    Node* a = m.Int32Constant(0);
    Node* b = m.Int32Constant(1);
    Node* add = m.IntPtrSub(a, b);
    EXPECT_THAT(add, IsIntPtrSub(a, b));
  }
}

TARGET_TEST_F(InterpreterAssemblerTest, WordShl) {
  TRACED_FOREACH(interpreter::Bytecode, bytecode, kBytecodes) {
    InterpreterAssemblerForTest m(this, bytecode);
    Node* a = m.IntPtrConstant(0);
    Node* add = m.WordShl(a, 10);
    EXPECT_THAT(add, IsWordShl(a, IsIntPtrConstant(10)));
  }
}

TARGET_TEST_F(InterpreterAssemblerTest, LoadConstantPoolEntry) {
  TRACED_FOREACH(interpreter::Bytecode, bytecode, kBytecodes) {
    InterpreterAssemblerForTest m(this, bytecode);
    Node* index = m.IntPtrConstant(2);
    Node* load_constant = m.LoadConstantPoolEntry(index);
    Matcher<Node*> constant_pool_matcher = m.IsLoad(
        MachineType::AnyTagged(),
        IsParameter(InterpreterDispatchDescriptor::kBytecodeArrayParameter),
        IsIntPtrConstant(BytecodeArray::kConstantPoolOffset - kHeapObjectTag));
    EXPECT_THAT(
        load_constant,
        m.IsLoad(MachineType::AnyTagged(), constant_pool_matcher,
                 IsIntPtrAdd(
                     IsIntPtrConstant(FixedArray::kHeaderSize - kHeapObjectTag),
                     IsWordShl(index, IsIntPtrConstant(kPointerSizeLog2)))));
  }
}

TARGET_TEST_F(InterpreterAssemblerTest, LoadObjectField) {
  TRACED_FOREACH(interpreter::Bytecode, bytecode, kBytecodes) {
    InterpreterAssemblerForTest m(this, bytecode);
    Node* object = m.IntPtrConstant(0xdeadbeef);
    int offset = 16;
    Node* load_field = m.LoadObjectField(object, offset);
    EXPECT_THAT(load_field,
                m.IsLoad(MachineType::AnyTagged(), object,
                         IsIntPtrConstant(offset - kHeapObjectTag)));
  }
}

TARGET_TEST_F(InterpreterAssemblerTest, LoadContextSlot) {
  TRACED_FOREACH(interpreter::Bytecode, bytecode, kBytecodes) {
    InterpreterAssemblerForTest m(this, bytecode);
    Node* context = m.IntPtrConstant(1);
    Node* slot_index = m.IntPtrConstant(22);
    Node* load_context_slot = m.LoadContextSlot(context, slot_index);

    Matcher<Node*> offset =
        IsIntPtrAdd(IsWordShl(slot_index, IsIntPtrConstant(kPointerSizeLog2)),
                    IsIntPtrConstant(Context::kHeaderSize - kHeapObjectTag));
    EXPECT_THAT(load_context_slot,
                m.IsLoad(MachineType::AnyTagged(), context, offset));
  }
}

TARGET_TEST_F(InterpreterAssemblerTest, StoreContextSlot) {
  TRACED_FOREACH(interpreter::Bytecode, bytecode, kBytecodes) {
    InterpreterAssemblerForTest m(this, bytecode);
    Node* context = m.IntPtrConstant(1);
    Node* slot_index = m.IntPtrConstant(22);
    Node* value = m.SmiConstant(Smi::FromInt(100));
    Node* store_context_slot = m.StoreContextSlot(context, slot_index, value);

    Matcher<Node*> offset =
        IsIntPtrAdd(IsWordShl(slot_index, IsIntPtrConstant(kPointerSizeLog2)),
                    IsIntPtrConstant(Context::kHeaderSize - kHeapObjectTag));
    EXPECT_THAT(store_context_slot,
                m.IsStore(StoreRepresentation(MachineRepresentation::kTagged,
                                              kFullWriteBarrier),
                          context, offset, value));
  }
}

TARGET_TEST_F(InterpreterAssemblerTest, CallRuntime2) {
  TRACED_FOREACH(interpreter::Bytecode, bytecode, kBytecodes) {
    InterpreterAssemblerForTest m(this, bytecode);
    Node* arg1 = m.Int32Constant(2);
    Node* arg2 = m.Int32Constant(3);
    Node* context =
        m.Parameter(InterpreterDispatchDescriptor::kContextParameter);
    Node* call_runtime = m.CallRuntime(Runtime::kAdd, context, arg1, arg2);
    EXPECT_THAT(
        call_runtime,
        IsCall(_, _, arg1, arg2, _, IsInt32Constant(2),
               IsParameter(InterpreterDispatchDescriptor::kContextParameter), _,
               _));
  }
}

TARGET_TEST_F(InterpreterAssemblerTest, CallRuntime) {
  const int kResultSizes[] = {1, 2};
  TRACED_FOREACH(interpreter::Bytecode, bytecode, kBytecodes) {
    TRACED_FOREACH(int, result_size, kResultSizes) {
      InterpreterAssemblerForTest m(this, bytecode);
      Callable builtin = CodeFactory::InterpreterCEntry(isolate(), result_size);

      Node* function_id = m.Int32Constant(0);
      Node* first_arg = m.Int32Constant(1);
      Node* arg_count = m.Int32Constant(2);
      Node* context =
          m.Parameter(InterpreterDispatchDescriptor::kContextParameter);

      Matcher<Node*> function_table = IsExternalConstant(
          ExternalReference::runtime_function_table_address(isolate()));
      Matcher<Node*> function = IsIntPtrAdd(
          function_table,
          IsInt32Mul(function_id, IsInt32Constant(sizeof(Runtime::Function))));
      Matcher<Node*> function_entry =
          m.IsLoad(MachineType::Pointer(), function,
                   IsIntPtrConstant(offsetof(Runtime::Function, entry)));

      Node* call_runtime = m.CallRuntimeN(function_id, context, first_arg,
                                          arg_count, result_size);
      EXPECT_THAT(
          call_runtime,
          IsCall(_, IsHeapConstant(builtin.code()), arg_count, first_arg,
                 function_entry,
                 IsParameter(InterpreterDispatchDescriptor::kContextParameter),
                 _, _));
    }
  }
}

TARGET_TEST_F(InterpreterAssemblerTest, CallJS) {
  TailCallMode tail_call_modes[] = {TailCallMode::kDisallow,
                                    TailCallMode::kAllow};
  TRACED_FOREACH(TailCallMode, tail_call_mode, tail_call_modes) {
    TRACED_FOREACH(interpreter::Bytecode, bytecode, kBytecodes) {
      InterpreterAssemblerForTest m(this, bytecode);
      Callable builtin =
          CodeFactory::InterpreterPushArgsAndCall(isolate(), tail_call_mode);
      Node* function = m.Int32Constant(0);
      Node* first_arg = m.Int32Constant(1);
      Node* arg_count = m.Int32Constant(2);
      Node* context =
          m.Parameter(InterpreterDispatchDescriptor::kContextParameter);
      Node* call_js =
          m.CallJS(function, context, first_arg, arg_count, tail_call_mode);
      EXPECT_THAT(
          call_js,
          IsCall(_, IsHeapConstant(builtin.code()), arg_count, first_arg,
                 function,
                 IsParameter(InterpreterDispatchDescriptor::kContextParameter),
                 _, _));
    }
  }
}

TARGET_TEST_F(InterpreterAssemblerTest, LoadTypeFeedbackVector) {
  TRACED_FOREACH(interpreter::Bytecode, bytecode, kBytecodes) {
    InterpreterAssemblerForTest m(this, bytecode);
    Node* feedback_vector = m.LoadTypeFeedbackVector();

    Matcher<Node*> load_function_matcher = m.IsLoad(
        MachineType::AnyTagged(),
        IsParameter(InterpreterDispatchDescriptor::kRegisterFileParameter),
        IsIntPtrConstant(
            InterpreterFrameConstants::kFunctionFromRegisterPointer));
    Matcher<Node*> load_shared_function_info_matcher =
        m.IsLoad(MachineType::AnyTagged(), load_function_matcher,
                 IsIntPtrConstant(JSFunction::kSharedFunctionInfoOffset -
                                  kHeapObjectTag));

    EXPECT_THAT(
        feedback_vector,
        m.IsLoad(MachineType::AnyTagged(), load_shared_function_info_matcher,
                 IsIntPtrConstant(SharedFunctionInfo::kFeedbackVectorOffset -
                                  kHeapObjectTag)));
  }
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
