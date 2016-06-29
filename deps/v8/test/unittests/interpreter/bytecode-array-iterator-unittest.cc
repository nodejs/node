// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/interpreter/bytecode-array-builder.h"
#include "src/interpreter/bytecode-array-iterator.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace interpreter {

class BytecodeArrayIteratorTest : public TestWithIsolateAndZone {
 public:
  BytecodeArrayIteratorTest() {}
  ~BytecodeArrayIteratorTest() override {}
};


TEST_F(BytecodeArrayIteratorTest, IteratesBytecodeArray) {
  // Use a builder to create an array with containing multiple bytecodes
  // with 0, 1 and 2 operands.
  BytecodeArrayBuilder builder(isolate(), zone(), 3, 3, 0);
  Factory* factory = isolate()->factory();
  Handle<HeapObject> heap_num_0 = factory->NewHeapNumber(2.718);
  Handle<HeapObject> heap_num_1 = factory->NewHeapNumber(2147483647);
  Smi* zero = Smi::FromInt(0);
  Smi* smi_0 = Smi::FromInt(64);
  Smi* smi_1 = Smi::FromInt(-65536);
  Register reg_0(0);
  Register reg_1(1);
  Register param = Register::FromParameterIndex(2, builder.parameter_count());
  Handle<String> name = factory->NewStringFromStaticChars("abc");
  int name_index = 2;
  int feedback_slot = 97;

  builder.LoadLiteral(heap_num_0)
      .LoadLiteral(heap_num_1)
      .LoadLiteral(zero)
      .LoadLiteral(smi_0)
      .LoadLiteral(smi_1)
      .LoadAccumulatorWithRegister(reg_0)
      .LoadNamedProperty(reg_1, name, feedback_slot)
      .StoreAccumulatorInRegister(param)
      .CallRuntimeForPair(Runtime::kLoadLookupSlotForCall, param, 1, reg_0)
      .ForInPrepare(reg_0)
      .CallRuntime(Runtime::kLoadIC_Miss, reg_0, 1)
      .Debugger()
      .LoadGlobal(name, 0x10000000, TypeofMode::NOT_INSIDE_TYPEOF)
      .Return();

  // Test iterator sees the expected output from the builder.
  BytecodeArrayIterator iterator(builder.ToBytecodeArray());
  const int kPrefixByteSize = 1;
  int offset = 0;

  CHECK_EQ(iterator.current_bytecode(), Bytecode::kLdaConstant);
  CHECK_EQ(iterator.current_offset(), offset);
  CHECK_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  CHECK(iterator.GetConstantForIndexOperand(0).is_identical_to(heap_num_0));
  CHECK(!iterator.done());
  offset += Bytecodes::Size(Bytecode::kLdaConstant, OperandScale::kSingle);
  iterator.Advance();

  CHECK_EQ(iterator.current_bytecode(), Bytecode::kLdaConstant);
  CHECK_EQ(iterator.current_offset(), offset);
  CHECK_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  CHECK(iterator.GetConstantForIndexOperand(0).is_identical_to(heap_num_1));
  CHECK(!iterator.done());
  offset += Bytecodes::Size(Bytecode::kLdaConstant, OperandScale::kSingle);
  iterator.Advance();

  CHECK_EQ(iterator.current_bytecode(), Bytecode::kLdaZero);
  CHECK_EQ(iterator.current_offset(), offset);
  CHECK_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  CHECK(!iterator.done());
  offset += Bytecodes::Size(Bytecode::kLdaZero, OperandScale::kSingle);
  iterator.Advance();

  CHECK_EQ(iterator.current_bytecode(), Bytecode::kLdaSmi);
  CHECK_EQ(iterator.current_offset(), offset);
  CHECK_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  CHECK_EQ(Smi::FromInt(iterator.GetImmediateOperand(0)), smi_0);
  CHECK(!iterator.done());
  offset += Bytecodes::Size(Bytecode::kLdaSmi, OperandScale::kSingle);
  iterator.Advance();

  CHECK_EQ(iterator.current_bytecode(), Bytecode::kLdaSmi);
  CHECK_EQ(iterator.current_offset(), offset);
  CHECK_EQ(iterator.current_operand_scale(), OperandScale::kQuadruple);
  CHECK_EQ(Smi::FromInt(iterator.GetImmediateOperand(0)), smi_1);
  CHECK(!iterator.done());
  offset += Bytecodes::Size(Bytecode::kLdaSmi, OperandScale::kQuadruple) +
            kPrefixByteSize;
  iterator.Advance();

  CHECK_EQ(iterator.current_bytecode(), Bytecode::kLdar);
  CHECK_EQ(iterator.current_offset(), offset);
  CHECK_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  CHECK_EQ(iterator.GetRegisterOperand(0).index(), reg_0.index());
  CHECK(!iterator.done());
  offset += Bytecodes::Size(Bytecode::kLdar, OperandScale::kSingle);
  iterator.Advance();

  CHECK_EQ(iterator.current_bytecode(), Bytecode::kLoadIC);
  CHECK_EQ(iterator.current_offset(), offset);
  CHECK_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  CHECK_EQ(iterator.GetRegisterOperand(0).index(), reg_1.index());
  CHECK_EQ(iterator.GetIndexOperand(1), name_index);
  CHECK_EQ(iterator.GetIndexOperand(2), feedback_slot);
  CHECK(!iterator.done());
  offset += Bytecodes::Size(Bytecode::kLoadIC, OperandScale::kSingle);
  iterator.Advance();

  CHECK_EQ(iterator.current_bytecode(), Bytecode::kStar);
  CHECK_EQ(iterator.current_offset(), offset);
  CHECK_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  CHECK_EQ(iterator.GetRegisterOperand(0).index(), param.index());
  CHECK_EQ(iterator.GetRegisterOperandRange(0), 1);
  CHECK(!iterator.done());
  offset += Bytecodes::Size(Bytecode::kStar, OperandScale::kSingle);
  iterator.Advance();

  CHECK_EQ(iterator.current_bytecode(), Bytecode::kCallRuntimeForPair);
  CHECK_EQ(iterator.current_offset(), offset);
  CHECK_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  CHECK_EQ(iterator.GetRuntimeIdOperand(0), Runtime::kLoadLookupSlotForCall);
  CHECK_EQ(iterator.GetRegisterOperand(1).index(), param.index());
  CHECK_EQ(iterator.GetRegisterOperandRange(1), 1);
  CHECK_EQ(iterator.GetRegisterCountOperand(2), 1);
  CHECK_EQ(iterator.GetRegisterOperand(3).index(), reg_0.index());
  CHECK_EQ(iterator.GetRegisterOperandRange(3), 2);
  CHECK(!iterator.done());
  offset +=
      Bytecodes::Size(Bytecode::kCallRuntimeForPair, OperandScale::kSingle);
  iterator.Advance();

  CHECK_EQ(iterator.current_bytecode(), Bytecode::kForInPrepare);
  CHECK_EQ(iterator.current_offset(), offset);
  CHECK_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  CHECK_EQ(iterator.GetRegisterOperand(0).index(), reg_0.index());
  CHECK_EQ(iterator.GetRegisterOperandRange(0), 3);
  CHECK(!iterator.done());
  offset += Bytecodes::Size(Bytecode::kForInPrepare, OperandScale::kSingle);
  iterator.Advance();

  CHECK_EQ(iterator.current_bytecode(), Bytecode::kCallRuntime);
  CHECK_EQ(iterator.current_offset(), offset);
  CHECK_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  CHECK_EQ(static_cast<Runtime::FunctionId>(iterator.GetRuntimeIdOperand(0)),
           Runtime::kLoadIC_Miss);
  CHECK_EQ(iterator.GetRegisterOperand(1).index(), reg_0.index());
  CHECK_EQ(iterator.GetRegisterCountOperand(2), 1);
  CHECK(!iterator.done());
  offset += Bytecodes::Size(Bytecode::kCallRuntime, OperandScale::kSingle);
  iterator.Advance();

  CHECK_EQ(iterator.current_bytecode(), Bytecode::kDebugger);
  CHECK_EQ(iterator.current_offset(), offset);
  CHECK_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  CHECK(!iterator.done());
  offset += Bytecodes::Size(Bytecode::kDebugger, OperandScale::kSingle);
  iterator.Advance();

  CHECK_EQ(iterator.current_bytecode(), Bytecode::kLdaGlobal);
  CHECK_EQ(iterator.current_offset(), offset);
  CHECK_EQ(iterator.current_operand_scale(), OperandScale::kQuadruple);
  CHECK_EQ(iterator.current_bytecode_size(), 10);
  CHECK_EQ(iterator.GetIndexOperand(1), 0x10000000);
  offset += Bytecodes::Size(Bytecode::kLdaGlobal, OperandScale::kQuadruple) +
            kPrefixByteSize;
  iterator.Advance();

  CHECK_EQ(iterator.current_bytecode(), Bytecode::kReturn);
  CHECK_EQ(iterator.current_offset(), offset);
  CHECK_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  CHECK(!iterator.done());
  iterator.Advance();
  CHECK(iterator.done());
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
