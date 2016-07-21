// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/factory.h"
#include "src/interpreter/bytecode-peephole-optimizer.h"
#include "src/interpreter/constant-array-builder.h"
#include "src/objects-inl.h"
#include "src/objects.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace interpreter {

class BytecodePeepholeOptimizerTest : public BytecodePipelineStage,
                                      public TestWithIsolateAndZone {
 public:
  BytecodePeepholeOptimizerTest()
      : constant_array_builder_(isolate(), zone()),
        peephole_optimizer_(&constant_array_builder_, this) {}
  ~BytecodePeepholeOptimizerTest() override {}

  size_t FlushForOffset() override {
    flush_for_offset_count_++;
    return 0;
  };

  void FlushBasicBlock() override { flush_basic_block_count_++; }

  void Write(BytecodeNode* node) override {
    write_count_++;
    last_written_.Clone(node);
  }

  BytecodePeepholeOptimizer* optimizer() { return &peephole_optimizer_; }
  ConstantArrayBuilder* constant_array() { return &constant_array_builder_; }

  int flush_for_offset_count() const { return flush_for_offset_count_; }
  int flush_basic_block_count() const { return flush_basic_block_count_; }
  int write_count() const { return write_count_; }
  const BytecodeNode& last_written() const { return last_written_; }

 private:
  ConstantArrayBuilder constant_array_builder_;
  BytecodePeepholeOptimizer peephole_optimizer_;

  int flush_for_offset_count_ = 0;
  int flush_basic_block_count_ = 0;
  int write_count_ = 0;
  BytecodeNode last_written_;
};

// Sanity tests.

TEST_F(BytecodePeepholeOptimizerTest, FlushForOffsetPassThrough) {
  CHECK_EQ(flush_for_offset_count(), 0);
  CHECK_EQ(optimizer()->FlushForOffset(), 0);
  CHECK_EQ(flush_for_offset_count(), 1);
}

TEST_F(BytecodePeepholeOptimizerTest, FlushForOffsetRightSize) {
  BytecodeNode node(Bytecode::kAdd, Register(0).ToOperand(),
                    OperandScale::kQuadruple);
  optimizer()->Write(&node);
  CHECK_EQ(optimizer()->FlushForOffset(), node.Size());
  CHECK_EQ(flush_for_offset_count(), 1);
  CHECK_EQ(write_count(), 0);
}

TEST_F(BytecodePeepholeOptimizerTest, FlushForOffsetNop) {
  BytecodeNode node(Bytecode::kNop);
  optimizer()->Write(&node);
  CHECK_EQ(optimizer()->FlushForOffset(), 0);
  CHECK_EQ(flush_for_offset_count(), 1);
  CHECK_EQ(write_count(), 0);
}

TEST_F(BytecodePeepholeOptimizerTest, FlushForOffsetNopExpression) {
  BytecodeNode node(Bytecode::kNop);
  node.source_info().Update({3, false});
  optimizer()->Write(&node);
  CHECK_EQ(optimizer()->FlushForOffset(), 0);
  CHECK_EQ(flush_for_offset_count(), 1);
  CHECK_EQ(write_count(), 0);
}

TEST_F(BytecodePeepholeOptimizerTest, FlushForOffsetNopStatement) {
  BytecodeNode node(Bytecode::kNop);
  node.source_info().Update({3, true});
  optimizer()->Write(&node);
  CHECK_EQ(optimizer()->FlushForOffset(), node.Size());
  CHECK_EQ(flush_for_offset_count(), 1);
  CHECK_EQ(write_count(), 0);
}

TEST_F(BytecodePeepholeOptimizerTest, FlushBasicBlockPassThrough) {
  CHECK_EQ(flush_basic_block_count(), 0);
  optimizer()->FlushBasicBlock();
  CHECK_EQ(flush_basic_block_count(), 1);
  CHECK_EQ(write_count(), 0);
}

TEST_F(BytecodePeepholeOptimizerTest, WriteOneFlushBasicBlock) {
  BytecodeNode node(Bytecode::kAdd, Register(0).ToOperand(),
                    OperandScale::kQuadruple);
  optimizer()->Write(&node);
  CHECK_EQ(write_count(), 0);
  optimizer()->FlushBasicBlock();
  CHECK_EQ(write_count(), 1);
  CHECK_EQ(node, last_written());
}

