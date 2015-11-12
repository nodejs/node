// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/interpreter/bytecode-array-builder.h"
#include "src/interpreter/bytecode-array-iterator.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace interpreter {

class BytecodeArrayBuilderTest : public TestWithIsolateAndZone {
 public:
  BytecodeArrayBuilderTest() {}
  ~BytecodeArrayBuilderTest() override {}
};


TEST_F(BytecodeArrayBuilderTest, AllBytecodesGenerated) {
  BytecodeArrayBuilder builder(isolate(), zone());

  builder.set_locals_count(1);
  builder.set_parameter_count(0);
  CHECK_EQ(builder.locals_count(), 1);

  // Emit constant loads.
  builder.LoadLiteral(Smi::FromInt(0))
      .LoadLiteral(Smi::FromInt(8))
      .LoadLiteral(Smi::FromInt(10000000))
      .LoadUndefined()
      .LoadNull()
      .LoadTheHole()
      .LoadTrue()
      .LoadFalse();

  // Emit accumulator transfers.
  Register reg(0);
  builder.LoadAccumulatorWithRegister(reg).StoreAccumulatorInRegister(reg);

  // Emit global load operations.
  builder.LoadGlobal(1);

  // Emit load / store property operations.
  builder.LoadNamedProperty(reg, 0, LanguageMode::SLOPPY)
      .LoadKeyedProperty(reg, 0, LanguageMode::SLOPPY)
      .StoreNamedProperty(reg, reg, 0, LanguageMode::SLOPPY)
      .StoreKeyedProperty(reg, reg, 0, LanguageMode::SLOPPY);

  // Call operations.
  builder.Call(reg, reg, 0);

  // Emit binary operator invocations.
  builder.BinaryOperation(Token::Value::ADD, reg)
      .BinaryOperation(Token::Value::SUB, reg)
      .BinaryOperation(Token::Value::MUL, reg)
      .BinaryOperation(Token::Value::DIV, reg)
      .BinaryOperation(Token::Value::MOD, reg);

  // Emit test operator invocations.
  builder.CompareOperation(Token::Value::EQ, reg, LanguageMode::SLOPPY)
      .CompareOperation(Token::Value::NE, reg, LanguageMode::SLOPPY)
      .CompareOperation(Token::Value::EQ_STRICT, reg, LanguageMode::SLOPPY)
      .CompareOperation(Token::Value::NE_STRICT, reg, LanguageMode::SLOPPY)
      .CompareOperation(Token::Value::LT, reg, LanguageMode::SLOPPY)
      .CompareOperation(Token::Value::GT, reg, LanguageMode::SLOPPY)
      .CompareOperation(Token::Value::LTE, reg, LanguageMode::SLOPPY)
      .CompareOperation(Token::Value::GTE, reg, LanguageMode::SLOPPY)
      .CompareOperation(Token::Value::INSTANCEOF, reg, LanguageMode::SLOPPY)
      .CompareOperation(Token::Value::IN, reg, LanguageMode::SLOPPY);

  // Emit cast operator invocations.
  builder.LoadNull().CastAccumulatorToBoolean();

  // Emit control flow. Return must be the last instruction.
  BytecodeLabel start;
  builder.Bind(&start);
  // Short jumps with Imm8 operands
  builder.Jump(&start).JumpIfTrue(&start).JumpIfFalse(&start);
  // Insert dummy ops to force longer jumps
  for (int i = 0; i < 128; i++) {
    builder.LoadTrue();
  }
  // Longer jumps requiring Constant operand
  builder.Jump(&start).JumpIfTrue(&start).JumpIfFalse(&start);
  builder.Return();

  // Generate BytecodeArray.
  Handle<BytecodeArray> the_array = builder.ToBytecodeArray();
  CHECK_EQ(the_array->frame_size(), builder.locals_count() * kPointerSize);

  // Build scorecard of bytecodes encountered in the BytecodeArray.
  std::vector<int> scorecard(Bytecodes::ToByte(Bytecode::kLast) + 1);
  Bytecode final_bytecode = Bytecode::kLdaZero;
  for (int i = 0; i < the_array->length(); i++) {
    uint8_t code = the_array->get(i);
    scorecard[code] += 1;
    int operands = Bytecodes::NumberOfOperands(Bytecodes::FromByte(code));
    CHECK_LE(operands, Bytecodes::MaximumNumberOfOperands());
    final_bytecode = Bytecodes::FromByte(code);
    i += operands;
  }

  // Check return occurs at the end and only once in the BytecodeArray.
  CHECK_EQ(final_bytecode, Bytecode::kReturn);
  CHECK_EQ(scorecard[Bytecodes::ToByte(final_bytecode)], 1);

#define CHECK_BYTECODE_PRESENT(Name, ...)     \
  /* Check Bytecode is marked in scorecard */ \
  CHECK_GE(scorecard[Bytecodes::ToByte(Bytecode::k##Name)], 1);
  BYTECODE_LIST(CHECK_BYTECODE_PRESENT)
#undef CHECK_BYTECODE_PRESENT
}


TEST_F(BytecodeArrayBuilderTest, FrameSizesLookGood) {
  for (int locals = 0; locals < 5; locals++) {
    for (int temps = 0; temps < 3; temps++) {
      BytecodeArrayBuilder builder(isolate(), zone());
      builder.set_parameter_count(0);
      builder.set_locals_count(locals);
      builder.Return();

      TemporaryRegisterScope temporaries(&builder);
      for (int i = 0; i < temps; i++) {
        temporaries.NewRegister();
      }

      Handle<BytecodeArray> the_array = builder.ToBytecodeArray();
      int total_registers = locals + temps;
      CHECK_EQ(the_array->frame_size(), total_registers * kPointerSize);
    }
  }
}


TEST_F(BytecodeArrayBuilderTest, TemporariesRecycled) {
  BytecodeArrayBuilder builder(isolate(), zone());
  builder.set_parameter_count(0);
  builder.set_locals_count(0);
  builder.Return();

  int first;
  {
    TemporaryRegisterScope temporaries(&builder);
    first = temporaries.NewRegister().index();
    temporaries.NewRegister();
    temporaries.NewRegister();
    temporaries.NewRegister();
  }

  int second;
  {
    TemporaryRegisterScope temporaries(&builder);
    second = temporaries.NewRegister().index();
  }

  CHECK_EQ(first, second);
}


TEST_F(BytecodeArrayBuilderTest, RegisterValues) {
  int index = 1;
  uint8_t operand = static_cast<uint8_t>(-index);

  Register the_register(index);
  CHECK_EQ(the_register.index(), index);

  int actual_operand = the_register.ToOperand();
  CHECK_EQ(actual_operand, operand);

  int actual_index = Register::FromOperand(actual_operand).index();
  CHECK_EQ(actual_index, index);
}


TEST_F(BytecodeArrayBuilderTest, Parameters) {
  BytecodeArrayBuilder builder(isolate(), zone());
  builder.set_parameter_count(10);
  builder.set_locals_count(0);

  Register param0(builder.Parameter(0));
  Register param9(builder.Parameter(9));
  CHECK_EQ(param9.index() - param0.index(), 9);
}


TEST_F(BytecodeArrayBuilderTest, Constants) {
  BytecodeArrayBuilder builder(isolate(), zone());
  builder.set_parameter_count(0);
  builder.set_locals_count(0);

  Factory* factory = isolate()->factory();
  Handle<HeapObject> heap_num_1 = factory->NewHeapNumber(3.14);
  Handle<HeapObject> heap_num_2 = factory->NewHeapNumber(5.2);
  Handle<Object> large_smi(Smi::FromInt(0x12345678), isolate());
  Handle<HeapObject> heap_num_2_copy(*heap_num_2);
  builder.LoadLiteral(heap_num_1)
      .LoadLiteral(heap_num_2)
      .LoadLiteral(large_smi)
      .LoadLiteral(heap_num_1)
      .LoadLiteral(heap_num_1)
      .LoadLiteral(heap_num_2_copy);

  Handle<BytecodeArray> array = builder.ToBytecodeArray();
  // Should only have one entry for each identical constant.
  CHECK_EQ(array->constant_pool()->length(), 3);
}


TEST_F(BytecodeArrayBuilderTest, ForwardJumps) {
  static const int kFarJumpDistance = 256;

  BytecodeArrayBuilder builder(isolate(), zone());
  builder.set_parameter_count(0);
  builder.set_locals_count(0);

  BytecodeLabel far0, far1, far2;
  BytecodeLabel near0, near1, near2;

  builder.Jump(&near0)
      .JumpIfTrue(&near1)
      .JumpIfFalse(&near2)
      .Bind(&near0)
      .Bind(&near1)
      .Bind(&near2)
      .Jump(&far0)
      .JumpIfTrue(&far1)
      .JumpIfFalse(&far2);
  for (int i = 0; i < kFarJumpDistance - 6; i++) {
    builder.LoadUndefined();
  }
  builder.Bind(&far0).Bind(&far1).Bind(&far2);
  builder.Return();

  Handle<BytecodeArray> array = builder.ToBytecodeArray();
  DCHECK_EQ(array->length(), 12 + kFarJumpDistance - 6 + 1);

  BytecodeArrayIterator iterator(array);
  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJump);
  CHECK_EQ(iterator.GetSmi8Operand(0), 6);
  iterator.Advance();

  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJumpIfTrue);
  CHECK_EQ(iterator.GetSmi8Operand(0), 4);
  iterator.Advance();

  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJumpIfFalse);
  CHECK_EQ(iterator.GetSmi8Operand(0), 2);
  iterator.Advance();

  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJumpConstant);
  CHECK_EQ(*iterator.GetConstantForIndexOperand(0),
           Smi::FromInt(kFarJumpDistance));
  CHECK_EQ(
      array->get(iterator.current_offset() +
                 Smi::cast(*iterator.GetConstantForIndexOperand(0))->value()),
      Bytecodes::ToByte(Bytecode::kReturn));
  iterator.Advance();

  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJumpIfTrueConstant);
  CHECK_EQ(*iterator.GetConstantForIndexOperand(0),
           Smi::FromInt(kFarJumpDistance - 2));
  CHECK_EQ(
      array->get(iterator.current_offset() +
                 Smi::cast(*iterator.GetConstantForIndexOperand(0))->value()),
      Bytecodes::ToByte(Bytecode::kReturn));
  iterator.Advance();

  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJumpIfFalseConstant);
  CHECK_EQ(*iterator.GetConstantForIndexOperand(0),
           Smi::FromInt(kFarJumpDistance - 4));
  CHECK_EQ(
      array->get(iterator.current_offset() +
                 Smi::cast(*iterator.GetConstantForIndexOperand(0))->value()),
      Bytecodes::ToByte(Bytecode::kReturn));
  iterator.Advance();
}


