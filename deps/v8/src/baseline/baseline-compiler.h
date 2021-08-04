// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASELINE_BASELINE_COMPILER_H_
#define V8_BASELINE_BASELINE_COMPILER_H_

// TODO(v8:11421): Remove #if once baseline compiler is ported to other
// architectures.
#if V8_TARGET_ARCH_IA32 || V8_TARGET_ARCH_X64 || V8_TARGET_ARCH_ARM64 || \
    V8_TARGET_ARCH_ARM || V8_TARGET_ARCH_RISCV64

#include "src/base/logging.h"
#include "src/base/threaded-list.h"
#include "src/base/vlq.h"
#include "src/baseline/baseline-assembler.h"
#include "src/handles/handles.h"
#include "src/interpreter/bytecode-array-iterator.h"
#include "src/interpreter/bytecode-register.h"
#include "src/interpreter/interpreter-intrinsics.h"
#include "src/logging/counters.h"
#include "src/objects/map.h"
#include "src/objects/tagged-index.h"

namespace v8 {
namespace internal {

class BytecodeArray;

namespace baseline {

class BytecodeOffsetTableBuilder {
 public:
  void AddPosition(size_t pc_offset) {
    size_t pc_diff = pc_offset - previous_pc_;
    DCHECK_GE(pc_diff, 0);
    DCHECK_LE(pc_diff, std::numeric_limits<uint32_t>::max());
    base::VLQEncodeUnsigned(&bytes_, static_cast<uint32_t>(pc_diff));
    previous_pc_ = pc_offset;
  }

  template <typename IsolateT>
  Handle<ByteArray> ToBytecodeOffsetTable(IsolateT* isolate);

  void Reserve(size_t size) { bytes_.reserve(size); }

 private:
  size_t previous_pc_ = 0;
  std::vector<byte> bytes_;
};

class BaselineCompiler {
 public:
  explicit BaselineCompiler(Isolate* isolate,
                            Handle<SharedFunctionInfo> shared_function_info,
                            Handle<BytecodeArray> bytecode);

  void GenerateCode();
  MaybeHandle<Code> Build(Isolate* isolate);

 private:
  void Prologue();
  void PrologueFillFrame();
  void PrologueHandleOptimizationState(Register feedback_vector);

  void PreVisitSingleBytecode();
  void VisitSingleBytecode();

  void VerifyFrame();
  void VerifyFrameSize();

  // Register operands.
  interpreter::Register RegisterOperand(int operand_index);
  void LoadRegister(Register output, int operand_index);
  void StoreRegister(int operand_index, Register value);
  void StoreRegisterPair(int operand_index, Register val0, Register val1);

  // Constant pool operands.
  template <typename Type>
  Handle<Type> Constant(int operand_index);
  Smi ConstantSmi(int operand_index);
  template <typename Type>
  void LoadConstant(Register output, int operand_index);

  // Immediate value operands.
  uint32_t Uint(int operand_index);
  int32_t Int(int operand_index);
  uint32_t Index(int operand_index);
  uint32_t Flag(int operand_index);
  uint32_t RegisterCount(int operand_index);
  TaggedIndex IndexAsTagged(int operand_index);
  TaggedIndex UintAsTagged(int operand_index);
  Smi IndexAsSmi(int operand_index);
  Smi IntAsSmi(int operand_index);
  Smi FlagAsSmi(int operand_index);

  // Jump helpers.
  Label* NewLabel();
  Label* BuildForwardJumpLabel();
  void UpdateInterruptBudgetAndJumpToLabel(int weight, Label* label,
                                           Label* skip_interrupt_label);
  void UpdateInterruptBudgetAndDoInterpreterJump();
  void UpdateInterruptBudgetAndDoInterpreterJumpIfRoot(RootIndex root);
  void UpdateInterruptBudgetAndDoInterpreterJumpIfNotRoot(RootIndex root);

  // Feedback vector.
  MemOperand FeedbackVector();
  void LoadFeedbackVector(Register output);
  void LoadClosureFeedbackArray(Register output);

  // Position mapping.
  void AddPosition();

  // Misc. helpers.

  void UpdateMaxCallArgs(int max_call_args) {
    max_call_args_ = std::max(max_call_args_, max_call_args);
  }

  // Select the root boolean constant based on the jump in the given
  // `jump_func` -- the function should jump to the given label if we want to
  // select "true", otherwise it should fall through.
  void SelectBooleanConstant(
      Register output, std::function<void(Label*, Label::Distance)> jump_func);

  // Jumps based on calling ToBoolean on kInterpreterAccumulatorRegister.
  void JumpIfToBoolean(bool do_jump_if_true, Label* label,
                       Label::Distance distance = Label::kFar);

  // Call helpers.
  template <Builtins::Name kBuiltin, typename... Args>
  void CallBuiltin(Args... args);
  template <typename... Args>
  void CallRuntime(Runtime::FunctionId function, Args... args);

  template <Builtins::Name kBuiltin, typename... Args>
  void TailCallBuiltin(Args... args);

  template <ConvertReceiverMode kMode, typename... Args>
  void BuildCall(uint32_t slot, uint32_t arg_count, Args... args);

#ifdef V8_TRACE_UNOPTIMIZED
  void TraceBytecode(Runtime::FunctionId function_id);
#endif

  // Single bytecode visitors.
#define DECLARE_VISITOR(name, ...) void Visit##name();
  BYTECODE_LIST(DECLARE_VISITOR)
#undef DECLARE_VISITOR

  // Intrinsic call visitors.
#define DECLARE_VISITOR(name, ...) \
  void VisitIntrinsic##name(interpreter::RegisterList args);
  INTRINSICS_LIST(DECLARE_VISITOR)
#undef DECLARE_VISITOR

  const interpreter::BytecodeArrayIterator& iterator() { return iterator_; }

  Isolate* isolate_;
  RuntimeCallStats* stats_;
  Handle<SharedFunctionInfo> shared_function_info_;
  Handle<BytecodeArray> bytecode_;
  MacroAssembler masm_;
  BaselineAssembler basm_;
  interpreter::BytecodeArrayIterator iterator_;
  BytecodeOffsetTableBuilder bytecode_offset_table_builder_;
  Zone zone_;

  int max_call_args_ = 0;

  struct ThreadedLabel {
    Label label;
    ThreadedLabel* ptr;
    ThreadedLabel** next() { return &ptr; }
  };

  struct BaselineLabels {
    base::ThreadedList<ThreadedLabel> linked;
    Label unlinked;
  };

  BaselineLabels* EnsureLabels(int i) {
    if (labels_[i] == nullptr) {
      labels_[i] = zone_.New<BaselineLabels>();
    }
    return labels_[i];
  }

  BaselineLabels** labels_;
};

}  // namespace baseline
}  // namespace internal
}  // namespace v8

#endif

#endif  // V8_BASELINE_BASELINE_COMPILER_H_