// Tests covering BytecodePeepholeOptimizer::UpdateCurrentBytecode().

TEST_F(BytecodePeepholeOptimizerTest, KeepJumpIfToBooleanTrue) {
  BytecodeNode first(Bytecode::kLdaNull);
  BytecodeNode second(Bytecode::kJumpIfToBooleanTrue, 3, OperandScale::kSingle);
  optimizer()->Write(&first);
  CHECK_EQ(write_count(), 0);
  optimizer()->Write(&second);
  CHECK_EQ(write_count(), 1);
  CHECK_EQ(last_written(), first);
  optimizer()->FlushBasicBlock();
  CHECK_EQ(write_count(), 2);
  CHECK_EQ(last_written(), second);
}

TEST_F(BytecodePeepholeOptimizerTest, ElideJumpIfToBooleanTrue) {
  BytecodeNode first(Bytecode::kLdaTrue);
  BytecodeNode second(Bytecode::kJumpIfToBooleanTrue, 3, OperandScale::kSingle);
  optimizer()->Write(&first);
  CHECK_EQ(write_count(), 0);
  optimizer()->Write(&second);
  CHECK_EQ(write_count(), 1);
  CHECK_EQ(last_written(), first);
  optimizer()->FlushBasicBlock();
  CHECK_EQ(write_count(), 2);
  CHECK_EQ(last_written().bytecode(), Bytecode::kJumpIfTrue);
  CHECK_EQ(last_written().operand(0), second.operand(0));
}

TEST_F(BytecodePeepholeOptimizerTest, KeepToBooleanLogicalNot) {
  BytecodeNode first(Bytecode::kLdaNull);
  BytecodeNode second(Bytecode::kToBooleanLogicalNot);
  optimizer()->Write(&first);
  CHECK_EQ(write_count(), 0);
  optimizer()->Write(&second);
  CHECK_EQ(write_count(), 1);
  CHECK_EQ(last_written(), first);
  optimizer()->FlushBasicBlock();
  CHECK_EQ(write_count(), 2);
  CHECK_EQ(last_written(), second);
}

TEST_F(BytecodePeepholeOptimizerTest, ElideToBooleanLogicalNot) {
  BytecodeNode first(Bytecode::kLdaTrue);
  BytecodeNode second(Bytecode::kToBooleanLogicalNot);
  optimizer()->Write(&first);
  CHECK_EQ(write_count(), 0);
  optimizer()->Write(&second);
  CHECK_EQ(write_count(), 1);
  CHECK_EQ(last_written(), first);
  optimizer()->FlushBasicBlock();
  CHECK_EQ(write_count(), 2);
  CHECK_EQ(last_written().bytecode(), Bytecode::kLogicalNot);
}

// Tests covering BytecodePeepholeOptimizer::CanElideCurrent().

TEST_F(BytecodePeepholeOptimizerTest, LdarRxLdarRy) {
  BytecodeNode first(Bytecode::kLdar, Register(0).ToOperand(),
                     OperandScale::kSingle);
  BytecodeNode second(Bytecode::kLdar, Register(1).ToOperand(),
                      OperandScale::kSingle);
  optimizer()->Write(&first);
  optimizer()->FlushForOffset();  // Prevent CanElideLast removing |first|.
  CHECK_EQ(write_count(), 0);
  optimizer()->Write(&second);
  CHECK_EQ(write_count(), 1);
  CHECK_EQ(last_written(), first);
  optimizer()->FlushBasicBlock();
  CHECK_EQ(write_count(), 2);
  CHECK_EQ(last_written(), second);
}

TEST_F(BytecodePeepholeOptimizerTest, LdarRxLdarRx) {
  BytecodeNode first(Bytecode::kLdar, Register(0).ToOperand(),
                     OperandScale::kSingle);
  BytecodeNode second(Bytecode::kLdar, Register(0).ToOperand(),
                      OperandScale::kSingle);
  optimizer()->Write(&first);
  CHECK_EQ(write_count(), 0);
  optimizer()->FlushForOffset();  // Prevent CanElideLast removing |first|.
  optimizer()->Write(&second);
  CHECK_EQ(write_count(), 1);
  CHECK_EQ(last_written(), first);
  optimizer()->FlushBasicBlock();
  CHECK_EQ(write_count(), 1);
}

