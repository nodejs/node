// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/interpreter/bytecode-array-builder.h"
#include "src/interpreter/bytecode-array-random-iterator.h"
#include "src/objects-inl.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace interpreter {

class BytecodeArrayRandomIteratorTest : public TestWithIsolateAndZone {
 public:
  BytecodeArrayRandomIteratorTest() {}
  ~BytecodeArrayRandomIteratorTest() override {}
};

TEST_F(BytecodeArrayRandomIteratorTest, InvalidBeforeStart) {
  // Use a builder to create an array with containing multiple bytecodes
  // with 0, 1 and 2 operands.
  BytecodeArrayBuilder builder(isolate(), zone(), 3, 3, 0);
  AstValueFactory ast_factory(zone(), isolate()->ast_string_constants(),
                              isolate()->heap()->HashSeed());
  const AstValue* heap_num_0 = ast_factory.NewNumber(2.718);
  const AstValue* heap_num_1 = ast_factory.NewNumber(2.0 * Smi::kMaxValue);
  Smi* zero = Smi::kZero;
  Smi* smi_0 = Smi::FromInt(64);
  Smi* smi_1 = Smi::FromInt(-65536);
  Register reg_0(0);
  Register reg_1(1);
  RegisterList pair(0, 2);
  RegisterList triple(0, 3);
  Register param = Register::FromParameterIndex(2, builder.parameter_count());
  const AstRawString* name = ast_factory.GetOneByteString("abc");
  uint32_t feedback_slot = 97;

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
      .LoadNamedProperty(reg_1, name, feedback_slot)
      .BinaryOperation(Token::Value::ADD, reg_0, 3)
      .StoreAccumulatorInRegister(param)
      .CallRuntimeForPair(Runtime::kLoadLookupSlotForCall, param, pair)
      .ForInPrepare(reg_0, triple)
      .CallRuntime(Runtime::kLoadIC_Miss, reg_0)
      .Debugger()
      .LoadGlobal(name, 0x10000000, TypeofMode::NOT_INSIDE_TYPEOF)
      .Return();

  ast_factory.Internalize(isolate());
  Handle<BytecodeArray> bytecodeArray = builder.ToBytecodeArray(isolate());
  BytecodeArrayRandomIterator iterator(bytecodeArray, zone());

  iterator.GoToStart();
  ASSERT_TRUE(iterator.IsValid());
  --iterator;
  ASSERT_FALSE(iterator.IsValid());
}

TEST_F(BytecodeArrayRandomIteratorTest, InvalidAfterEnd) {
  // Use a builder to create an array with containing multiple bytecodes
  // with 0, 1 and 2 operands.
  BytecodeArrayBuilder builder(isolate(), zone(), 3, 3, 0);
  AstValueFactory ast_factory(zone(), isolate()->ast_string_constants(),
                              isolate()->heap()->HashSeed());
  const AstValue* heap_num_0 = ast_factory.NewNumber(2.718);
  const AstValue* heap_num_1 = ast_factory.NewNumber(2.0 * Smi::kMaxValue);
  Smi* zero = Smi::kZero;
  Smi* smi_0 = Smi::FromInt(64);
  Smi* smi_1 = Smi::FromInt(-65536);
  Register reg_0(0);
  Register reg_1(1);
  RegisterList pair(0, 2);
  RegisterList triple(0, 3);
  Register param = Register::FromParameterIndex(2, builder.parameter_count());
  const AstRawString* name = ast_factory.GetOneByteString("abc");
  uint32_t feedback_slot = 97;

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
      .LoadNamedProperty(reg_1, name, feedback_slot)
      .BinaryOperation(Token::Value::ADD, reg_0, 3)
      .StoreAccumulatorInRegister(param)
      .CallRuntimeForPair(Runtime::kLoadLookupSlotForCall, param, pair)
      .ForInPrepare(reg_0, triple)
      .CallRuntime(Runtime::kLoadIC_Miss, reg_0)
      .Debugger()
      .LoadGlobal(name, 0x10000000, TypeofMode::NOT_INSIDE_TYPEOF)
      .Return();

  ast_factory.Internalize(isolate());
  Handle<BytecodeArray> bytecodeArray = builder.ToBytecodeArray(isolate());
  BytecodeArrayRandomIterator iterator(bytecodeArray, zone());

  iterator.GoToEnd();
  ASSERT_TRUE(iterator.IsValid());
  ++iterator;
  ASSERT_FALSE(iterator.IsValid());
}

