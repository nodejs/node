// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/interpreter/bytecode-array-builder.h"
#include "src/interpreter/bytecode-array-iterator.h"
#include "src/interpreter/bytecode-label.h"
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
  CanonicalHandleScope canonical(isolate());
  BytecodeArrayBuilder builder(isolate(), zone(), 0, 1, 131);
  Factory* factory = isolate()->factory();

  CHECK_EQ(builder.locals_count(), 131);
  CHECK_EQ(builder.context_count(), 1);
  CHECK_EQ(builder.fixed_register_count(), 132);

  Register reg(0);
  Register other(reg.index() + 1);
  Register wide(128);

  // Emit argument creation operations.
  builder.CreateArguments(CreateArgumentsType::kMappedArguments)
      .CreateArguments(CreateArgumentsType::kUnmappedArguments)
      .CreateArguments(CreateArgumentsType::kRestParameter);

  // Emit constant loads.
  builder.LoadLiteral(Smi::FromInt(0))
      .StoreAccumulatorInRegister(reg)
      .LoadLiteral(Smi::FromInt(8))
      .CompareOperation(Token::Value::NE, reg)  // Prevent peephole optimization
                                                // LdaSmi, Star -> LdrSmi.
      .StoreAccumulatorInRegister(reg)
      .LoadLiteral(Smi::FromInt(10000000))
      .StoreAccumulatorInRegister(reg)
      .LoadLiteral(factory->NewStringFromStaticChars("A constant"))
      .StoreAccumulatorInRegister(reg)
      .LoadUndefined()
      .Debugger()  // Prevent peephole optimization LdaNull, Star -> LdrNull.
      .LoadNull()
      .StoreAccumulatorInRegister(reg)
      .LoadTheHole()
      .StoreAccumulatorInRegister(reg)
      .LoadTrue()
      .StoreAccumulatorInRegister(reg)
      .LoadFalse()
      .StoreAccumulatorInRegister(wide);

  // Emit Ldar and Star taking care to foil the register optimizer.
  builder.StackCheck(0)
      .LoadAccumulatorWithRegister(other)
      .BinaryOperation(Token::ADD, reg, 1)
      .StoreAccumulatorInRegister(reg)
      .LoadNull();

  // Emit register-register transfer.
  builder.MoveRegister(reg, other);
  builder.MoveRegister(reg, wide);

  // Emit global load / store operations.
  Handle<String> name = factory->NewStringFromStaticChars("var_name");
  builder.LoadGlobal(1, TypeofMode::NOT_INSIDE_TYPEOF)
      .LoadGlobal(1, TypeofMode::INSIDE_TYPEOF)
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
  builder.CreateClosure(0, NOT_TENURED);

  // Emit create context operation.
  builder.CreateBlockContext(factory->NewScopeInfo(1));
  builder.CreateCatchContext(reg, name);
  builder.CreateFunctionContext(1);
  builder.CreateWithContext(reg);

  // Emit literal creation operations.
  builder.CreateRegExpLiteral(factory->NewStringFromStaticChars("a"), 0, 0)
      .CreateArrayLiteral(factory->NewFixedArray(1), 0, 0)
      .CreateObjectLiteral(factory->NewFixedArray(1), 0, 0, reg);

  // Call operations.
  builder.Call(reg, other, 0, 1)
      .Call(reg, wide, 0, 1)
      .TailCall(reg, other, 0, 1)
      .TailCall(reg, wide, 0, 1)
      .CallRuntime(Runtime::kIsArray, reg, 1)
      .CallRuntime(Runtime::kIsArray, wide, 1)
      .CallRuntimeForPair(Runtime::kLoadLookupSlotForCall, reg, 1, other)
      .CallRuntimeForPair(Runtime::kLoadLookupSlotForCall, wide, 1, other)
      .CallJSRuntime(Context::SPREAD_ITERABLE_INDEX, reg, 1)
      .CallJSRuntime(Context::SPREAD_ITERABLE_INDEX, wide, 1);

  // Emit binary operator invocations.
  builder.BinaryOperation(Token::Value::ADD, reg, 1)
      .BinaryOperation(Token::Value::SUB, reg, 2)
      .BinaryOperation(Token::Value::MUL, reg, 3)
      .BinaryOperation(Token::Value::DIV, reg, 4)
      .BinaryOperation(Token::Value::MOD, reg, 5);

  // Emit bitwise operator invocations
  builder.BinaryOperation(Token::Value::BIT_OR, reg, 6)
      .BinaryOperation(Token::Value::BIT_XOR, reg, 7)
      .BinaryOperation(Token::Value::BIT_AND, reg, 8);

  // Emit shift operator invocations
  builder.BinaryOperation(Token::Value::SHL, reg, 9)
      .BinaryOperation(Token::Value::SAR, reg, 10)
      .BinaryOperation(Token::Value::SHR, reg, 11);

  // Emit peephole optimizations of LdaSmi followed by binary operation.
  builder.LoadLiteral(Smi::FromInt(1))
      .BinaryOperation(Token::Value::ADD, reg, 1)
      .LoadLiteral(Smi::FromInt(2))
      .BinaryOperation(Token::Value::SUB, reg, 2)
      .LoadLiteral(Smi::FromInt(3))
      .BinaryOperation(Token::Value::BIT_AND, reg, 3)
      .LoadLiteral(Smi::FromInt(4))
      .BinaryOperation(Token::Value::BIT_OR, reg, 4)
      .LoadLiteral(Smi::FromInt(5))
      .BinaryOperation(Token::Value::SHL, reg, 5)
      .LoadLiteral(Smi::FromInt(6))
      .BinaryOperation(Token::Value::SAR, reg, 6);

  // Emit count operatior invocations
  builder.CountOperation(Token::Value::ADD, 1)
      .CountOperation(Token::Value::SUB, 1);

  // Emit unary operator invocations.
  builder
      .LogicalNot()  // ToBooleanLogicalNot
      .LogicalNot()  // non-ToBoolean LogicalNot
      .TypeOf();

  // Emit delete
  builder.Delete(reg, LanguageMode::SLOPPY).Delete(reg, LanguageMode::STRICT);

  // Emit new.
  builder.New(reg, reg, 0);
  builder.New(wide, wide, 0);

  // Emit test operator invocations.
  builder.CompareOperation(Token::Value::EQ, reg)
      .CompareOperation(Token::Value::NE, reg)
      .CompareOperation(Token::Value::EQ_STRICT, reg)
      .CompareOperation(Token::Value::LT, reg)
      .CompareOperation(Token::Value::GT, reg)
      .CompareOperation(Token::Value::LTE, reg)
      .CompareOperation(Token::Value::GTE, reg)
      .CompareOperation(Token::Value::INSTANCEOF, reg)
      .CompareOperation(Token::Value::IN, reg);

  // Emit cast operator invocations.
  builder.CastAccumulatorToNumber(reg)
      .CastAccumulatorToJSObject(reg)
      .CastAccumulatorToName(reg);

  // Emit control flow. Return must be the last instruction.
  BytecodeLabel start;
  builder.Bind(&start);
  {
    // Short jumps with Imm8 operands
    BytecodeLabel after_jump;
    builder.Jump(&start)
        .Bind(&after_jump)
        .JumpIfNull(&start)
        .JumpIfUndefined(&start)
        .JumpIfNotHole(&start);
  }

  // Longer jumps with constant operands
  BytecodeLabel end[8];
  {
    BytecodeLabel after_jump;
    builder.Jump(&end[0])
        .Bind(&after_jump)
        .LoadTrue()
        .JumpIfTrue(&end[1])
        .LoadTrue()
        .JumpIfFalse(&end[2])
        .LoadLiteral(Smi::FromInt(0))
        .JumpIfTrue(&end[3])
        .LoadLiteral(Smi::FromInt(0))
        .JumpIfFalse(&end[4])
        .JumpIfNull(&end[5])
        .JumpIfUndefined(&end[6])
        .JumpIfNotHole(&end[7]);
  }

  // Perform an operation that returns boolean value to
  // generate JumpIfTrue/False
  builder.CompareOperation(Token::Value::EQ, reg)
      .JumpIfTrue(&start)
      .CompareOperation(Token::Value::EQ, reg)
      .JumpIfFalse(&start);
  // Perform an operation that returns a non-boolean operation to
  // generate JumpIfToBooleanTrue/False.
  builder.BinaryOperation(Token::Value::ADD, reg, 1)
      .JumpIfTrue(&start)
      .BinaryOperation(Token::Value::ADD, reg, 2)
      .JumpIfFalse(&start);
  // Insert dummy ops to force longer jumps
  for (int i = 0; i < 128; i++) {
    builder.LoadTrue();
  }
  // Longer jumps requiring Constant operand
  {
    BytecodeLabel after_jump;
    builder.Jump(&start)
        .Bind(&after_jump)
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
    builder.BinaryOperation(Token::Value::ADD, reg, 1)
        .JumpIfTrue(&start)
        .BinaryOperation(Token::Value::ADD, reg, 2)
        .JumpIfFalse(&start);
  }

  // Emit stack check bytecode.
  builder.StackCheck(0);

  // Emit an OSR poll bytecode.
  builder.OsrPoll(1);

  // Emit throw and re-throw in it's own basic block so that the rest of the
  // code isn't omitted due to being dead.
  BytecodeLabel after_throw;
  builder.Throw().Bind(&after_throw);
  BytecodeLabel after_rethrow;
  builder.ReThrow().Bind(&after_rethrow);

  builder.ForInPrepare(reg, reg)
      .ForInDone(reg, reg)
      .ForInNext(reg, reg, reg, 1)
      .ForInStep(reg);
  builder.ForInPrepare(reg, wide)
      .ForInDone(reg, other)
      .ForInNext(wide, wide, wide, 1024)
      .ForInStep(reg);

  // Wide constant pool loads
  for (int i = 0; i < 256; i++) {
    // Emit junk in constant pool to force wide constant pool index.
    builder.LoadLiteral(factory->NewNumber(2.5321 + i));
  }
  builder.LoadLiteral(Smi::FromInt(20000000));
  Handle<String> wide_name = factory->NewStringFromStaticChars("var_wide_name");

  // Emit wide global load / store operations.
  builder.LoadGlobal(1024, TypeofMode::NOT_INSIDE_TYPEOF)
      .LoadGlobal(1024, TypeofMode::INSIDE_TYPEOF)
      .LoadGlobal(1024, TypeofMode::INSIDE_TYPEOF)
      .StoreGlobal(name, 1024, LanguageMode::SLOPPY)
      .StoreGlobal(wide_name, 1, LanguageMode::STRICT);

  // Emit extra wide global load.
  builder.LoadGlobal(1024 * 1024, TypeofMode::NOT_INSIDE_TYPEOF);

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

  // Emit loads which will be transformed to Ldr equivalents by the peephole
  // optimizer.
  builder.LoadNamedProperty(reg, name, 0)
      .StoreAccumulatorInRegister(reg)
      .LoadKeyedProperty(reg, 0)
      .StoreAccumulatorInRegister(reg)
      .LoadContextSlot(reg, 1)
      .StoreAccumulatorInRegister(reg)
      .LoadGlobal(0, TypeofMode::NOT_INSIDE_TYPEOF)
      .StoreAccumulatorInRegister(reg)
      .LoadUndefined()
      .StoreAccumulatorInRegister(reg);

  // CreateClosureWide
  builder.CreateClosure(1000, NOT_TENURED);

  // Emit wide variant of literal creation operations.
  builder.CreateRegExpLiteral(factory->NewStringFromStaticChars("wide_literal"),
                              0, 0)
      .CreateArrayLiteral(factory->NewFixedArray(2), 0, 0)
      .CreateObjectLiteral(factory->NewFixedArray(2), 0, 0, reg);

  // Longer jumps requiring ConstantWide operand
  {
    BytecodeLabel after_jump;
    builder.Jump(&start)
        .Bind(&after_jump)
        .JumpIfNull(&start)
        .JumpIfUndefined(&start)
        .JumpIfNotHole(&start);
  }

  // Perform an operation that returns boolean value to
  // generate JumpIfTrue/False
  builder.CompareOperation(Token::Value::EQ, reg)
      .JumpIfTrue(&start)
      .CompareOperation(Token::Value::EQ, reg)
      .JumpIfFalse(&start);

  // Perform an operation that returns a non-boolean operation to
  // generate JumpIfToBooleanTrue/False.
  builder.BinaryOperation(Token::Value::ADD, reg, 1)
      .JumpIfTrue(&start)
      .BinaryOperation(Token::Value::ADD, reg, 2)
      .JumpIfFalse(&start);

  // Emit generator operations
  builder.SuspendGenerator(reg)
      .ResumeGenerator(reg);

  // Intrinsics handled by the interpreter.
  builder.CallRuntime(Runtime::kInlineIsArray, reg, 1)
      .CallRuntime(Runtime::kInlineIsArray, wide, 1);

  builder.Debugger();
  for (size_t i = 0; i < arraysize(end); i++) {
    builder.Bind(&end[i]);
  }
  builder.Return();

  // Generate BytecodeArray.
  Handle<BytecodeArray> the_array = builder.ToBytecodeArray(isolate());
  CHECK_EQ(the_array->frame_size(),
           builder.fixed_and_temporary_register_count() * kPointerSize);

  // Build scorecard of bytecodes encountered in the BytecodeArray.
  std::vector<int> scorecard(Bytecodes::ToByte(Bytecode::kLast) + 1);

  Bytecode final_bytecode = Bytecode::kLdaZero;
  int i = 0;
  while (i < the_array->length()) {
    uint8_t code = the_array->get(i);
    scorecard[code] += 1;
    final_bytecode = Bytecodes::FromByte(code);
    OperandScale operand_scale = OperandScale::kSingle;
    int prefix_offset = 0;
    if (Bytecodes::IsPrefixScalingBytecode(final_bytecode)) {
      operand_scale = Bytecodes::PrefixBytecodeToOperandScale(final_bytecode);
      prefix_offset = 1;
      code = the_array->get(i + 1);
      final_bytecode = Bytecodes::FromByte(code);
    }
    i += prefix_offset + Bytecodes::Size(final_bytecode, operand_scale);
  }

  // Insert entry for illegal bytecode as this is never willingly emitted.
  scorecard[Bytecodes::ToByte(Bytecode::kIllegal)] = 1;

  // Insert entry for nop bytecode as this often gets optimized out.
  scorecard[Bytecodes::ToByte(Bytecode::kNop)] = 1;

  if (!FLAG_ignition_peephole) {
    // Insert entries for bytecodes only emitted by peephole optimizer.
    scorecard[Bytecodes::ToByte(Bytecode::kLdrNamedProperty)] = 1;
    scorecard[Bytecodes::ToByte(Bytecode::kLdrKeyedProperty)] = 1;
    scorecard[Bytecodes::ToByte(Bytecode::kLdrGlobal)] = 1;
    scorecard[Bytecodes::ToByte(Bytecode::kLdrContextSlot)] = 1;
    scorecard[Bytecodes::ToByte(Bytecode::kLdrUndefined)] = 1;
    scorecard[Bytecodes::ToByte(Bytecode::kLogicalNot)] = 1;
    scorecard[Bytecodes::ToByte(Bytecode::kJump)] = 1;
    scorecard[Bytecodes::ToByte(Bytecode::kJumpIfTrue)] = 1;
    scorecard[Bytecodes::ToByte(Bytecode::kJumpIfFalse)] = 1;
    scorecard[Bytecodes::ToByte(Bytecode::kJumpIfTrueConstant)] = 1;
    scorecard[Bytecodes::ToByte(Bytecode::kJumpIfFalseConstant)] = 1;
    scorecard[Bytecodes::ToByte(Bytecode::kAddSmi)] = 1;
    scorecard[Bytecodes::ToByte(Bytecode::kSubSmi)] = 1;
    scorecard[Bytecodes::ToByte(Bytecode::kBitwiseAndSmi)] = 1;
    scorecard[Bytecodes::ToByte(Bytecode::kBitwiseOrSmi)] = 1;
    scorecard[Bytecodes::ToByte(Bytecode::kShiftLeftSmi)] = 1;
    scorecard[Bytecodes::ToByte(Bytecode::kShiftRightSmi)] = 1;
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
  CanonicalHandleScope canonical(isolate());
  for (int locals = 0; locals < 5; locals++) {
    for (int contexts = 0; contexts < 4; contexts++) {
      for (int temps = 0; temps < 3; temps++) {
        BytecodeArrayBuilder builder(isolate(), zone(), 0, contexts, locals);
        BytecodeRegisterAllocator temporaries(
            zone(), builder.temporary_register_allocator());
        for (int i = 0; i < locals + contexts; i++) {
          builder.LoadLiteral(Smi::FromInt(0));
          builder.StoreAccumulatorInRegister(Register(i));
        }
        for (int i = 0; i < temps; i++) {
          builder.LoadLiteral(Smi::FromInt(0));
          builder.StoreAccumulatorInRegister(temporaries.NewRegister());
        }
        if (temps > 0) {
          // Ensure temporaries are used so not optimized away by the
          // register optimizer.
          builder.New(Register(locals + contexts), Register(locals + contexts),
                      static_cast<size_t>(temps));
        }
        builder.Return();

        Handle<BytecodeArray> the_array = builder.ToBytecodeArray(isolate());
        int total_registers = locals + contexts + temps;
        CHECK_EQ(the_array->frame_size(), total_registers * kPointerSize);
      }
    }
  }
}


