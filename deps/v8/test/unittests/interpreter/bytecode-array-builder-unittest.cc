// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/interpreter/bytecode-array-builder.h"
#include "src/interpreter/bytecode-array-iterator.h"
#include "src/interpreter/bytecode-register-allocator.h"
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
  BytecodeArrayBuilder builder(isolate(), zone(), 0, 1, 131);

  CHECK_EQ(builder.locals_count(), 131);
  CHECK_EQ(builder.context_count(), 1);
  CHECK_EQ(builder.fixed_register_count(), 132);

  // Emit argument creation operations.
  builder.CreateArguments(CreateArgumentsType::kMappedArguments)
      .CreateArguments(CreateArgumentsType::kUnmappedArguments)
      .CreateArguments(CreateArgumentsType::kRestParameter);

  // Emit constant loads.
  builder.LoadLiteral(Smi::FromInt(0))
      .LoadLiteral(Smi::FromInt(8))
      .LoadLiteral(Smi::FromInt(10000000))
      .LoadUndefined()
      .LoadNull()
      .LoadTheHole()
      .LoadTrue()
      .LoadFalse();

  Register reg(0);
  Register other(reg.index() + 1);
  Register wide(128);

  builder.LoadAccumulatorWithRegister(reg)
      .LoadNull()
      .StoreAccumulatorInRegister(reg);

  // Emit register-register transfer.
  builder.MoveRegister(reg, other);
  builder.MoveRegister(reg, wide);

  // Emit global load / store operations.
  Factory* factory = isolate()->factory();
  Handle<String> name = factory->NewStringFromStaticChars("var_name");
  builder.LoadGlobal(name, 1, TypeofMode::NOT_INSIDE_TYPEOF)
      .LoadGlobal(name, 1, TypeofMode::INSIDE_TYPEOF)
      .StoreGlobal(name, 1, LanguageMode::SLOPPY)
      .StoreGlobal(name, 1, LanguageMode::STRICT);

  // Emit context operations.
  builder.PushContext(reg)
      .PopContext(reg)
      .LoadContextSlot(reg, 1)
      .StoreContextSlot(reg, 1);

  // Emit load / store property operations.
  builder.LoadNamedProperty(reg, name, 0)
      .LoadKeyedProperty(reg, 0)
      .StoreNamedProperty(reg, name, 0, LanguageMode::SLOPPY)
      .StoreKeyedProperty(reg, reg, 0, LanguageMode::SLOPPY)
      .StoreNamedProperty(reg, name, 0, LanguageMode::STRICT)
      .StoreKeyedProperty(reg, reg, 0, LanguageMode::STRICT);

  // Emit load / store lookup slots.
  builder.LoadLookupSlot(name, TypeofMode::NOT_INSIDE_TYPEOF)
      .LoadLookupSlot(name, TypeofMode::INSIDE_TYPEOF)
      .StoreLookupSlot(name, LanguageMode::SLOPPY)
      .StoreLookupSlot(name, LanguageMode::STRICT);

  // Emit closure operations.
  Handle<SharedFunctionInfo> shared_info = factory->NewSharedFunctionInfo(
      factory->NewStringFromStaticChars("function_a"), MaybeHandle<Code>(),
      false);
  builder.CreateClosure(shared_info, NOT_TENURED);

  // Emit literal creation operations.
  builder.CreateRegExpLiteral(factory->NewStringFromStaticChars("a"), 0, 0)
      .CreateArrayLiteral(factory->NewFixedArray(1), 0, 0)
      .CreateObjectLiteral(factory->NewFixedArray(1), 0, 0);

  // Call operations.
  builder.Call(reg, other, 1, 0)
      .Call(reg, wide, 1, 0)
      .TailCall(reg, other, 1, 0)
      .TailCall(reg, wide, 1, 0)
      .CallRuntime(Runtime::kIsArray, reg, 1)
      .CallRuntime(Runtime::kIsArray, wide, 1)
      .CallRuntimeForPair(Runtime::kLoadLookupSlotForCall, reg, 1, other)
      .CallRuntimeForPair(Runtime::kLoadLookupSlotForCall, wide, 1, other)
      .CallJSRuntime(Context::SPREAD_ITERABLE_INDEX, reg, 1)
      .CallJSRuntime(Context::SPREAD_ITERABLE_INDEX, wide, 1);

  // Emit binary operator invocations.
  builder.BinaryOperation(Token::Value::ADD, reg)
      .BinaryOperation(Token::Value::SUB, reg)
      .BinaryOperation(Token::Value::MUL, reg)
      .BinaryOperation(Token::Value::DIV, reg)
      .BinaryOperation(Token::Value::MOD, reg);

  // Emit bitwise operator invocations
  builder.BinaryOperation(Token::Value::BIT_OR, reg)
      .BinaryOperation(Token::Value::BIT_XOR, reg)
      .BinaryOperation(Token::Value::BIT_AND, reg);

  // Emit shift operator invocations
  builder.BinaryOperation(Token::Value::SHL, reg)
      .BinaryOperation(Token::Value::SAR, reg)
      .BinaryOperation(Token::Value::SHR, reg);

  // Emit count operatior invocations
  builder.CountOperation(Token::Value::ADD).CountOperation(Token::Value::SUB);

  // Emit unary operator invocations.
  builder.LogicalNot().TypeOf();

  // Emit delete
  builder.Delete(reg, LanguageMode::SLOPPY).Delete(reg, LanguageMode::STRICT);

  // Emit new.
  builder.New(reg, reg, 0);
  builder.New(wide, wide, 0);

  // Emit test operator invocations.
  builder.CompareOperation(Token::Value::EQ, reg)
      .CompareOperation(Token::Value::NE, reg)
      .CompareOperation(Token::Value::EQ_STRICT, reg)
      .CompareOperation(Token::Value::NE_STRICT, reg)
      .CompareOperation(Token::Value::LT, reg)
      .CompareOperation(Token::Value::GT, reg)
      .CompareOperation(Token::Value::LTE, reg)
      .CompareOperation(Token::Value::GTE, reg)
      .CompareOperation(Token::Value::INSTANCEOF, reg)
      .CompareOperation(Token::Value::IN, reg);

  // Emit cast operator invocations.
  builder.CastAccumulatorToNumber()
      .CastAccumulatorToJSObject()
      .CastAccumulatorToName();

  // Emit control flow. Return must be the last instruction.
  BytecodeLabel start;
  builder.Bind(&start);
  // Short jumps with Imm8 operands
  builder.Jump(&start)
      .JumpIfNull(&start)
      .JumpIfUndefined(&start)
      .JumpIfNotHole(&start);

  // Perform an operation that returns boolean value to
  // generate JumpIfTrue/False
  builder.CompareOperation(Token::Value::EQ, reg)
      .JumpIfTrue(&start)
      .CompareOperation(Token::Value::EQ, reg)
      .JumpIfFalse(&start);
  // Perform an operation that returns a non-boolean operation to
  // generate JumpIfToBooleanTrue/False.
  builder.BinaryOperation(Token::Value::ADD, reg)
      .JumpIfTrue(&start)
      .BinaryOperation(Token::Value::ADD, reg)
      .JumpIfFalse(&start);
  // Insert dummy ops to force longer jumps
  for (int i = 0; i < 128; i++) {
    builder.LoadTrue();
  }
  // Longer jumps requiring Constant operand
  builder.Jump(&start).JumpIfNull(&start).JumpIfUndefined(&start).JumpIfNotHole(
      &start);
  // Perform an operation that returns boolean value to
  // generate JumpIfTrue/False
  builder.CompareOperation(Token::Value::EQ, reg)
      .JumpIfTrue(&start)
      .CompareOperation(Token::Value::EQ, reg)
      .JumpIfFalse(&start);
  // Perform an operation that returns a non-boolean operation to
  // generate JumpIfToBooleanTrue/False.
  builder.BinaryOperation(Token::Value::ADD, reg)
      .JumpIfTrue(&start)
      .BinaryOperation(Token::Value::ADD, reg)
      .JumpIfFalse(&start);

  // Emit stack check bytecode.
  builder.StackCheck();

  // Emit throw and re-throw in it's own basic block so that the rest of the
  // code isn't omitted due to being dead.
  BytecodeLabel after_throw;
  builder.Jump(&after_throw).Throw().Bind(&after_throw);
  BytecodeLabel after_rethrow;
  builder.Jump(&after_rethrow).ReThrow().Bind(&after_rethrow);

  builder.ForInPrepare(reg)
      .ForInDone(reg, reg)
      .ForInNext(reg, reg, reg)
      .ForInStep(reg);
  builder.ForInPrepare(wide)
      .ForInDone(reg, other)
      .ForInNext(wide, wide, wide)
      .ForInStep(reg);

  // Wide constant pool loads
  for (int i = 0; i < 256; i++) {
    // Emit junk in constant pool to force wide constant pool index.
    builder.LoadLiteral(factory->NewNumber(2.5321 + i));
  }
  builder.LoadLiteral(Smi::FromInt(20000000));
  Handle<String> wide_name = factory->NewStringFromStaticChars("var_wide_name");

  // Emit wide global load / store operations.
  builder.LoadGlobal(name, 1024, TypeofMode::NOT_INSIDE_TYPEOF)
      .LoadGlobal(name, 1024, TypeofMode::INSIDE_TYPEOF)
      .StoreGlobal(name, 1024, LanguageMode::SLOPPY)
      .StoreGlobal(wide_name, 1, LanguageMode::STRICT);

  // Emit wide load / store property operations.
  builder.LoadNamedProperty(reg, wide_name, 0)
      .LoadKeyedProperty(reg, 2056)
      .StoreNamedProperty(reg, wide_name, 0, LanguageMode::SLOPPY)
      .StoreKeyedProperty(reg, reg, 2056, LanguageMode::SLOPPY)
      .StoreNamedProperty(reg, wide_name, 0, LanguageMode::STRICT)
      .StoreKeyedProperty(reg, reg, 2056, LanguageMode::STRICT);

  // Emit wide context operations.
  builder.LoadContextSlot(reg, 1024).StoreContextSlot(reg, 1024);

  // Emit wide load / store lookup slots.
  builder.LoadLookupSlot(wide_name, TypeofMode::NOT_INSIDE_TYPEOF)
      .LoadLookupSlot(wide_name, TypeofMode::INSIDE_TYPEOF)
      .StoreLookupSlot(wide_name, LanguageMode::SLOPPY)
      .StoreLookupSlot(wide_name, LanguageMode::STRICT);

  // CreateClosureWide
  Handle<SharedFunctionInfo> shared_info2 = factory->NewSharedFunctionInfo(
      factory->NewStringFromStaticChars("function_b"), MaybeHandle<Code>(),
      false);
  builder.CreateClosure(shared_info2, NOT_TENURED);

  // Emit wide variant of literal creation operations.
  builder.CreateRegExpLiteral(factory->NewStringFromStaticChars("wide_literal"),
                              0, 0)
      .CreateArrayLiteral(factory->NewFixedArray(2), 0, 0)
      .CreateObjectLiteral(factory->NewFixedArray(2), 0, 0);

  // Longer jumps requiring ConstantWide operand
  builder.Jump(&start).JumpIfNull(&start).JumpIfUndefined(&start).JumpIfNotHole(
      &start);
  // Perform an operation that returns boolean value to
  // generate JumpIfTrue/False
  builder.CompareOperation(Token::Value::EQ, reg)
      .JumpIfTrue(&start)
      .CompareOperation(Token::Value::EQ, reg)
      .JumpIfFalse(&start);
  // Perform an operation that returns a non-boolean operation to
  // generate JumpIfToBooleanTrue/False.
  builder.BinaryOperation(Token::Value::ADD, reg)
      .JumpIfTrue(&start)
      .BinaryOperation(Token::Value::ADD, reg)
      .JumpIfFalse(&start);

  builder.Debugger();

  builder.Return();

  // Generate BytecodeArray.
  Handle<BytecodeArray> the_array = builder.ToBytecodeArray();
  CHECK_EQ(the_array->frame_size(),
           (builder.fixed_and_temporary_register_count() +
            builder.translation_register_count()) *
               kPointerSize);

  // Build scorecard of bytecodes encountered in the BytecodeArray.
  std::vector<int> scorecard(Bytecodes::ToByte(Bytecode::kLast) + 1);
  Bytecode final_bytecode = Bytecode::kLdaZero;
  int i = 0;
  while (i < the_array->length()) {
    uint8_t code = the_array->get(i);
    scorecard[code] += 1;
    final_bytecode = Bytecodes::FromByte(code);
    i += Bytecodes::Size(Bytecodes::FromByte(code));
  }

  // Check return occurs at the end and only once in the BytecodeArray.
  CHECK_EQ(final_bytecode, Bytecode::kReturn);
  CHECK_EQ(scorecard[Bytecodes::ToByte(final_bytecode)], 1);