TEST_F(BytecodeArrayRandomIteratorTest, AccessesFirst) {
  // Use a builder to create an array with containing multiple bytecodes
  // with 0, 1 and 2 operands.
  BytecodeArrayBuilder builder(isolate(), zone(), 3, 3, 0);
  AstValueFactory ast_factory(zone(), isolate()->ast_string_constants(),
                              isolate()->heap()->HashSeed());
  const AstValue* heap_num_0 = ast_factory.NewNumber(2.718);
  const AstValue* heap_num_1 = ast_factory.NewNumber(2.0 * Smi::kMaxValue);
  Smi* zero = Smi::kZero;
  Smi* smi_0 = Smi::FromInt(64);
  Smi* smi_1 = Smi::FromInt(-65536);
  Register reg_0(0);
  Register reg_1(1);
  RegisterList pair(0, 2);
  RegisterList triple(0, 3);
  Register param = Register::FromParameterIndex(2, builder.parameter_count());
  const AstRawString* name = ast_factory.GetOneByteString("abc");
  uint32_t feedback_slot = 97;

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
      .LoadNamedProperty(reg_1, name, feedback_slot)
      .BinaryOperation(Token::Value::ADD, reg_0, 3)
      .StoreAccumulatorInRegister(param)
      .CallRuntimeForPair(Runtime::kLoadLookupSlotForCall, param, pair)
      .ForInPrepare(reg_0, triple)
      .CallRuntime(Runtime::kLoadIC_Miss, reg_0)
      .Debugger()
      .LoadGlobal(name, 0x10000000, TypeofMode::NOT_INSIDE_TYPEOF)
      .Return();

  ast_factory.Internalize(isolate());
  Handle<BytecodeArray> bytecodeArray = builder.ToBytecodeArray(isolate());
  BytecodeArrayRandomIterator iterator(bytecodeArray, zone());

  iterator.GoToStart();

  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kLdaConstant);
  EXPECT_EQ(iterator.current_index(), 0);
  EXPECT_EQ(iterator.current_offset(), 0);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_TRUE(iterator.GetConstantForIndexOperand(0).is_identical_to(
      heap_num_0->value()));
  ASSERT_TRUE(iterator.IsValid());
}

TEST_F(BytecodeArrayRandomIteratorTest, AccessesLast) {
  // Use a builder to create an array with containing multiple bytecodes
  // with 0, 1 and 2 operands.
  BytecodeArrayBuilder builder(isolate(), zone(), 3, 3, 0);
  AstValueFactory ast_factory(zone(), isolate()->ast_string_constants(),
                              isolate()->heap()->HashSeed());
  const AstValue* heap_num_0 = ast_factory.NewNumber(2.718);
  const AstValue* heap_num_1 = ast_factory.NewNumber(2.0 * Smi::kMaxValue);
  Smi* zero = Smi::kZero;
  Smi* smi_0 = Smi::FromInt(64);
  Smi* smi_1 = Smi::FromInt(-65536);
  Register reg_0(0);
  Register reg_1(1);
  RegisterList pair(0, 2);
  RegisterList triple(0, 3);
  Register param = Register::FromParameterIndex(2, builder.parameter_count());
  const AstRawString* name = ast_factory.GetOneByteString("abc");
  uint32_t feedback_slot = 97;

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
      .LoadNamedProperty(reg_1, name, feedback_slot)
      .BinaryOperation(Token::Value::ADD, reg_0, 3)
      .StoreAccumulatorInRegister(param)
      .CallRuntimeForPair(Runtime::kLoadLookupSlotForCall, param, pair)
      .ForInPrepare(reg_0, triple)
      .CallRuntime(Runtime::kLoadIC_Miss, reg_0)
      .Debugger()
      .LoadGlobal(name, 0x10000000, TypeofMode::NOT_INSIDE_TYPEOF)
      .Return();

  ast_factory.Internalize(isolate());
  Handle<BytecodeArray> bytecodeArray = builder.ToBytecodeArray(isolate());
  BytecodeArrayRandomIterator iterator(bytecodeArray, zone());

  iterator.GoToEnd();

  int offset = bytecodeArray->length() -
               Bytecodes::Size(Bytecode::kReturn, OperandScale::kSingle);
  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kReturn);
  EXPECT_EQ(iterator.current_index(), 23);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  ASSERT_TRUE(iterator.IsValid());
}