TEST_F(BytecodePeepholeOptimizerTest, LdarRxLdarRxStatement) {
  BytecodeNode first(Bytecode::kLdar, Register(0).ToOperand(),
                     OperandScale::kSingle);
  BytecodeNode second(Bytecode::kLdar, Register(0).ToOperand(),
                      OperandScale::kSingle);
  second.source_info().Update({0, true});
  optimizer()->Write(&first);
  CHECK_EQ(write_count(), 0);
  optimizer()->FlushForOffset();  // Prevent CanElideLast removing |first|.
  optimizer()->Write(&second);
  CHECK_EQ(write_count(), 1);
  CHECK_EQ(last_written(), first);
  optimizer()->FlushBasicBlock();
  CHECK_EQ(write_count(), 2);
  CHECK_EQ(last_written().bytecode(), Bytecode::kNop);
  CHECK_EQ(last_written().source_info(), second.source_info());
}

TEST_F(BytecodePeepholeOptimizerTest, LdarRxLdarRxStatementStarRy) {
  BytecodeNode first(Bytecode::kLdar, Register(0).ToOperand(),
                     OperandScale::kSingle);
  BytecodeNode second(Bytecode::kLdar, Register(0).ToOperand(),
                      OperandScale::kSingle);
  BytecodeNode third(Bytecode::kStar, Register(3).ToOperand(),
                     OperandScale::kSingle);
  second.source_info().Update({0, true});
  optimizer()->Write(&first);
  CHECK_EQ(write_count(), 0);
  optimizer()->FlushForOffset();  // Prevent CanElideLast removing |first|.
  optimizer()->Write(&second);
  CHECK_EQ(write_count(), 1);
  CHECK_EQ(last_written(), first);
  optimizer()->Write(&third);
  CHECK_EQ(write_count(), 1);
  optimizer()->FlushBasicBlock();
  CHECK_EQ(write_count(), 2);
  // Source position should move |second| to |third| when |second| is elided.
  third.source_info().Update(second.source_info());
  CHECK_EQ(last_written(), third);
}

TEST_F(BytecodePeepholeOptimizerTest, LdarToName) {
  BytecodeNode first(Bytecode::kLdar, Register(0).ToOperand(),
                     OperandScale::kSingle);
  BytecodeNode second(Bytecode::kToName);
  optimizer()->Write(&first);
  CHECK_EQ(write_count(), 0);
  optimizer()->Write(&second);
  CHECK_EQ(write_count(), 1);
  CHECK_EQ(last_written(), first);
  optimizer()->FlushBasicBlock();
  CHECK_EQ(write_count(), 2);
  CHECK_EQ(last_written(), second);
}

TEST_F(BytecodePeepholeOptimizerTest, ToNameToName) {
  BytecodeNode first(Bytecode::kToName);
  BytecodeNode second(Bytecode::kToName);
  optimizer()->Write(&first);
  CHECK_EQ(write_count(), 0);
  optimizer()->Write(&second);
  CHECK_EQ(write_count(), 1);
  CHECK_EQ(last_written(), first);
  optimizer()->FlushBasicBlock();
  CHECK_EQ(write_count(), 1);
}

TEST_F(BytecodePeepholeOptimizerTest, TypeOfToName) {
  BytecodeNode first(Bytecode::kTypeOf);
  BytecodeNode second(Bytecode::kToName);
  optimizer()->Write(&first);
  CHECK_EQ(write_count(), 0);
  optimizer()->Write(&second);
  CHECK_EQ(write_count(), 1);
  CHECK_EQ(last_written(), first);
  optimizer()->FlushBasicBlock();
  CHECK_EQ(write_count(), 1);
}