TEST_F(BytecodeArrayBuilderTest, BackwardJumps) {
  BytecodeArrayBuilder builder(isolate(), zone());
  builder.set_parameter_count(0);
  builder.set_locals_count(0);

  BytecodeLabel label0, label1, label2;
  builder.Bind(&label0)
      .Jump(&label0)
      .Bind(&label1)
      .JumpIfTrue(&label1)
      .Bind(&label2)
      .JumpIfFalse(&label2);
  for (int i = 0; i < 64; i++) {
    builder.Jump(&label2);
  }
  builder.JumpIfFalse(&label2);
  builder.JumpIfTrue(&label1);
  builder.Jump(&label0);
  builder.Return();

  Handle<BytecodeArray> array = builder.ToBytecodeArray();
  BytecodeArrayIterator iterator(array);
  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJump);
  CHECK_EQ(iterator.GetSmi8Operand(0), 0);
  iterator.Advance();
  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJumpIfTrue);
  CHECK_EQ(iterator.GetSmi8Operand(0), 0);
  iterator.Advance();
  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJumpIfFalse);
  CHECK_EQ(iterator.GetSmi8Operand(0), 0);
  iterator.Advance();
  for (int i = 0; i < 64; i++) {
    CHECK_EQ(iterator.current_bytecode(), Bytecode::kJump);
    CHECK_EQ(iterator.GetSmi8Operand(0), -i * 2 - 2);
    iterator.Advance();
  }
  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJumpIfFalseConstant);
  CHECK_EQ(Smi::cast(*iterator.GetConstantForIndexOperand(0))->value(), -130);
  iterator.Advance();
  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJumpIfTrueConstant);
  CHECK_EQ(Smi::cast(*iterator.GetConstantForIndexOperand(0))->value(), -134);
  iterator.Advance();
  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJumpConstant);
  CHECK_EQ(Smi::cast(*iterator.GetConstantForIndexOperand(0))->value(), -138);
  iterator.Advance();
  CHECK_EQ(iterator.current_bytecode(), Bytecode::kReturn);
  iterator.Advance();
  CHECK(iterator.done());
}