TEST_F(BytecodeArrayRandomIteratorTest, RandomAccessValid) {
  // Use a builder to create an array with containing multiple bytecodes
  // with 0, 1 and 2 operands.
  BytecodeArrayBuilder builder(isolate(), zone(), 3, 3, 0);
  AstValueFactory ast_factory(zone(), isolate()->ast_string_constants(),
                              isolate()->heap()->HashSeed());
  const AstValue* heap_num_0 = ast_factory.NewNumber(2.718);
  const AstValue* heap_num_1 = ast_factory.NewNumber(2.0 * Smi::kMaxValue);
  Smi* zero = Smi::kZero;
  Smi* smi_0 = Smi::FromInt(64);
  Smi* smi_1 = Smi::FromInt(-65536);
  Register reg_0(0);
  Register reg_1(1);
  RegisterList pair(0, 2);
  RegisterList triple(0, 3);
  Register param = Register::FromParameterIndex(2, builder.parameter_count());
  const AstRawString* name = ast_factory.GetOneByteString("abc");
  uint32_t name_index = 2;
  uint32_t feedback_slot = 97;

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
      .LoadNamedProperty(reg_1, name, feedback_slot)
      .BinaryOperation(Token::Value::ADD, reg_0, 3)
      .StoreAccumulatorInRegister(param)
      .CallRuntimeForPair(Runtime::kLoadLookupSlotForCall, param, pair)
      .ForInPrepare(reg_0, triple)
      .CallRuntime(Runtime::kLoadIC_Miss, reg_0)
      .Debugger()
      .LoadGlobal(name, 0x10000000, TypeofMode::NOT_INSIDE_TYPEOF)
      .Return();

  // Test iterator sees the expected output from the builder.
  ast_factory.Internalize(isolate());
  BytecodeArrayRandomIterator iterator(builder.ToBytecodeArray(isolate()),
                                       zone());
  const int kPrefixByteSize = 1;
  int offset = 0;

  iterator.GoToIndex(13);
  offset = Bytecodes::Size(Bytecode::kLdaConstant, OperandScale::kSingle);
  offset += Bytecodes::Size(Bytecode::kStar, OperandScale::kSingle);
  offset += Bytecodes::Size(Bytecode::kLdaConstant, OperandScale::kSingle);
  offset += Bytecodes::Size(Bytecode::kStar, OperandScale::kSingle);
  offset += Bytecodes::Size(Bytecode::kLdaZero, OperandScale::kSingle);
  offset += Bytecodes::Size(Bytecode::kStar, OperandScale::kSingle);
  offset += Bytecodes::Size(Bytecode::kLdaSmi, OperandScale::kSingle);
  offset += Bytecodes::Size(Bytecode::kStackCheck, OperandScale::kSingle);
  offset += Bytecodes::Size(Bytecode::kStar, OperandScale::kSingle);
  offset += Bytecodes::Size(Bytecode::kLdaSmi, OperandScale::kQuadruple) +
            kPrefixByteSize;
  offset += Bytecodes::Size(Bytecode::kStackCheck, OperandScale::kSingle);
  offset += Bytecodes::Size(Bytecode::kStar, OperandScale::kSingle);
  offset += Bytecodes::Size(Bytecode::kLdar, OperandScale::kSingle);

  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kAdd);
  EXPECT_EQ(iterator.current_index(), 13);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(iterator.GetRegisterOperand(0).index(), reg_0.index());
  EXPECT_EQ(iterator.GetRegisterOperandRange(0), 1);
  ASSERT_TRUE(iterator.IsValid());

  iterator.GoToIndex(2);
  offset = Bytecodes::Size(Bytecode::kLdaConstant, OperandScale::kSingle);
  offset += Bytecodes::Size(Bytecode::kStar, OperandScale::kSingle);

  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kLdaConstant);
  EXPECT_EQ(iterator.current_index(), 2);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_TRUE(iterator.GetConstantForIndexOperand(0).is_identical_to(
      heap_num_1->value()));
  ASSERT_TRUE(iterator.IsValid());

  iterator.GoToIndex(18);
  offset = Bytecodes::Size(Bytecode::kLdaConstant, OperandScale::kSingle);
  offset += Bytecodes::Size(Bytecode::kStar, OperandScale::kSingle);
  offset += Bytecodes::Size(Bytecode::kLdaConstant, OperandScale::kSingle);
  offset += Bytecodes::Size(Bytecode::kStar, OperandScale::kSingle);
  offset += Bytecodes::Size(Bytecode::kLdaZero, OperandScale::kSingle);
  offset += Bytecodes::Size(Bytecode::kStar, OperandScale::kSingle);
  offset += Bytecodes::Size(Bytecode::kLdaSmi, OperandScale::kSingle);
  offset += Bytecodes::Size(Bytecode::kStackCheck, OperandScale::kSingle);
  offset += Bytecodes::Size(Bytecode::kStar, OperandScale::kSingle);
  offset += Bytecodes::Size(Bytecode::kLdaSmi, OperandScale::kQuadruple) +
            kPrefixByteSize;
  offset += Bytecodes::Size(Bytecode::kStackCheck, OperandScale::kSingle);
  offset += Bytecodes::Size(Bytecode::kStar, OperandScale::kSingle);
  offset += Bytecodes::Size(Bytecode::kLdar, OperandScale::kSingle);
  offset += Bytecodes::Size(Bytecode::kAdd, OperandScale::kSingle);
  offset += Bytecodes::Size(Bytecode::kStar, OperandScale::kSingle);
  offset += Bytecodes::Size(Bytecode::kLdaNamedProperty, OperandScale::kSingle);
  offset += Bytecodes::Size(Bytecode::kAdd, OperandScale::kSingle);
  offset += Bytecodes::Size(Bytecode::kStar, OperandScale::kSingle);

  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kCallRuntimeForPair);
  EXPECT_EQ(iterator.current_index(), 18);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(iterator.GetRuntimeIdOperand(0), Runtime::kLoadLookupSlotForCall);
  EXPECT_EQ(iterator.GetRegisterOperand(1).index(), param.index());
  EXPECT_EQ(iterator.GetRegisterOperandRange(1), 1);
  EXPECT_EQ(iterator.GetRegisterCountOperand(2), 1u);
  EXPECT_EQ(iterator.GetRegisterOperand(3).index(), reg_0.index());
  EXPECT_EQ(iterator.GetRegisterOperandRange(3), 2);
  ASSERT_TRUE(iterator.IsValid());

  iterator -= 3;
  offset -= Bytecodes::Size(Bytecode::kStar, OperandScale::kSingle);
  offset -= Bytecodes::Size(Bytecode::kAdd, OperandScale::kSingle);
  offset -= Bytecodes::Size(Bytecode::kLdaNamedProperty, OperandScale::kSingle);

  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kLdaNamedProperty);
  EXPECT_EQ(iterator.current_index(), 15);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(iterator.GetRegisterOperand(0).index(), reg_1.index());
  EXPECT_EQ(iterator.GetIndexOperand(1), name_index);
  EXPECT_EQ(iterator.GetIndexOperand(2), feedback_slot);
  ASSERT_TRUE(iterator.IsValid());

  iterator += 2;
  offset += Bytecodes::Size(Bytecode::kLdaNamedProperty, OperandScale::kSingle);
  offset += Bytecodes::Size(Bytecode::kAdd, OperandScale::kSingle);

  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kStar);
  EXPECT_EQ(iterator.current_index(), 17);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(iterator.GetRegisterOperand(0).index(), param.index());
  EXPECT_EQ(iterator.GetRegisterOperandRange(0), 1);
  ASSERT_TRUE(iterator.IsValid());

  iterator.GoToIndex(23);
  offset = Bytecodes::Size(Bytecode::kLdaConstant, OperandScale::kSingle);
  offset += Bytecodes::Size(Bytecode::kStar, OperandScale::kSingle);
  offset += Bytecodes::Size(Bytecode::kLdaConstant, OperandScale::kSingle);
  offset += Bytecodes::Size(Bytecode::kStar, OperandScale::kSingle);
  offset += Bytecodes::Size(Bytecode::kLdaZero, OperandScale::kSingle);
  offset += Bytecodes::Size(Bytecode::kStar, OperandScale::kSingle);
  offset += Bytecodes::Size(Bytecode::kLdaSmi, OperandScale::kSingle);
  offset += Bytecodes::Size(Bytecode::kStackCheck, OperandScale::kSingle);
  offset += Bytecodes::Size(Bytecode::kStar, OperandScale::kSingle);
  offset += Bytecodes::Size(Bytecode::kLdaSmi, OperandScale::kQuadruple) +
            kPrefixByteSize;
  offset += Bytecodes::Size(Bytecode::kStackCheck, OperandScale::kSingle);
  offset += Bytecodes::Size(Bytecode::kStar, OperandScale::kSingle);
  offset += Bytecodes::Size(Bytecode::kLdar, OperandScale::kSingle);
  offset += Bytecodes::Size(Bytecode::kAdd, OperandScale::kSingle);
  offset += Bytecodes::Size(Bytecode::kStar, OperandScale::kSingle);
  offset += Bytecodes::Size(Bytecode::kLdaNamedProperty, OperandScale::kSingle);
  offset += Bytecodes::Size(Bytecode::kAdd, OperandScale::kSingle);
  offset += Bytecodes::Size(Bytecode::kStar, OperandScale::kSingle);
  offset +=
      Bytecodes::Size(Bytecode::kCallRuntimeForPair, OperandScale::kSingle);
  offset += Bytecodes::Size(Bytecode::kForInPrepare, OperandScale::kSingle);
  offset += Bytecodes::Size(Bytecode::kCallRuntime, OperandScale::kSingle);
  offset += Bytecodes::Size(Bytecode::kDebugger, OperandScale::kSingle);
  offset += Bytecodes::Size(Bytecode::kLdaGlobal, OperandScale::kQuadruple) +
            kPrefixByteSize;

  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kReturn);
  EXPECT_EQ(iterator.current_index(), 23);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  ASSERT_TRUE(iterator.IsValid());

  iterator.GoToIndex(24);
  EXPECT_FALSE(iterator.IsValid());

  iterator.GoToIndex(-5);
  EXPECT_FALSE(iterator.IsValid());
}