TEST_F(BytecodePeepholeOptimizerTest, LdaConstantStringToName) {
  Handle<Object> word =
      isolate()->factory()->NewStringFromStaticChars("optimizing");
  size_t index = constant_array()->Insert(word);
  BytecodeNode first(Bytecode::kLdaConstant, static_cast<uint32_t>(index),
                     OperandScale::kSingle);
  BytecodeNode second(Bytecode::kToName);
  optimizer()->Write(&first);
  CHECK_EQ(write_count(), 0);
  optimizer()->Write(&second);
  CHECK_EQ(write_count(), 1);
  CHECK_EQ(last_written(), first);
  optimizer()->FlushBasicBlock();
  CHECK_EQ(write_count(), 1);
}

TEST_F(BytecodePeepholeOptimizerTest, LdaConstantNumberToName) {
  Handle<Object> word = isolate()->factory()->NewNumber(0.380);
  size_t index = constant_array()->Insert(word);
  BytecodeNode first(Bytecode::kLdaConstant, static_cast<uint32_t>(index),
                     OperandScale::kSingle);
  BytecodeNode second(Bytecode::kToName);
  optimizer()->Write(&first);
  CHECK_EQ(write_count(), 0);
  optimizer()->Write(&second);
  CHECK_EQ(write_count(), 1);
  CHECK_EQ(last_written(), first);
  optimizer()->FlushBasicBlock();
  CHECK_EQ(write_count(), 2);
  CHECK_EQ(last_written(), second);
}

// Tests covering BytecodePeepholeOptimizer::CanElideLast().

TEST_F(BytecodePeepholeOptimizerTest, LdaTrueLdaFalseNotDiscardable) {
  BytecodeNode first(Bytecode::kLdaTrue);
  BytecodeNode second(Bytecode::kLdaFalse);
  optimizer()->Write(&first);
  optimizer()->FlushForOffset();  // Prevent discarding of |first|.
  CHECK_EQ(write_count(), 0);
  optimizer()->Write(&second);
  CHECK_EQ(write_count(), 1);
  CHECK_EQ(last_written(), first);
  optimizer()->FlushBasicBlock();
  CHECK_EQ(write_count(), 2);
  CHECK_EQ(last_written(), second);
}

TEST_F(BytecodePeepholeOptimizerTest, LdaTrueLdaFalse) {
  BytecodeNode first(Bytecode::kLdaTrue);
  BytecodeNode second(Bytecode::kLdaFalse);
  optimizer()->Write(&first);
  CHECK_EQ(write_count(), 0);
  optimizer()->Write(&second);
  CHECK_EQ(write_count(), 0);
  optimizer()->FlushBasicBlock();
  CHECK_EQ(write_count(), 1);
  CHECK_EQ(last_written(), second);
}

TEST_F(BytecodePeepholeOptimizerTest, LdaTrueStatementLdaFalse) {
  BytecodeNode first(Bytecode::kLdaTrue);
  first.source_info().Update({3, false});
  BytecodeNode second(Bytecode::kLdaFalse);
  optimizer()->Write(&first);
  CHECK_EQ(write_count(), 0);
  optimizer()->Write(&second);
  CHECK_EQ(write_count(), 0);
  optimizer()->FlushBasicBlock();
  CHECK_EQ(write_count(), 1);
  second.source_info().Update(first.source_info());
  CHECK_EQ(last_written(), second);
}

TEST_F(BytecodePeepholeOptimizerTest, NopStackCheck) {
  BytecodeNode first(Bytecode::kNop);
  BytecodeNode second(Bytecode::kStackCheck);
  optimizer()->Write(&first);
  CHECK_EQ(write_count(), 0);
  optimizer()->Write(&second);
  CHECK_EQ(write_count(), 0);
  optimizer()->FlushBasicBlock();
  CHECK_EQ(write_count(), 1);
  CHECK_EQ(last_written(), second);
}

TEST_F(BytecodePeepholeOptimizerTest, NopStatementStackCheck) {
  BytecodeNode first(Bytecode::kNop);
  first.source_info().Update({3, false});
  BytecodeNode second(Bytecode::kStackCheck);
  optimizer()->Write(&first);
  CHECK_EQ(write_count(), 0);
  optimizer()->Write(&second);
  CHECK_EQ(write_count(), 0);
  optimizer()->FlushBasicBlock();
  CHECK_EQ(write_count(), 1);
  second.source_info().Update(first.source_info());
  CHECK_EQ(last_written(), second);
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