TEST_F(BytecodeArrayBuilderTest, LabelReuse) {
  BytecodeArrayBuilder builder(isolate(), zone());
  builder.set_parameter_count(0);
  builder.set_locals_count(0);

  // Labels can only have 1 forward reference, but
  // can be referred to mulitple times once bound.
  BytecodeLabel label;

  builder.Jump(&label).Bind(&label).Jump(&label).Jump(&label).Return();

  Handle<BytecodeArray> array = builder.ToBytecodeArray();
  BytecodeArrayIterator iterator(array);
  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJump);
  CHECK_EQ(iterator.GetSmi8Operand(0), 2);
  iterator.Advance();
  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJump);
  CHECK_EQ(iterator.GetSmi8Operand(0), 0);
  iterator.Advance();
  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJump);
  CHECK_EQ(iterator.GetSmi8Operand(0), -2);
  iterator.Advance();
  CHECK_EQ(iterator.current_bytecode(), Bytecode::kReturn);
  iterator.Advance();
  CHECK(iterator.done());
}


TEST_F(BytecodeArrayBuilderTest, LabelAddressReuse) {
  static const int kRepeats = 3;

  BytecodeArrayBuilder builder(isolate(), zone());
  builder.set_parameter_count(0);
  builder.set_locals_count(0);

  for (int i = 0; i < kRepeats; i++) {
    BytecodeLabel label;
    builder.Jump(&label).Bind(&label).Jump(&label).Jump(&label);
  }

  builder.Return();

  Handle<BytecodeArray> array = builder.ToBytecodeArray();
  BytecodeArrayIterator iterator(array);
  for (int i = 0; i < kRepeats; i++) {
    CHECK_EQ(iterator.current_bytecode(), Bytecode::kJump);
    CHECK_EQ(iterator.GetSmi8Operand(0), 2);
    iterator.Advance();
    CHECK_EQ(iterator.current_bytecode(), Bytecode::kJump);
    CHECK_EQ(iterator.GetSmi8Operand(0), 0);
    iterator.Advance();
    CHECK_EQ(iterator.current_bytecode(), Bytecode::kJump);
    CHECK_EQ(iterator.GetSmi8Operand(0), -2);
    iterator.Advance();
  }
  CHECK_EQ(iterator.current_bytecode(), Bytecode::kReturn);
  iterator.Advance();
  CHECK(iterator.done());
}


}  // namespace interpreter
}  // namespace internal
}  // namespace v8