TEST_F(BytecodeArrayRandomIteratorTest, IteratesBytecodeArray) {
  // Use a builder to create an array with containing multiple bytecodes
  // with 0, 1 and 2 operands.
  BytecodeArrayBuilder builder(isolate(), zone(), 3, 3, 0);
  AstValueFactory ast_factory(zone(), isolate()->ast_string_constants(),
                              isolate()->heap()->HashSeed());
  const AstValue* heap_num_0 = ast_factory.NewNumber(2.718);
  const AstValue* heap_num_1 = ast_factory.NewNumber(2.0 * Smi::kMaxValue);
  Smi* zero = Smi::kZero;
  Smi* smi_0 = Smi::FromInt(64);
  Smi* smi_1 = Smi::FromInt(-65536);
  Register reg_0(0);
  Register reg_1(1);
  RegisterList pair(0, 2);
  RegisterList triple(0, 3);
  Register param = Register::FromParameterIndex(2, builder.parameter_count());
  const AstRawString* name = ast_factory.GetOneByteString("abc");
  uint32_t name_index = 2;
  uint32_t feedback_slot = 97;

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
      .LoadNamedProperty(reg_1, name, feedback_slot)
      .BinaryOperation(Token::Value::ADD, reg_0, 3)
      .StoreAccumulatorInRegister(param)
      .CallRuntimeForPair(Runtime::kLoadLookupSlotForCall, param, pair)
      .ForInPrepare(reg_0, triple)
      .CallRuntime(Runtime::kLoadIC_Miss, reg_0)
      .Debugger()
      .LoadGlobal(name, 0x10000000, TypeofMode::NOT_INSIDE_TYPEOF)
      .Return();

  // Test iterator sees the expected output from the builder.
  ast_factory.Internalize(isolate());
  BytecodeArrayRandomIterator iterator(builder.ToBytecodeArray(isolate()),
                                       zone());
  const int kPrefixByteSize = 1;
  int offset = 0;

  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kLdaConstant);
  EXPECT_EQ(iterator.current_index(), 0);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_TRUE(iterator.GetConstantForIndexOperand(0).is_identical_to(
      heap_num_0->value()));
  ASSERT_TRUE(iterator.IsValid());
  offset += Bytecodes::Size(Bytecode::kLdaConstant, OperandScale::kSingle);
  ++iterator;

  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kStar);
  EXPECT_EQ(iterator.current_index(), 1);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(iterator.GetRegisterOperand(0).index(), reg_0.index());
  EXPECT_EQ(iterator.GetRegisterOperandRange(0), 1);
  ASSERT_TRUE(iterator.IsValid());
  offset += Bytecodes::Size(Bytecode::kStar, OperandScale::kSingle);
  ++iterator;

  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kLdaConstant);
  EXPECT_EQ(iterator.current_index(), 2);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_TRUE(iterator.GetConstantForIndexOperand(0).is_identical_to(
      heap_num_1->value()));
  ASSERT_TRUE(iterator.IsValid());
  offset += Bytecodes::Size(Bytecode::kLdaConstant, OperandScale::kSingle);
  ++iterator;

  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kStar);
  EXPECT_EQ(iterator.current_index(), 3);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(iterator.GetRegisterOperand(0).index(), reg_0.index());
  EXPECT_EQ(iterator.GetRegisterOperandRange(0), 1);
  ASSERT_TRUE(iterator.IsValid());
  offset += Bytecodes::Size(Bytecode::kStar, OperandScale::kSingle);
  ++iterator;

  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kLdaZero);
  EXPECT_EQ(iterator.current_index(), 4);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  ASSERT_TRUE(iterator.IsValid());
  offset += Bytecodes::Size(Bytecode::kLdaZero, OperandScale::kSingle);
  ++iterator;

  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kStar);
  EXPECT_EQ(iterator.current_index(), 5);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(iterator.GetRegisterOperand(0).index(), reg_0.index());
  EXPECT_EQ(iterator.GetRegisterOperandRange(0), 1);
  ASSERT_TRUE(iterator.IsValid());
  offset += Bytecodes::Size(Bytecode::kStar, OperandScale::kSingle);
  ++iterator;

  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kLdaSmi);
  EXPECT_EQ(iterator.current_index(), 6);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(Smi::FromInt(iterator.GetImmediateOperand(0)), smi_0);
  ASSERT_TRUE(iterator.IsValid());
  offset += Bytecodes::Size(Bytecode::kLdaSmi, OperandScale::kSingle);
  ++iterator;

  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kStackCheck);
  EXPECT_EQ(iterator.current_index(), 7);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(Bytecodes::NumberOfOperands(iterator.current_bytecode()), 0);
  ASSERT_TRUE(iterator.IsValid());
  offset += Bytecodes::Size(Bytecode::kStackCheck, OperandScale::kSingle);
  ++iterator;

  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kStar);
  EXPECT_EQ(iterator.current_index(), 8);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(iterator.GetRegisterOperand(0).index(), reg_0.index());
  EXPECT_EQ(iterator.GetRegisterOperandRange(0), 1);
  ASSERT_TRUE(iterator.IsValid());
  offset += Bytecodes::Size(Bytecode::kStar, OperandScale::kSingle);
  ++iterator;

  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kLdaSmi);
  EXPECT_EQ(iterator.current_index(), 9);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kQuadruple);
  EXPECT_EQ(Smi::FromInt(iterator.GetImmediateOperand(0)), smi_1);
  ASSERT_TRUE(iterator.IsValid());
  offset += Bytecodes::Size(Bytecode::kLdaSmi, OperandScale::kQuadruple) +
            kPrefixByteSize;
  ++iterator;

  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kStackCheck);
  EXPECT_EQ(iterator.current_index(), 10);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(Bytecodes::NumberOfOperands(iterator.current_bytecode()), 0);
  ASSERT_TRUE(iterator.IsValid());
  offset += Bytecodes::Size(Bytecode::kStackCheck, OperandScale::kSingle);
  ++iterator;

  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kStar);
  EXPECT_EQ(iterator.current_index(), 11);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(iterator.GetRegisterOperand(0).index(), reg_1.index());
  EXPECT_EQ(iterator.GetRegisterOperandRange(0), 1);
  ASSERT_TRUE(iterator.IsValid());
  offset += Bytecodes::Size(Bytecode::kStar, OperandScale::kSingle);
  ++iterator;

  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kLdar);
  EXPECT_EQ(iterator.current_index(), 12);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(iterator.GetRegisterOperand(0).index(), reg_0.index());
  ASSERT_TRUE(iterator.IsValid());
  offset += Bytecodes::Size(Bytecode::kLdar, OperandScale::kSingle);
  ++iterator;

  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kAdd);
  EXPECT_EQ(iterator.current_index(), 13);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(iterator.GetRegisterOperand(0).index(), reg_0.index());
  EXPECT_EQ(iterator.GetRegisterOperandRange(0), 1);
  ASSERT_TRUE(iterator.IsValid());
  offset += Bytecodes::Size(Bytecode::kAdd, OperandScale::kSingle);
  ++iterator;

  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kStar);
  EXPECT_EQ(iterator.current_index(), 14);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(iterator.GetRegisterOperand(0).index(), reg_1.index());
  EXPECT_EQ(iterator.GetRegisterOperandRange(0), 1);
  ASSERT_TRUE(iterator.IsValid());
  offset += Bytecodes::Size(Bytecode::kStar, OperandScale::kSingle);
  ++iterator;

  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kLdaNamedProperty);
  EXPECT_EQ(iterator.current_index(), 15);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(iterator.GetRegisterOperand(0).index(), reg_1.index());
  EXPECT_EQ(iterator.GetIndexOperand(1), name_index);
  EXPECT_EQ(iterator.GetIndexOperand(2), feedback_slot);
  ASSERT_TRUE(iterator.IsValid());
  offset += Bytecodes::Size(Bytecode::kLdaNamedProperty, OperandScale::kSingle);
  ++iterator;

  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kAdd);
  EXPECT_EQ(iterator.current_index(), 16);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(iterator.GetRegisterOperand(0).index(), reg_0.index());
  EXPECT_EQ(iterator.GetRegisterOperandRange(0), 1);
  ASSERT_TRUE(iterator.IsValid());
  offset += Bytecodes::Size(Bytecode::kAdd, OperandScale::kSingle);
  ++iterator;

  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kStar);
  EXPECT_EQ(iterator.current_index(), 17);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(iterator.GetRegisterOperand(0).index(), param.index());
  EXPECT_EQ(iterator.GetRegisterOperandRange(0), 1);
  ASSERT_TRUE(iterator.IsValid());
  offset += Bytecodes::Size(Bytecode::kStar, OperandScale::kSingle);
  ++iterator;

  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kCallRuntimeForPair);
  EXPECT_EQ(iterator.current_index(), 18);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(iterator.GetRuntimeIdOperand(0), Runtime::kLoadLookupSlotForCall);
  EXPECT_EQ(iterator.GetRegisterOperand(1).index(), param.index());
  EXPECT_EQ(iterator.GetRegisterOperandRange(1), 1);
  EXPECT_EQ(iterator.GetRegisterCountOperand(2), 1u);
  EXPECT_EQ(iterator.GetRegisterOperand(3).index(), reg_0.index());
  EXPECT_EQ(iterator.GetRegisterOperandRange(3), 2);
  ASSERT_TRUE(iterator.IsValid());
  offset +=
      Bytecodes::Size(Bytecode::kCallRuntimeForPair, OperandScale::kSingle);
  ++iterator;

  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kForInPrepare);
  EXPECT_EQ(iterator.current_index(), 19);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(iterator.GetRegisterOperand(0).index(), reg_0.index());
  EXPECT_EQ(iterator.GetRegisterOperandRange(0), 1);
  EXPECT_EQ(iterator.GetRegisterOperand(1).index(), reg_0.index());
  EXPECT_EQ(iterator.GetRegisterOperandRange(1), 3);
  ASSERT_TRUE(iterator.IsValid());
  offset += Bytecodes::Size(Bytecode::kForInPrepare, OperandScale::kSingle);
  ++iterator;

  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kCallRuntime);
  EXPECT_EQ(iterator.current_index(), 20);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(iterator.GetRuntimeIdOperand(0), Runtime::kLoadIC_Miss);
  EXPECT_EQ(iterator.GetRegisterOperand(1).index(), reg_0.index());
  EXPECT_EQ(iterator.GetRegisterCountOperand(2), 1u);
  ASSERT_TRUE(iterator.IsValid());
  offset += Bytecodes::Size(Bytecode::kCallRuntime, OperandScale::kSingle);
  ++iterator;

  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kDebugger);
  EXPECT_EQ(iterator.current_index(), 21);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  ASSERT_TRUE(iterator.IsValid());
  offset += Bytecodes::Size(Bytecode::kDebugger, OperandScale::kSingle);
  ++iterator;

  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kLdaGlobal);
  EXPECT_EQ(iterator.current_index(), 22);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kQuadruple);
  EXPECT_EQ(iterator.current_bytecode_size(), 10);
  EXPECT_EQ(iterator.GetIndexOperand(1), 0x10000000u);
  offset += Bytecodes::Size(Bytecode::kLdaGlobal, OperandScale::kQuadruple) +
            kPrefixByteSize;
  ++iterator;

  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kReturn);
  EXPECT_EQ(iterator.current_index(), 23);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  ASSERT_TRUE(iterator.IsValid());
  ++iterator;
  ASSERT_TRUE(!iterator.IsValid());
}