TEST_F(BytecodeArrayBuilderTest, RegisterValues) {
  CanonicalHandleScope canonical(isolate());
  int index = 1;

  Register the_register(index);
  CHECK_EQ(the_register.index(), index);

  int actual_operand = the_register.ToOperand();
  int actual_index = Register::FromOperand(actual_operand).index();
  CHECK_EQ(actual_index, index);
}


TEST_F(BytecodeArrayBuilderTest, Parameters) {
  CanonicalHandleScope canonical(isolate());
  BytecodeArrayBuilder builder(isolate(), zone(), 10, 0, 0);

  Register param0(builder.Parameter(0));
  Register param9(builder.Parameter(9));
  CHECK_EQ(param9.index() - param0.index(), 9);
}


TEST_F(BytecodeArrayBuilderTest, RegisterType) {
  CanonicalHandleScope canonical(isolate());
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
  CanonicalHandleScope canonical(isolate());
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

  Handle<BytecodeArray> array = builder.ToBytecodeArray(isolate());
  // Should only have one entry for each identical constant.
  CHECK_EQ(array->constant_pool()->length(), 3);
}

static Bytecode PeepholeToBoolean(Bytecode jump_bytecode) {
  return FLAG_ignition_peephole
             ? Bytecodes::GetJumpWithoutToBoolean(jump_bytecode)
             : jump_bytecode;
}

