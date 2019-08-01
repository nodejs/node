// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/init/v8.h"

#include "src/interpreter/bytecode-array-builder.h"
#include "src/interpreter/bytecode-array-iterator.h"
#include "src/numbers/hash-seed-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/smi.h"
#include "test/unittests/interpreter/bytecode-utils.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace interpreter {

class BytecodeArrayIteratorTest : public TestWithIsolateAndZone {
 public:
  BytecodeArrayIteratorTest() = default;
  ~BytecodeArrayIteratorTest() override = default;
};

TEST_F(BytecodeArrayIteratorTest, IteratesBytecodeArray) {
  // Use a builder to create an array with containing multiple bytecodes
  // with 0, 1 and 2 operands.
  FeedbackVectorSpec feedback_spec(zone());
  BytecodeArrayBuilder builder(zone(), 3, 3, &feedback_spec);
  AstValueFactory ast_factory(zone(), isolate()->ast_string_constants(),
                              HashSeed(isolate()));
  double heap_num_0 = 2.718;
  double heap_num_1 = 2.0 * Smi::kMaxValue;
  Smi zero = Smi::zero();
  Smi smi_0 = Smi::FromInt(64);
  Smi smi_1 = Smi::FromInt(-65536);
  Register reg_0(0);
  Register reg_1(1);
  RegisterList pair = BytecodeUtils::NewRegisterList(0, 2);
  RegisterList triple = BytecodeUtils::NewRegisterList(0, 3);
  Register param = Register::FromParameterIndex(2, builder.parameter_count());
  const AstRawString* name = ast_factory.GetOneByteString("abc");
  uint32_t name_index = 2;
  uint32_t load_feedback_slot = feedback_spec.AddLoadICSlot().ToInt();
  uint32_t forin_feedback_slot = feedback_spec.AddForInSlot().ToInt();
  uint32_t load_global_feedback_slot =
      feedback_spec.AddLoadGlobalICSlot(TypeofMode::NOT_INSIDE_TYPEOF).ToInt();

  builder.LoadLiteral(heap_num_0)
      .StoreAccumulatorInRegister(reg_0)
      .LoadLiteral(heap_num_1)
      .StoreAccumulatorInRegister(reg_0)
      .LoadLiteral(zero)
      .StoreAccumulatorInRegister(reg_0)
      .LoadLiteral(smi_0)
      .StackCheck(0)
      .StoreAccumulatorInRegister(reg_0)
      .LoadLiteral(smi_1)
      .StackCheck(1)
      .StoreAccumulatorInRegister(reg_1)
      .LoadAccumulatorWithRegister(reg_0)
      .BinaryOperation(Token::Value::ADD, reg_0, 2)
      .StoreAccumulatorInRegister(reg_1)
      .LoadNamedProperty(reg_1, name, load_feedback_slot)
      .BinaryOperation(Token::Value::ADD, reg_0, 3)
      .StoreAccumulatorInRegister(param)
      .CallRuntimeForPair(Runtime::kLoadLookupSlotForCall, param, pair)
      .ForInPrepare(triple, forin_feedback_slot)
      .CallRuntime(Runtime::kLoadIC_Miss, reg_0)
      .Debugger()
      .LoadGlobal(name, load_global_feedback_slot,
                  TypeofMode::NOT_INSIDE_TYPEOF)
      .Return();

  // Test iterator sees the expected output from the builder.
  ast_factory.Internalize(isolate());
  BytecodeArrayIterator iterator(builder.ToBytecodeArray(isolate()));
  const int kPrefixByteSize = 1;
  int offset = 0;

  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kLdaConstant);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(iterator.GetConstantForIndexOperand(0).Number(), heap_num_0);
  CHECK(!iterator.done());
  offset += Bytecodes::Size(Bytecode::kLdaConstant, OperandScale::kSingle);
  iterator.Advance();

  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kStar);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(iterator.GetRegisterOperand(0).index(), reg_0.index());
  EXPECT_EQ(iterator.GetRegisterOperandRange(0), 1);
  CHECK(!iterator.done());
  offset += Bytecodes::Size(Bytecode::kStar, OperandScale::kSingle);
  iterator.Advance();

  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kLdaConstant);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(iterator.GetConstantForIndexOperand(0).Number(), heap_num_1);
  CHECK(!iterator.done());
  offset += Bytecodes::Size(Bytecode::kLdaConstant, OperandScale::kSingle);
  iterator.Advance();

  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kStar);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(iterator.GetRegisterOperand(0).index(), reg_0.index());
  EXPECT_EQ(iterator.GetRegisterOperandRange(0), 1);
  CHECK(!iterator.done());
  offset += Bytecodes::Size(Bytecode::kStar, OperandScale::kSingle);
  iterator.Advance();

  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kLdaZero);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  CHECK(!iterator.done());
  offset += Bytecodes::Size(Bytecode::kLdaZero, OperandScale::kSingle);
  iterator.Advance();

  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kStar);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(iterator.GetRegisterOperand(0).index(), reg_0.index());
  EXPECT_EQ(iterator.GetRegisterOperandRange(0), 1);
  CHECK(!iterator.done());
  offset += Bytecodes::Size(Bytecode::kStar, OperandScale::kSingle);
  iterator.Advance();

  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kLdaSmi);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(Smi::FromInt(iterator.GetImmediateOperand(0)), smi_0);
  CHECK(!iterator.done());
  offset += Bytecodes::Size(Bytecode::kLdaSmi, OperandScale::kSingle);
  iterator.Advance();

  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kStackCheck);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(Bytecodes::NumberOfOperands(iterator.current_bytecode()), 0);
  CHECK(!iterator.done());
  offset += Bytecodes::Size(Bytecode::kStackCheck, OperandScale::kSingle);
  iterator.Advance();

  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kStar);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(iterator.GetRegisterOperand(0).index(), reg_0.index());
  EXPECT_EQ(iterator.GetRegisterOperandRange(0), 1);
  CHECK(!iterator.done());
  offset += Bytecodes::Size(Bytecode::kStar, OperandScale::kSingle);
  iterator.Advance();

  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kLdaSmi);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kQuadruple);
  EXPECT_EQ(Smi::FromInt(iterator.GetImmediateOperand(0)), smi_1);
  CHECK(!iterator.done());
  offset += Bytecodes::Size(Bytecode::kLdaSmi, OperandScale::kQuadruple) +
            kPrefixByteSize;
  iterator.Advance();

  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kStackCheck);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(Bytecodes::NumberOfOperands(iterator.current_bytecode()), 0);
  CHECK(!iterator.done());
  offset += Bytecodes::Size(Bytecode::kStackCheck, OperandScale::kSingle);
  iterator.Advance();

  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kStar);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(iterator.GetRegisterOperand(0).index(), reg_1.index());
  EXPECT_EQ(iterator.GetRegisterOperandRange(0), 1);
  CHECK(!iterator.done());
  offset += Bytecodes::Size(Bytecode::kStar, OperandScale::kSingle);
  iterator.Advance();

  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kLdar);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(iterator.GetRegisterOperand(0).index(), reg_0.index());
  CHECK(!iterator.done());
  offset += Bytecodes::Size(Bytecode::kLdar, OperandScale::kSingle);
  iterator.Advance();

  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kAdd);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(iterator.GetRegisterOperand(0).index(), reg_0.index());
  EXPECT_EQ(iterator.GetRegisterOperandRange(0), 1);
  CHECK(!iterator.done());
  offset += Bytecodes::Size(Bytecode::kAdd, OperandScale::kSingle);
  iterator.Advance();

  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kStar);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(iterator.GetRegisterOperand(0).index(), reg_1.index());
  EXPECT_EQ(iterator.GetRegisterOperandRange(0), 1);
  CHECK(!iterator.done());
  offset += Bytecodes::Size(Bytecode::kStar, OperandScale::kSingle);
  iterator.Advance();

  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kLdaNamedProperty);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(iterator.GetRegisterOperand(0).index(), reg_1.index());
  EXPECT_EQ(iterator.GetIndexOperand(1), name_index);
  EXPECT_EQ(iterator.GetIndexOperand(2), load_feedback_slot);
  CHECK(!iterator.done());
  offset += Bytecodes::Size(Bytecode::kLdaNamedProperty, OperandScale::kSingle);
  iterator.Advance();

  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kAdd);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(iterator.GetRegisterOperand(0).index(), reg_0.index());
  EXPECT_EQ(iterator.GetRegisterOperandRange(0), 1);
  CHECK(!iterator.done());
  offset += Bytecodes::Size(Bytecode::kAdd, OperandScale::kSingle);
  iterator.Advance();

  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kStar);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(iterator.GetRegisterOperand(0).index(), param.index());
  EXPECT_EQ(iterator.GetRegisterOperandRange(0), 1);
  CHECK(!iterator.done());
  offset += Bytecodes::Size(Bytecode::kStar, OperandScale::kSingle);
  iterator.Advance();

  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kCallRuntimeForPair);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(iterator.GetRuntimeIdOperand(0), Runtime::kLoadLookupSlotForCall);
  EXPECT_EQ(iterator.GetRegisterOperand(1).index(), param.index());
  EXPECT_EQ(iterator.GetRegisterOperandRange(1), 1);
  EXPECT_EQ(iterator.GetRegisterCountOperand(2), 1u);
  EXPECT_EQ(iterator.GetRegisterOperand(3).index(), reg_0.index());
  EXPECT_EQ(iterator.GetRegisterOperandRange(3), 2);
  CHECK(!iterator.done());
  offset +=
      Bytecodes::Size(Bytecode::kCallRuntimeForPair, OperandScale::kSingle);
  iterator.Advance();

  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kForInPrepare);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(iterator.GetRegisterOperand(0).index(), reg_0.index());
  EXPECT_EQ(iterator.GetRegisterOperandRange(0), 3);
  EXPECT_EQ(iterator.GetIndexOperand(1), forin_feedback_slot);
  CHECK(!iterator.done());
  offset += Bytecodes::Size(Bytecode::kForInPrepare, OperandScale::kSingle);
  iterator.Advance();

  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kCallRuntime);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(iterator.GetRuntimeIdOperand(0), Runtime::kLoadIC_Miss);
  EXPECT_EQ(iterator.GetRegisterOperand(1).index(), reg_0.index());
  EXPECT_EQ(iterator.GetRegisterCountOperand(2), 1u);
  CHECK(!iterator.done());
  offset += Bytecodes::Size(Bytecode::kCallRuntime, OperandScale::kSingle);
  iterator.Advance();

  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kDebugger);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  CHECK(!iterator.done());
  offset += Bytecodes::Size(Bytecode::kDebugger, OperandScale::kSingle);
  iterator.Advance();

  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kLdaGlobal);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(iterator.current_bytecode_size(), 3);
  EXPECT_EQ(iterator.GetIndexOperand(1), load_global_feedback_slot);
  offset += Bytecodes::Size(Bytecode::kLdaGlobal, OperandScale::kSingle);
  iterator.Advance();

  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kReturn);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  CHECK(!iterator.done());
  iterator.Advance();
  CHECK(iterator.done());
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
