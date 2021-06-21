// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-lazy-gen.h"

#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/common/globals.h"
#include "src/objects/feedback-vector.h"
#include "src/objects/shared-function-info.h"

namespace v8 {
namespace internal {

void LazyBuiltinsAssembler::GenerateTailCallToJSCode(
    TNode<Code> code, TNode<JSFunction> function) {
  auto argc = UncheckedParameter<Int32T>(Descriptor::kActualArgumentsCount);
  auto context = Parameter<Context>(Descriptor::kContext);
  auto new_target = Parameter<Object>(Descriptor::kNewTarget);

  TailCallJSCode(code, context, function, new_target, argc);
}

void LazyBuiltinsAssembler::GenerateTailCallToReturnedCode(
    Runtime::FunctionId function_id, TNode<JSFunction> function) {
  auto context = Parameter<Context>(Descriptor::kContext);
  TNode<Code> code = CAST(CallRuntime(function_id, context, function));
  GenerateTailCallToJSCode(code, function);
}

void LazyBuiltinsAssembler::TailCallRuntimeIfMarkerEquals(
    TNode<Uint32T> marker, OptimizationMarker expected_marker,
    Runtime::FunctionId function_id, TNode<JSFunction> function) {
  Label no_match(this);
  GotoIfNot(Word32Equal(marker, Uint32Constant(expected_marker)), &no_match);
  GenerateTailCallToReturnedCode(function_id, function);
  BIND(&no_match);
}

void LazyBuiltinsAssembler::MaybeTailCallOptimizedCodeSlot(
    TNode<JSFunction> function, TNode<FeedbackVector> feedback_vector) {
  Label fallthrough(this), may_have_optimized_code(this);

  TNode<Uint32T> optimization_state =
      LoadObjectField<Uint32T>(feedback_vector, FeedbackVector::kFlagsOffset);

  // Fall through if no optimization trigger or optimized code.
  GotoIfNot(IsSetWord32(
                optimization_state,
                FeedbackVector::kHasOptimizedCodeOrCompileOptimizedMarkerMask),
            &fallthrough);

  GotoIfNot(IsSetWord32(
                optimization_state,
                FeedbackVector::kHasCompileOptimizedOrLogFirstExecutionMarker),
            &may_have_optimized_code);

  // TODO(ishell): introduce Runtime::kHandleOptimizationMarker and check
  // all these marker values there.
  TNode<Uint32T> marker =
      DecodeWord32<FeedbackVector::OptimizationMarkerBits>(optimization_state);
  TailCallRuntimeIfMarkerEquals(marker, OptimizationMarker::kLogFirstExecution,
                                Runtime::kFunctionFirstExecution, function);
  TailCallRuntimeIfMarkerEquals(marker, OptimizationMarker::kCompileOptimized,
                                Runtime::kCompileOptimized_NotConcurrent,
                                function);
  TailCallRuntimeIfMarkerEquals(
      marker, OptimizationMarker::kCompileOptimizedConcurrent,
      Runtime::kCompileOptimized_Concurrent, function);

  Unreachable();
  BIND(&may_have_optimized_code);
  {
    Label heal_optimized_code_slot(this);
    TNode<MaybeObject> maybe_optimized_code_entry = LoadMaybeWeakObjectField(
        feedback_vector, FeedbackVector::kMaybeOptimizedCodeOffset);
    // Optimized code slot is a weak reference.
    TNode<Code> optimized_code = CAST(GetHeapObjectAssumeWeak(
        maybe_optimized_code_entry, &heal_optimized_code_slot));

    // Check if the optimized code is marked for deopt. If it is, call the
    // runtime to clear it.
    TNode<CodeDataContainer> code_data_container =
        CAST(LoadObjectField(optimized_code, Code::kCodeDataContainerOffset));

    TNode<Int32T> code_kind_specific_flags = LoadObjectField<Int32T>(
        code_data_container, CodeDataContainer::kKindSpecificFlagsOffset);
    GotoIf(IsSetWord32<Code::MarkedForDeoptimizationField>(
               code_kind_specific_flags),
           &heal_optimized_code_slot);

    // Optimized code is good, get it into the closure and link the closure into
    // the optimized functions list, then tail call the optimized code.
    StoreObjectField(function, JSFunction::kCodeOffset, optimized_code);
    GenerateTailCallToJSCode(optimized_code, function);

    // Optimized code slot contains deoptimized code or code is cleared and
    // optimized code marker isn't updated. Evict the code, update the marker
    // and re-enter the closure's code.
    BIND(&heal_optimized_code_slot);
    GenerateTailCallToReturnedCode(Runtime::kHealOptimizedCodeSlot, function);
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
  TVARIABLE(Uint16T, sfi_data_type);
  TNode<Code> sfi_code =
      GetSharedFunctionInfoCode(shared, &sfi_data_type, &compile_function);

  TNode<HeapObject> feedback_cell_value = LoadFeedbackCellValue(function);

  // If feedback cell isn't initialized, compile function
  GotoIf(IsUndefined(feedback_cell_value), &compile_function);

  Label maybe_use_sfi_code(this);
  // If there is no feedback, don't check for optimized code.
  GotoIf(HasInstanceType(feedback_cell_value, CLOSURE_FEEDBACK_CELL_ARRAY_TYPE),
         &maybe_use_sfi_code);

  // If it isn't undefined or fixed array it must be a feedback vector.
  CSA_ASSERT(this, IsFeedbackVector(feedback_cell_value));

  // Is there an optimization marker or optimized code in the feedback vector?
  MaybeTailCallOptimizedCodeSlot(function, CAST(feedback_cell_value));
  Goto(&maybe_use_sfi_code);

  // At this point we have a candidate Code object. It's *not* a cached
  // optimized Code object (we'd have tail-called it above). A usual case would
  // be the InterpreterEntryTrampoline to start executing existing bytecode.
  BIND(&maybe_use_sfi_code);
  CSA_ASSERT(this, TaggedNotEqual(sfi_code, HeapConstant(BUILTIN_CODE(
                                                isolate(), CompileLazy))));
  StoreObjectField(function, JSFunction::kCodeOffset, sfi_code);

  Label tailcall_code(this);
  Label baseline(this);

  TVARIABLE(Code, code);

  // Check if we have baseline code.
  GotoIf(InstanceTypeEqual(sfi_data_type.value(), BASELINE_DATA_TYPE),
         &baseline);

  // Finally, check for presence of an NCI cached Code object - if an entry
  // possibly exists, call into runtime to query the cache.
  TNode<Uint8T> flags2 =
      LoadObjectField<Uint8T>(shared, SharedFunctionInfo::kFlags2Offset);
  TNode<BoolT> may_have_cached_code =
      IsSetWord32<SharedFunctionInfo::MayHaveCachedCodeBit>(flags2);
  code = Select<Code>(
      may_have_cached_code,
      [=]() {
        return CAST(CallRuntime(Runtime::kTryInstallNCICode,
                                Parameter<Context>(Descriptor::kContext),
                                function));
      },
      [=]() { return sfi_code; });
  Goto(&tailcall_code);

  BIND(&baseline);
  // Ensure we have a feedback vector.
  code = Select<Code>(
      IsFeedbackVector(feedback_cell_value), [=]() { return sfi_code; },
      [=]() {
        return CAST(CallRuntime(Runtime::kInstallBaselineCode,
                                Parameter<Context>(Descriptor::kContext),
                                function));
      });
  Goto(&tailcall_code);
  BIND(&tailcall_code);
  // Jump to the selected code entry.
  GenerateTailCallToJSCode(code.value(), function);

  BIND(&compile_function);
  GenerateTailCallToReturnedCode(Runtime::kCompileLazy, function);
}

TF_BUILTIN(CompileLazy, LazyBuiltinsAssembler) {
  auto function = Parameter<JSFunction>(Descriptor::kTarget);

  CompileLazy(function);
}

TF_BUILTIN(CompileLazyDeoptimizedCode, LazyBuiltinsAssembler) {
  auto function = Parameter<JSFunction>(Descriptor::kTarget);

  // Set the code slot inside the JSFunction to CompileLazy.
  TNode<Code> code = HeapConstant(BUILTIN_CODE(isolate(), CompileLazy));
  StoreObjectField(function, JSFunction::kCodeOffset, code);
  GenerateTailCallToJSCode(code, function);
}

}  // namespace internal
}  // namespace v8