TEST_F(BytecodeArrayBuilderTest, ForwardJumps) {
  CanonicalHandleScope canonical(isolate());
  static const int kFarJumpDistance = 256;

  BytecodeArrayBuilder builder(isolate(), zone(), 0, 0, 1);

  Register reg(0);
  BytecodeLabel far0, far1, far2, far3, far4;
  BytecodeLabel near0, near1, near2, near3, near4;
  BytecodeLabel after_jump0, after_jump1;

  builder.Jump(&near0)
      .Bind(&after_jump0)
      .CompareOperation(Token::Value::EQ, reg)
      .JumpIfTrue(&near1)
      .CompareOperation(Token::Value::EQ, reg)
      .JumpIfFalse(&near2)
      .BinaryOperation(Token::Value::ADD, reg, 1)
      .JumpIfTrue(&near3)
      .BinaryOperation(Token::Value::ADD, reg, 2)
      .JumpIfFalse(&near4)
      .Bind(&near0)
      .Bind(&near1)
      .Bind(&near2)
      .Bind(&near3)
      .Bind(&near4)
      .Jump(&far0)
      .Bind(&after_jump1)
      .CompareOperation(Token::Value::EQ, reg)
      .JumpIfTrue(&far1)
      .CompareOperation(Token::Value::EQ, reg)
      .JumpIfFalse(&far2)
      .BinaryOperation(Token::Value::ADD, reg, 3)
      .JumpIfTrue(&far3)
      .BinaryOperation(Token::Value::ADD, reg, 4)
      .JumpIfFalse(&far4);
  for (int i = 0; i < kFarJumpDistance - 20; i++) {
    builder.Debugger();
  }
  builder.Bind(&far0).Bind(&far1).Bind(&far2).Bind(&far3).Bind(&far4);
  builder.Return();

  Handle<BytecodeArray> array = builder.ToBytecodeArray(isolate());
  DCHECK_EQ(array->length(), 40 + kFarJumpDistance - 20 + 1);

  BytecodeArrayIterator iterator(array);
  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJump);
  CHECK_EQ(iterator.GetImmediateOperand(0), 20);
  iterator.Advance();

  // Ignore compare operation.
  iterator.Advance();

  CHECK_EQ(iterator.current_bytecode(),
           PeepholeToBoolean(Bytecode::kJumpIfToBooleanTrue));
  CHECK_EQ(iterator.GetImmediateOperand(0), 16);
  iterator.Advance();

  // Ignore compare operation.
  iterator.Advance();

  CHECK_EQ(iterator.current_bytecode(),
           PeepholeToBoolean(Bytecode::kJumpIfToBooleanFalse));
  CHECK_EQ(iterator.GetImmediateOperand(0), 12);
  iterator.Advance();

  // Ignore add operation.
  iterator.Advance();

  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJumpIfToBooleanTrue);
  CHECK_EQ(iterator.GetImmediateOperand(0), 7);
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

  CHECK_EQ(iterator.current_bytecode(),
           PeepholeToBoolean(Bytecode::kJumpIfToBooleanTrueConstant));
  CHECK_EQ(*iterator.GetConstantForIndexOperand(0),
           Smi::FromInt(kFarJumpDistance - 4));
  iterator.Advance();

  // Ignore compare operation.
  iterator.Advance();

  CHECK_EQ(iterator.current_bytecode(),
           PeepholeToBoolean(Bytecode::kJumpIfToBooleanFalseConstant));
  CHECK_EQ(*iterator.GetConstantForIndexOperand(0),
           Smi::FromInt(kFarJumpDistance - 8));
  iterator.Advance();

  // Ignore add operation.
  iterator.Advance();

  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJumpIfToBooleanTrueConstant);
  CHECK_EQ(*iterator.GetConstantForIndexOperand(0),
           Smi::FromInt(kFarJumpDistance - 13));
  iterator.Advance();

  // Ignore add operation.
  iterator.Advance();

  CHECK_EQ(iterator.current_bytecode(),
           Bytecode::kJumpIfToBooleanFalseConstant);
  CHECK_EQ(*iterator.GetConstantForIndexOperand(0),
           Smi::FromInt(kFarJumpDistance - 18));
  iterator.Advance();
}