#define CHECK_BYTECODE_PRESENT(Name, ...)                                \
  /* Check Bytecode is marked in scorecard, unless it's a debug break */ \
  if (!Bytecodes::IsDebugBreak(Bytecode::k##Name)) {                     \
    CHECK_GE(scorecard[Bytecodes::ToByte(Bytecode::k##Name)], 1);        \
  }
  BYTECODE_LIST(CHECK_BYTECODE_PRESENT)
#undef CHECK_BYTECODE_PRESENT
}


TEST_F(BytecodeArrayBuilderTest, FrameSizesLookGood) {
  for (int locals = 0; locals < 5; locals++) {
    for (int contexts = 0; contexts < 4; contexts++) {
      for (int temps = 0; temps < 3; temps++) {
        BytecodeArrayBuilder builder(isolate(), zone(), 0, contexts, locals);
        BytecodeRegisterAllocator temporaries(
            zone(), builder.temporary_register_allocator());
        for (int i = 0; i < temps; i++) {
          builder.StoreAccumulatorInRegister(temporaries.NewRegister());
        }
        builder.Return();

        Handle<BytecodeArray> the_array = builder.ToBytecodeArray();
        int total_registers = locals + contexts + temps;
        CHECK_EQ(the_array->frame_size(), total_registers * kPointerSize);
      }
    }
  }
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
  BytecodeArrayBuilder builder(isolate(), zone(), 10, 0, 0);
  Register param0(builder.Parameter(0));
  Register param9(builder.Parameter(9));
  CHECK_EQ(param9.index() - param0.index(), 9);
}


TEST_F(BytecodeArrayBuilderTest, RegisterType) {
  BytecodeArrayBuilder builder(isolate(), zone(), 10, 0, 3);
  BytecodeRegisterAllocator register_allocator(
      zone(), builder.temporary_register_allocator());
  Register temp0 = register_allocator.NewRegister();
  Register param0(builder.Parameter(0));
  Register param9(builder.Parameter(9));
  Register temp1 = register_allocator.NewRegister();
  Register reg0(0);
  Register reg1(1);
  Register reg2(2);
  Register temp2 = register_allocator.NewRegister();
  CHECK_EQ(builder.RegisterIsParameterOrLocal(temp0), false);
  CHECK_EQ(builder.RegisterIsParameterOrLocal(temp1), false);
  CHECK_EQ(builder.RegisterIsParameterOrLocal(temp2), false);
  CHECK_EQ(builder.RegisterIsParameterOrLocal(param0), true);
  CHECK_EQ(builder.RegisterIsParameterOrLocal(param9), true);
  CHECK_EQ(builder.RegisterIsParameterOrLocal(reg0), true);
  CHECK_EQ(builder.RegisterIsParameterOrLocal(reg1), true);
  CHECK_EQ(builder.RegisterIsParameterOrLocal(reg2), true);
}


TEST_F(BytecodeArrayBuilderTest, Constants) {
  BytecodeArrayBuilder builder(isolate(), zone(), 0, 0, 0);
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
      .LoadLiteral(heap_num_2_copy)
      .Return();

  Handle<BytecodeArray> array = builder.ToBytecodeArray();
  // Should only have one entry for each identical constant.
  CHECK_EQ(array->constant_pool()->length(), 3);
}


TEST_F(BytecodeArrayBuilderTest, ForwardJumps) {
  static const int kFarJumpDistance = 256;

  BytecodeArrayBuilder builder(isolate(), zone(), 0, 0, 1);
  Register reg(0);
  BytecodeLabel far0, far1, far2, far3, far4;
  BytecodeLabel near0, near1, near2, near3, near4;

  builder.Jump(&near0)
      .CompareOperation(Token::Value::EQ, reg)
      .JumpIfTrue(&near1)
      .CompareOperation(Token::Value::EQ, reg)
      .JumpIfFalse(&near2)
      .BinaryOperation(Token::Value::ADD, reg)
      .JumpIfTrue(&near3)
      .BinaryOperation(Token::Value::ADD, reg)
      .JumpIfFalse(&near4)
      .Bind(&near0)
      .Bind(&near1)
      .Bind(&near2)
      .Bind(&near3)
      .Bind(&near4)
      .Jump(&far0)
      .CompareOperation(Token::Value::EQ, reg)
      .JumpIfTrue(&far1)
      .CompareOperation(Token::Value::EQ, reg)
      .JumpIfFalse(&far2)
      .BinaryOperation(Token::Value::ADD, reg)
      .JumpIfTrue(&far3)
      .BinaryOperation(Token::Value::ADD, reg)
      .JumpIfFalse(&far4);
  for (int i = 0; i < kFarJumpDistance - 18; i++) {
    builder.LoadUndefined();
  }
  builder.Bind(&far0).Bind(&far1).Bind(&far2).Bind(&far3).Bind(&far4);
  builder.Return();

  Handle<BytecodeArray> array = builder.ToBytecodeArray();
  DCHECK_EQ(array->length(), 36 + kFarJumpDistance - 18 + 1);

  BytecodeArrayIterator iterator(array);
  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJump);
  CHECK_EQ(iterator.GetImmediateOperand(0), 18);
  iterator.Advance();

  // Ignore compare operation.
  iterator.Advance();

  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJumpIfTrue);
  CHECK_EQ(iterator.GetImmediateOperand(0), 14);
  iterator.Advance();

  // Ignore compare operation.
  iterator.Advance();

  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJumpIfFalse);
  CHECK_EQ(iterator.GetImmediateOperand(0), 10);
  iterator.Advance();

  // Ignore add operation.
  iterator.Advance();

  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJumpIfToBooleanTrue);
  CHECK_EQ(iterator.GetImmediateOperand(0), 6);
  iterator.Advance();

  // Ignore add operation.
  iterator.Advance();

  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJumpIfToBooleanFalse);
  CHECK_EQ(iterator.GetImmediateOperand(0), 2);
  iterator.Advance();


  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJumpConstant);
  CHECK_EQ(*iterator.GetConstantForIndexOperand(0),
           Smi::FromInt(kFarJumpDistance));
  iterator.Advance();

  // Ignore compare operation.
  iterator.Advance();

  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJumpIfTrueConstant);
  CHECK_EQ(*iterator.GetConstantForIndexOperand(0),
           Smi::FromInt(kFarJumpDistance - 4));
  iterator.Advance();

  // Ignore compare operation.
  iterator.Advance();

  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJumpIfFalseConstant);
  CHECK_EQ(*iterator.GetConstantForIndexOperand(0),
           Smi::FromInt(kFarJumpDistance - 8));
  iterator.Advance();

  // Ignore add operation.
  iterator.Advance();

  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJumpIfToBooleanTrueConstant);
  CHECK_EQ(*iterator.GetConstantForIndexOperand(0),
           Smi::FromInt(kFarJumpDistance - 12));
  iterator.Advance();

  // Ignore add operation.
  iterator.Advance();

  CHECK_EQ(iterator.current_bytecode(),
           Bytecode::kJumpIfToBooleanFalseConstant);
  CHECK_EQ(*iterator.GetConstantForIndexOperand(0),
           Smi::FromInt(kFarJumpDistance - 16));
  iterator.Advance();
}


