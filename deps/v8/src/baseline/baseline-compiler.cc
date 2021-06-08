// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(v8:11421): Remove #if once baseline compiler is ported to other
// architectures.
#if V8_TARGET_ARCH_IA32 || V8_TARGET_ARCH_X64 || V8_TARGET_ARCH_ARM64 || \
    V8_TARGET_ARCH_ARM

#include "src/baseline/baseline-compiler.h"

#include <algorithm>
#include <type_traits>

#include "src/baseline/baseline-assembler-inl.h"
#include "src/baseline/baseline-assembler.h"
#include "src/builtins/builtins-constructor.h"
#include "src/builtins/builtins-descriptors.h"
#include "src/builtins/builtins.h"
#include "src/codegen/assembler.h"
#include "src/codegen/compiler.h"
#include "src/codegen/interface-descriptors.h"
#include "src/codegen/machine-type.h"
#include "src/codegen/macro-assembler-inl.h"
#include "src/common/globals.h"
#include "src/execution/frame-constants.h"
#include "src/interpreter/bytecode-array-iterator.h"
#include "src/interpreter/bytecode-flags.h"
#include "src/objects/code.h"
#include "src/objects/heap-object.h"
#include "src/objects/instance-type.h"
#include "src/objects/shared-function-info-inl.h"
#include "src/roots/roots.h"

#if V8_TARGET_ARCH_X64
#include "src/baseline/x64/baseline-compiler-x64-inl.h"
#elif V8_TARGET_ARCH_ARM64
#include "src/baseline/arm64/baseline-compiler-arm64-inl.h"
#elif V8_TARGET_ARCH_IA32
#include "src/baseline/ia32/baseline-compiler-ia32-inl.h"
#elif V8_TARGET_ARCH_ARM
#include "src/baseline/arm/baseline-compiler-arm-inl.h"
#else
#error Unsupported target architecture.
#endif

