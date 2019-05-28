// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-lazy-gen.h"

#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/feedback-vector.h"
#include "src/globals.h"
#include "src/objects/shared-function-info.h"

namespace v8 {
namespace internal {

void LazyBuiltinsAssembler::GenerateTailCallToJSCode(
    TNode<Code> code, TNode<JSFunction> function) {
  TNode<Int32T> argc =
      UncheckedCast<Int32T>(Parameter(Descriptor::kActualArgumentsCount));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> new_target = CAST(Parameter(Descriptor::kNewTarget));

  TailCallJSCode(code, context, function, new_target, argc);
}

void LazyBuiltinsAssembler::GenerateTailCallToReturnedCode(
    Runtime::FunctionId function_id, TNode<JSFunction> function) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Code> code = CAST(CallRuntime(function_id, context, function));
  GenerateTailCallToJSCode(code, function);
}

void LazyBuiltinsAssembler::TailCallRuntimeIfMarkerEquals(
    TNode<Smi> marker, OptimizationMarker expected_marker,
    Runtime::FunctionId function_id, TNode<JSFunction> function) {
  Label no_match(this);
  GotoIfNot(SmiEqual(marker, SmiConstant(expected_marker)), &no_match);
  GenerateTailCallToReturnedCode(function_id, function);
  BIND(&no_match);
}

void LazyBuiltinsAssembler::MaybeTailCallOptimizedCodeSlot(
    TNode<JSFunction> function, TNode<FeedbackVector> feedback_vector) {
  Label fallthrough(this);

  TNode<MaybeObject> maybe_optimized_code_entry = LoadMaybeWeakObjectField(
      feedback_vector, FeedbackVector::kOptimizedCodeOffset);

  // Check if the code entry is a Smi. If yes, we interpret it as an
  // optimisation marker. Otherwise, interpret it as a weak reference to a code
  // object.
  Label optimized_code_slot_is_smi(this), optimized_code_slot_is_weak_ref(this);
  Branch(TaggedIsSmi(maybe_optimized_code_entry), &optimized_code_slot_is_smi,
         &optimized_code_slot_is_weak_ref);

  BIND(&optimized_code_slot_is_smi);
  {
    // Optimized code slot is a Smi optimization marker.
    TNode<Smi> marker = CAST(maybe_optimized_code_entry);

    // Fall through if no optimization trigger.
    GotoIf(SmiEqual(marker, SmiConstant(OptimizationMarker::kNone)),
           &fallthrough);

    // TODO(ishell): introduce Runtime::kHandleOptimizationMarker and check
    // all these marker values there.
    TailCallRuntimeIfMarkerEquals(marker,
                                  OptimizationMarker::kLogFirstExecution,
                                  Runtime::kFunctionFirstExecution, function);
    TailCallRuntimeIfMarkerEquals(marker, OptimizationMarker::kCompileOptimized,
                                  Runtime::kCompileOptimized_NotConcurrent,
                                  function);
    TailCallRuntimeIfMarkerEquals(
        marker, OptimizationMarker::kCompileOptimizedConcurrent,
        Runtime::kCompileOptimized_Concurrent, function);

    // Otherwise, the marker is InOptimizationQueue, so fall through hoping
    // that an interrupt will eventually update the slot with optimized code.
    CSA_ASSERT(this,
               SmiEqual(marker,
                        SmiConstant(OptimizationMarker::kInOptimizationQueue)));
    Goto(&fallthrough);
  }

  BIND(&optimized_code_slot_is_weak_ref);
  {
    // Optimized code slot is a weak reference.
    TNode<Code> optimized_code =
        CAST(GetHeapObjectAssumeWeak(maybe_optimized_code_entry, &fallthrough));

    // Check if the optimized code is marked for deopt. If it is, call the
    // runtime to clear it.
    Label found_deoptimized_code(this);
    TNode<CodeDataContainer> code_data_container =
        CAST(LoadObjectField(optimized_code, Code::kCodeDataContainerOffset));

    TNode<Int32T> code_kind_specific_flags = LoadObjectField<Int32T>(
        code_data_container, CodeDataContainer::kKindSpecificFlagsOffset);
    GotoIf(IsSetWord32<Code::MarkedForDeoptimizationField>(
               code_kind_specific_flags),
           &found_deoptimized_code);

    // Optimized code is good, get it into the closure and link the closure into
    // the optimized functions list, then tail call the optimized code.
    StoreObjectField(function, JSFunction::kCodeOffset, optimized_code);
    GenerateTailCallToJSCode(optimized_code, function);

    // Optimized code slot contains deoptimized code, evict it and re-enter the
    // closure's code.
    BIND(&found_deoptimized_code);
    GenerateTailCallToReturnedCode(Runtime::kEvictOptimizedCodeSlot, function);
  }

  // Fall-through if the optimized code cell is clear and there is no
  // optimization marker.
  BIND(&fallthrough);
}

void LazyBuiltinsAssembler::CompileLazy(TNode<JSFunction> function) {
  // First lookup code, maybe we don't need to compile!
  Label compile_function(this, Label::kDeferred);

  // Check the code object for the SFI. If SFI's code entry points to
  // CompileLazy, then we need to lazy compile regardless of the function or
  // feedback vector marker.
  TNode<SharedFunctionInfo> shared =
      CAST(LoadObjectField(function, JSFunction::kSharedFunctionInfoOffset));
  TNode<Code> sfi_code = GetSharedFunctionInfoCode(shared, &compile_function);

  TNode<HeapObject> feedback_cell_value = LoadFeedbackCellValue(function);

  // If feedback cell isn't initialized, compile function
  GotoIf(IsUndefined(feedback_cell_value), &compile_function);

  Label use_sfi_code(this);
  // If there is no feedback, don't check for optimized code.
  GotoIf(HasInstanceType(feedback_cell_value, CLOSURE_FEEDBACK_CELL_ARRAY_TYPE),
         &use_sfi_code);

  // If it isn't undefined or fixed array it must be a feedback vector.
  CSA_ASSERT(this, IsFeedbackVector(feedback_cell_value));

  // Is there an optimization marker or optimized code in the feedback vector?
  MaybeTailCallOptimizedCodeSlot(function, CAST(feedback_cell_value));
  Goto(&use_sfi_code);

  BIND(&use_sfi_code);
  // If not, install the SFI's code entry and jump to that.
  CSA_ASSERT(this, WordNotEqual(sfi_code, HeapConstant(BUILTIN_CODE(
                                              isolate(), CompileLazy))));
  StoreObjectField(function, JSFunction::kCodeOffset, sfi_code);
  GenerateTailCallToJSCode(sfi_code, function);

  BIND(&compile_function);
  GenerateTailCallToReturnedCode(Runtime::kCompileLazy, function);
}

TF_BUILTIN(CompileLazy, LazyBuiltinsAssembler) {
  TNode<JSFunction> function = CAST(Parameter(Descriptor::kTarget));

  CompileLazy(function);
}

TF_BUILTIN(CompileLazyDeoptimizedCode, LazyBuiltinsAssembler) {
  TNode<JSFunction> function = CAST(Parameter(Descriptor::kTarget));

  // Set the code slot inside the JSFunction to CompileLazy.
  TNode<Code> code = HeapConstant(BUILTIN_CODE(isolate(), CompileLazy));
  StoreObjectField(function, JSFunction::kCodeOffset, code);
  GenerateTailCallToJSCode(code, function);
}

}  // namespace internal
}  // namespace v8
