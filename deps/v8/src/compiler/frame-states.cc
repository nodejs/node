// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/frame-states.h"

#include <optional>

#include "src/base/hashing.h"
#include "src/codegen/callable.h"
#include "src/codegen/interface-descriptors-inl.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/node.h"
#include "src/compiler/turbofan-graph.h"
#include "src/deoptimizer/translated-state.h"
#include "src/handles/handles-inl.h"
#include "src/objects/objects-inl.h"

#if V8_ENABLE_WEBASSEMBLY
#include "src/wasm/value-type.h"
#endif  // V8_ENABLE_WEBASSEMBLY

namespace v8 {
namespace internal {
namespace compiler {

size_t hash_value(OutputFrameStateCombine const& sc) {
  return base::hash_value(sc.parameter_);
}

std::ostream& operator<<(std::ostream& os, OutputFrameStateCombine const& sc) {
  if (sc.parameter_ == OutputFrameStateCombine::kInvalidIndex)
    return os << "Ignore";
  return os << "PokeAt(" << sc.parameter_ << ")";
}

bool operator==(FrameStateFunctionInfo const& lhs,
                FrameStateFunctionInfo const& rhs) {
#if V8_HOST_ARCH_X64
// If this static_assert fails, then you've probably added a new field to
// FrameStateFunctionInfo. Make sure to take it into account in this equality
// function, and update the static_assert.
#if V8_ENABLE_WEBASSEMBLY
  static_assert(sizeof(FrameStateFunctionInfo) == 40);
#else
  static_assert(sizeof(FrameStateFunctionInfo) == 32);
#endif
#endif

#if V8_ENABLE_WEBASSEMBLY
  if (lhs.wasm_liftoff_frame_size() != rhs.wasm_liftoff_frame_size() ||
      lhs.wasm_function_index() != rhs.wasm_function_index()) {
    return false;
  }
#endif

  return lhs.type() == rhs.type() &&
         lhs.parameter_count() == rhs.parameter_count() &&
         lhs.max_arguments() == rhs.max_arguments() &&
         lhs.local_count() == rhs.local_count() &&
         lhs.shared_info().equals(rhs.shared_info()) &&
         lhs.bytecode_array().equals(rhs.bytecode_array());
}

bool operator==(FrameStateInfo const& lhs, FrameStateInfo const& rhs) {
#if V8_HOST_ARCH_X64
  // If this static_assert fails, then you've probably added a new field to
  // FrameStateInfo. Make sure to take it into account in this equality
  // function, and update the static_assert.
  static_assert(sizeof(FrameStateInfo) == 24);
#endif

  return lhs.type() == rhs.type() && lhs.bailout_id() == rhs.bailout_id() &&
         lhs.state_combine() == rhs.state_combine() &&
         *lhs.function_info() == *rhs.function_info();
}

bool operator!=(FrameStateInfo const& lhs, FrameStateInfo const& rhs) {
  return !(lhs == rhs);
}

size_t hash_value(FrameStateInfo const& info) {
  return base::hash_combine(static_cast<int>(info.type()), info.bailout_id(),
                            info.state_combine());
}

std::ostream& operator<<(std::ostream& os, FrameStateType type) {
  switch (type) {
    case FrameStateType::kUnoptimizedFunction:
      os << "UNOPTIMIZED_FRAME";
      break;
    case FrameStateType::kInlinedExtraArguments:
      os << "INLINED_EXTRA_ARGUMENTS";
      break;
    case FrameStateType::kConstructCreateStub:
      os << "CONSTRUCT_CREATE_STUB";
      break;
    case FrameStateType::kConstructInvokeStub:
      os << "CONSTRUCT_INVOKE_STUB";
      break;
    case FrameStateType::kBuiltinContinuation:
      os << "BUILTIN_CONTINUATION_FRAME";
      break;
#if V8_ENABLE_WEBASSEMBLY
    case FrameStateType::kWasmInlinedIntoJS:
      os << "WASM_INLINED_INTO_JS_FRAME";
      break;
    case FrameStateType::kJSToWasmBuiltinContinuation:
      os << "JS_TO_WASM_BUILTIN_CONTINUATION_FRAME";
      break;
    case FrameStateType::kLiftoffFunction:
      os << "LIFTOFF_FRAME";
      break;
#endif  // V8_ENABLE_WEBASSEMBLY
    case FrameStateType::kJavaScriptBuiltinContinuation:
      os << "JAVASCRIPT_BUILTIN_CONTINUATION_FRAME";
      break;
    case FrameStateType::kJavaScriptBuiltinContinuationWithCatch:
      os << "JAVASCRIPT_BUILTIN_CONTINUATION_WITH_CATCH_FRAME";
      break;
  }
  return os;
}

std::ostream& operator<<(std::ostream& os, FrameStateInfo const& info) {
  os << info.type() << ", " << info.bailout_id() << ", "
     << info.state_combine();
  DirectHandle<SharedFunctionInfo> shared_info;
  if (info.shared_info().ToHandle(&shared_info)) {
    os << ", " << Brief(*shared_info);
  }
  return os;
}

// Lazy deopt points where the frame state is associated with a call get an
// additional parameter for the return result from the call. The return result
// is added by the deoptimizer and not explicitly specified in the frame state.
// Lazy deopt points which can catch exceptions further get an additional
// parameter, namely the exception thrown. The exception is also added by the
// deoptimizer.
uint8_t DeoptimizerParameterCountFor(ContinuationFrameStateMode mode) {
  switch (mode) {
    case ContinuationFrameStateMode::EAGER:
      return 0;
    case ContinuationFrameStateMode::LAZY:
      return 1;
    case ContinuationFrameStateMode::LAZY_WITH_CATCH:
      return 2;
  }
  UNREACHABLE();
}

namespace {

FrameState CreateBuiltinContinuationFrameStateCommon(
    JSGraph* jsgraph, FrameStateType frame_type, Builtin name, Node* closure,
    Node* context, Node* const* parameters, int parameter_count,
    Node* outer_frame_state,
    Handle<SharedFunctionInfo> shared = Handle<SharedFunctionInfo>(),
    const wasm::CanonicalSig* signature = nullptr) {
  TFGraph* const graph = jsgraph->graph();
  CommonOperatorBuilder* const common = jsgraph->common();

  const Operator* op_param =
      common->StateValues(parameter_count, SparseInputMask::Dense());
  Node* params_node = graph->NewNode(op_param, parameter_count, parameters);

  BytecodeOffset bailout_id = Builtins::GetContinuationBytecodeOffset(name);
#if V8_ENABLE_WEBASSEMBLY
  const FrameStateFunctionInfo* state_info =
      signature ? common->CreateJSToWasmFrameStateFunctionInfo(
                      frame_type, parameter_count, 0, shared, signature)
                : common->CreateFrameStateFunctionInfo(
                      frame_type, parameter_count, 0, 0, shared, {});
#else
  DCHECK_NULL(signature);
  const FrameStateFunctionInfo* state_info =
      common->CreateFrameStateFunctionInfo(frame_type, parameter_count, 0, 0,
                                           shared, {});
#endif  // V8_ENABLE_WEBASSEMBLY

  const Operator* op = common->FrameState(
      bailout_id, OutputFrameStateCombine::Ignore(), state_info);
  return FrameState(graph->NewNode(op, params_node, jsgraph->EmptyStateValues(),
                                   jsgraph->EmptyStateValues(), context,
                                   closure, outer_frame_state));
}

}  // namespace

FrameState CreateStubBuiltinContinuationFrameState(
    JSGraph* jsgraph, Builtin name, Node* context, Node* const* parameters,
    int parameter_count, Node* outer_frame_state,
    ContinuationFrameStateMode mode, const wasm::CanonicalSig* signature) {
#ifdef DEBUG
  // Verify that the parameter count matches the builtin's expected parameter
  // count.
  CallInterfaceDescriptor descriptor =
      Builtins::CallInterfaceDescriptorFor(name);

  int register_parameter_count = descriptor.GetRegisterParameterCount();
  int stack_parameter_count = descriptor.GetStackParameterCount();

  // An abort deopt should just cause a crash, it doesn't matter if the lazy
  // deopt passes in a few extra stack parameters.
  if (name == Builtin::kAbort) {
    // Just make sure we have enough parameters for the builtin.
    DCHECK_EQ(parameter_count,
              register_parameter_count + stack_parameter_count);
  } else {
    // Depending on {mode}, final parameters are added by the deoptimizer and
    // aren't explicitly passed in the frame state.
    int parameters_passed_by_deoptimizer = DeoptimizerParameterCountFor(mode);

    // Ensure the parameters added by the deoptimizer are passed on the stack.
    // This check prevents using TFS builtins as continuations while doing the
    // lazy deopt. Use TFC or TFJ builtin as a lazy deopt continuation which
    // would pass the result parameter on the stack.
    DCHECK_GE(stack_parameter_count, parameters_passed_by_deoptimizer);

    // The given parameters are the register parameters and stack parameters,
    // the context will be added by instruction selector during FrameState
    // translation.
    DCHECK_EQ(parameter_count, register_parameter_count +
                                   stack_parameter_count -
                                   parameters_passed_by_deoptimizer);
  }
#endif

  FrameStateType frame_state_type = FrameStateType::kBuiltinContinuation;
#if V8_ENABLE_WEBASSEMBLY
  if (name == Builtin::kJSToWasmLazyDeoptContinuation) {
    CHECK_NOT_NULL(signature);
    frame_state_type = FrameStateType::kJSToWasmBuiltinContinuation;
  }
#endif  // V8_ENABLE_WEBASSEMBLY

  return CreateBuiltinContinuationFrameStateCommon(
      jsgraph, frame_state_type, name, jsgraph->UndefinedConstant(), context,
      parameters, parameter_count, outer_frame_state,
      Handle<SharedFunctionInfo>(), signature);
}

#if V8_ENABLE_WEBASSEMBLY
FrameState CreateJSWasmCallBuiltinContinuationFrameState(
    JSGraph* jsgraph, Node* context, Node* outer_frame_state,
    const wasm::CanonicalSig* signature) {
  return CreateStubBuiltinContinuationFrameState(
      jsgraph, Builtin::kJSToWasmLazyDeoptContinuation, context, nullptr, 0,
      outer_frame_state, ContinuationFrameStateMode::LAZY, signature);
}
#endif  // V8_ENABLE_WEBASSEMBLY

FrameState CreateJavaScriptBuiltinContinuationFrameState(
    JSGraph* jsgraph, SharedFunctionInfoRef shared, Builtin name, Node* target,
    Node* context, Node* const* stack_parameters, int stack_parameter_count,
    Node* outer_frame_state, ContinuationFrameStateMode mode) {
  // Depending on {mode}, final parameters are added by the deoptimizer
  // and aren't explicitly passed in the frame state.
  DCHECK_EQ(Builtins::GetStackParameterCount(name),
            stack_parameter_count + DeoptimizerParameterCountFor(mode));

  Node* argc = jsgraph->ConstantNoHole(Builtins::GetStackParameterCount(name));
  Node* new_target = jsgraph->UndefinedConstant();

  constexpr int kFixedJSFrameRegisterParameters =
      JSTrampolineDescriptor::GetRegisterParameterCount();
  DCHECK_EQ(
      Builtins::CallInterfaceDescriptorFor(name).GetRegisterParameterCount(),
      kFixedJSFrameRegisterParameters);

  std::vector<Node*> actual_parameters;
  actual_parameters.reserve(stack_parameter_count +
                            kFixedJSFrameRegisterParameters);

  // Stack parameters first. They must be first because the receiver is expected
  // to be the second value in the translation when creating stack crawls
  // (e.g. Error.stack) of optimized JavaScript frames.
  static_assert(TranslatedFrame::kReceiverIsFirstParameterInJSFrames);
  actual_parameters.reserve(stack_parameter_count);
  for (int i = 0; i < stack_parameter_count; ++i) {
    actual_parameters.push_back(stack_parameters[i]);
  }

  // Register parameters follow stack parameters. The context will be added by
  // instruction selector during FrameState translation.
  actual_parameters.push_back(target);      // kJavaScriptCallTargetRegister
  actual_parameters.push_back(new_target);  // kJavaScriptCallNewTargetRegister
  actual_parameters.push_back(argc);        // kJavaScriptCallArgCountRegister
#ifdef V8_JS_LINKAGE_INCLUDES_DISPATCH_HANDLE
  // The dispatch handle isn't used by the continuation builtins.
  Node* handle = jsgraph->ConstantNoHole(kInvalidDispatchHandle.value());
  actual_parameters.push_back(handle);  // kJavaScriptDispatchHandleRegister
  static_assert(kFixedJSFrameRegisterParameters == 4);
#else
  static_assert(kFixedJSFrameRegisterParameters == 3);
#endif

  return CreateBuiltinContinuationFrameStateCommon(
      jsgraph,
      mode == ContinuationFrameStateMode::LAZY_WITH_CATCH
          ? FrameStateType::kJavaScriptBuiltinContinuationWithCatch
          : FrameStateType::kJavaScriptBuiltinContinuation,
      name, target, context, &actual_parameters[0],
      static_cast<int>(actual_parameters.size()), outer_frame_state,
      shared.object());
}

FrameState CreateGenericLazyDeoptContinuationFrameState(
    JSGraph* graph, SharedFunctionInfoRef shared, Node* target, Node* context,
    Node* receiver, Node* outer_frame_state) {
  Node* stack_parameters[]{receiver};
  const int stack_parameter_count = arraysize(stack_parameters);
  return CreateJavaScriptBuiltinContinuationFrameState(
      graph, shared, Builtin::kGenericLazyDeoptContinuation, target, context,
      stack_parameters, stack_parameter_count, outer_frame_state,
      ContinuationFrameStateMode::LAZY);
}

Node* CreateInlinedApiFunctionFrameState(JSGraph* graph,
                                         SharedFunctionInfoRef shared,
                                         Node* target, Node* context,
                                         Node* receiver,
                                         Node* outer_frame_state) {
  return outer_frame_state;
}

FrameState CloneFrameState(JSGraph* jsgraph, FrameState frame_state,
                           OutputFrameStateCombine changed_state_combine) {
  TFGraph* graph = jsgraph->graph();
  CommonOperatorBuilder* common = jsgraph->common();

  DCHECK_EQ(IrOpcode::kFrameState, frame_state->op()->opcode());

  const Operator* op = common->FrameState(
      frame_state.frame_state_info().bailout_id(), changed_state_combine,
      frame_state.frame_state_info().function_info());
  return FrameState(
      graph->NewNode(op, frame_state.parameters(), frame_state.locals(),
                     frame_state.stack(), frame_state.context(),
                     frame_state.function(), frame_state.outer_frame_state()));
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