namespace v8 {
namespace internal {
namespace baseline {

template <typename LocalIsolate>
Handle<ByteArray> BytecodeOffsetTableBuilder::ToBytecodeOffsetTable(
    LocalIsolate* isolate) {
  if (bytes_.empty()) return isolate->factory()->empty_byte_array();
  Handle<ByteArray> table = isolate->factory()->NewByteArray(
      static_cast<int>(bytes_.size()), AllocationType::kOld);
  MemCopy(table->GetDataStartAddress(), bytes_.data(), bytes_.size());
  return table;
}

namespace detail {

#ifdef DEBUG
bool Clobbers(Register target, Register reg) { return target == reg; }
bool Clobbers(Register target, Handle<Object> handle) { return false; }
bool Clobbers(Register target, Smi smi) { return false; }
bool Clobbers(Register target, TaggedIndex index) { return false; }
bool Clobbers(Register target, int32_t imm) { return false; }
bool Clobbers(Register target, RootIndex index) { return false; }
bool Clobbers(Register target, interpreter::Register reg) { return false; }

// We don't know what's inside machine registers or operands, so assume they
// match.
bool MachineTypeMatches(MachineType type, Register reg) { return true; }
bool MachineTypeMatches(MachineType type, MemOperand reg) { return true; }
bool MachineTypeMatches(MachineType type, Handle<HeapObject> handle) {
  return type.IsTagged() && !type.IsTaggedSigned();
}
bool MachineTypeMatches(MachineType type, Smi handle) {
  return type.IsTagged() && !type.IsTaggedPointer();
}
bool MachineTypeMatches(MachineType type, TaggedIndex handle) {
  // TaggedIndex doesn't have a separate type, so check for the same type as for
  // Smis.
  return type.IsTagged() && !type.IsTaggedPointer();
}
bool MachineTypeMatches(MachineType type, int32_t imm) {
  // 32-bit immediates can be used for 64-bit params -- they'll be
  // zero-extended.
  return type.representation() == MachineRepresentation::kWord32 ||
         type.representation() == MachineRepresentation::kWord64;
}
bool MachineTypeMatches(MachineType type, RootIndex index) {
  return type.IsTagged() && !type.IsTaggedSigned();
}
bool MachineTypeMatches(MachineType type, interpreter::Register reg) {
  return type.IsTagged();
}

template <typename... Args>
struct CheckArgsHelper;

template <>
struct CheckArgsHelper<> {
  static void Check(BaselineAssembler* masm, CallInterfaceDescriptor descriptor,
                    int i) {
    if (descriptor.AllowVarArgs()) {
      CHECK_GE(i, descriptor.GetParameterCount());
    } else {
      CHECK_EQ(i, descriptor.GetParameterCount());
    }
  }
};

template <typename Arg, typename... Args>
struct CheckArgsHelper<Arg, Args...> {
  static void Check(BaselineAssembler* masm, CallInterfaceDescriptor descriptor,
                    int i, Arg arg, Args... args) {
    if (i >= descriptor.GetParameterCount()) {
      CHECK(descriptor.AllowVarArgs());
      return;
    }
    CHECK(MachineTypeMatches(descriptor.GetParameterType(i), arg));
    CheckArgsHelper<Args...>::Check(masm, descriptor, i + 1, args...);
  }
};

template <typename... Args>
struct CheckArgsHelper<interpreter::RegisterList, Args...> {
  static void Check(BaselineAssembler* masm, CallInterfaceDescriptor descriptor,
                    int i, interpreter::RegisterList list, Args... args) {
    for (int reg_index = 0; reg_index < list.register_count();
         ++reg_index, ++i) {
      if (i >= descriptor.GetParameterCount()) {
        CHECK(descriptor.AllowVarArgs());
        return;
      }
      CHECK(
          MachineTypeMatches(descriptor.GetParameterType(i), list[reg_index]));
    }
    CheckArgsHelper<Args...>::Check(masm, descriptor, i, args...);
  }
};

template <typename... Args>
void CheckArgs(BaselineAssembler* masm, CallInterfaceDescriptor descriptor,
               Args... args) {
  CheckArgsHelper<Args...>::Check(masm, descriptor, 0, args...);
}

#else  // DEBUG

template <typename... Args>
void CheckArgs(Args... args) {}

#endif  // DEBUG

template <typename... Args>
struct ArgumentSettingHelper;

template <>
struct ArgumentSettingHelper<> {
  static void Set(BaselineAssembler* masm, CallInterfaceDescriptor descriptor,
                  int i) {}
  static void CheckSettingDoesntClobber(Register target, int arg_index) {}
};

template <typename Arg, typename... Args>
struct ArgumentSettingHelper<Arg, Args...> {
  static void Set(BaselineAssembler* masm, CallInterfaceDescriptor descriptor,
                  int i, Arg arg, Args... args) {
    if (i < descriptor.GetRegisterParameterCount()) {
      Register target = descriptor.GetRegisterParameter(i);
      ArgumentSettingHelper<Args...>::CheckSettingDoesntClobber(target, i + 1,
                                                                args...);
      masm->Move(target, arg);
      ArgumentSettingHelper<Args...>::Set(masm, descriptor, i + 1, args...);
    } else if (descriptor.GetStackArgumentOrder() ==
               StackArgumentOrder::kDefault) {
      masm->Push(arg, args...);
    } else {
      masm->PushReverse(arg, args...);
    }
  }
  static void CheckSettingDoesntClobber(Register target, int arg_index, Arg arg,
                                        Args... args) {
    DCHECK(!Clobbers(target, arg));
    ArgumentSettingHelper<Args...>::CheckSettingDoesntClobber(
        target, arg_index + 1, args...);
  }
};

// Specialization for interpreter::RegisterList which iterates it.
// RegisterLists are only allowed to be the last argument.
template <>
struct ArgumentSettingHelper<interpreter::RegisterList> {
  static void Set(BaselineAssembler* masm, CallInterfaceDescriptor descriptor,
                  int i, interpreter::RegisterList list) {
    // Either all the values are in machine registers, or they're all on the
    // stack.
    if (i < descriptor.GetRegisterParameterCount()) {
      for (int reg_index = 0; reg_index < list.register_count();
           ++reg_index, ++i) {
        Register target = descriptor.GetRegisterParameter(i);
        masm->Move(target, masm->RegisterFrameOperand(list[reg_index]));
      }
    } else if (descriptor.GetStackArgumentOrder() ==
               StackArgumentOrder::kDefault) {
      masm->Push(list);
    } else {
      masm->PushReverse(list);
    }
  }
  static void CheckSettingDoesntClobber(Register target, int arg_index,
                                        interpreter::RegisterList arg) {}
};

template <typename... Args>
void MoveArgumentsForDescriptor(BaselineAssembler* masm,
                                CallInterfaceDescriptor descriptor,
                                Args... args) {
  CheckArgs(masm, descriptor, args...);
  ArgumentSettingHelper<Args...>::Set(masm, descriptor, 0, args...);
}

}  // namespace detail

BaselineCompiler::BaselineCompiler(
    Isolate* isolate, Handle<SharedFunctionInfo> shared_function_info,
    Handle<BytecodeArray> bytecode)
    : isolate_(isolate),
      stats_(isolate->counters()->runtime_call_stats()),
      shared_function_info_(shared_function_info),
      bytecode_(bytecode),
      masm_(isolate, CodeObjectRequired::kNo),
      basm_(&masm_),
      iterator_(bytecode_),
      zone_(isolate->allocator(), ZONE_NAME),
      labels_(zone_.NewArray<BaselineLabels*>(bytecode_->length())) {
  MemsetPointer(labels_, nullptr, bytecode_->length());
}

#define __ basm_.

void BaselineCompiler::GenerateCode() {
  {
    RuntimeCallTimerScope runtimeTimer(
        stats_, RuntimeCallCounterId::kCompileBaselinePreVisit);
    for (; !iterator_.done(); iterator_.Advance()) {
      PreVisitSingleBytecode();
    }
    iterator_.Reset();
  }

  // No code generated yet.
  DCHECK_EQ(__ pc_offset(), 0);
  __ CodeEntry();

  {
    RuntimeCallTimerScope runtimeTimer(
        stats_, RuntimeCallCounterId::kCompileBaselineVisit);
    Prologue();
    AddPosition();
    for (; !iterator_.done(); iterator_.Advance()) {
      VisitSingleBytecode();
      AddPosition();
    }
  }
}

MaybeHandle<Code> BaselineCompiler::Build(Isolate* isolate) {
  CodeDesc desc;
  __ GetCode(isolate, &desc);
  // Allocate the bytecode offset table.
  Handle<ByteArray> bytecode_offset_table =
      bytecode_offset_table_builder_.ToBytecodeOffsetTable(isolate);
  return Factory::CodeBuilder(isolate, desc, CodeKind::BASELINE)
      .set_bytecode_offset_table(bytecode_offset_table)
      .TryBuild();
}

interpreter::Register BaselineCompiler::RegisterOperand(int operand_index) {
  return iterator().GetRegisterOperand(operand_index);
}

void BaselineCompiler::LoadRegister(Register output, int operand_index) {
  __ LoadRegister(output, RegisterOperand(operand_index));
}

void BaselineCompiler::StoreRegister(int operand_index, Register value) {
  __ Move(RegisterOperand(operand_index), value);
}

void BaselineCompiler::StoreRegisterPair(int operand_index, Register val0,
                                         Register val1) {
  interpreter::Register reg0, reg1;
  std::tie(reg0, reg1) = iterator().GetRegisterPairOperand(operand_index);
  __ StoreRegister(reg0, val0);
  __ StoreRegister(reg1, val1);
}
template <typename Type>
Handle<Type> BaselineCompiler::Constant(int operand_index) {
  return Handle<Type>::cast(
      iterator().GetConstantForIndexOperand(operand_index, isolate_));
}
Smi BaselineCompiler::ConstantSmi(int operand_index) {
  return iterator().GetConstantAtIndexAsSmi(operand_index);
}
template <typename Type>
void BaselineCompiler::LoadConstant(Register output, int operand_index) {
  __ Move(output, Constant<Type>(operand_index));
}
uint32_t BaselineCompiler::Uint(int operand_index) {
  return iterator().GetUnsignedImmediateOperand(operand_index);
}
int32_t BaselineCompiler::Int(int operand_index) {
  return iterator().GetImmediateOperand(operand_index);
}
uint32_t BaselineCompiler::Index(int operand_index) {
  return iterator().GetIndexOperand(operand_index);
}
uint32_t BaselineCompiler::Flag(int operand_index) {
  return iterator().GetFlagOperand(operand_index);
}
uint32_t BaselineCompiler::RegisterCount(int operand_index) {
  return iterator().GetRegisterCountOperand(operand_index);
}
TaggedIndex BaselineCompiler::IndexAsTagged(int operand_index) {
  return TaggedIndex::FromIntptr(Index(operand_index));
}
TaggedIndex BaselineCompiler::UintAsTagged(int operand_index) {
  return TaggedIndex::FromIntptr(Uint(operand_index));
}
Smi BaselineCompiler::IndexAsSmi(int operand_index) {
  return Smi::FromInt(Index(operand_index));
}
Smi BaselineCompiler::IntAsSmi(int operand_index) {
  return Smi::FromInt(Int(operand_index));
}
Smi BaselineCompiler::FlagAsSmi(int operand_index) {
  return Smi::FromInt(Flag(operand_index));
}

MemOperand BaselineCompiler::FeedbackVector() {
  return __ FeedbackVectorOperand();
}

void BaselineCompiler::LoadFeedbackVector(Register output) {
  __ RecordComment("[ LoadFeedbackVector");
  __ Move(output, __ FeedbackVectorOperand());
  __ RecordComment("]");
}

void BaselineCompiler::LoadClosureFeedbackArray(Register output) {
  LoadFeedbackVector(output);
  __ LoadTaggedPointerField(output, output,
                            FeedbackVector::kClosureFeedbackCellArrayOffset);
}

void BaselineCompiler::SelectBooleanConstant(
    Register output, std::function<void(Label*, Label::Distance)> jump_func) {
  Label done, set_true;
  jump_func(&set_true, Label::kNear);
  __ LoadRoot(output, RootIndex::kFalseValue);
  __ Jump(&done, Label::kNear);
  __ Bind(&set_true);
  __ LoadRoot(output, RootIndex::kTrueValue);
  __ Bind(&done);
}

void BaselineCompiler::AddPosition() {
  bytecode_offset_table_builder_.AddPosition(__ pc_offset());
}

void BaselineCompiler::PreVisitSingleBytecode() {
  switch (iterator().current_bytecode()) {
    case interpreter::Bytecode::kJumpLoop:
      EnsureLabels(iterator().GetJumpTargetOffset());
      break;

    // TODO(leszeks): Update the max_call_args as part of the main bytecode
    // visit loop, by patching the value passed to the prologue.
    case interpreter::Bytecode::kCallProperty:
    case interpreter::Bytecode::kCallAnyReceiver:
    case interpreter::Bytecode::kCallWithSpread:
    case interpreter::Bytecode::kCallNoFeedback:
    case interpreter::Bytecode::kConstruct:
    case interpreter::Bytecode::kConstructWithSpread:
      return UpdateMaxCallArgs(
          iterator().GetRegisterListOperand(1).register_count());
    case interpreter::Bytecode::kCallUndefinedReceiver:
      return UpdateMaxCallArgs(
          iterator().GetRegisterListOperand(1).register_count() + 1);
    case interpreter::Bytecode::kCallProperty0:
    case interpreter::Bytecode::kCallUndefinedReceiver0:
      return UpdateMaxCallArgs(1);
    case interpreter::Bytecode::kCallProperty1:
    case interpreter::Bytecode::kCallUndefinedReceiver1:
      return UpdateMaxCallArgs(2);
    case interpreter::Bytecode::kCallProperty2:
    case interpreter::Bytecode::kCallUndefinedReceiver2:
      return UpdateMaxCallArgs(3);

    default:
      break;
  }
}

void BaselineCompiler::VisitSingleBytecode() {
  int offset = iterator().current_offset();
  if (labels_[offset]) {
    // Bind labels for this offset that have already been linked to a
    // jump (i.e. forward jumps, excluding jump tables).
    for (auto&& label : labels_[offset]->linked) {
      __ BindWithoutJumpTarget(&label->label);
    }
#ifdef DEBUG
    labels_[offset]->linked.Clear();
#endif
    __ BindWithoutJumpTarget(&labels_[offset]->unlinked);
  }

  // Mark position as valid jump target. This is required for the deoptimizer
  // and exception handling, when CFI is enabled.
  __ JumpTarget();

  if (FLAG_code_comments) {
    std::ostringstream str;
    str << "[ ";
    iterator().PrintTo(str);
    __ RecordComment(str.str().c_str());
  }

  VerifyFrame();

#ifdef V8_TRACE_UNOPTIMIZED
  TraceBytecode(Runtime::kTraceUnoptimizedBytecodeEntry);
#endif

  switch (iterator().current_bytecode()) {
#define BYTECODE_CASE(name, ...)       \
  case interpreter::Bytecode::k##name: \
    Visit##name();                     \
    break;
    BYTECODE_LIST(BYTECODE_CASE)
#undef BYTECODE_CASE
  }
  __ RecordComment("]");

#ifdef V8_TRACE_UNOPTIMIZED
  TraceBytecode(Runtime::kTraceUnoptimizedBytecodeExit);
#endif
}

void BaselineCompiler::VerifyFrame() {
  if (__ emit_debug_code()) {
    __ RecordComment("[ Verify frame");
    __ RecordComment(" -- Verify frame size");
    VerifyFrameSize();

    __ RecordComment(" -- Verify feedback vector");
    {
      BaselineAssembler::ScratchRegisterScope temps(&basm_);
      Register scratch = temps.AcquireScratch();
      __ Move(scratch, __ FeedbackVectorOperand());
      Label is_smi, is_ok;
      __ JumpIfSmi(scratch, &is_smi);
      __ CmpObjectType(scratch, FEEDBACK_VECTOR_TYPE, scratch);
      __ JumpIf(Condition::kEqual, &is_ok);
      __ Bind(&is_smi);
      __ masm()->Abort(AbortReason::kExpectedFeedbackVector);
      __ Bind(&is_ok);
    }

    // TODO(leszeks): More verification.

    __ RecordComment("]");
  }
}

#ifdef V8_TRACE_UNOPTIMIZED
void BaselineCompiler::TraceBytecode(Runtime::FunctionId function_id) {
  if (!FLAG_trace_baseline_exec) return;

  __ RecordComment(function_id == Runtime::kTraceUnoptimizedBytecodeEntry
                       ? "[ Trace bytecode entry"
                       : "[ Trace bytecode exit");
  SaveAccumulatorScope accumulator_scope(&basm_);
  CallRuntime(function_id, bytecode_,
              Smi::FromInt(BytecodeArray::kHeaderSize - kHeapObjectTag +
                           iterator().current_offset()),
              kInterpreterAccumulatorRegister);
  __ RecordComment("]");
}
#endif

#define DECLARE_VISITOR(name, ...) void Visit##name();
BYTECODE_LIST(DECLARE_VISITOR)
#undef DECLARE_VISITOR

#define DECLARE_VISITOR(name, ...) \
  void VisitIntrinsic##name(interpreter::RegisterList args);
INTRINSICS_LIST(DECLARE_VISITOR)
#undef DECLARE_VISITOR

void BaselineCompiler::UpdateInterruptBudgetAndJumpToLabel(
    int weight, Label* label, Label* skip_interrupt_label) {
  if (weight != 0) {
    __ RecordComment("[ Update Interrupt Budget");
    __ AddToInterruptBudget(weight);

    if (weight < 0) {
      // Use compare flags set by AddToInterruptBudget
      __ JumpIf(Condition::kGreaterThanEqual, skip_interrupt_label);
      SaveAccumulatorScope accumulator_scope(&basm_);
      CallRuntime(Runtime::kBytecodeBudgetInterruptFromBytecode,
                  __ FunctionOperand());
    }
  }
  if (label) __ Jump(label);
  if (weight != 0) __ RecordComment("]");
}

void BaselineCompiler::UpdateInterruptBudgetAndDoInterpreterJump() {
  int weight = iterator().GetRelativeJumpTargetOffset() -
               iterator().current_bytecode_size_without_prefix();
  UpdateInterruptBudgetAndJumpToLabel(weight, BuildForwardJumpLabel(), nullptr);
}

void BaselineCompiler::UpdateInterruptBudgetAndDoInterpreterJumpIfRoot(
    RootIndex root) {
  Label dont_jump;
  __ JumpIfNotRoot(kInterpreterAccumulatorRegister, root, &dont_jump,
                   Label::kNear);
  UpdateInterruptBudgetAndDoInterpreterJump();
  __ Bind(&dont_jump);
}

void BaselineCompiler::UpdateInterruptBudgetAndDoInterpreterJumpIfNotRoot(
    RootIndex root) {
  Label dont_jump;
  __ JumpIfRoot(kInterpreterAccumulatorRegister, root, &dont_jump,
                Label::kNear);
  UpdateInterruptBudgetAndDoInterpreterJump();
  __ Bind(&dont_jump);
}

Label* BaselineCompiler::BuildForwardJumpLabel() {
  int target_offset = iterator().GetJumpTargetOffset();
  ThreadedLabel* threaded_label = zone_.New<ThreadedLabel>();
  EnsureLabels(target_offset)->linked.Add(threaded_label);
  return &threaded_label->label;
}

template <typename... Args>
void BaselineCompiler::CallBuiltin(Builtins::Name builtin, Args... args) {
  __ RecordComment("[ CallBuiltin");
  CallInterfaceDescriptor descriptor =
      Builtins::CallInterfaceDescriptorFor(builtin);
  detail::MoveArgumentsForDescriptor(&basm_, descriptor, args...);
  if (descriptor.HasContextParameter()) {
    __ LoadContext(descriptor.ContextRegister());
  }
  __ CallBuiltin(builtin);
  __ RecordComment("]");
}

template <typename... Args>
void BaselineCompiler::TailCallBuiltin(Builtins::Name builtin, Args... args) {
  CallInterfaceDescriptor descriptor =
      Builtins::CallInterfaceDescriptorFor(builtin);
  detail::MoveArgumentsForDescriptor(&basm_, descriptor, args...);
  if (descriptor.HasContextParameter()) {
    __ LoadContext(descriptor.ContextRegister());
  }
  __ TailCallBuiltin(builtin);
}

template <typename... Args>
void BaselineCompiler::CallRuntime(Runtime::FunctionId function, Args... args) {
  __ LoadContext(kContextRegister);
  int nargs = __ Push(args...);
  __ CallRuntime(function, nargs);
}

// Returns into kInterpreterAccumulatorRegister
void BaselineCompiler::JumpIfToBoolean(bool do_jump_if_true, Register reg,
                                       Label* label, Label::Distance distance) {
  Label end;
  Label::Distance end_distance = Label::kNear;

  Label* true_label = do_jump_if_true ? label : &end;
  Label::Distance true_distance = do_jump_if_true ? distance : end_distance;
  Label* false_label = do_jump_if_true ? &end : label;
  Label::Distance false_distance = do_jump_if_true ? end_distance : distance;

  BaselineAssembler::ScratchRegisterScope scratch_scope(&basm_);
  Register to_boolean = scratch_scope.AcquireScratch();
  {
    SaveAccumulatorScope accumulator_scope(&basm_);
    CallBuiltin(Builtins::kToBoolean, reg);
    __ Move(to_boolean, kInterpreterAccumulatorRegister);
  }
  __ JumpIfRoot(to_boolean, RootIndex::kTrueValue, true_label, true_distance);
  if (false_label != &end) __ Jump(false_label, false_distance);

  __ Bind(&end);
}

void BaselineCompiler::VisitLdaZero() {
  __ Move(kInterpreterAccumulatorRegister, Smi::FromInt(0));
}

void BaselineCompiler::VisitLdaSmi() {
  Smi constant = Smi::FromInt(iterator().GetImmediateOperand(0));
  __ Move(kInterpreterAccumulatorRegister, constant);
}

void BaselineCompiler::VisitLdaUndefined() {
  __ LoadRoot(kInterpreterAccumulatorRegister, RootIndex::kUndefinedValue);
}

void BaselineCompiler::VisitLdaNull() {
  __ LoadRoot(kInterpreterAccumulatorRegister, RootIndex::kNullValue);
}

void BaselineCompiler::VisitLdaTheHole() {
  __ LoadRoot(kInterpreterAccumulatorRegister, RootIndex::kTheHoleValue);
}

void BaselineCompiler::VisitLdaTrue() {
  __ LoadRoot(kInterpreterAccumulatorRegister, RootIndex::kTrueValue);
}

void BaselineCompiler::VisitLdaFalse() {
  __ LoadRoot(kInterpreterAccumulatorRegister, RootIndex::kFalseValue);
}

void BaselineCompiler::VisitLdaConstant() {
  LoadConstant<HeapObject>(kInterpreterAccumulatorRegister, 0);
}

void BaselineCompiler::VisitLdaGlobal() {
  CallBuiltin(Builtins::kLoadGlobalICBaseline,
              Constant<Name>(0),  // name
              IndexAsTagged(1));  // slot
}

void BaselineCompiler::VisitLdaGlobalInsideTypeof() {
  CallBuiltin(Builtins::kLoadGlobalICInsideTypeofBaseline,
              Constant<Name>(0),  // name
              IndexAsTagged(1));  // slot
}

void BaselineCompiler::VisitStaGlobal() {
  CallBuiltin(Builtins::kStoreGlobalICBaseline,
              Constant<Name>(0),                // name
              kInterpreterAccumulatorRegister,  // value
              IndexAsTagged(1));                // slot
}

void BaselineCompiler::VisitPushContext() {
  BaselineAssembler::ScratchRegisterScope scratch_scope(&basm_);
  Register context = scratch_scope.AcquireScratch();
  __ LoadContext(context);
  __ StoreContext(kInterpreterAccumulatorRegister);
  StoreRegister(0, context);
}

void BaselineCompiler::VisitPopContext() {
  BaselineAssembler::ScratchRegisterScope scratch_scope(&basm_);
  Register context = scratch_scope.AcquireScratch();
  LoadRegister(context, 0);
  __ StoreContext(context);
}

void BaselineCompiler::VisitLdaContextSlot() {
  BaselineAssembler::ScratchRegisterScope scratch_scope(&basm_);
  Register context = scratch_scope.AcquireScratch();
  LoadRegister(context, 0);
  int depth = Uint(2);
  for (; depth > 0; --depth) {
    __ LoadTaggedPointerField(context, context, Context::kPreviousOffset);
  }
  __ LoadTaggedAnyField(kInterpreterAccumulatorRegister, context,
                        Context::OffsetOfElementAt(Index(1)));
}

void BaselineCompiler::VisitLdaImmutableContextSlot() { VisitLdaContextSlot(); }

void BaselineCompiler::VisitLdaCurrentContextSlot() {
  BaselineAssembler::ScratchRegisterScope scratch_scope(&basm_);
  Register context = scratch_scope.AcquireScratch();
  __ LoadContext(context);
  __ LoadTaggedAnyField(kInterpreterAccumulatorRegister, context,
                        Context::OffsetOfElementAt(Index(0)));
}

void BaselineCompiler::VisitLdaImmutableCurrentContextSlot() {
  VisitLdaCurrentContextSlot();
}

void BaselineCompiler::VisitStaContextSlot() {
  BaselineAssembler::ScratchRegisterScope scratch_scope(&basm_);
  Register context = scratch_scope.AcquireScratch();
  LoadRegister(context, 0);
  int depth = Uint(2);
  for (; depth > 0; --depth) {
    __ LoadTaggedPointerField(context, context, Context::kPreviousOffset);
  }
  Register value = scratch_scope.AcquireScratch();
  __ Move(value, kInterpreterAccumulatorRegister);
  __ StoreTaggedFieldWithWriteBarrier(
      context, Context::OffsetOfElementAt(iterator().GetIndexOperand(1)),
      value);
}

void BaselineCompiler::VisitStaCurrentContextSlot() {
  BaselineAssembler::ScratchRegisterScope scratch_scope(&basm_);
  Register context = scratch_scope.AcquireScratch();
  __ LoadContext(context);
  Register value = scratch_scope.AcquireScratch();
  __ Move(value, kInterpreterAccumulatorRegister);
  __ StoreTaggedFieldWithWriteBarrier(
      context, Context::OffsetOfElementAt(Index(0)), value);
}

void BaselineCompiler::VisitLdaLookupSlot() {
  CallRuntime(Runtime::kLoadLookupSlot, Constant<Name>(0));
}

void BaselineCompiler::VisitLdaLookupContextSlot() {
  CallBuiltin(Builtins::kLookupContextBaseline, Constant<Name>(0),
              UintAsTagged(2), IndexAsTagged(1));
}

void BaselineCompiler::VisitLdaLookupGlobalSlot() {
  CallBuiltin(Builtins::kLookupGlobalICBaseline, Constant<Name>(0),
              UintAsTagged(2), IndexAsTagged(1));
}

void BaselineCompiler::VisitLdaLookupSlotInsideTypeof() {
  CallRuntime(Runtime::kLoadLookupSlotInsideTypeof, Constant<Name>(0));
}

void BaselineCompiler::VisitLdaLookupContextSlotInsideTypeof() {
  CallBuiltin(Builtins::kLookupContextInsideTypeofBaseline, Constant<Name>(0),
              UintAsTagged(2), IndexAsTagged(1));
}

void BaselineCompiler::VisitLdaLookupGlobalSlotInsideTypeof() {
  CallBuiltin(Builtins::kLookupGlobalICInsideTypeofBaseline, Constant<Name>(0),
              UintAsTagged(2), IndexAsTagged(1));
}

void BaselineCompiler::VisitStaLookupSlot() {
  uint32_t flags = Flag(1);
  Runtime::FunctionId function_id;
  if (flags & interpreter::StoreLookupSlotFlags::LanguageModeBit::kMask) {
    function_id = Runtime::kStoreLookupSlot_Strict;
  } else if (flags &
             interpreter::StoreLookupSlotFlags::LookupHoistingModeBit::kMask) {
    function_id = Runtime::kStoreLookupSlot_SloppyHoisting;
  } else {
    function_id = Runtime::kStoreLookupSlot_Sloppy;
  }
  CallRuntime(function_id, Constant<Name>(0),    // name
              kInterpreterAccumulatorRegister);  // value
}

void BaselineCompiler::VisitLdar() {
  LoadRegister(kInterpreterAccumulatorRegister, 0);
}

void BaselineCompiler::VisitStar() {
  StoreRegister(0, kInterpreterAccumulatorRegister);
}

#define SHORT_STAR_VISITOR(Name, ...)                                         \
  void BaselineCompiler::Visit##Name() {                                      \
    __ StoreRegister(                                                         \
        interpreter::Register::FromShortStar(interpreter::Bytecode::k##Name), \
        kInterpreterAccumulatorRegister);                                     \
  }
SHORT_STAR_BYTECODE_LIST(SHORT_STAR_VISITOR)
#undef SHORT_STAR_VISITOR

void BaselineCompiler::VisitMov() {
  BaselineAssembler::ScratchRegisterScope scratch_scope(&basm_);
  Register scratch = scratch_scope.AcquireScratch();
  LoadRegister(scratch, 0);
  StoreRegister(1, scratch);
}

void BaselineCompiler::VisitLdaNamedProperty() {
  CallBuiltin(Builtins::kLoadICBaseline,
              RegisterOperand(0),  // object
              Constant<Name>(1),   // name
              IndexAsTagged(2));   // slot
}

void BaselineCompiler::VisitLdaNamedPropertyNoFeedback() {
  CallBuiltin(Builtins::kGetProperty, RegisterOperand(0), Constant<Name>(1));
}

void BaselineCompiler::VisitLdaNamedPropertyFromSuper() {
  __ LoadPrototype(
      LoadWithReceiverAndVectorDescriptor::LookupStartObjectRegister(),
      kInterpreterAccumulatorRegister);

  CallBuiltin(Builtins::kLoadSuperICBaseline,
              RegisterOperand(0),  // object
              LoadWithReceiverAndVectorDescriptor::
                  LookupStartObjectRegister(),  // lookup start
              Constant<Name>(1),                // name
              IndexAsTagged(2));                // slot
}

void BaselineCompiler::VisitLdaKeyedProperty() {
  CallBuiltin(Builtins::kKeyedLoadICBaseline,
              RegisterOperand(0),               // object
              kInterpreterAccumulatorRegister,  // key
              IndexAsTagged(1));                // slot
}

void BaselineCompiler::VisitLdaModuleVariable() {
  BaselineAssembler::ScratchRegisterScope scratch_scope(&basm_);
  Register scratch = scratch_scope.AcquireScratch();
  __ LoadContext(scratch);
  int depth = Uint(1);
  for (; depth > 0; --depth) {
    __ LoadTaggedPointerField(scratch, scratch, Context::kPreviousOffset);
  }
  __ LoadTaggedPointerField(scratch, scratch, Context::kExtensionOffset);
  int cell_index = Int(0);
  if (cell_index > 0) {
    __ LoadTaggedPointerField(scratch, scratch,
                              SourceTextModule::kRegularExportsOffset);
    // The actual array index is (cell_index - 1).
    cell_index -= 1;
  } else {
    __ LoadTaggedPointerField(scratch, scratch,
                              SourceTextModule::kRegularImportsOffset);
    // The actual array index is (-cell_index - 1).
    cell_index = -cell_index - 1;
  }
  __ LoadFixedArrayElement(scratch, scratch, cell_index);
  __ LoadTaggedAnyField(kInterpreterAccumulatorRegister, scratch,
                        Cell::kValueOffset);
}

void BaselineCompiler::VisitStaModuleVariable() {
  BaselineAssembler::ScratchRegisterScope scratch_scope(&basm_);
  Register scratch = scratch_scope.AcquireScratch();
  __ LoadContext(scratch);
  int depth = Uint(1);
  for (; depth > 0; --depth) {
    __ LoadTaggedPointerField(scratch, scratch, Context::kPreviousOffset);
  }
  __ LoadTaggedPointerField(scratch, scratch, Context::kExtensionOffset);
  int cell_index = Int(0);
  if (cell_index > 0) {
    __ LoadTaggedPointerField(scratch, scratch,
                              SourceTextModule::kRegularExportsOffset);
    // The actual array index is (cell_index - 1).
    cell_index -= 1;
    __ LoadFixedArrayElement(scratch, scratch, cell_index);
    SaveAccumulatorScope save_accumulator(&basm_);
    __ StoreTaggedFieldWithWriteBarrier(scratch, Cell::kValueOffset,
                                        kInterpreterAccumulatorRegister);
  } else {
    // Not supported (probably never).
    CallRuntime(Runtime::kAbort,
                Smi::FromInt(static_cast<int>(
                    AbortReason::kUnsupportedModuleOperation)));
    __ Trap();
  }
}

void BaselineCompiler::VisitStaNamedProperty() {
  CallBuiltin(Builtins::kStoreICBaseline,
              RegisterOperand(0),               // object
              Constant<Name>(1),                // name
              kInterpreterAccumulatorRegister,  // value
              IndexAsTagged(2));                // slot
}

void BaselineCompiler::VisitStaNamedPropertyNoFeedback() {
  CallRuntime(Runtime::kSetNamedProperty,
              RegisterOperand(0),                // object
              Constant<Name>(1),                 // name
              kInterpreterAccumulatorRegister);  // value
}

void BaselineCompiler::VisitStaNamedOwnProperty() {
  // TODO(v8:11429,ishell): Currently we use StoreOwnIC only for storing
  // properties that already exist in the boilerplate therefore we can use
  // StoreIC.
  VisitStaNamedProperty();
}

void BaselineCompiler::VisitStaKeyedProperty() {
  CallBuiltin(Builtins::kKeyedStoreICBaseline,
              RegisterOperand(0),               // object
              RegisterOperand(1),               // key
              kInterpreterAccumulatorRegister,  // value
              IndexAsTagged(2));                // slot
}

void BaselineCompiler::VisitStaInArrayLiteral() {
  CallBuiltin(Builtins::kStoreInArrayLiteralICBaseline,
              RegisterOperand(0),               // object
              RegisterOperand(1),               // name
              kInterpreterAccumulatorRegister,  // value
              IndexAsTagged(2));                // slot
}

void BaselineCompiler::VisitStaDataPropertyInLiteral() {
  CallRuntime(Runtime::kDefineDataPropertyInLiteral,
              RegisterOperand(0),               // object
              RegisterOperand(1),               // name
              kInterpreterAccumulatorRegister,  // value
              FlagAsSmi(2),                     // flags
              FeedbackVector(),                 // feedback vector
              IndexAsTagged(3));                // slot
}

void BaselineCompiler::VisitCollectTypeProfile() {
  SaveAccumulatorScope accumulator_scope(&basm_);
  CallRuntime(Runtime::kCollectTypeProfile,
              IntAsSmi(0),                      // position
              kInterpreterAccumulatorRegister,  // value
              FeedbackVector());                // feedback vector
}

void BaselineCompiler::VisitAdd() {
  CallBuiltin(Builtins::kAdd_Baseline, RegisterOperand(0),
              kInterpreterAccumulatorRegister, Index(1));
}

void BaselineCompiler::VisitSub() {
  CallBuiltin(Builtins::kSubtract_Baseline, RegisterOperand(0),
              kInterpreterAccumulatorRegister, Index(1));
}

void BaselineCompiler::VisitMul() {
  CallBuiltin(Builtins::kMultiply_Baseline, RegisterOperand(0),
              kInterpreterAccumulatorRegister, Index(1));
}

void BaselineCompiler::VisitDiv() {
  CallBuiltin(Builtins::kDivide_Baseline, RegisterOperand(0),
              kInterpreterAccumulatorRegister, Index(1));
}

void BaselineCompiler::VisitMod() {
  CallBuiltin(Builtins::kModulus_Baseline, RegisterOperand(0),
              kInterpreterAccumulatorRegister, Index(1));
}

void BaselineCompiler::VisitExp() {
  CallBuiltin(Builtins::kExponentiate_Baseline, RegisterOperand(0),
              kInterpreterAccumulatorRegister, Index(1));
}

void BaselineCompiler::VisitBitwiseOr() {
  CallBuiltin(Builtins::kBitwiseOr_Baseline, RegisterOperand(0),
              kInterpreterAccumulatorRegister, Index(1));
}

void BaselineCompiler::VisitBitwiseXor() {
  CallBuiltin(Builtins::kBitwiseXor_Baseline, RegisterOperand(0),
              kInterpreterAccumulatorRegister, Index(1));
}

void BaselineCompiler::VisitBitwiseAnd() {
  CallBuiltin(Builtins::kBitwiseAnd_Baseline, RegisterOperand(0),
              kInterpreterAccumulatorRegister, Index(1));
}

void BaselineCompiler::VisitShiftLeft() {
  CallBuiltin(Builtins::kShiftLeft_Baseline, RegisterOperand(0),
              kInterpreterAccumulatorRegister, Index(1));
}

void BaselineCompiler::VisitShiftRight() {
  CallBuiltin(Builtins::kShiftRight_Baseline, RegisterOperand(0),
              kInterpreterAccumulatorRegister, Index(1));
}

void BaselineCompiler::VisitShiftRightLogical() {
  CallBuiltin(Builtins::kShiftRightLogical_Baseline, RegisterOperand(0),
              kInterpreterAccumulatorRegister, Index(1));
}

void BaselineCompiler::BuildBinopWithConstant(Builtins::Name builtin_name) {
  CallBuiltin(builtin_name, kInterpreterAccumulatorRegister, IntAsSmi(0),
              Index(1));
}

void BaselineCompiler::VisitAddSmi() {
  BuildBinopWithConstant(Builtins::kAdd_Baseline);
}

void BaselineCompiler::VisitSubSmi() {
  BuildBinopWithConstant(Builtins::kSubtract_Baseline);
}

void BaselineCompiler::VisitMulSmi() {
  BuildBinopWithConstant(Builtins::kMultiply_Baseline);
}

void BaselineCompiler::VisitDivSmi() {
  BuildBinopWithConstant(Builtins::kDivide_Baseline);
}

void BaselineCompiler::VisitModSmi() {
  BuildBinopWithConstant(Builtins::kModulus_Baseline);
}

void BaselineCompiler::VisitExpSmi() {
  BuildBinopWithConstant(Builtins::kExponentiate_Baseline);
}

void BaselineCompiler::VisitBitwiseOrSmi() {
  BuildBinopWithConstant(Builtins::kBitwiseOr_Baseline);
}

void BaselineCompiler::VisitBitwiseXorSmi() {
  BuildBinopWithConstant(Builtins::kBitwiseXor_Baseline);
}

void BaselineCompiler::VisitBitwiseAndSmi() {
  BuildBinopWithConstant(Builtins::kBitwiseAnd_Baseline);
}

void BaselineCompiler::VisitShiftLeftSmi() {
  BuildBinopWithConstant(Builtins::kShiftLeft_Baseline);
}

void BaselineCompiler::VisitShiftRightSmi() {
  BuildBinopWithConstant(Builtins::kShiftRight_Baseline);
}

void BaselineCompiler::VisitShiftRightLogicalSmi() {
  BuildBinopWithConstant(Builtins::kShiftRightLogical_Baseline);
}

void BaselineCompiler::BuildUnop(Builtins::Name builtin_name) {
  CallBuiltin(builtin_name,
              kInterpreterAccumulatorRegister,  // value
              Index(0));                        // slot
}

void BaselineCompiler::VisitInc() { BuildUnop(Builtins::kIncrement_Baseline); }

void BaselineCompiler::VisitDec() { BuildUnop(Builtins::kDecrement_Baseline); }

void BaselineCompiler::VisitNegate() { BuildUnop(Builtins::kNegate_Baseline); }

void BaselineCompiler::VisitBitwiseNot() {
  BuildUnop(Builtins::kBitwiseNot_Baseline);
}

void BaselineCompiler::VisitToBooleanLogicalNot() {
  SelectBooleanConstant(kInterpreterAccumulatorRegister,
                        [&](Label* if_true, Label::Distance distance) {
                          JumpIfToBoolean(false,
                                          kInterpreterAccumulatorRegister,
                                          if_true, distance);
                        });
}

void BaselineCompiler::VisitLogicalNot() {
  SelectBooleanConstant(kInterpreterAccumulatorRegister,
                        [&](Label* if_true, Label::Distance distance) {
                          __ JumpIfRoot(kInterpreterAccumulatorRegister,
                                        RootIndex::kFalseValue, if_true,
                                        distance);
                        });
}

void BaselineCompiler::VisitTypeOf() {
  CallBuiltin(Builtins::kTypeof, kInterpreterAccumulatorRegister);
}

void BaselineCompiler::VisitDeletePropertyStrict() {
  BaselineAssembler::ScratchRegisterScope scratch_scope(&basm_);
  Register scratch = scratch_scope.AcquireScratch();
  __ Move(scratch, kInterpreterAccumulatorRegister);
  CallBuiltin(Builtins::kDeleteProperty, RegisterOperand(0), scratch,
              Smi::FromEnum(LanguageMode::kStrict));
}

void BaselineCompiler::VisitDeletePropertySloppy() {
  BaselineAssembler::ScratchRegisterScope scratch_scope(&basm_);
  Register scratch = scratch_scope.AcquireScratch();
  __ Move(scratch, kInterpreterAccumulatorRegister);
  CallBuiltin(Builtins::kDeleteProperty, RegisterOperand(0), scratch,
              Smi::FromEnum(LanguageMode::kSloppy));
}

void BaselineCompiler::VisitGetSuperConstructor() {
  BaselineAssembler::ScratchRegisterScope scratch_scope(&basm_);
  Register prototype = scratch_scope.AcquireScratch();
  __ LoadPrototype(prototype, kInterpreterAccumulatorRegister);
  StoreRegister(0, prototype);
}
template <typename... Args>
void BaselineCompiler::BuildCall(ConvertReceiverMode mode, uint32_t slot,
                                 uint32_t arg_count, Args... args) {
  Builtins::Name builtin;
  switch (mode) {
    case ConvertReceiverMode::kAny:
      builtin = Builtins::kCall_ReceiverIsAny_Baseline;
      break;
    case ConvertReceiverMode::kNullOrUndefined:
      builtin = Builtins::kCall_ReceiverIsNullOrUndefined_Baseline;
      break;
    case ConvertReceiverMode::kNotNullOrUndefined:
      builtin = Builtins::kCall_ReceiverIsNotNullOrUndefined_Baseline;
      break;
    default:
      UNREACHABLE();
  }
  CallBuiltin(builtin,
              RegisterOperand(0),  // kFunction
              arg_count,           // kActualArgumentsCount
              slot,                // kSlot
              args...);            // Arguments
}

void BaselineCompiler::VisitCallAnyReceiver() {
  interpreter::RegisterList args = iterator().GetRegisterListOperand(1);
  uint32_t arg_count = args.register_count() - 1;  // Remove receiver.
  BuildCall(ConvertReceiverMode::kAny, Index(3), arg_count, args);
}

void BaselineCompiler::VisitCallProperty() {
  interpreter::RegisterList args = iterator().GetRegisterListOperand(1);
  uint32_t arg_count = args.register_count() - 1;  // Remove receiver.
  BuildCall(ConvertReceiverMode::kNotNullOrUndefined, Index(3), arg_count,
            args);
}

void BaselineCompiler::VisitCallProperty0() {
  BuildCall(ConvertReceiverMode::kNotNullOrUndefined, Index(2), 0,
            RegisterOperand(1));
}

void BaselineCompiler::VisitCallProperty1() {
  BuildCall(ConvertReceiverMode::kNotNullOrUndefined, Index(3), 1,
            RegisterOperand(1), RegisterOperand(2));
}

void BaselineCompiler::VisitCallProperty2() {
  BuildCall(ConvertReceiverMode::kNotNullOrUndefined, Index(4), 2,
            RegisterOperand(1), RegisterOperand(2), RegisterOperand(3));
}

void BaselineCompiler::VisitCallUndefinedReceiver() {
  interpreter::RegisterList args = iterator().GetRegisterListOperand(1);
  uint32_t arg_count = args.register_count();
  BuildCall(ConvertReceiverMode::kNullOrUndefined, Index(3), arg_count,
            RootIndex::kUndefinedValue, args);
}

void BaselineCompiler::VisitCallUndefinedReceiver0() {
  BuildCall(ConvertReceiverMode::kNullOrUndefined, Index(1), 0,
            RootIndex::kUndefinedValue);
}

void BaselineCompiler::VisitCallUndefinedReceiver1() {
  BuildCall(ConvertReceiverMode::kNullOrUndefined, Index(2), 1,
            RootIndex::kUndefinedValue, RegisterOperand(1));
}

void BaselineCompiler::VisitCallUndefinedReceiver2() {
  BuildCall(ConvertReceiverMode::kNullOrUndefined, Index(3), 2,
            RootIndex::kUndefinedValue, RegisterOperand(1), RegisterOperand(2));
}

void BaselineCompiler::VisitCallNoFeedback() {
  interpreter::RegisterList args = iterator().GetRegisterListOperand(1);
  uint32_t arg_count = args.register_count();
  CallBuiltin(Builtins::kCall_ReceiverIsAny,
              RegisterOperand(0),  // kFunction
              arg_count - 1,       // kActualArgumentsCount
              args);
}

void BaselineCompiler::VisitCallWithSpread() {
  interpreter::RegisterList args = iterator().GetRegisterListOperand(1);

  // Do not push the spread argument
  interpreter::Register spread_register = args.last_register();
  args = args.Truncate(args.register_count() - 1);

  uint32_t arg_count = args.register_count() - 1;  // Remove receiver.

  CallBuiltin(Builtins::kCallWithSpread_Baseline,
              RegisterOperand(0),  // kFunction
              arg_count,           // kActualArgumentsCount
              spread_register,     // kSpread
              Index(3),            // kSlot
              args);
}

void BaselineCompiler::VisitCallRuntime() {
  CallRuntime(iterator().GetRuntimeIdOperand(0),
              iterator().GetRegisterListOperand(1));
}

void BaselineCompiler::VisitCallRuntimeForPair() {
  SaveAccumulatorScope accumulator_scope(&basm_);
  CallRuntime(iterator().GetRuntimeIdOperand(0),
              iterator().GetRegisterListOperand(1));
  StoreRegisterPair(3, kReturnRegister0, kReturnRegister1);
}

void BaselineCompiler::VisitCallJSRuntime() {
  interpreter::RegisterList args = iterator().GetRegisterListOperand(1);
  uint32_t arg_count = args.register_count();

  // Load context for LoadNativeContextSlot.
  __ LoadContext(kContextRegister);
  __ LoadNativeContextSlot(kJavaScriptCallTargetRegister,
                           iterator().GetNativeContextIndexOperand(0));
  CallBuiltin(Builtins::kCall_ReceiverIsNullOrUndefined,
              kJavaScriptCallTargetRegister,  // kFunction
              arg_count,                      // kActualArgumentsCount
              RootIndex::kUndefinedValue,     // kReceiver
              args);
}

void BaselineCompiler::VisitInvokeIntrinsic() {
  Runtime::FunctionId intrinsic_id = iterator().GetIntrinsicIdOperand(0);
  interpreter::RegisterList args = iterator().GetRegisterListOperand(1);
  switch (intrinsic_id) {
#define CASE(Name, ...)         \
  case Runtime::kInline##Name:  \
    VisitIntrinsic##Name(args); \
    break;
    INTRINSICS_LIST(CASE)
#undef CASE

    default:
      UNREACHABLE();
  }
}

void BaselineCompiler::VisitIntrinsicIsJSReceiver(
    interpreter::RegisterList args) {
  SelectBooleanConstant(
      kInterpreterAccumulatorRegister,
      [&](Label* is_true, Label::Distance distance) {
        BaselineAssembler::ScratchRegisterScope scratch_scope(&basm_);
        __ LoadRegister(kInterpreterAccumulatorRegister, args[0]);

        Label is_smi;
        __ JumpIfSmi(kInterpreterAccumulatorRegister, &is_smi, Label::kNear);

        // If we ever added more instance types after LAST_JS_RECEIVER_TYPE,
        // this would have to become a range check.
        STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
        __ CmpObjectType(kInterpreterAccumulatorRegister,
                         FIRST_JS_RECEIVER_TYPE,
                         scratch_scope.AcquireScratch());
        __ JumpIf(Condition::kGreaterThanEqual, is_true, distance);

        __ Bind(&is_smi);
      });
}

void BaselineCompiler::VisitIntrinsicIsArray(interpreter::RegisterList args) {
  SelectBooleanConstant(
      kInterpreterAccumulatorRegister,
      [&](Label* is_true, Label::Distance distance) {
        BaselineAssembler::ScratchRegisterScope scratch_scope(&basm_);
        __ LoadRegister(kInterpreterAccumulatorRegister, args[0]);

        Label is_smi;
        __ JumpIfSmi(kInterpreterAccumulatorRegister, &is_smi, Label::kNear);

        __ CmpObjectType(kInterpreterAccumulatorRegister, JS_ARRAY_TYPE,
                         scratch_scope.AcquireScratch());
        __ JumpIf(Condition::kEqual, is_true, distance);

        __ Bind(&is_smi);
      });
}

void BaselineCompiler::VisitIntrinsicIsSmi(interpreter::RegisterList args) {
  SelectBooleanConstant(
      kInterpreterAccumulatorRegister,
      [&](Label* is_true, Label::Distance distance) {
        __ LoadRegister(kInterpreterAccumulatorRegister, args[0]);
        __ JumpIfSmi(kInterpreterAccumulatorRegister, is_true, distance);
      });
}

void BaselineCompiler::VisitIntrinsicCopyDataProperties(
    interpreter::RegisterList args) {
  CallBuiltin(Builtins::kCopyDataProperties, args);
}

void BaselineCompiler::VisitIntrinsicCreateIterResultObject(
    interpreter::RegisterList args) {
  CallBuiltin(Builtins::kCreateIterResultObject, args);
}

void BaselineCompiler::VisitIntrinsicHasProperty(
    interpreter::RegisterList args) {
  CallBuiltin(Builtins::kHasProperty, args);
}

void BaselineCompiler::VisitIntrinsicToString(interpreter::RegisterList args) {
  CallBuiltin(Builtins::kToString, args);
}

void BaselineCompiler::VisitIntrinsicToLength(interpreter::RegisterList args) {
  CallBuiltin(Builtins::kToLength, args);
}

void BaselineCompiler::VisitIntrinsicToObject(interpreter::RegisterList args) {
  CallBuiltin(Builtins::kToObject, args);
}

void BaselineCompiler::VisitIntrinsicCall(interpreter::RegisterList args) {
  // First argument register contains the function target.
  __ LoadRegister(kJavaScriptCallTargetRegister, args.first_register());

  // The arguments for the target function are from the second runtime call
  // argument.
  args = args.PopLeft();

  uint32_t arg_count = args.register_count();
  CallBuiltin(Builtins::kCall_ReceiverIsAny,
              kJavaScriptCallTargetRegister,  // kFunction
              arg_count - 1,                  // kActualArgumentsCount
              args);
}

void BaselineCompiler::VisitIntrinsicCreateAsyncFromSyncIterator(
    interpreter::RegisterList args) {
  CallBuiltin(Builtins::kCreateAsyncFromSyncIteratorBaseline, args[0]);
}

void BaselineCompiler::VisitIntrinsicCreateJSGeneratorObject(
    interpreter::RegisterList args) {
  CallBuiltin(Builtins::kCreateGeneratorObject, args);
}

void BaselineCompiler::VisitIntrinsicGeneratorGetResumeMode(
    interpreter::RegisterList args) {
  __ LoadRegister(kInterpreterAccumulatorRegister, args[0]);
  __ LoadTaggedAnyField(kInterpreterAccumulatorRegister,
                        kInterpreterAccumulatorRegister,
                        JSGeneratorObject::kResumeModeOffset);
}

void BaselineCompiler::VisitIntrinsicGeneratorClose(
    interpreter::RegisterList args) {
  __ LoadRegister(kInterpreterAccumulatorRegister, args[0]);
  __ StoreTaggedSignedField(kInterpreterAccumulatorRegister,
                            JSGeneratorObject::kContinuationOffset,
                            Smi::FromInt(JSGeneratorObject::kGeneratorClosed));
  __ LoadRoot(kInterpreterAccumulatorRegister, RootIndex::kUndefinedValue);
}

void BaselineCompiler::VisitIntrinsicGetImportMetaObject(
    interpreter::RegisterList args) {
  CallBuiltin(Builtins::kGetImportMetaObjectBaseline);
}

void BaselineCompiler::VisitIntrinsicAsyncFunctionAwaitCaught(
    interpreter::RegisterList args) {
  CallBuiltin(Builtins::kAsyncFunctionAwaitCaught, args);
}

void BaselineCompiler::VisitIntrinsicAsyncFunctionAwaitUncaught(
    interpreter::RegisterList args) {
  CallBuiltin(Builtins::kAsyncFunctionAwaitUncaught, args);
}

void BaselineCompiler::VisitIntrinsicAsyncFunctionEnter(
    interpreter::RegisterList args) {
  CallBuiltin(Builtins::kAsyncFunctionEnter, args);
}

void BaselineCompiler::VisitIntrinsicAsyncFunctionReject(
    interpreter::RegisterList args) {
  CallBuiltin(Builtins::kAsyncFunctionReject, args);
}

void BaselineCompiler::VisitIntrinsicAsyncFunctionResolve(
    interpreter::RegisterList args) {
  CallBuiltin(Builtins::kAsyncFunctionResolve, args);
}

void BaselineCompiler::VisitIntrinsicAsyncGeneratorAwaitCaught(
    interpreter::RegisterList args) {
  CallBuiltin(Builtins::kAsyncGeneratorAwaitCaught, args);
}

void BaselineCompiler::VisitIntrinsicAsyncGeneratorAwaitUncaught(
    interpreter::RegisterList args) {
  CallBuiltin(Builtins::kAsyncGeneratorAwaitUncaught, args);
}

void BaselineCompiler::VisitIntrinsicAsyncGeneratorReject(
    interpreter::RegisterList args) {
  CallBuiltin(Builtins::kAsyncGeneratorReject, args);
}

void BaselineCompiler::VisitIntrinsicAsyncGeneratorResolve(
    interpreter::RegisterList args) {
  CallBuiltin(Builtins::kAsyncGeneratorResolve, args);
}

void BaselineCompiler::VisitIntrinsicAsyncGeneratorYield(
    interpreter::RegisterList args) {
  CallBuiltin(Builtins::kAsyncGeneratorYield, args);
}

void BaselineCompiler::VisitConstruct() {
  interpreter::RegisterList args = iterator().GetRegisterListOperand(1);
  uint32_t arg_count = args.register_count();
  CallBuiltin(Builtins::kConstruct_Baseline,
              RegisterOperand(0),               // kFunction
              kInterpreterAccumulatorRegister,  // kNewTarget
              arg_count,                        // kActualArgumentsCount
              Index(3),                         // kSlot
              RootIndex::kUndefinedValue,       // kReceiver
              args);
}

void BaselineCompiler::VisitConstructWithSpread() {
  interpreter::RegisterList args = iterator().GetRegisterListOperand(1);

  // Do not push the spread argument
  interpreter::Register spread_register = args.last_register();
  args = args.Truncate(args.register_count() - 1);

  uint32_t arg_count = args.register_count();

  Register new_target =
      Builtins::CallInterfaceDescriptorFor(
          Builtins::kConstructWithSpread_Baseline)
          .GetRegisterParameter(
              ConstructWithSpread_BaselineDescriptor::kNewTarget);
  __ Move(new_target, kInterpreterAccumulatorRegister);

  CallBuiltin(Builtins::kConstructWithSpread_Baseline,
              RegisterOperand(0),          // kFunction
              new_target,                  // kNewTarget
              arg_count,                   // kActualArgumentsCount
              Index(3),                    // kSlot
              spread_register,             // kSpread
              RootIndex::kUndefinedValue,  // kReceiver
              args);
}

void BaselineCompiler::BuildCompare(Builtins::Name builtin_name) {
  CallBuiltin(builtin_name, RegisterOperand(0),  // lhs
              kInterpreterAccumulatorRegister,   // rhs
              Index(1));                         // slot
}

void BaselineCompiler::VisitTestEqual() {
  BuildCompare(Builtins::kEqual_Baseline);
}

void BaselineCompiler::VisitTestEqualStrict() {
  BuildCompare(Builtins::kStrictEqual_Baseline);
}

void BaselineCompiler::VisitTestLessThan() {
  BuildCompare(Builtins::kLessThan_Baseline);
}

void BaselineCompiler::VisitTestGreaterThan() {
  BuildCompare(Builtins::kGreaterThan_Baseline);
}

void BaselineCompiler::VisitTestLessThanOrEqual() {
  BuildCompare(Builtins::kLessThanOrEqual_Baseline);
}

void BaselineCompiler::VisitTestGreaterThanOrEqual() {
  BuildCompare(Builtins::kGreaterThanOrEqual_Baseline);
}

void BaselineCompiler::VisitTestReferenceEqual() {
  SelectBooleanConstant(kInterpreterAccumulatorRegister,
                        [&](Label* is_true, Label::Distance distance) {
                          __ CompareTagged(
                              __ RegisterFrameOperand(RegisterOperand(0)),
                              kInterpreterAccumulatorRegister);
                          __ JumpIf(Condition::kEqual, is_true, distance);
                        });
}

void BaselineCompiler::VisitTestInstanceOf() {
  Register callable =
      Builtins::CallInterfaceDescriptorFor(Builtins::kInstanceOf_Baseline)
          .GetRegisterParameter(Compare_BaselineDescriptor::kRight);
  __ Move(callable, kInterpreterAccumulatorRegister);
  CallBuiltin(Builtins::kInstanceOf_Baseline,
              RegisterOperand(0),  // object
              callable,            // callable
              Index(1));           // slot
}

void BaselineCompiler::VisitTestIn() {
  CallBuiltin(Builtins::kKeyedHasICBaseline,
              kInterpreterAccumulatorRegister,  // object
              RegisterOperand(0),               // name
              IndexAsTagged(1));                // slot
}

void BaselineCompiler::VisitTestUndetectable() {
  Label done, is_smi, not_undetectable;
  __ JumpIfSmi(kInterpreterAccumulatorRegister, &is_smi, Label::kNear);

  Register map_bit_field = kInterpreterAccumulatorRegister;
  __ LoadMap(map_bit_field, kInterpreterAccumulatorRegister);
  __ LoadByteField(map_bit_field, map_bit_field, Map::kBitFieldOffset);
  __ Test(map_bit_field, Map::Bits1::IsUndetectableBit::kMask);
  __ JumpIf(Condition::kZero, &not_undetectable, Label::kNear);

  __ LoadRoot(kInterpreterAccumulatorRegister, RootIndex::kTrueValue);
  __ Jump(&done, Label::kNear);

  __ Bind(&is_smi);
  __ Bind(&not_undetectable);
  __ LoadRoot(kInterpreterAccumulatorRegister, RootIndex::kFalseValue);
  __ Bind(&done);
}

void BaselineCompiler::VisitTestNull() {
  SelectBooleanConstant(kInterpreterAccumulatorRegister,
                        [&](Label* is_true, Label::Distance distance) {
                          __ JumpIfRoot(kInterpreterAccumulatorRegister,
                                        RootIndex::kNullValue, is_true,
                                        distance);
                        });
}

void BaselineCompiler::VisitTestUndefined() {
  SelectBooleanConstant(kInterpreterAccumulatorRegister,
                        [&](Label* is_true, Label::Distance distance) {
                          __ JumpIfRoot(kInterpreterAccumulatorRegister,
                                        RootIndex::kUndefinedValue, is_true,
                                        distance);
                        });
}

void BaselineCompiler::VisitTestTypeOf() {
  BaselineAssembler::ScratchRegisterScope scratch_scope(&basm_);

  auto literal_flag =
      static_cast<interpreter::TestTypeOfFlags::LiteralFlag>(Flag(0));

  Label done;
  switch (literal_flag) {
    case interpreter::TestTypeOfFlags::LiteralFlag::kNumber: {
      Label is_smi, is_heap_number;
      __ JumpIfSmi(kInterpreterAccumulatorRegister, &is_smi, Label::kNear);
      __ CmpObjectType(kInterpreterAccumulatorRegister, HEAP_NUMBER_TYPE,
                       scratch_scope.AcquireScratch());
      __ JumpIf(Condition::kEqual, &is_heap_number, Label::kNear);

      __ LoadRoot(kInterpreterAccumulatorRegister, RootIndex::kFalseValue);
      __ Jump(&done, Label::kNear);

      __ Bind(&is_smi);
      __ Bind(&is_heap_number);
      __ LoadRoot(kInterpreterAccumulatorRegister, RootIndex::kTrueValue);
      break;
    }
    case interpreter::TestTypeOfFlags::LiteralFlag::kString: {
      Label is_smi, bad_instance_type;
      __ JumpIfSmi(kInterpreterAccumulatorRegister, &is_smi, Label::kNear);
      STATIC_ASSERT(INTERNALIZED_STRING_TYPE == FIRST_TYPE);
      __ CmpObjectType(kInterpreterAccumulatorRegister, FIRST_NONSTRING_TYPE,
                       scratch_scope.AcquireScratch());
      __ JumpIf(Condition::kGreaterThanEqual, &bad_instance_type, Label::kNear);

      __ LoadRoot(kInterpreterAccumulatorRegister, RootIndex::kTrueValue);
      __ Jump(&done, Label::kNear);

      __ Bind(&is_smi);
      __ Bind(&bad_instance_type);
      __ LoadRoot(kInterpreterAccumulatorRegister, RootIndex::kFalseValue);
      break;
    }
    case interpreter::TestTypeOfFlags::LiteralFlag::kSymbol: {
      Label is_smi, bad_instance_type;
      __ JumpIfSmi(kInterpreterAccumulatorRegister, &is_smi, Label::kNear);
      __ CmpObjectType(kInterpreterAccumulatorRegister, SYMBOL_TYPE,
                       scratch_scope.AcquireScratch());
      __ JumpIf(Condition::kNotEqual, &bad_instance_type, Label::kNear);

      __ LoadRoot(kInterpreterAccumulatorRegister, RootIndex::kTrueValue);
      __ Jump(&done, Label::kNear);

      __ Bind(&is_smi);
      __ Bind(&bad_instance_type);
      __ LoadRoot(kInterpreterAccumulatorRegister, RootIndex::kFalseValue);
      break;
    }
    case interpreter::TestTypeOfFlags::LiteralFlag::kBoolean: {
      Label is_true, is_false;
      __ JumpIfRoot(kInterpreterAccumulatorRegister, RootIndex::kTrueValue,
                    &is_true, Label::kNear);
      __ JumpIfRoot(kInterpreterAccumulatorRegister, RootIndex::kFalseValue,
                    &is_false, Label::kNear);

      __ LoadRoot(kInterpreterAccumulatorRegister, RootIndex::kFalseValue);
      __ Jump(&done, Label::kNear);

      __ Bind(&is_true);
      __ Bind(&is_false);
      __ LoadRoot(kInterpreterAccumulatorRegister, RootIndex::kTrueValue);
      break;
    }
    case interpreter::TestTypeOfFlags::LiteralFlag::kBigInt: {
      Label is_smi, bad_instance_type;
      __ JumpIfSmi(kInterpreterAccumulatorRegister, &is_smi, Label::kNear);
      __ CmpObjectType(kInterpreterAccumulatorRegister, BIGINT_TYPE,
                       scratch_scope.AcquireScratch());
      __ JumpIf(Condition::kNotEqual, &bad_instance_type, Label::kNear);

      __ LoadRoot(kInterpreterAccumulatorRegister, RootIndex::kTrueValue);
      __ Jump(&done, Label::kNear);

      __ Bind(&is_smi);
      __ Bind(&bad_instance_type);
      __ LoadRoot(kInterpreterAccumulatorRegister, RootIndex::kFalseValue);
      break;
    }
    case interpreter::TestTypeOfFlags::LiteralFlag::kUndefined: {
      Label is_smi, is_null, not_undetectable;
      __ JumpIfSmi(kInterpreterAccumulatorRegister, &is_smi, Label::kNear);

      // null is undetectable, so test it explicitly, and return false.
      __ JumpIfRoot(kInterpreterAccumulatorRegister, RootIndex::kNullValue,
                    &is_null, Label::kNear);

      // All other undetectable maps are typeof undefined.
      Register map_bit_field = kInterpreterAccumulatorRegister;
      __ LoadMap(map_bit_field, kInterpreterAccumulatorRegister);
      __ LoadByteField(map_bit_field, map_bit_field, Map::kBitFieldOffset);
      __ Test(map_bit_field, Map::Bits1::IsUndetectableBit::kMask);
      __ JumpIf(Condition::kZero, &not_undetectable, Label::kNear);

      __ LoadRoot(kInterpreterAccumulatorRegister, RootIndex::kTrueValue);
      __ Jump(&done, Label::kNear);

      __ Bind(&is_smi);
      __ Bind(&is_null);
      __ Bind(&not_undetectable);
      __ LoadRoot(kInterpreterAccumulatorRegister, RootIndex::kFalseValue);
      break;
    }
    case interpreter::TestTypeOfFlags::LiteralFlag::kFunction: {
      Label is_smi, not_callable, undetectable;
      __ JumpIfSmi(kInterpreterAccumulatorRegister, &is_smi, Label::kNear);

      // Check if the map is callable but not undetectable.
      Register map_bit_field = kInterpreterAccumulatorRegister;
      __ LoadMap(map_bit_field, kInterpreterAccumulatorRegister);
      __ LoadByteField(map_bit_field, map_bit_field, Map::kBitFieldOffset);
      __ Test(map_bit_field, Map::Bits1::IsCallableBit::kMask);
      __ JumpIf(Condition::kZero, &not_callable, Label::kNear);
      __ Test(map_bit_field, Map::Bits1::IsUndetectableBit::kMask);
      __ JumpIf(Condition::kNotZero, &undetectable, Label::kNear);

      __ LoadRoot(kInterpreterAccumulatorRegister, RootIndex::kTrueValue);
      __ Jump(&done, Label::kNear);

      __ Bind(&is_smi);
      __ Bind(&not_callable);
      __ Bind(&undetectable);
      __ LoadRoot(kInterpreterAccumulatorRegister, RootIndex::kFalseValue);
      break;
    }
    case interpreter::TestTypeOfFlags::LiteralFlag::kObject: {
      Label is_smi, is_null, bad_instance_type, undetectable_or_callable;
      __ JumpIfSmi(kInterpreterAccumulatorRegister, &is_smi, Label::kNear);

      // If the object is null, return true.
      __ JumpIfRoot(kInterpreterAccumulatorRegister, RootIndex::kNullValue,
                    &is_null, Label::kNear);

      // If the object's instance type isn't within the range, return false.
      STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
      Register map = scratch_scope.AcquireScratch();
      __ CmpObjectType(kInterpreterAccumulatorRegister, FIRST_JS_RECEIVER_TYPE,
                       map);
      __ JumpIf(Condition::kLessThan, &bad_instance_type, Label::kNear);

      // If the map is undetectable or callable, return false.
      Register map_bit_field = kInterpreterAccumulatorRegister;
      __ LoadByteField(map_bit_field, map, Map::kBitFieldOffset);
      __ Test(map_bit_field, Map::Bits1::IsUndetectableBit::kMask |
                                 Map::Bits1::IsCallableBit::kMask);
      __ JumpIf(Condition::kNotZero, &undetectable_or_callable, Label::kNear);

      __ Bind(&is_null);
      __ LoadRoot(kInterpreterAccumulatorRegister, RootIndex::kTrueValue);
      __ Jump(&done, Label::kNear);

      __ Bind(&is_smi);
      __ Bind(&bad_instance_type);
      __ Bind(&undetectable_or_callable);
      __ LoadRoot(kInterpreterAccumulatorRegister, RootIndex::kFalseValue);
      break;
    }
    case interpreter::TestTypeOfFlags::LiteralFlag::kOther:
    default:
      UNREACHABLE();
  }
  __ Bind(&done);
}

void BaselineCompiler::VisitToName() {
  SaveAccumulatorScope save_accumulator(&basm_);
  CallBuiltin(Builtins::kToName, kInterpreterAccumulatorRegister);
  StoreRegister(0, kInterpreterAccumulatorRegister);
}

void BaselineCompiler::VisitToNumber() {
  CallBuiltin(Builtins::kToNumber_Baseline, kInterpreterAccumulatorRegister,
              Index(0));
}

void BaselineCompiler::VisitToNumeric() {
  CallBuiltin(Builtins::kToNumeric_Baseline, kInterpreterAccumulatorRegister,
              Index(0));
}

void BaselineCompiler::VisitToObject() {
  SaveAccumulatorScope save_accumulator(&basm_);
  CallBuiltin(Builtins::kToObject, kInterpreterAccumulatorRegister);
  StoreRegister(0, kInterpreterAccumulatorRegister);
}

void BaselineCompiler::VisitToString() {
  CallBuiltin(Builtins::kToString, kInterpreterAccumulatorRegister);
}

void BaselineCompiler::VisitCreateRegExpLiteral() {
  CallBuiltin(Builtins::kCreateRegExpLiteral,
              FeedbackVector(),         // feedback vector
              IndexAsTagged(1),         // slot
              Constant<HeapObject>(0),  // pattern
              FlagAsSmi(2));            // flags
}

void BaselineCompiler::VisitCreateArrayLiteral() {
  uint32_t flags = Flag(2);
  int32_t flags_raw = static_cast<int32_t>(
      interpreter::CreateArrayLiteralFlags::FlagsBits::decode(flags));
  if (flags &
      interpreter::CreateArrayLiteralFlags::FastCloneSupportedBit::kMask) {
    CallBuiltin(Builtins::kCreateShallowArrayLiteral,
                FeedbackVector(),          // feedback vector
                IndexAsTagged(1),          // slot
                Constant<HeapObject>(0),   // constant elements
                Smi::FromInt(flags_raw));  // flags
  } else {
    CallRuntime(Runtime::kCreateArrayLiteral,
                FeedbackVector(),          // feedback vector
                IndexAsTagged(1),          // slot
                Constant<HeapObject>(0),   // constant elements
                Smi::FromInt(flags_raw));  // flags
  }
}

void BaselineCompiler::VisitCreateArrayFromIterable() {
  CallBuiltin(Builtins::kIterableToListWithSymbolLookup,
              kInterpreterAccumulatorRegister);  // iterable
}

void BaselineCompiler::VisitCreateEmptyArrayLiteral() {
  CallBuiltin(Builtins::kCreateEmptyArrayLiteral, FeedbackVector(),
              IndexAsTagged(0));
}

void BaselineCompiler::VisitCreateObjectLiteral() {
  uint32_t flags = Flag(2);
  int32_t flags_raw = static_cast<int32_t>(
      interpreter::CreateObjectLiteralFlags::FlagsBits::decode(flags));
  if (flags &
      interpreter::CreateObjectLiteralFlags::FastCloneSupportedBit::kMask) {
    CallBuiltin(Builtins::kCreateShallowObjectLiteral,
                FeedbackVector(),                           // feedback vector
                IndexAsTagged(1),                           // slot
                Constant<ObjectBoilerplateDescription>(0),  // boilerplate
                Smi::FromInt(flags_raw));                   // flags
  } else {
    CallRuntime(Runtime::kCreateObjectLiteral,
                FeedbackVector(),                           // feedback vector
                IndexAsTagged(1),                           // slot
                Constant<ObjectBoilerplateDescription>(0),  // boilerplate
                Smi::FromInt(flags_raw));                   // flags
  }
}

void BaselineCompiler::VisitCreateEmptyObjectLiteral() {
  CallBuiltin(Builtins::kCreateEmptyLiteralObject);
}

void BaselineCompiler::VisitCloneObject() {
  uint32_t flags = Flag(1);
  int32_t raw_flags =
      interpreter::CreateObjectLiteralFlags::FlagsBits::decode(flags);
  CallBuiltin(Builtins::kCloneObjectICBaseline,
              RegisterOperand(0),       // source
              Smi::FromInt(raw_flags),  // flags
              IndexAsTagged(2));        // slot
}

void BaselineCompiler::VisitGetTemplateObject() {
  BaselineAssembler::ScratchRegisterScope scratch_scope(&basm_);
  CallBuiltin(Builtins::kGetTemplateObject,
              shared_function_info_,    // shared function info
              Constant<HeapObject>(0),  // description
              Index(1),                 // slot
              FeedbackVector());        // feedback_vector
}

void BaselineCompiler::VisitCreateClosure() {
  Register feedback_cell =
      Builtins::CallInterfaceDescriptorFor(Builtins::kFastNewClosure)
          .GetRegisterParameter(FastNewClosureDescriptor::kFeedbackCell);
  LoadClosureFeedbackArray(feedback_cell);
  __ LoadFixedArrayElement(feedback_cell, feedback_cell, Index(1));

  uint32_t flags = Flag(2);
  if (interpreter::CreateClosureFlags::FastNewClosureBit::decode(flags)) {
    CallBuiltin(Builtins::kFastNewClosure, Constant<SharedFunctionInfo>(0),
                feedback_cell);
  } else {
    Runtime::FunctionId function_id =
        interpreter::CreateClosureFlags::PretenuredBit::decode(flags)
            ? Runtime::kNewClosure_Tenured
            : Runtime::kNewClosure;
    CallRuntime(function_id, Constant<SharedFunctionInfo>(0), feedback_cell);
  }
}

void BaselineCompiler::VisitCreateBlockContext() {
  CallRuntime(Runtime::kPushBlockContext, Constant<ScopeInfo>(0));
}

void BaselineCompiler::VisitCreateCatchContext() {
  CallRuntime(Runtime::kPushCatchContext,
              RegisterOperand(0),  // exception
              Constant<ScopeInfo>(1));
}

void BaselineCompiler::VisitCreateFunctionContext() {
  Handle<ScopeInfo> info = Constant<ScopeInfo>(0);
  uint32_t slot_count = Uint(1);
  if (slot_count < static_cast<uint32_t>(
                       ConstructorBuiltins::MaximumFunctionContextSlots())) {
    DCHECK_EQ(info->scope_type(), ScopeType::FUNCTION_SCOPE);
    CallBuiltin(Builtins::kFastNewFunctionContextFunction, info, slot_count);
  } else {
    CallRuntime(Runtime::kNewFunctionContext, Constant<ScopeInfo>(0));
  }
}

void BaselineCompiler::VisitCreateEvalContext() {
  Handle<ScopeInfo> info = Constant<ScopeInfo>(0);
  uint32_t slot_count = Uint(1);
  if (slot_count < static_cast<uint32_t>(
                       ConstructorBuiltins::MaximumFunctionContextSlots())) {
    DCHECK_EQ(info->scope_type(), ScopeType::EVAL_SCOPE);
    CallBuiltin(Builtins::kFastNewFunctionContextEval, info, slot_count);
  } else {
    CallRuntime(Runtime::kNewFunctionContext, Constant<ScopeInfo>(0));
  }
}

void BaselineCompiler::VisitCreateWithContext() {
  CallRuntime(Runtime::kPushWithContext,
              RegisterOperand(0),  // object
              Constant<ScopeInfo>(1));
}

void BaselineCompiler::VisitCreateMappedArguments() {
  if (shared_function_info_->has_duplicate_parameters()) {
    CallRuntime(Runtime::kNewSloppyArguments, __ FunctionOperand());
  } else {
    CallBuiltin(Builtins::kFastNewSloppyArguments, __ FunctionOperand());
  }
}

void BaselineCompiler::VisitCreateUnmappedArguments() {
  CallBuiltin(Builtins::kFastNewStrictArguments, __ FunctionOperand());
}

void BaselineCompiler::VisitCreateRestParameter() {
  CallBuiltin(Builtins::kFastNewRestArguments, __ FunctionOperand());
}

void BaselineCompiler::VisitJumpLoop() {
  BaselineAssembler::ScratchRegisterScope scope(&basm_);
  Register scratch = scope.AcquireScratch();
  Label osr_not_armed;
  __ RecordComment("[ OSR Check Armed");
  Register osr_level = scratch;
  __ LoadRegister(osr_level, interpreter::Register::bytecode_array());
  __ LoadByteField(osr_level, osr_level, BytecodeArray::kOsrNestingLevelOffset);
  int loop_depth = iterator().GetImmediateOperand(1);
  __ CompareByte(osr_level, loop_depth);
  __ JumpIf(Condition::kUnsignedLessThanEqual, &osr_not_armed);
  CallBuiltin(Builtins::kBaselineOnStackReplacement);
  __ RecordComment("]");

  __ Bind(&osr_not_armed);
  Label* label = &labels_[iterator().GetJumpTargetOffset()]->unlinked;
  int weight = iterator().GetRelativeJumpTargetOffset() -
               iterator().current_bytecode_size_without_prefix();
  // We can pass in the same label twice since it's a back edge and thus already
  // bound.
  DCHECK(label->is_bound());
  UpdateInterruptBudgetAndJumpToLabel(weight, label, label);
}

void BaselineCompiler::VisitJump() {
  UpdateInterruptBudgetAndDoInterpreterJump();
}

void BaselineCompiler::VisitJumpConstant() { VisitJump(); }

void BaselineCompiler::VisitJumpIfNullConstant() { VisitJumpIfNull(); }

void BaselineCompiler::VisitJumpIfNotNullConstant() { VisitJumpIfNotNull(); }

void BaselineCompiler::VisitJumpIfUndefinedConstant() {
  VisitJumpIfUndefined();
}

void BaselineCompiler::VisitJumpIfNotUndefinedConstant() {
  VisitJumpIfNotUndefined();
}

void BaselineCompiler::VisitJumpIfUndefinedOrNullConstant() {
  VisitJumpIfUndefinedOrNull();
}

void BaselineCompiler::VisitJumpIfTrueConstant() { VisitJumpIfTrue(); }

void BaselineCompiler::VisitJumpIfFalseConstant() { VisitJumpIfFalse(); }

void BaselineCompiler::VisitJumpIfJSReceiverConstant() {
  VisitJumpIfJSReceiver();
}

void BaselineCompiler::VisitJumpIfToBooleanTrueConstant() {
  VisitJumpIfToBooleanTrue();
}

void BaselineCompiler::VisitJumpIfToBooleanFalseConstant() {
  VisitJumpIfToBooleanFalse();
}

void BaselineCompiler::VisitJumpIfToBooleanTrue() {
  Label dont_jump;
  JumpIfToBoolean(false, kInterpreterAccumulatorRegister, &dont_jump,
                  Label::kNear);
  UpdateInterruptBudgetAndDoInterpreterJump();
  __ Bind(&dont_jump);
}

void BaselineCompiler::VisitJumpIfToBooleanFalse() {
  Label dont_jump;
  JumpIfToBoolean(true, kInterpreterAccumulatorRegister, &dont_jump,
                  Label::kNear);
  UpdateInterruptBudgetAndDoInterpreterJump();
  __ Bind(&dont_jump);
}

void BaselineCompiler::VisitJumpIfTrue() {
  UpdateInterruptBudgetAndDoInterpreterJumpIfRoot(RootIndex::kTrueValue);
}

void BaselineCompiler::VisitJumpIfFalse() {
  UpdateInterruptBudgetAndDoInterpreterJumpIfRoot(RootIndex::kFalseValue);
}

void BaselineCompiler::VisitJumpIfNull() {
  UpdateInterruptBudgetAndDoInterpreterJumpIfRoot(RootIndex::kNullValue);
}

void BaselineCompiler::VisitJumpIfNotNull() {
  UpdateInterruptBudgetAndDoInterpreterJumpIfNotRoot(RootIndex::kNullValue);
}

void BaselineCompiler::VisitJumpIfUndefined() {
  UpdateInterruptBudgetAndDoInterpreterJumpIfRoot(RootIndex::kUndefinedValue);
}

void BaselineCompiler::VisitJumpIfNotUndefined() {
  UpdateInterruptBudgetAndDoInterpreterJumpIfNotRoot(
      RootIndex::kUndefinedValue);
}

void BaselineCompiler::VisitJumpIfUndefinedOrNull() {
  Label do_jump, dont_jump;
  __ JumpIfRoot(kInterpreterAccumulatorRegister, RootIndex::kUndefinedValue,
                &do_jump);
  __ JumpIfNotRoot(kInterpreterAccumulatorRegister, RootIndex::kNullValue,
                   &dont_jump, Label::kNear);
  __ Bind(&do_jump);
  UpdateInterruptBudgetAndDoInterpreterJump();
  __ Bind(&dont_jump);
}

void BaselineCompiler::VisitJumpIfJSReceiver() {
  BaselineAssembler::ScratchRegisterScope scratch_scope(&basm_);

  Label is_smi, dont_jump;
  __ JumpIfSmi(kInterpreterAccumulatorRegister, &is_smi, Label::kNear);

  __ CmpObjectType(kInterpreterAccumulatorRegister, FIRST_JS_RECEIVER_TYPE,
                   scratch_scope.AcquireScratch());
  __ JumpIf(Condition::kLessThan, &dont_jump);
  UpdateInterruptBudgetAndDoInterpreterJump();

  __ Bind(&is_smi);
  __ Bind(&dont_jump);
}

void BaselineCompiler::VisitSwitchOnSmiNoFeedback() {
  BaselineAssembler::ScratchRegisterScope scratch_scope(&basm_);
  interpreter::JumpTableTargetOffsets offsets =
      iterator().GetJumpTableTargetOffsets();

  if (offsets.size() == 0) return;

  int case_value_base = (*offsets.begin()).case_value;

  std::unique_ptr<Label*[]> labels = std::make_unique<Label*[]>(offsets.size());
  for (const interpreter::JumpTableTargetOffset& offset : offsets) {
    labels[offset.case_value - case_value_base] =
        &EnsureLabels(offset.target_offset)->unlinked;
  }
  Register case_value = scratch_scope.AcquireScratch();
  __ SmiUntag(case_value, kInterpreterAccumulatorRegister);
  __ Switch(case_value, case_value_base, labels.get(), offsets.size());
}

void BaselineCompiler::VisitForInEnumerate() {
  CallBuiltin(Builtins::kForInEnumerate, RegisterOperand(0));
}

void BaselineCompiler::VisitForInPrepare() {
  StoreRegister(0, kInterpreterAccumulatorRegister);
  CallBuiltin(Builtins::kForInPrepare, kInterpreterAccumulatorRegister,
              IndexAsTagged(1), FeedbackVector());
  interpreter::Register first = iterator().GetRegisterOperand(0);
  interpreter::Register second(first.index() + 1);
  interpreter::Register third(first.index() + 2);
  __ StoreRegister(second, kReturnRegister0);
  __ StoreRegister(third, kReturnRegister1);
}

void BaselineCompiler::VisitForInContinue() {
  SelectBooleanConstant(kInterpreterAccumulatorRegister,
                        [&](Label* is_true, Label::Distance distance) {
                          LoadRegister(kInterpreterAccumulatorRegister, 0);
                          __ CompareTagged(
                              kInterpreterAccumulatorRegister,
                              __ RegisterFrameOperand(RegisterOperand(1)));
                          __ JumpIf(Condition::kNotEqual, is_true, distance);
                        });
}

void BaselineCompiler::VisitForInNext() {
  interpreter::Register cache_type, cache_array;
  std::tie(cache_type, cache_array) = iterator().GetRegisterPairOperand(2);
  CallBuiltin(Builtins::kForInNext,
              Index(3),            // vector slot
              RegisterOperand(0),  // object
              cache_array,         // cache array
              cache_type,          // cache type
              RegisterOperand(1),  // index
              FeedbackVector());   // feedback vector
}

void BaselineCompiler::VisitForInStep() {
  LoadRegister(kInterpreterAccumulatorRegister, 0);
  __ AddSmi(kInterpreterAccumulatorRegister, Smi::FromInt(1));
}

void BaselineCompiler::VisitSetPendingMessage() {
  BaselineAssembler::ScratchRegisterScope scratch_scope(&basm_);
  Register pending_message = scratch_scope.AcquireScratch();
  __ Move(pending_message,
          ExternalReference::address_of_pending_message_obj(isolate_));
  Register tmp = scratch_scope.AcquireScratch();
  __ Move(tmp, kInterpreterAccumulatorRegister);
  __ Move(kInterpreterAccumulatorRegister, MemOperand(pending_message, 0));
  __ Move(MemOperand(pending_message, 0), tmp);
}

void BaselineCompiler::VisitThrow() {
  CallRuntime(Runtime::kThrow, kInterpreterAccumulatorRegister);
  __ Trap();
}

void BaselineCompiler::VisitReThrow() {
  CallRuntime(Runtime::kReThrow, kInterpreterAccumulatorRegister);
  __ Trap();
}

void BaselineCompiler::VisitReturn() {
  __ RecordComment("[ Return");
  int profiling_weight = iterator().current_offset() +
                         iterator().current_bytecode_size_without_prefix();
  int parameter_count = bytecode_->parameter_count();

  // We must pop all arguments from the stack (including the receiver). This
  // number of arguments is given by max(1 + argc_reg, parameter_count).
  int parameter_count_without_receiver =
      parameter_count - 1;  // Exclude the receiver to simplify the
                            // computation. We'll account for it at the end.
  TailCallBuiltin(Builtins::kBaselineLeaveFrame,
                  parameter_count_without_receiver, -profiling_weight);
  __ RecordComment("]");
}

void BaselineCompiler::VisitThrowReferenceErrorIfHole() {
  Label done;
  __ JumpIfNotRoot(kInterpreterAccumulatorRegister, RootIndex::kTheHoleValue,
                   &done);
  CallRuntime(Runtime::kThrowAccessedUninitializedVariable, Constant<Name>(0));
  // Unreachable.
  __ Trap();
  __ Bind(&done);
}

void BaselineCompiler::VisitThrowSuperNotCalledIfHole() {
  Label done;
  __ JumpIfNotRoot(kInterpreterAccumulatorRegister, RootIndex::kTheHoleValue,
                   &done);
  CallRuntime(Runtime::kThrowSuperNotCalled);
  // Unreachable.
  __ Trap();
  __ Bind(&done);
}

void BaselineCompiler::VisitThrowSuperAlreadyCalledIfNotHole() {
  Label done;
  __ JumpIfRoot(kInterpreterAccumulatorRegister, RootIndex::kTheHoleValue,
                &done);
  CallRuntime(Runtime::kThrowSuperAlreadyCalledError);
  // Unreachable.
  __ Trap();
  __ Bind(&done);
}

void BaselineCompiler::VisitThrowIfNotSuperConstructor() {
  Label done;

  BaselineAssembler::ScratchRegisterScope scratch_scope(&basm_);
  Register reg = scratch_scope.AcquireScratch();
  LoadRegister(reg, 0);
  Register map_bit_field = scratch_scope.AcquireScratch();
  __ LoadMap(map_bit_field, reg);
  __ LoadByteField(map_bit_field, map_bit_field, Map::kBitFieldOffset);
  __ Test(map_bit_field, Map::Bits1::IsConstructorBit::kMask);
  __ JumpIf(Condition::kNotZero, &done, Label::kNear);

  CallRuntime(Runtime::kThrowNotSuperConstructor, reg, __ FunctionOperand());

  __ Bind(&done);
}

void BaselineCompiler::VisitSwitchOnGeneratorState() {
  BaselineAssembler::ScratchRegisterScope scratch_scope(&basm_);

  Label fallthrough;

  Register generator_object = scratch_scope.AcquireScratch();
  LoadRegister(generator_object, 0);
  __ JumpIfRoot(generator_object, RootIndex::kUndefinedValue, &fallthrough);

  Register continuation = scratch_scope.AcquireScratch();
  __ LoadTaggedAnyField(continuation, generator_object,
                        JSGeneratorObject::kContinuationOffset);
  __ StoreTaggedSignedField(
      generator_object, JSGeneratorObject::kContinuationOffset,
      Smi::FromInt(JSGeneratorObject::kGeneratorExecuting));

  Register context = scratch_scope.AcquireScratch();
  __ LoadTaggedAnyField(context, generator_object,
                        JSGeneratorObject::kContextOffset);
  __ StoreContext(context);

  interpreter::JumpTableTargetOffsets offsets =
      iterator().GetJumpTableTargetOffsets();

  if (0 < offsets.size()) {
    DCHECK_EQ(0, (*offsets.begin()).case_value);

    std::unique_ptr<Label*[]> labels =
        std::make_unique<Label*[]>(offsets.size());
    for (const interpreter::JumpTableTargetOffset& offset : offsets) {
      labels[offset.case_value] = &EnsureLabels(offset.target_offset)->unlinked;
    }
    __ SmiUntag(continuation);
    __ Switch(continuation, 0, labels.get(), offsets.size());
    // We should never fall through this switch.
    // TODO(v8:11429,leszeks): Maybe remove the fallthrough check in the Switch?
    __ Trap();
  }

  __ Bind(&fallthrough);
}

void BaselineCompiler::VisitSuspendGenerator() {
  DCHECK_EQ(iterator().GetRegisterOperand(1), interpreter::Register(0));
  BaselineAssembler::ScratchRegisterScope scratch_scope(&basm_);
  Register generator_object = scratch_scope.AcquireScratch();
  LoadRegister(generator_object, 0);
  {
    SaveAccumulatorScope accumulator_scope(&basm_);

    int bytecode_offset =
        BytecodeArray::kHeaderSize + iterator().current_offset();
    CallBuiltin(Builtins::kSuspendGeneratorBaseline, generator_object,
                static_cast<int>(Uint(3)),  // suspend_id
                bytecode_offset,
                static_cast<int>(RegisterCount(2)));  // register_count
  }
  VisitReturn();
}

void BaselineCompiler::VisitResumeGenerator() {
  DCHECK_EQ(iterator().GetRegisterOperand(1), interpreter::Register(0));
  BaselineAssembler::ScratchRegisterScope scratch_scope(&basm_);
  Register generator_object = scratch_scope.AcquireScratch();
  LoadRegister(generator_object, 0);
  CallBuiltin(Builtins::kResumeGeneratorBaseline, generator_object,
              static_cast<int>(RegisterCount(2)));  // register_count
}

void BaselineCompiler::VisitGetIterator() {
  CallBuiltin(Builtins::kGetIteratorBaseline,
              RegisterOperand(0),  // receiver
              IndexAsTagged(1),    // load_slot
              IndexAsTagged(2));   // call_slot
}

void BaselineCompiler::VisitDebugger() {
  SaveAccumulatorScope accumulator_scope(&basm_);
  CallBuiltin(Builtins::kHandleDebuggerStatement);
}

void BaselineCompiler::VisitIncBlockCounter() {
  SaveAccumulatorScope accumulator_scope(&basm_);
  CallBuiltin(Builtins::kIncBlockCounter, __ FunctionOperand(),
              IndexAsSmi(0));  // coverage array slot
}

void BaselineCompiler::VisitAbort() {
  CallRuntime(Runtime::kAbort, Smi::FromInt(Index(0)));
  __ Trap();
}

void BaselineCompiler::VisitWide() {
  // Consumed by the BytecodeArrayIterator.
  UNREACHABLE();
}

void BaselineCompiler::VisitExtraWide() {
  // Consumed by the BytecodeArrayIterator.
  UNREACHABLE();
}

void BaselineCompiler::VisitIllegal() {
  // Not emitted in valid bytecode.
  UNREACHABLE();
}
#define DEBUG_BREAK(Name, ...) \
  void BaselineCompiler::Visit##Name() { UNREACHABLE(); }
DEBUG_BREAK_BYTECODE_LIST(DEBUG_BREAK)
#undef DEBUG_BREAK

}  // namespace baseline
}  // namespace internal
}  // namespace v8

#endif
