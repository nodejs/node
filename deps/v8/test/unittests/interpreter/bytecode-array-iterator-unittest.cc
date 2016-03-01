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
  BytecodeArrayBuilder builder(isolate(), zone());
  builder.set_parameter_count(3);
  builder.set_locals_count(2);
  builder.set_context_count(0);

  Factory* factory = isolate()->factory();
  Handle<HeapObject> heap_num_0 = factory->NewHeapNumber(2.718);
  Handle<HeapObject> heap_num_1 = factory->NewHeapNumber(2147483647);
  Smi* zero = Smi::FromInt(0);
  Smi* smi_0 = Smi::FromInt(64);
  Smi* smi_1 = Smi::FromInt(-65536);
  Register reg_0(0);
  Register reg_1(1);
  Register reg_2 = Register::FromParameterIndex(2, builder.parameter_count());
  Handle<String> name = factory->NewStringFromStaticChars("abc");
  int name_index = 3;
  int feedback_slot = 97;

  builder.LoadLiteral(heap_num_0)
      .LoadLiteral(heap_num_1)
      .LoadLiteral(zero)
      .LoadLiteral(smi_0)
      .LoadLiteral(smi_1)
      .LoadAccumulatorWithRegister(reg_0)
      .LoadNamedProperty(reg_1, name, feedback_slot, LanguageMode::SLOPPY)
      .StoreAccumulatorInRegister(reg_2)
      .CallRuntime(Runtime::kLoadIC_Miss, reg_0, 1)
      .Return();

  // Test iterator sees the expected output from the builder.
  BytecodeArrayIterator iterator(builder.ToBytecodeArray());
  CHECK_EQ(iterator.current_bytecode(), Bytecode::kLdaConstant);
  CHECK(iterator.GetConstantForIndexOperand(0).is_identical_to(heap_num_0));
  CHECK(!iterator.done());
  iterator.Advance();

  CHECK_EQ(iterator.current_bytecode(), Bytecode::kLdaConstant);
  CHECK(iterator.GetConstantForIndexOperand(0).is_identical_to(heap_num_1));
  CHECK(!iterator.done());
  iterator.Advance();

  CHECK_EQ(iterator.current_bytecode(), Bytecode::kLdaZero);
  CHECK(!iterator.done());
  iterator.Advance();

  CHECK_EQ(iterator.current_bytecode(), Bytecode::kLdaSmi8);
  CHECK_EQ(Smi::FromInt(iterator.GetImmediateOperand(0)), smi_0);
  CHECK(!iterator.done());
  iterator.Advance();

  CHECK_EQ(iterator.current_bytecode(), Bytecode::kLdaConstant);
  CHECK_EQ(*iterator.GetConstantForIndexOperand(0), smi_1);
  CHECK(!iterator.done());
  iterator.Advance();

  CHECK_EQ(iterator.current_bytecode(), Bytecode::kLdar);
  CHECK_EQ(iterator.GetRegisterOperand(0).index(), reg_0.index());
  CHECK(!iterator.done());
  iterator.Advance();

  CHECK_EQ(iterator.current_bytecode(), Bytecode::kLoadICSloppy);
  CHECK_EQ(iterator.GetRegisterOperand(0).index(), reg_1.index());
  CHECK_EQ(iterator.GetIndexOperand(1), name_index);
  CHECK_EQ(iterator.GetIndexOperand(2), feedback_slot);
  CHECK(!iterator.done());
  iterator.Advance();

  CHECK_EQ(iterator.current_bytecode(), Bytecode::kStar);
  CHECK_EQ(iterator.GetRegisterOperand(0).index(), reg_2.index());
  CHECK(!iterator.done());
  iterator.Advance();

  CHECK_EQ(iterator.current_bytecode(), Bytecode::kCallRuntime);
  CHECK_EQ(static_cast<Runtime::FunctionId>(iterator.GetIndexOperand(0)),
           Runtime::kLoadIC_Miss);
  CHECK_EQ(iterator.GetRegisterOperand(1).index(), reg_0.index());
  CHECK_EQ(iterator.GetCountOperand(2), 1);
  CHECK(!iterator.done());
  iterator.Advance();

  CHECK_EQ(iterator.current_bytecode(), Bytecode::kReturn);
  CHECK(!iterator.done());
  iterator.Advance();
  CHECK(iterator.done());
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
