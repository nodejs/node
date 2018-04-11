// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/frame-states.h"

#include "src/base/functional.h"
#include "src/callable.h"
#include "src/compiler/graph.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/node.h"
#include "src/handles-inl.h"
#include "src/objects-inl.h"

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


bool operator==(FrameStateInfo const& lhs, FrameStateInfo const& rhs) {
  return lhs.type() == rhs.type() && lhs.bailout_id() == rhs.bailout_id() &&
         lhs.state_combine() == rhs.state_combine() &&
         lhs.function_info() == rhs.function_info();
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
    case FrameStateType::kInterpretedFunction:
      os << "INTERPRETED_FRAME";
      break;
    case FrameStateType::kArgumentsAdaptor:
      os << "ARGUMENTS_ADAPTOR";
      break;
    case FrameStateType::kConstructStub:
      os << "CONSTRUCT_STUB";
      break;
    case FrameStateType::kBuiltinContinuation:
      os << "BUILTIN_CONTINUATION_FRAME";
      break;
    case FrameStateType::kJavaScriptBuiltinContinuation:
      os << "JAVA_SCRIPT_BUILTIN_CONTINUATION_FRAME";
      break;
  }
  return os;
}


std::ostream& operator<<(std::ostream& os, FrameStateInfo const& info) {
  os << info.type() << ", " << info.bailout_id() << ", "
     << info.state_combine();
  Handle<SharedFunctionInfo> shared_info;
  if (info.shared_info().ToHandle(&shared_info)) {
    os << ", " << Brief(*shared_info);
  }
  return os;
}

namespace {

Node* CreateBuiltinContinuationFrameStateCommon(
    JSGraph* jsgraph, FrameStateType frame_type, Builtins::Name name,
    Node* closure, Node* context, Node** parameters, int parameter_count,
    Node* outer_frame_state,
    Handle<SharedFunctionInfo> shared = Handle<SharedFunctionInfo>()) {
  Isolate* const isolate = jsgraph->isolate();
  Graph* const graph = jsgraph->graph();
  CommonOperatorBuilder* const common = jsgraph->common();

  BailoutId bailout_id = Builtins::GetContinuationBailoutId(name);
  Callable callable = Builtins::CallableFor(isolate, name);

  const Operator* op_param =
      common->StateValues(parameter_count, SparseInputMask::Dense());
  Node* params_node = graph->NewNode(op_param, parameter_count, parameters);

  const FrameStateFunctionInfo* state_info =
      common->CreateFrameStateFunctionInfo(frame_type, parameter_count, 0,
                                           shared);
  const Operator* op = common->FrameState(
      bailout_id, OutputFrameStateCombine::Ignore(), state_info);

  Node* frame_state = graph->NewNode(
      op, params_node, jsgraph->EmptyStateValues(), jsgraph->EmptyStateValues(),
      context, closure, outer_frame_state);

  return frame_state;
}

}  // namespace

Node* CreateStubBuiltinContinuationFrameState(
    JSGraph* jsgraph, Builtins::Name name, Node* context,
    Node* const* parameters, int parameter_count, Node* outer_frame_state,
    ContinuationFrameStateMode mode) {
  Isolate* isolate = jsgraph->isolate();
  Callable callable = Builtins::CallableFor(isolate, name);
  CallInterfaceDescriptor descriptor = callable.descriptor();

  std::vector<Node*> actual_parameters;
  // Stack parameters first. If the deoptimization is LAZY, the final parameter
  // is added by the deoptimizer and isn't explicitly passed in the frame state.
  int stack_parameter_count =
      descriptor.GetRegisterParameterCount() -
      (mode == ContinuationFrameStateMode::LAZY ? 1 : 0);
  for (int i = 0; i < stack_parameter_count; ++i) {
    actual_parameters.push_back(
        parameters[descriptor.GetRegisterParameterCount() + i]);
  }
  // Register parameters follow, context will be added by instruction selector
  // during FrameState translation.
  for (int i = 0; i < descriptor.GetRegisterParameterCount(); ++i) {
    actual_parameters.push_back(parameters[i]);
  }

  return CreateBuiltinContinuationFrameStateCommon(
      jsgraph, FrameStateType::kBuiltinContinuation, name,
      jsgraph->UndefinedConstant(), context, actual_parameters.data(),
      static_cast<int>(actual_parameters.size()), outer_frame_state);
}

Node* CreateJavaScriptBuiltinContinuationFrameState(
    JSGraph* jsgraph, Handle<SharedFunctionInfo> shared, Builtins::Name name,
    Node* target, Node* context, Node* const* stack_parameters,
    int stack_parameter_count, Node* outer_frame_state,
    ContinuationFrameStateMode mode) {
  Isolate* const isolate = jsgraph->isolate();
  Callable const callable = Builtins::CallableFor(isolate, name);

  // Lazy deopt points where the frame state is assocated with a call get an
  // additional parameter for the return result from the call that's added by
  // the deoptimizer and not explicitly specified in the frame state. Check that
  // there is not a mismatch between the number of frame state parameters and
  // the stack parameters required by the builtin taking this into account.
  DCHECK_EQ(Builtins::GetStackParameterCount(name) + 1,  // add receiver
            stack_parameter_count +
                (mode == ContinuationFrameStateMode::EAGER ? 0 : 1));

  Node* argc =
      jsgraph->Constant(stack_parameter_count -
                        (mode == ContinuationFrameStateMode::EAGER ? 1 : 0));

  // Stack parameters first. They must be first because the receiver is expected
  // to be the second value in the translation when creating stack crawls
  // (e.g. Error.stack) of optimized JavaScript frames.
  std::vector<Node*> actual_parameters;
  for (int i = 0; i < stack_parameter_count; ++i) {
    actual_parameters.push_back(stack_parameters[i]);
  }

  // Register parameters follow stack paraemters. The context will be added by
  // instruction selector during FrameState translation.
  actual_parameters.push_back(target);
  actual_parameters.push_back(jsgraph->UndefinedConstant());
  actual_parameters.push_back(argc);

  return CreateBuiltinContinuationFrameStateCommon(
      jsgraph, FrameStateType::kJavaScriptBuiltinContinuation, name, target,
      context, &actual_parameters[0],
      static_cast<int>(actual_parameters.size()), outer_frame_state, shared);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