TEST_F(BytecodeArrayRandomIteratorTest, IteratesBytecodeArrayBackwards) {
  // Use a builder to create an array with containing multiple bytecodes
  // with 0, 1 and 2 operands.
  BytecodeArrayBuilder builder(isolate(), zone(), 3, 3, 0);
  AstValueFactory ast_factory(zone(), isolate()->ast_string_constants(),
                              isolate()->heap()->HashSeed());
  const AstValue* heap_num_0 = ast_factory.NewNumber(2.718);
  const AstValue* heap_num_1 = ast_factory.NewNumber(2.0 * Smi::kMaxValue);
  Smi* zero = Smi::kZero;
  Smi* smi_0 = Smi::FromInt(64);
  Smi* smi_1 = Smi::FromInt(-65536);
  Register reg_0(0);
  Register reg_1(1);
  RegisterList pair(0, 2);
  RegisterList triple(0, 3);
  Register param = Register::FromParameterIndex(2, builder.parameter_count());
  const AstRawString* name = ast_factory.GetOneByteString("abc");
  uint32_t name_index = 2;
  uint32_t feedback_slot = 97;

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
      .LoadNamedProperty(reg_1, name, feedback_slot)
      .BinaryOperation(Token::Value::ADD, reg_0, 3)
      .StoreAccumulatorInRegister(param)
      .CallRuntimeForPair(Runtime::kLoadLookupSlotForCall, param, pair)
      .ForInPrepare(reg_0, triple)
      .CallRuntime(Runtime::kLoadIC_Miss, reg_0)
      .Debugger()
      .LoadGlobal(name, 0x10000000, TypeofMode::NOT_INSIDE_TYPEOF)
      .Return();

  // Test iterator sees the expected output from the builder.
  ast_factory.Internalize(isolate());
  Handle<BytecodeArray> bytecodeArray = builder.ToBytecodeArray(isolate());
  BytecodeArrayRandomIterator iterator(bytecodeArray, zone());
  const int kPrefixByteSize = 1;
  int offset = bytecodeArray->length();

  iterator.GoToEnd();

  offset -= Bytecodes::Size(Bytecode::kReturn, OperandScale::kSingle);
  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kReturn);
  EXPECT_EQ(iterator.current_index(), 23);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  ASSERT_TRUE(iterator.IsValid());
  --iterator;

  offset -= Bytecodes::Size(Bytecode::kLdaGlobal, OperandScale::kQuadruple) +
            kPrefixByteSize;
  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kLdaGlobal);
  EXPECT_EQ(iterator.current_index(), 22);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kQuadruple);
  EXPECT_EQ(iterator.current_bytecode_size(), 10);
  EXPECT_EQ(iterator.GetIndexOperand(1), 0x10000000u);
  --iterator;

  offset -= Bytecodes::Size(Bytecode::kDebugger, OperandScale::kSingle);
  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kDebugger);
  EXPECT_EQ(iterator.current_index(), 21);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  ASSERT_TRUE(iterator.IsValid());
  --iterator;

  offset -= Bytecodes::Size(Bytecode::kCallRuntime, OperandScale::kSingle);
  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kCallRuntime);
  EXPECT_EQ(iterator.current_index(), 20);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(iterator.GetRuntimeIdOperand(0), Runtime::kLoadIC_Miss);
  EXPECT_EQ(iterator.GetRegisterOperand(1).index(), reg_0.index());
  EXPECT_EQ(iterator.GetRegisterCountOperand(2), 1u);
  ASSERT_TRUE(iterator.IsValid());
  --iterator;

  offset -= Bytecodes::Size(Bytecode::kForInPrepare, OperandScale::kSingle);
  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kForInPrepare);
  EXPECT_EQ(iterator.current_index(), 19);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(iterator.GetRegisterOperand(0).index(), reg_0.index());
  EXPECT_EQ(iterator.GetRegisterOperandRange(0), 1);
  EXPECT_EQ(iterator.GetRegisterOperand(1).index(), reg_0.index());
  EXPECT_EQ(iterator.GetRegisterOperandRange(1), 3);
  ASSERT_TRUE(iterator.IsValid());
  --iterator;

  offset -=
      Bytecodes::Size(Bytecode::kCallRuntimeForPair, OperandScale::kSingle);
  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kCallRuntimeForPair);
  EXPECT_EQ(iterator.current_index(), 18);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(iterator.GetRuntimeIdOperand(0), Runtime::kLoadLookupSlotForCall);
  EXPECT_EQ(iterator.GetRegisterOperand(1).index(), param.index());
  EXPECT_EQ(iterator.GetRegisterOperandRange(1), 1);
  EXPECT_EQ(iterator.GetRegisterCountOperand(2), 1u);
  EXPECT_EQ(iterator.GetRegisterOperand(3).index(), reg_0.index());
  EXPECT_EQ(iterator.GetRegisterOperandRange(3), 2);
  ASSERT_TRUE(iterator.IsValid());
  --iterator;

  offset -= Bytecodes::Size(Bytecode::kStar, OperandScale::kSingle);
  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kStar);
  EXPECT_EQ(iterator.current_index(), 17);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(iterator.GetRegisterOperand(0).index(), param.index());
  EXPECT_EQ(iterator.GetRegisterOperandRange(0), 1);
  ASSERT_TRUE(iterator.IsValid());
  --iterator;

  offset -= Bytecodes::Size(Bytecode::kAdd, OperandScale::kSingle);
  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kAdd);
  EXPECT_EQ(iterator.current_index(), 16);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(iterator.GetRegisterOperand(0).index(), reg_0.index());
  EXPECT_EQ(iterator.GetRegisterOperandRange(0), 1);
  ASSERT_TRUE(iterator.IsValid());
  --iterator;

  offset -= Bytecodes::Size(Bytecode::kLdaNamedProperty, OperandScale::kSingle);
  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kLdaNamedProperty);
  EXPECT_EQ(iterator.current_index(), 15);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(iterator.GetRegisterOperand(0).index(), reg_1.index());
  EXPECT_EQ(iterator.GetIndexOperand(1), name_index);
  EXPECT_EQ(iterator.GetIndexOperand(2), feedback_slot);
  ASSERT_TRUE(iterator.IsValid());
  --iterator;

  offset -= Bytecodes::Size(Bytecode::kStar, OperandScale::kSingle);
  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kStar);
  EXPECT_EQ(iterator.current_index(), 14);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(iterator.GetRegisterOperand(0).index(), reg_1.index());
  EXPECT_EQ(iterator.GetRegisterOperandRange(0), 1);
  ASSERT_TRUE(iterator.IsValid());
  --iterator;

  offset -= Bytecodes::Size(Bytecode::kAdd, OperandScale::kSingle);
  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kAdd);
  EXPECT_EQ(iterator.current_index(), 13);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(iterator.GetRegisterOperand(0).index(), reg_0.index());
  EXPECT_EQ(iterator.GetRegisterOperandRange(0), 1);
  ASSERT_TRUE(iterator.IsValid());
  --iterator;

  offset -= Bytecodes::Size(Bytecode::kLdar, OperandScale::kSingle);
  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kLdar);
  EXPECT_EQ(iterator.current_index(), 12);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(iterator.GetRegisterOperand(0).index(), reg_0.index());
  ASSERT_TRUE(iterator.IsValid());
  --iterator;

  offset -= Bytecodes::Size(Bytecode::kStar, OperandScale::kSingle);
  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kStar);
  EXPECT_EQ(iterator.current_index(), 11);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(iterator.GetRegisterOperand(0).index(), reg_1.index());
  EXPECT_EQ(iterator.GetRegisterOperandRange(0), 1);
  ASSERT_TRUE(iterator.IsValid());
  --iterator;

  offset -= Bytecodes::Size(Bytecode::kStackCheck, OperandScale::kSingle);
  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kStackCheck);
  EXPECT_EQ(iterator.current_index(), 10);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(Bytecodes::NumberOfOperands(iterator.current_bytecode()), 0);
  ASSERT_TRUE(iterator.IsValid());
  --iterator;

  offset -= Bytecodes::Size(Bytecode::kLdaSmi, OperandScale::kQuadruple) +
            kPrefixByteSize;
  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kLdaSmi);
  EXPECT_EQ(iterator.current_index(), 9);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kQuadruple);
  EXPECT_EQ(Smi::FromInt(iterator.GetImmediateOperand(0)), smi_1);
  ASSERT_TRUE(iterator.IsValid());
  --iterator;

  offset -= Bytecodes::Size(Bytecode::kStar, OperandScale::kSingle);
  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kStar);
  EXPECT_EQ(iterator.current_index(), 8);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(iterator.GetRegisterOperand(0).index(), reg_0.index());
  EXPECT_EQ(iterator.GetRegisterOperandRange(0), 1);
  ASSERT_TRUE(iterator.IsValid());
  --iterator;

  offset -= Bytecodes::Size(Bytecode::kStackCheck, OperandScale::kSingle);
  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kStackCheck);
  EXPECT_EQ(iterator.current_index(), 7);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(Bytecodes::NumberOfOperands(iterator.current_bytecode()), 0);
  ASSERT_TRUE(iterator.IsValid());
  --iterator;

  offset -= Bytecodes::Size(Bytecode::kLdaSmi, OperandScale::kSingle);
  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kLdaSmi);
  EXPECT_EQ(iterator.current_index(), 6);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(Smi::FromInt(iterator.GetImmediateOperand(0)), smi_0);
  ASSERT_TRUE(iterator.IsValid());
  --iterator;

  offset -= Bytecodes::Size(Bytecode::kStar, OperandScale::kSingle);
  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kStar);
  EXPECT_EQ(iterator.current_index(), 5);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(iterator.GetRegisterOperand(0).index(), reg_0.index());
  EXPECT_EQ(iterator.GetRegisterOperandRange(0), 1);
  ASSERT_TRUE(iterator.IsValid());
  --iterator;

  offset -= Bytecodes::Size(Bytecode::kLdaZero, OperandScale::kSingle);
  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kLdaZero);
  EXPECT_EQ(iterator.current_index(), 4);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  ASSERT_TRUE(iterator.IsValid());
  --iterator;

  offset -= Bytecodes::Size(Bytecode::kStar, OperandScale::kSingle);
  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kStar);
  EXPECT_EQ(iterator.current_index(), 3);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(iterator.GetRegisterOperand(0).index(), reg_0.index());
  EXPECT_EQ(iterator.GetRegisterOperandRange(0), 1);
  ASSERT_TRUE(iterator.IsValid());
  --iterator;

  offset -= Bytecodes::Size(Bytecode::kLdaConstant, OperandScale::kSingle);
  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kLdaConstant);
  EXPECT_EQ(iterator.current_index(), 2);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_TRUE(iterator.GetConstantForIndexOperand(0).is_identical_to(
      heap_num_1->value()));
  ASSERT_TRUE(iterator.IsValid());
  --iterator;

  offset -= Bytecodes::Size(Bytecode::kStar, OperandScale::kSingle);
  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kStar);
  EXPECT_EQ(iterator.current_index(), 1);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_EQ(iterator.GetRegisterOperand(0).index(), reg_0.index());
  EXPECT_EQ(iterator.GetRegisterOperandRange(0), 1);
  ASSERT_TRUE(iterator.IsValid());
  --iterator;

  offset -= Bytecodes::Size(Bytecode::kLdaConstant, OperandScale::kSingle);
  EXPECT_EQ(iterator.current_bytecode(), Bytecode::kLdaConstant);
  EXPECT_EQ(iterator.current_index(), 0);
  EXPECT_EQ(iterator.current_offset(), offset);
  EXPECT_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  EXPECT_TRUE(iterator.GetConstantForIndexOperand(0).is_identical_to(
      heap_num_0->value()));
  ASSERT_TRUE(iterator.IsValid());
  --iterator;
  ASSERT_FALSE(iterator.IsValid());
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
