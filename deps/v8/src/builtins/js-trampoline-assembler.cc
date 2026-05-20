// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/js-trampoline-assembler.h"

#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/common/globals.h"
#include "src/objects/code-inl.h"
#include "src/objects/feedback-vector-inl.h"
#include "src/objects/shared-function-info.h"

namespace v8 {
namespace internal {

#include "src/codegen/define-code-stub-assembler-macros.inc"

void JSTrampolineAssembler::TailCallJSFunction(TNode<JSFunction> function) {
  auto argc = UncheckedParameter<Int32T>(Descriptor::kActualArgumentsCount);
  auto context = Parameter<Context>(Descriptor::kContext);
  auto new_target = Parameter<Object>(Descriptor::kNewTarget);
#ifdef V8_JS_LINKAGE_INCLUDES_DISPATCH_HANDLE
  auto dispatch_handle =
      UncheckedParameter<JSDispatchHandleT>(Descriptor::kDispatchHandle);
#else
  TNode<JSDispatchHandleT> dispatch_handle = LoadObjectField<JSDispatchHandleT>(
      function, JSFunction::kDispatchHandleOffset);
#endif
  CSA_DCHECK(this,
             Word32Equal(dispatch_handle,
                         LoadObjectField<JSDispatchHandleT>(
                             function, JSFunction::kDispatchHandleOffset)));

  // TailCallJSCode will load the code from the dispatch table.
  TailCallJSCode(context, function, new_target, argc, dispatch_handle);
}


void JSTrampolineAssembler::CompileLazy(TNode<JSFunction> function,
                                        TNode<Context> context) {
  // First lookup code, maybe we don't need to compile!
  Label compile_function(this, Label::kDeferred);

  // Check the code object for the SFI. If SFI's code entry points to
  // CompileLazy, then we need to lazy compile regardless of the function or
  // tiering state.
  TNode<SharedFunctionInfo> shared =
      CAST(LoadObjectField(function, JSFunction::kSharedFunctionInfoOffset));
  TVARIABLE(Uint16T, sfi_data_type);
  TNode<Code> sfi_code =
      GetSharedFunctionInfoCode(shared, &sfi_data_type, &compile_function);

  TNode<HeapObject> feedback_cell_value = LoadFeedbackCellValue(function);

  // If feedback cell isn't initialized, compile function
  GotoIf(IsUndefined(feedback_cell_value), &compile_function);

  CSA_DCHECK(this, SafeNotEqual(sfi_code, HeapConstantNoHole(BUILTIN_CODE(
                                              isolate(), CompileLazy))));
  USE(sfi_code);

  Label maybe_use_sfi_code(this);
  // If there is no feedback, don't check for optimized code.
  GotoIf(HasInstanceType(feedback_cell_value, CLOSURE_FEEDBACK_CELL_ARRAY_TYPE),
         &maybe_use_sfi_code);

  // If it isn't undefined or fixed array it must be a feedback vector.
  CSA_DCHECK(this, IsFeedbackVector(feedback_cell_value));

  Goto(&maybe_use_sfi_code);

  // At this point we have a candidate InstructionStream object. It's *not* a
  // cached optimized InstructionStream object (we'd have tail-called it above).
  // A usual case would be the InterpreterEntryTrampoline to start executing
  // existing bytecode.
  BIND(&maybe_use_sfi_code);
  // In the leaptiering case, we now simply install the code of the SFI on the
  // function's dispatch table entry and call it. Installing the code is
  // necessary as the dispatch table entry may still contain the CompileLazy
  // builtin at this point (we can only update dispatch table code from C++).
  CallRuntime(Runtime::kInstallSFICode, context, function);
  TailCallJSFunction(function);

  BIND(&compile_function);
  CallRuntime(Runtime::kCompileLazy, context, function);
  TailCallJSFunction(function);
}

TF_BUILTIN(CompileLazy, JSTrampolineAssembler) {
  auto function = Parameter<JSFunction>(Descriptor::kTarget);
  auto context = Parameter<Context>(Descriptor::kContext);

  CompileLazy(function, context);
}


template <typename Function>
void JSTrampolineAssembler::TieringBuiltinImpl(const Function& Impl) {
  auto function = Parameter<JSFunction>(Descriptor::kTarget);
  auto context = Parameter<Context>(Descriptor::kContext);
  auto argc = UncheckedParameter<Int32T>(Descriptor::kActualArgumentsCount);
  auto new_target = Parameter<Object>(Descriptor::kNewTarget);

#ifdef V8_JS_LINKAGE_INCLUDES_DISPATCH_HANDLE
  auto dispatch_handle =
      UncheckedParameter<JSDispatchHandleT>(Descriptor::kDispatchHandle);
#else
  CHECK(!V8_ENABLE_SANDBOX_BOOL);
  auto dispatch_handle = LoadObjectField<JSDispatchHandleT>(
      function, JSFunction::kDispatchHandleOffset);
#endif

  // Apply the actual tiering. This function must uninstall the tiering builtin.
  Impl(context, function);

  // The dispatch handle of the function shouldn't change.
  CSA_DCHECK(this,
             Word32Equal(dispatch_handle,
                         LoadObjectField<JSDispatchHandleT>(
                             function, JSFunction::kDispatchHandleOffset)));

  // TailCallJSCode will load the code from the dispatch table to guarantee
  // that the signature of the code matches with the number of arguments
  // passed when calling into this trampoline.
  TailCallJSCode(context, function, new_target, argc, dispatch_handle);
}

TF_BUILTIN(FunctionLogNextExecution, JSTrampolineAssembler) {
  TieringBuiltinImpl([&](TNode<Context> context, TNode<JSFunction> function) {
    CallRuntime(Runtime::kFunctionLogNextExecution, context, function);
  });
}

TF_BUILTIN(StartMaglevOptimizeJob, JSTrampolineAssembler) {
  TieringBuiltinImpl([&](TNode<Context> context, TNode<JSFunction> function) {
    CallRuntime(Runtime::kStartMaglevOptimizeJob, context, function);
  });
}

TF_BUILTIN(StartTurbofanOptimizeJob, JSTrampolineAssembler) {
  TieringBuiltinImpl([&](TNode<Context> context, TNode<JSFunction> function) {
    CallRuntime(Runtime::kStartTurbofanOptimizeJob, context, function);
  });
}

TF_BUILTIN(OptimizeMaglevEager, JSTrampolineAssembler) {
  TieringBuiltinImpl([&](TNode<Context> context, TNode<JSFunction> function) {
    CallRuntime(Runtime::kOptimizeMaglevEager, context, function);
  });
}

TF_BUILTIN(OptimizeTurbofanEager, JSTrampolineAssembler) {
  TieringBuiltinImpl([&](TNode<Context> context, TNode<JSFunction> function) {
    CallRuntime(Runtime::kOptimizeTurbofanEager, context, function);
  });
}

TF_BUILTIN(MarkLazyDeoptimized, JSTrampolineAssembler) {
  TieringBuiltinImpl([&](TNode<Context> context, TNode<JSFunction> function) {
    CallRuntime(Runtime::kMarkLazyDeoptimized, context, function,
                /* reoptimize */ SmiConstant(false));
  });
}

TF_BUILTIN(MarkReoptimizeLazyDeoptimized, JSTrampolineAssembler) {
  TieringBuiltinImpl([&](TNode<Context> context, TNode<JSFunction> function) {
    CallRuntime(Runtime::kMarkLazyDeoptimized, context, function,
                /* reoptimize */ SmiConstant(true));
  });
}


#include "src/codegen/undef-code-stub-assembler-macros.inc"

}  // namespace internal
}  // namespace v8