TEST_F(BytecodeArrayBuilderTest, BackwardJumps) {
  CanonicalHandleScope canonical(isolate());
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
      .BinaryOperation(Token::Value::ADD, reg, 1)
      .JumpIfTrue(&label3)
      .Bind(&label4)
      .BinaryOperation(Token::Value::ADD, reg, 2)
      .JumpIfFalse(&label4);
  for (int i = 0; i < 62; i++) {
    BytecodeLabel after_jump;
    builder.Jump(&label4).Bind(&after_jump);
  }

  // Add padding to force wide backwards jumps.
  for (int i = 0; i < 256; i++) {
    builder.Debugger();
  }

  builder.BinaryOperation(Token::Value::ADD, reg, 1).JumpIfFalse(&label4);
  builder.BinaryOperation(Token::Value::ADD, reg, 2).JumpIfTrue(&label3);
  builder.CompareOperation(Token::Value::EQ, reg).JumpIfFalse(&label2);
  builder.CompareOperation(Token::Value::EQ, reg).JumpIfTrue(&label1);
  builder.Jump(&label0);
  BytecodeLabel end;
  builder.Bind(&end);
  builder.Return();

  Handle<BytecodeArray> array = builder.ToBytecodeArray(isolate());
  BytecodeArrayIterator iterator(array);
  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJump);
  CHECK_EQ(iterator.GetImmediateOperand(0), 0);
  iterator.Advance();
  // Ignore compare operation.
  iterator.Advance();
  CHECK_EQ(iterator.current_bytecode(),
           PeepholeToBoolean(Bytecode::kJumpIfToBooleanTrue));
  CHECK_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  CHECK_EQ(iterator.GetImmediateOperand(0), -2);
  iterator.Advance();
  // Ignore compare operation.
  iterator.Advance();
  CHECK_EQ(iterator.current_bytecode(),
           PeepholeToBoolean(Bytecode::kJumpIfToBooleanFalse));
  CHECK_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  CHECK_EQ(iterator.GetImmediateOperand(0), -2);
  iterator.Advance();
  // Ignore binary operation.
  iterator.Advance();
  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJumpIfToBooleanTrue);
  CHECK_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  CHECK_EQ(iterator.GetImmediateOperand(0), -3);
  iterator.Advance();
  // Ignore binary operation.
  iterator.Advance();
  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJumpIfToBooleanFalse);
  CHECK_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  CHECK_EQ(iterator.GetImmediateOperand(0), -3);
  iterator.Advance();
  for (int i = 0; i < 62; i++) {
    CHECK_EQ(iterator.current_bytecode(), Bytecode::kJump);
    CHECK_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
    // offset of 5 (3 for binary operation and 2 for jump)
    CHECK_EQ(iterator.GetImmediateOperand(0), -i * 2 - 5);
    iterator.Advance();
  }
  // Check padding to force wide backwards jumps.
  for (int i = 0; i < 256; i++) {
    CHECK_EQ(iterator.current_bytecode(), Bytecode::kDebugger);
    iterator.Advance();
  }
  // Ignore binary operation.
  iterator.Advance();
  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJumpIfToBooleanFalse);
  CHECK_EQ(iterator.current_operand_scale(), OperandScale::kDouble);
  CHECK_EQ(iterator.GetImmediateOperand(0), -389);
  iterator.Advance();
  // Ignore binary operation.
  iterator.Advance();
  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJumpIfToBooleanTrue);
  CHECK_EQ(iterator.current_operand_scale(), OperandScale::kDouble);
  CHECK_EQ(iterator.GetImmediateOperand(0), -401);
  iterator.Advance();
  // Ignore compare operation.
  iterator.Advance();
  CHECK_EQ(iterator.current_bytecode(),
           PeepholeToBoolean(Bytecode::kJumpIfToBooleanFalse));
  CHECK_EQ(iterator.current_operand_scale(), OperandScale::kDouble);
  CHECK_EQ(iterator.GetImmediateOperand(0), -411);
  iterator.Advance();
  // Ignore compare operation.
  iterator.Advance();
  CHECK_EQ(iterator.current_bytecode(),
           PeepholeToBoolean(Bytecode::kJumpIfToBooleanTrue));
  CHECK_EQ(iterator.current_operand_scale(), OperandScale::kDouble);
  CHECK_EQ(iterator.GetImmediateOperand(0), -421);
  iterator.Advance();
  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJump);
  CHECK_EQ(iterator.current_operand_scale(), OperandScale::kDouble);
  CHECK_EQ(iterator.GetImmediateOperand(0), -427);
  iterator.Advance();
  CHECK_EQ(iterator.current_bytecode(), Bytecode::kReturn);
  iterator.Advance();
  CHECK(iterator.done());
}


TEST_F(BytecodeArrayBuilderTest, LabelReuse) {
  CanonicalHandleScope canonical(isolate());
  BytecodeArrayBuilder builder(isolate(), zone(), 0, 0, 0);

  // Labels can only have 1 forward reference, but
  // can be referred to mulitple times once bound.
  BytecodeLabel label, after_jump0, after_jump1;

  builder.Jump(&label)
      .Bind(&label)
      .Jump(&label)
      .Bind(&after_jump0)
      .Jump(&label)
      .Bind(&after_jump1)
      .Return();

  Handle<BytecodeArray> array = builder.ToBytecodeArray(isolate());
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
  CanonicalHandleScope canonical(isolate());
  static const int kRepeats = 3;

  BytecodeArrayBuilder builder(isolate(), zone(), 0, 0, 0);
  for (int i = 0; i < kRepeats; i++) {
    BytecodeLabel label, after_jump0, after_jump1;
    builder.Jump(&label)
        .Bind(&label)
        .Jump(&label)
        .Bind(&after_jump0)
        .Jump(&label)
        .Bind(&after_jump1);
  }
  builder.Return();

  Handle<BytecodeArray> array = builder.ToBytecodeArray(isolate());
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
