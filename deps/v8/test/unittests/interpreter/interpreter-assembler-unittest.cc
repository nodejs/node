// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/interpreter/interpreter-assembler-unittest.h"

#include "src/code-factory.h"
#include "src/compiler/node.h"
#include "src/interface-descriptors.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "test/unittests/compiler/compiler-test-utils.h"
#include "test/unittests/compiler/node-test-utils.h"

using ::testing::_;

namespace v8 {
namespace internal {

using namespace compiler;

#ifdef ENABLE_VERIFY_CSA
#define IS_BITCAST_WORD_TO_TAGGED_SIGNED(x) IsBitcastWordToTaggedSigned(x)
#define IS_BITCAST_TAGGED_TO_WORD(x) IsBitcastTaggedToWord(x)
#else
#define IS_BITCAST_WORD_TO_TAGGED_SIGNED(x) (x)
#define IS_BITCAST_TAGGED_TO_WORD(x) (x)
#endif

namespace interpreter {

InterpreterAssemblerTestState::InterpreterAssemblerTestState(
    InterpreterAssemblerTest* test, Bytecode bytecode)
    : compiler::CodeAssemblerState(
          test->isolate(), test->zone(),
          InterpreterDispatchDescriptor(test->isolate()),
          Code::ComputeFlags(Code::BYTECODE_HANDLER),
          Bytecodes::ToString(bytecode), Bytecodes::ReturnCount(bytecode)) {}

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

Matcher<Node*> IsIntPtrMul(const Matcher<Node*>& lhs_matcher,
                           const Matcher<Node*>& rhs_matcher) {
  return kPointerSize == 8 ? IsInt64Mul(lhs_matcher, rhs_matcher)
                           : IsInt32Mul(lhs_matcher, rhs_matcher);
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

Matcher<Node*> IsChangeInt32ToIntPtr(const Matcher<Node*>& matcher) {
  return kPointerSize == 8 ? IsChangeInt32ToInt64(matcher) : matcher;
}

Matcher<Node*> IsChangeUint32ToWord(const Matcher<Node*>& matcher) {
  return kPointerSize == 8 ? IsChangeUint32ToUint64(matcher) : matcher;
}

Matcher<Node*> IsTruncateWordToWord32(const Matcher<Node*>& matcher) {
  return kPointerSize == 8 ? IsTruncateInt64ToInt32(matcher) : matcher;
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
      IsParameter(InterpreterDispatchDescriptor::kBytecodeArray),
      IsIntPtrAdd(IsParameter(InterpreterDispatchDescriptor::kBytecodeOffset),
                  IsIntPtrConstant(offset)));
}

Matcher<Node*>
InterpreterAssemblerTest::InterpreterAssemblerForTest::IsSignedByteOperand(
    int offset) {
  return IsLoad(
      MachineType::Int8(),
      IsParameter(InterpreterDispatchDescriptor::kBytecodeArray),
      IsIntPtrAdd(IsParameter(InterpreterDispatchDescriptor::kBytecodeOffset),
                  IsIntPtrConstant(offset)));
}

Matcher<Node*>
InterpreterAssemblerTest::InterpreterAssemblerForTest::IsUnsignedShortOperand(
    int offset) {
  if (TargetSupportsUnalignedAccess()) {
    return IsLoad(
        MachineType::Uint16(),
        IsParameter(InterpreterDispatchDescriptor::kBytecodeArray),
        IsIntPtrAdd(IsParameter(InterpreterDispatchDescriptor::kBytecodeOffset),
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
          IsParameter(InterpreterDispatchDescriptor::kBytecodeArray),
          IsIntPtrAdd(
              IsParameter(InterpreterDispatchDescriptor::kBytecodeOffset),
              IsIntPtrConstant(offset + kMsbOffset + kStep * i)));
    }
    return IsWord32Or(IsWord32Shl(bytes[0], IsInt32Constant(kBitsPerByte)),
                      bytes[1]);
  }
}

Matcher<Node*>
InterpreterAssemblerTest::InterpreterAssemblerForTest::IsSignedShortOperand(
    int offset) {
  if (TargetSupportsUnalignedAccess()) {
    return IsLoad(
        MachineType::Int16(),
        IsParameter(InterpreterDispatchDescriptor::kBytecodeArray),
        IsIntPtrAdd(IsParameter(InterpreterDispatchDescriptor::kBytecodeOffset),
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
          IsParameter(InterpreterDispatchDescriptor::kBytecodeArray),
          IsIntPtrAdd(
              IsParameter(InterpreterDispatchDescriptor::kBytecodeOffset),
              IsIntPtrConstant(offset + kMsbOffset + kStep * i)));
    }
    return IsWord32Or(IsWord32Shl(bytes[0], IsInt32Constant(kBitsPerByte)),
                      bytes[1]);
  }
}

Matcher<Node*>
InterpreterAssemblerTest::InterpreterAssemblerForTest::IsUnsignedQuadOperand(
    int offset) {
  if (TargetSupportsUnalignedAccess()) {
    return IsLoad(
        MachineType::Uint32(),
        IsParameter(InterpreterDispatchDescriptor::kBytecodeArray),
        IsIntPtrAdd(IsParameter(InterpreterDispatchDescriptor::kBytecodeOffset),
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
          IsParameter(InterpreterDispatchDescriptor::kBytecodeArray),
          IsIntPtrAdd(
              IsParameter(InterpreterDispatchDescriptor::kBytecodeOffset),
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
  if (TargetSupportsUnalignedAccess()) {
    return IsLoad(
        MachineType::Int32(),
        IsParameter(InterpreterDispatchDescriptor::kBytecodeArray),
        IsIntPtrAdd(IsParameter(InterpreterDispatchDescriptor::kBytecodeOffset),
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
          IsParameter(InterpreterDispatchDescriptor::kBytecodeArray),
          IsIntPtrAdd(
              IsParameter(InterpreterDispatchDescriptor::kBytecodeOffset),
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

TARGET_TEST_F(InterpreterAssemblerTest, Jump) {
  // If debug code is enabled we emit extra code in Jump.
  if (FLAG_debug_code) return;

  int jump_offsets[] = {-9710, -77, 0, +3, +97109};
  TRACED_FOREACH(int, jump_offset, jump_offsets) {
    TRACED_FOREACH(interpreter::Bytecode, bytecode, kBytecodes) {
      if (!interpreter::Bytecodes::IsJump(bytecode)) return;

      InterpreterAssemblerTestState state(this, bytecode);
      InterpreterAssemblerForTest m(&state, bytecode);
      Node* tail_call_node = m.Jump(m.IntPtrConstant(jump_offset));

      Matcher<Node*> next_bytecode_offset_matcher = IsIntPtrAdd(
          IsParameter(InterpreterDispatchDescriptor::kBytecodeOffset),
          IsIntPtrConstant(jump_offset));
      Matcher<Node*> target_bytecode_matcher =
          m.IsLoad(MachineType::Uint8(), _, next_bytecode_offset_matcher);
      target_bytecode_matcher = IsChangeUint32ToWord(target_bytecode_matcher);
      Matcher<Node*> code_target_matcher =
          m.IsLoad(MachineType::Pointer(),
                   IsParameter(InterpreterDispatchDescriptor::kDispatchTable),
                   IsWordShl(target_bytecode_matcher,
                             IsIntPtrConstant(kPointerSizeLog2)));

      EXPECT_THAT(
          tail_call_node,
          IsTailCall(_, code_target_matcher,
                     IsParameter(InterpreterDispatchDescriptor::kAccumulator),
                     next_bytecode_offset_matcher, _,
                     IsParameter(InterpreterDispatchDescriptor::kDispatchTable),
                     _, _));
    }
  }
}

TARGET_TEST_F(InterpreterAssemblerTest, BytecodeOperand) {
  static const OperandScale kOperandScales[] = {
      OperandScale::kSingle, OperandScale::kDouble, OperandScale::kQuadruple};
  TRACED_FOREACH(interpreter::Bytecode, bytecode, kBytecodes) {
    TRACED_FOREACH(interpreter::OperandScale, operand_scale, kOperandScales) {
      InterpreterAssemblerTestState state(this, bytecode);
      InterpreterAssemblerForTest m(&state, bytecode, operand_scale);
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
                        IsChangeUint32ToWord(
                            m.IsUnsignedOperand(offset, operand_size)));
            break;
          case interpreter::OperandType::kUImm:
            EXPECT_THAT(m.BytecodeOperandUImm(i),
                        m.IsUnsignedOperand(offset, operand_size));
            break;
          case interpreter::OperandType::kImm: {
            EXPECT_THAT(m.BytecodeOperandImm(i),
                        m.IsSignedOperand(offset, operand_size));
            break;
          }
          case interpreter::OperandType::kRegList:
          case interpreter::OperandType::kReg:
          case interpreter::OperandType::kRegOut:
          case interpreter::OperandType::kRegOutPair:
          case interpreter::OperandType::kRegOutTriple:
          case interpreter::OperandType::kRegPair:
            EXPECT_THAT(
                m.BytecodeOperandReg(i),
                IsChangeInt32ToIntPtr(m.IsSignedOperand(offset, operand_size)));
            break;
          case interpreter::OperandType::kRuntimeId:
            EXPECT_THAT(m.BytecodeOperandRuntimeId(i),
                        m.IsUnsignedOperand(offset, operand_size));
            break;
          case interpreter::OperandType::kIntrinsicId:
            EXPECT_THAT(m.BytecodeOperandIntrinsicId(i),
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

TARGET_TEST_F(InterpreterAssemblerTest, GetContext) {
  TRACED_FOREACH(interpreter::Bytecode, bytecode, kBytecodes) {
    InterpreterAssemblerTestState state(this, bytecode);
    InterpreterAssemblerForTest m(&state, bytecode);
    EXPECT_THAT(
        m.GetContext(),
        m.IsLoad(MachineType::AnyTagged(), IsLoadParentFramePointer(),
                 IsIntPtrConstant(Register::current_context().ToOperand()
                                  << kPointerSizeLog2)));
  }
}

TARGET_TEST_F(InterpreterAssemblerTest, RegisterLocation) {
  TRACED_FOREACH(interpreter::Bytecode, bytecode, kBytecodes) {
    InterpreterAssemblerTestState state(this, bytecode);
    InterpreterAssemblerForTest m(&state, bytecode);
    Node* reg_index_node = m.IntPtrConstant(44);
    Node* reg_location_node = m.RegisterLocation(reg_index_node);
    EXPECT_THAT(reg_location_node,
                IsIntPtrAdd(IsLoadParentFramePointer(),
                            IsWordShl(reg_index_node,
                                      IsIntPtrConstant(kPointerSizeLog2))));
  }
}

TARGET_TEST_F(InterpreterAssemblerTest, LoadRegister) {
  TRACED_FOREACH(interpreter::Bytecode, bytecode, kBytecodes) {
    InterpreterAssemblerTestState state(this, bytecode);
    InterpreterAssemblerForTest m(&state, bytecode);
    Node* reg_index_node = m.IntPtrConstant(44);
    Node* load_reg_node = m.LoadRegister(reg_index_node);
    EXPECT_THAT(load_reg_node,
                m.IsLoad(MachineType::AnyTagged(), IsLoadParentFramePointer(),
                         IsWordShl(reg_index_node,
                                   IsIntPtrConstant(kPointerSizeLog2))));
  }
}

TARGET_TEST_F(InterpreterAssemblerTest, StoreRegister) {
  TRACED_FOREACH(interpreter::Bytecode, bytecode, kBytecodes) {
    InterpreterAssemblerTestState state(this, bytecode);
    InterpreterAssemblerForTest m(&state, bytecode);
    Node* store_value = m.Int32Constant(0xdeadbeef);
    Node* reg_index_node = m.IntPtrConstant(44);
    Node* store_reg_node = m.StoreRegister(store_value, reg_index_node);
    EXPECT_THAT(
        store_reg_node,
        m.IsStore(StoreRepresentation(MachineRepresentation::kTagged,
                                      kNoWriteBarrier),
                  IsLoadParentFramePointer(),
                  IsWordShl(reg_index_node, IsIntPtrConstant(kPointerSizeLog2)),
                  store_value));
  }
}

TARGET_TEST_F(InterpreterAssemblerTest, SmiTag) {
  TRACED_FOREACH(interpreter::Bytecode, bytecode, kBytecodes) {
    InterpreterAssemblerTestState state(this, bytecode);
    InterpreterAssemblerForTest m(&state, bytecode);
    Node* value = m.Int32Constant(44);
    EXPECT_THAT(
        m.SmiTag(value),
        IS_BITCAST_WORD_TO_TAGGED_SIGNED(IsIntPtrConstant(
            static_cast<intptr_t>(44) << (kSmiShiftSize + kSmiTagSize))));
    EXPECT_THAT(m.SmiUntag(value),
                IsWordSar(IS_BITCAST_TAGGED_TO_WORD(value),
                          IsIntPtrConstant(kSmiShiftSize + kSmiTagSize)));
  }
}

TARGET_TEST_F(InterpreterAssemblerTest, IntPtrAdd) {
  TRACED_FOREACH(interpreter::Bytecode, bytecode, kBytecodes) {
    InterpreterAssemblerTestState state(this, bytecode);
    InterpreterAssemblerForTest m(&state, bytecode);
    Node* a = m.Parameter(0);
    Node* b = m.Int32Constant(1);
    Node* add = m.IntPtrAdd(a, b);
    EXPECT_THAT(add, IsIntPtrAdd(a, b));
  }
}

TARGET_TEST_F(InterpreterAssemblerTest, IntPtrSub) {
  TRACED_FOREACH(interpreter::Bytecode, bytecode, kBytecodes) {
    InterpreterAssemblerTestState state(this, bytecode);
    InterpreterAssemblerForTest m(&state, bytecode);
    Node* a = m.Parameter(0);
    Node* b = m.Int32Constant(1);
    Node* add = m.IntPtrSub(a, b);
    EXPECT_THAT(add, IsIntPtrSub(a, b));
  }
}

TARGET_TEST_F(InterpreterAssemblerTest, WordShl) {
  TRACED_FOREACH(interpreter::Bytecode, bytecode, kBytecodes) {
    InterpreterAssemblerTestState state(this, bytecode);
    InterpreterAssemblerForTest m(&state, bytecode);
    Node* a = m.IntPtrConstant(0);
    Node* add = m.WordShl(a, 10);
    EXPECT_THAT(add, IsWordShl(a, IsIntPtrConstant(10)));
  }
}

TARGET_TEST_F(InterpreterAssemblerTest, LoadConstantPoolEntry) {
  TRACED_FOREACH(interpreter::Bytecode, bytecode, kBytecodes) {
    InterpreterAssemblerTestState state(this, bytecode);
    InterpreterAssemblerForTest m(&state, bytecode);
    {
      Node* index = m.IntPtrConstant(2);
      Node* load_constant = m.LoadConstantPoolEntry(index);
      Matcher<Node*> constant_pool_matcher =
          m.IsLoad(MachineType::AnyTagged(),
                   IsParameter(InterpreterDispatchDescriptor::kBytecodeArray),
                   IsIntPtrConstant(BytecodeArray::kConstantPoolOffset -
                                    kHeapObjectTag));
      EXPECT_THAT(load_constant,
                  m.IsLoad(MachineType::AnyTagged(), constant_pool_matcher,
                           IsIntPtrConstant(FixedArray::OffsetOfElementAt(2) -
                                            kHeapObjectTag)));
    }
    {
      Node* index = m.Parameter(2);
      Node* load_constant = m.LoadConstantPoolEntry(index);
      Matcher<Node*> constant_pool_matcher =
          m.IsLoad(MachineType::AnyTagged(),
                   IsParameter(InterpreterDispatchDescriptor::kBytecodeArray),
                   IsIntPtrConstant(BytecodeArray::kConstantPoolOffset -
                                    kHeapObjectTag));
      EXPECT_THAT(
          load_constant,
          m.IsLoad(
              MachineType::AnyTagged(), constant_pool_matcher,
              IsIntPtrAdd(
                  IsIntPtrConstant(FixedArray::kHeaderSize - kHeapObjectTag),
                  IsWordShl(index, IsIntPtrConstant(kPointerSizeLog2)))));
    }
  }
}

TARGET_TEST_F(InterpreterAssemblerTest, LoadObjectField) {
  TRACED_FOREACH(interpreter::Bytecode, bytecode, kBytecodes) {
    InterpreterAssemblerTestState state(this, bytecode);
    InterpreterAssemblerForTest m(&state, bytecode);
    Node* object = m.IntPtrConstant(0xdeadbeef);
    int offset = 16;
    Node* load_field = m.LoadObjectField(object, offset);
    EXPECT_THAT(load_field,
                m.IsLoad(MachineType::AnyTagged(), object,
                         IsIntPtrConstant(offset - kHeapObjectTag)));
  }
}

TARGET_TEST_F(InterpreterAssemblerTest, CallRuntime2) {
  TRACED_FOREACH(interpreter::Bytecode, bytecode, kBytecodes) {
    InterpreterAssemblerTestState state(this, bytecode);
    InterpreterAssemblerForTest m(&state, bytecode);
    Node* arg1 = m.Int32Constant(2);
    Node* arg2 = m.Int32Constant(3);
    Node* context = m.Int32Constant(4);
    Node* call_runtime = m.CallRuntime(Runtime::kAdd, context, arg1, arg2);
    EXPECT_THAT(call_runtime,
                IsCall(_, _, arg1, arg2, _, IsInt32Constant(2), context, _, _));
  }
}

TARGET_TEST_F(InterpreterAssemblerTest, CallRuntime) {
  const int kResultSizes[] = {1, 2};
  TRACED_FOREACH(interpreter::Bytecode, bytecode, kBytecodes) {
    TRACED_FOREACH(int, result_size, kResultSizes) {
      if (Bytecodes::IsCallRuntime(bytecode)) {
        InterpreterAssemblerTestState state(this, bytecode);
        InterpreterAssemblerForTest m(&state, bytecode);
        Callable builtin =
            CodeFactory::InterpreterCEntry(isolate(), result_size);

        Node* function_id = m.Int32Constant(0);
        Node* first_arg = m.IntPtrConstant(1);
        Node* arg_count = m.Int32Constant(2);
        Node* context = m.IntPtrConstant(4);

        Matcher<Node*> function_table = IsExternalConstant(
            ExternalReference::runtime_function_table_address(isolate()));
        Matcher<Node*> function = IsIntPtrAdd(
            function_table,
            IsChangeUint32ToWord(IsInt32Mul(
                function_id, IsInt32Constant(sizeof(Runtime::Function)))));
        Matcher<Node*> function_entry =
            m.IsLoad(MachineType::Pointer(), function,
                     IsIntPtrConstant(offsetof(Runtime::Function, entry)));

        Node* call_runtime = m.CallRuntimeN(function_id, context, first_arg,
                                            arg_count, result_size);
        EXPECT_THAT(call_runtime,
                    IsCall(_, IsHeapConstant(builtin.code()), arg_count,
                           first_arg, function_entry, context, _, _));
      }
    }
  }
}

TARGET_TEST_F(InterpreterAssemblerTest, CallJS) {
  TRACED_FOREACH(interpreter::Bytecode, bytecode, kBytecodes) {
    if (Bytecodes::IsCallOrConstruct(bytecode) &&
        bytecode != Bytecode::kCallWithSpread) {
      InterpreterAssemblerTestState state(this, bytecode);
      InterpreterAssemblerForTest m(&state, bytecode);
      ConvertReceiverMode receiver_mode = Bytecodes::GetReceiverMode(bytecode);
      TailCallMode tail_call_mode = (bytecode == Bytecode::kTailCall)
                                        ? TailCallMode::kAllow
                                        : TailCallMode::kDisallow;

      Callable builtin = CodeFactory::InterpreterPushArgsThenCall(
          isolate(), receiver_mode, tail_call_mode,
          InterpreterPushArgsMode::kOther);
      Node* function = m.IntPtrConstant(0);
      Node* first_arg = m.IntPtrConstant(1);
      Node* arg_count = m.Int32Constant(2);
      Node* context = m.IntPtrConstant(3);
      Node* call_js = m.CallJS(function, context, first_arg, arg_count,
                               receiver_mode, tail_call_mode);
      EXPECT_THAT(call_js, IsCall(_, IsHeapConstant(builtin.code()), arg_count,
                                  first_arg, function, context, _, _));
    }
  }
}

TARGET_TEST_F(InterpreterAssemblerTest, LoadFeedbackVector) {
  TRACED_FOREACH(interpreter::Bytecode, bytecode, kBytecodes) {
    InterpreterAssemblerTestState state(this, bytecode);
    InterpreterAssemblerForTest m(&state, bytecode);
    Node* feedback_vector = m.LoadFeedbackVector();

    Matcher<Node*> load_function_matcher =
        m.IsLoad(MachineType::AnyTagged(), IsLoadParentFramePointer(),
                 IsIntPtrConstant(Register::function_closure().ToOperand()
                                  << kPointerSizeLog2));
    Matcher<Node*> load_vector_cell_matcher = m.IsLoad(
        MachineType::AnyTagged(), load_function_matcher,
        IsIntPtrConstant(JSFunction::kFeedbackVectorOffset - kHeapObjectTag));
    EXPECT_THAT(
        feedback_vector,
        m.IsLoad(MachineType::AnyTagged(), load_vector_cell_matcher,
                 IsIntPtrConstant(Cell::kValueOffset - kHeapObjectTag)));
  }
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