TEST_F(BytecodeArrayBuilderTest, BackwardJumps) {
  BytecodeArrayBuilder builder(isolate(), zone(), 0, 0, 1);
  Register reg(0);

  BytecodeLabel label0, label1, label2, label3, label4;
  builder.Bind(&label0)
      .Jump(&label0)
      .Bind(&label1)
      .CompareOperation(Token::Value::EQ, reg)
      .JumpIfTrue(&label1)
      .Bind(&label2)
      .CompareOperation(Token::Value::EQ, reg)
      .JumpIfFalse(&label2)
      .Bind(&label3)
      .BinaryOperation(Token::Value::ADD, reg)
      .JumpIfTrue(&label3)
      .Bind(&label4)
      .BinaryOperation(Token::Value::ADD, reg)
      .JumpIfFalse(&label4);
  for (int i = 0; i < 63; i++) {
    builder.Jump(&label4);
  }
  builder.BinaryOperation(Token::Value::ADD, reg).JumpIfFalse(&label4);
  builder.BinaryOperation(Token::Value::ADD, reg).JumpIfTrue(&label3);
  builder.CompareOperation(Token::Value::EQ, reg).JumpIfFalse(&label2);
  builder.CompareOperation(Token::Value::EQ, reg).JumpIfTrue(&label1);
  builder.Jump(&label0);
  builder.Return();

  Handle<BytecodeArray> array = builder.ToBytecodeArray();
  BytecodeArrayIterator iterator(array);
  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJump);
  CHECK_EQ(iterator.GetImmediateOperand(0), 0);
  iterator.Advance();
  // Ignore compare operation.
  iterator.Advance();
  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJumpIfTrue);
  CHECK_EQ(iterator.GetImmediateOperand(0), -2);
  iterator.Advance();
  // Ignore compare operation.
  iterator.Advance();
  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJumpIfFalse);
  CHECK_EQ(iterator.GetImmediateOperand(0), -2);
  iterator.Advance();
  // Ignore binary operation.
  iterator.Advance();
  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJumpIfToBooleanTrue);
  CHECK_EQ(iterator.GetImmediateOperand(0), -2);
  iterator.Advance();
  // Ignore binary operation.
  iterator.Advance();
  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJumpIfToBooleanFalse);
  CHECK_EQ(iterator.GetImmediateOperand(0), -2);
  iterator.Advance();
  for (int i = 0; i < 63; i++) {
    CHECK_EQ(iterator.current_bytecode(), Bytecode::kJump);
    CHECK_EQ(iterator.GetImmediateOperand(0), -i * 2 - 4);
    iterator.Advance();
  }
  // Ignore binary operation.
  iterator.Advance();
  CHECK_EQ(iterator.current_bytecode(),
           Bytecode::kJumpIfToBooleanFalseConstant);
  CHECK_EQ(Smi::cast(*iterator.GetConstantForIndexOperand(0))->value(), -132);
  iterator.Advance();
  // Ignore binary operation.
  iterator.Advance();
  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJumpIfToBooleanTrueConstant);
  CHECK_EQ(Smi::cast(*iterator.GetConstantForIndexOperand(0))->value(), -140);
  iterator.Advance();
  // Ignore compare operation.
  iterator.Advance();
  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJumpIfFalseConstant);
  CHECK_EQ(Smi::cast(*iterator.GetConstantForIndexOperand(0))->value(), -148);
  iterator.Advance();
  // Ignore compare operation.
  iterator.Advance();
  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJumpIfTrueConstant);
  CHECK_EQ(Smi::cast(*iterator.GetConstantForIndexOperand(0))->value(), -156);
  iterator.Advance();
  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJumpConstant);
  CHECK_EQ(Smi::cast(*iterator.GetConstantForIndexOperand(0))->value(), -160);
  iterator.Advance();
  CHECK_EQ(iterator.current_bytecode(), Bytecode::kReturn);
  iterator.Advance();
  CHECK(iterator.done());
}


TEST_F(BytecodeArrayBuilderTest, LabelReuse) {
  BytecodeArrayBuilder builder(isolate(), zone(), 0, 0, 0);

  // Labels can only have 1 forward reference, but
  // can be referred to mulitple times once bound.
  BytecodeLabel label;

  builder.Jump(&label).Bind(&label).Jump(&label).Jump(&label).Return();

  Handle<BytecodeArray> array = builder.ToBytecodeArray();
  BytecodeArrayIterator iterator(array);
  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJump);
  CHECK_EQ(iterator.GetImmediateOperand(0), 2);
  iterator.Advance();
  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJump);
  CHECK_EQ(iterator.GetImmediateOperand(0), 0);
  iterator.Advance();
  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJump);
  CHECK_EQ(iterator.GetImmediateOperand(0), -2);
  iterator.Advance();
  CHECK_EQ(iterator.current_bytecode(), Bytecode::kReturn);
  iterator.Advance();
  CHECK(iterator.done());
}


TEST_F(BytecodeArrayBuilderTest, LabelAddressReuse) {
  static const int kRepeats = 3;

  BytecodeArrayBuilder builder(isolate(), zone(), 0, 0, 0);
  for (int i = 0; i < kRepeats; i++) {
    BytecodeLabel label;
    builder.Jump(&label).Bind(&label).Jump(&label).Jump(&label);
  }
  builder.Return();

  Handle<BytecodeArray> array = builder.ToBytecodeArray();
  BytecodeArrayIterator iterator(array);
  for (int i = 0; i < kRepeats; i++) {
    CHECK_EQ(iterator.current_bytecode(), Bytecode::kJump);
    CHECK_EQ(iterator.GetImmediateOperand(0), 2);
    iterator.Advance();
    CHECK_EQ(iterator.current_bytecode(), Bytecode::kJump);
    CHECK_EQ(iterator.GetImmediateOperand(0), 0);
    iterator.Advance();
    CHECK_EQ(iterator.current_bytecode(), Bytecode::kJump);
    CHECK_EQ(iterator.GetImmediateOperand(0), -2);
    iterator.Advance();
  }
  CHECK_EQ(iterator.current_bytecode(), Bytecode::kReturn);
  iterator.Advance();
  CHECK(iterator.done());
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
