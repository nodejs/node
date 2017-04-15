// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-inlining.h"

#include "src/ast/ast.h"
#include "src/compilation-info.h"
#include "src/compiler.h"
#include "src/compiler/all-nodes.h"
#include "src/compiler/bytecode-graph-builder.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/compiler-source-position-table.h"
#include "src/compiler/graph-reducer.h"
#include "src/compiler/js-operator.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/operator-properties.h"
#include "src/compiler/simplified-operator.h"
#include "src/isolate-inl.h"
#include "src/parsing/parse-info.h"

namespace v8 {
namespace internal {
namespace compiler {

#define TRACE(...)                                      \
  do {                                                  \
    if (FLAG_trace_turbo_inlining) PrintF(__VA_ARGS__); \
  } while (false)


// Provides convenience accessors for the common layout of nodes having either
// the {JSCallFunction} or the {JSCallConstruct} operator.
class JSCallAccessor {
 public:
  explicit JSCallAccessor(Node* call) : call_(call) {
    DCHECK(call->opcode() == IrOpcode::kJSCallFunction ||
           call->opcode() == IrOpcode::kJSCallConstruct);
  }

  Node* target() {
    // Both, {JSCallFunction} and {JSCallConstruct}, have same layout here.
    return call_->InputAt(0);
  }

  Node* receiver() {
    DCHECK_EQ(IrOpcode::kJSCallFunction, call_->opcode());
    return call_->InputAt(1);
  }

  Node* new_target() {
    DCHECK_EQ(IrOpcode::kJSCallConstruct, call_->opcode());
    return call_->InputAt(formal_arguments() + 1);
  }

  Node* frame_state() {
    // Both, {JSCallFunction} and {JSCallConstruct}, have frame state.
    return NodeProperties::GetFrameStateInput(call_);
  }

  int formal_arguments() {
    // Both, {JSCallFunction} and {JSCallConstruct}, have two extra inputs:
    //  - JSCallConstruct: Includes target function and new target.
    //  - JSCallFunction: Includes target function and receiver.
    return call_->op()->ValueInputCount() - 2;
  }

  float frequency() const {
    return (call_->opcode() == IrOpcode::kJSCallFunction)
               ? CallFunctionParametersOf(call_->op()).frequency()
               : CallConstructParametersOf(call_->op()).frequency();
  }

 private:
  Node* call_;
};

Reduction JSInliner::InlineCall(Node* call, Node* new_target, Node* context,
                                Node* frame_state, Node* start, Node* end,
                                Node* exception_target,
                                const NodeVector& uncaught_subcalls) {
  // The scheduler is smart enough to place our code; we just ensure {control}
  // becomes the control input of the start of the inlinee, and {effect} becomes
  // the effect input of the start of the inlinee.
  Node* control = NodeProperties::GetControlInput(call);
  Node* effect = NodeProperties::GetEffectInput(call);

  int const inlinee_new_target_index =
      static_cast<int>(start->op()->ValueOutputCount()) - 3;
  int const inlinee_arity_index =
      static_cast<int>(start->op()->ValueOutputCount()) - 2;
  int const inlinee_context_index =
      static_cast<int>(start->op()->ValueOutputCount()) - 1;

  // {inliner_inputs} counts JSFunction, receiver, arguments, but not
  // new target value, argument count, context, effect or control.
  int inliner_inputs = call->op()->ValueInputCount();
  // Iterate over all uses of the start node.
  for (Edge edge : start->use_edges()) {
    Node* use = edge.from();
    switch (use->opcode()) {
      case IrOpcode::kParameter: {
        int index = 1 + ParameterIndexOf(use->op());
        DCHECK_LE(index, inlinee_context_index);
        if (index < inliner_inputs && index < inlinee_new_target_index) {
          // There is an input from the call, and the index is a value
          // projection but not the context, so rewire the input.
          Replace(use, call->InputAt(index));
        } else if (index == inlinee_new_target_index) {
          // The projection is requesting the new target value.
          Replace(use, new_target);
        } else if (index == inlinee_arity_index) {
          // The projection is requesting the number of arguments.
          Replace(use, jsgraph()->Constant(inliner_inputs - 2));
        } else if (index == inlinee_context_index) {
          // The projection is requesting the inlinee function context.
          Replace(use, context);
        } else {
          // Call has fewer arguments than required, fill with undefined.
          Replace(use, jsgraph()->UndefinedConstant());
        }
        break;
      }
      default:
        if (NodeProperties::IsEffectEdge(edge)) {
          edge.UpdateTo(effect);
        } else if (NodeProperties::IsControlEdge(edge)) {
          edge.UpdateTo(control);
        } else if (NodeProperties::IsFrameStateEdge(edge)) {
          edge.UpdateTo(frame_state);
        } else {
          UNREACHABLE();
        }
        break;
    }
  }

  if (exception_target != nullptr) {
    // Link uncaught calls in the inlinee to {exception_target}
    int subcall_count = static_cast<int>(uncaught_subcalls.size());
    if (subcall_count > 0) {
      TRACE(
          "Inlinee contains %d calls without IfException; "
          "linking to existing IfException\n",
          subcall_count);
    }
    NodeVector on_exception_nodes(local_zone_);
    for (Node* subcall : uncaught_subcalls) {
      Node* on_exception =
          graph()->NewNode(common()->IfException(), subcall, subcall);
      on_exception_nodes.push_back(on_exception);
    }

    DCHECK_EQ(subcall_count, static_cast<int>(on_exception_nodes.size()));
    if (subcall_count > 0) {
      Node* control_output =
          graph()->NewNode(common()->Merge(subcall_count), subcall_count,
                           &on_exception_nodes.front());
      NodeVector values_effects(local_zone_);
      values_effects = on_exception_nodes;
      values_effects.push_back(control_output);
      Node* value_output = graph()->NewNode(
          common()->Phi(MachineRepresentation::kTagged, subcall_count),
          subcall_count + 1, &values_effects.front());
      Node* effect_output =
          graph()->NewNode(common()->EffectPhi(subcall_count),
                           subcall_count + 1, &values_effects.front());
      ReplaceWithValue(exception_target, value_output, effect_output,
                       control_output);
    } else {
      ReplaceWithValue(exception_target, exception_target, exception_target,
                       jsgraph()->Dead());
    }
  }

  NodeVector values(local_zone_);
  NodeVector effects(local_zone_);
  NodeVector controls(local_zone_);
  for (Node* const input : end->inputs()) {
    switch (input->opcode()) {
      case IrOpcode::kReturn:
        values.push_back(NodeProperties::GetValueInput(input, 1));
        effects.push_back(NodeProperties::GetEffectInput(input));
        controls.push_back(NodeProperties::GetControlInput(input));
        break;
      case IrOpcode::kDeoptimize:
      case IrOpcode::kTerminate:
      case IrOpcode::kThrow:
        NodeProperties::MergeControlToEnd(graph(), common(), input);
        Revisit(graph()->end());
        break;
      default:
        UNREACHABLE();
        break;
    }
  }
  DCHECK_EQ(values.size(), effects.size());
  DCHECK_EQ(values.size(), controls.size());

  // Depending on whether the inlinee produces a value, we either replace value
  // uses with said value or kill value uses if no value can be returned.
  if (values.size() > 0) {
    int const input_count = static_cast<int>(controls.size());
    Node* control_output = graph()->NewNode(common()->Merge(input_count),
                                            input_count, &controls.front());
    values.push_back(control_output);
    effects.push_back(control_output);
    Node* value_output = graph()->NewNode(
        common()->Phi(MachineRepresentation::kTagged, input_count),
        static_cast<int>(values.size()), &values.front());
    Node* effect_output =
        graph()->NewNode(common()->EffectPhi(input_count),
                         static_cast<int>(effects.size()), &effects.front());
    ReplaceWithValue(call, value_output, effect_output, control_output);
    return Changed(value_output);
  } else {
    ReplaceWithValue(call, call, call, jsgraph()->Dead());
    return Changed(call);
  }
}


Node* JSInliner::CreateArtificialFrameState(Node* node, Node* outer_frame_state,
                                            int parameter_count,
                                            FrameStateType frame_state_type,
                                            Handle<SharedFunctionInfo> shared) {
  const FrameStateFunctionInfo* state_info =
      common()->CreateFrameStateFunctionInfo(frame_state_type,
                                             parameter_count + 1, 0, shared);

  const Operator* op = common()->FrameState(
      BailoutId(-1), OutputFrameStateCombine::Ignore(), state_info);
  const Operator* op0 = common()->StateValues(0, SparseInputMask::Dense());
  Node* node0 = graph()->NewNode(op0);
  NodeVector params(local_zone_);
  for (int parameter = 0; parameter < parameter_count + 1; ++parameter) {
    params.push_back(node->InputAt(1 + parameter));
  }
  const Operator* op_param = common()->StateValues(
      static_cast<int>(params.size()), SparseInputMask::Dense());
  Node* params_node = graph()->NewNode(
      op_param, static_cast<int>(params.size()), &params.front());
  return graph()->NewNode(op, params_node, node0, node0,
                          jsgraph()->UndefinedConstant(), node->InputAt(0),
                          outer_frame_state);
}

Node* JSInliner::CreateTailCallerFrameState(Node* node, Node* frame_state) {
  FrameStateInfo const& frame_info = OpParameter<FrameStateInfo>(frame_state);
  Handle<SharedFunctionInfo> shared;
  frame_info.shared_info().ToHandle(&shared);

  Node* function = frame_state->InputAt(kFrameStateFunctionInput);

  // If we are inlining a tail call drop caller's frame state and an
  // arguments adaptor if it exists.
  frame_state = NodeProperties::GetFrameStateInput(frame_state);
  if (frame_state->opcode() == IrOpcode::kFrameState) {
    FrameStateInfo const& frame_info = OpParameter<FrameStateInfo>(frame_state);
    if (frame_info.type() == FrameStateType::kArgumentsAdaptor) {
      frame_state = NodeProperties::GetFrameStateInput(frame_state);
    }
  }

  const FrameStateFunctionInfo* state_info =
      common()->CreateFrameStateFunctionInfo(
          FrameStateType::kTailCallerFunction, 0, 0, shared);

  const Operator* op = common()->FrameState(
      BailoutId(-1), OutputFrameStateCombine::Ignore(), state_info);
  const Operator* op0 = common()->StateValues(0, SparseInputMask::Dense());
  Node* node0 = graph()->NewNode(op0);
  return graph()->NewNode(op, node0, node0, node0,
                          jsgraph()->UndefinedConstant(), function,
                          frame_state);
}

namespace {

// TODO(turbofan): Shall we move this to the NodeProperties? Or some (untyped)
// alias analyzer?
bool IsSame(Node* a, Node* b) {
  if (a == b) {
    return true;
  } else if (a->opcode() == IrOpcode::kCheckHeapObject) {
    return IsSame(a->InputAt(0), b);
  } else if (b->opcode() == IrOpcode::kCheckHeapObject) {
    return IsSame(a, b->InputAt(0));
  }
  return false;
}

// TODO(bmeurer): Unify this with the witness helper functions in the
// js-builtin-reducer.cc once we have a better understanding of the
// map tracking we want to do, and eventually changed the CheckMaps
// operator to carry map constants on the operator instead of inputs.
// I.e. if the CheckMaps has some kind of SmallMapSet as operator
// parameter, then this could be changed to call a generic
//
//   SmallMapSet NodeProperties::CollectMapWitness(receiver, effect)
//
// function, which either returns the map set from the CheckMaps or
// a singleton set from a StoreField.
bool NeedsConvertReceiver(Node* receiver, Node* effect) {
  for (Node* dominator = effect;;) {
    if (dominator->opcode() == IrOpcode::kCheckMaps &&
        IsSame(dominator->InputAt(0), receiver)) {
      // Check if all maps have the given {instance_type}.
      ZoneHandleSet<Map> const& maps =
          CheckMapsParametersOf(dominator->op()).maps();
      for (size_t i = 0; i < maps.size(); ++i) {
        if (!maps[i]->IsJSReceiverMap()) return true;
      }
      return false;
    }
    switch (dominator->opcode()) {
      case IrOpcode::kStoreField: {
        FieldAccess const& access = FieldAccessOf(dominator->op());
        if (access.base_is_tagged == kTaggedBase &&
            access.offset == HeapObject::kMapOffset) {
          return true;
        }
        break;
      }
      case IrOpcode::kStoreElement:
      case IrOpcode::kStoreTypedElement:
        break;
      default: {
        DCHECK_EQ(1, dominator->op()->EffectOutputCount());
        if (dominator->op()->EffectInputCount() != 1 ||
            !dominator->op()->HasProperty(Operator::kNoWrite)) {
          // Didn't find any appropriate CheckMaps node.
          return true;
        }
        break;
      }
    }
    dominator = NodeProperties::GetEffectInput(dominator);
  }
}

// TODO(mstarzinger,verwaest): Move this predicate onto SharedFunctionInfo?
bool NeedsImplicitReceiver(Handle<SharedFunctionInfo> shared_info) {
  DisallowHeapAllocation no_gc;
  Isolate* const isolate = shared_info->GetIsolate();
  Code* const construct_stub = shared_info->construct_stub();
  return construct_stub != *isolate->builtins()->JSBuiltinsConstructStub() &&
         construct_stub !=
             *isolate->builtins()->JSBuiltinsConstructStubForDerived() &&
         construct_stub != *isolate->builtins()->JSConstructStubApi();
}

bool IsNonConstructible(Handle<SharedFunctionInfo> shared_info) {
  DisallowHeapAllocation no_gc;
  Isolate* const isolate = shared_info->GetIsolate();
  Code* const construct_stub = shared_info->construct_stub();
  return construct_stub == *isolate->builtins()->ConstructedNonConstructable();
}

}  // namespace


Reduction JSInliner::Reduce(Node* node) {
  if (!IrOpcode::IsInlineeOpcode(node->opcode())) return NoChange();

  // This reducer can handle both normal function calls as well a constructor
  // calls whenever the target is a constant function object, as follows:
  //  - JSCallFunction(target:constant, receiver, args...)
  //  - JSCallConstruct(target:constant, args..., new.target)
  HeapObjectMatcher match(node->InputAt(0));
  if (!match.HasValue() || !match.Value()->IsJSFunction()) return NoChange();
  Handle<JSFunction> function = Handle<JSFunction>::cast(match.Value());

  return ReduceJSCall(node, function);
}

Reduction JSInliner::ReduceJSCall(Node* node, Handle<JSFunction> function) {
  DCHECK(IrOpcode::IsInlineeOpcode(node->opcode()));
  JSCallAccessor call(node);
  Handle<SharedFunctionInfo> shared_info(function->shared());

  // Inlining is only supported in the bytecode pipeline.
  if (!info_->is_optimizing_from_bytecode()) {
    TRACE("Inlining %s into %s is not supported in the deprecated pipeline\n",
          shared_info->DebugName()->ToCString().get(),
          info_->shared_info()->DebugName()->ToCString().get());
    return NoChange();
  }

  // Function must be inlineable.
  if (!shared_info->IsInlineable()) {
    TRACE("Not inlining %s into %s because callee is not inlineable\n",
          shared_info->DebugName()->ToCString().get(),
          info_->shared_info()->DebugName()->ToCString().get());
    return NoChange();
  }

  // Constructor must be constructable.
  if (node->opcode() == IrOpcode::kJSCallConstruct &&
      IsNonConstructible(shared_info)) {
    TRACE("Not inlining %s into %s because constructor is not constructable.\n",
          shared_info->DebugName()->ToCString().get(),
          info_->shared_info()->DebugName()->ToCString().get());
    return NoChange();
  }

  // Class constructors are callable, but [[Call]] will raise an exception.
  // See ES6 section 9.2.1 [[Call]] ( thisArgument, argumentsList ).
  if (node->opcode() == IrOpcode::kJSCallFunction &&
      IsClassConstructor(shared_info->kind())) {
    TRACE("Not inlining %s into %s because callee is a class constructor.\n",
          shared_info->DebugName()->ToCString().get(),
          info_->shared_info()->DebugName()->ToCString().get());
    return NoChange();
  }

  // Function contains break points.
  if (shared_info->HasDebugInfo()) {
    TRACE("Not inlining %s into %s because callee may contain break points\n",
          shared_info->DebugName()->ToCString().get(),
          info_->shared_info()->DebugName()->ToCString().get());
    return NoChange();
  }

  // Disallow cross native-context inlining for now. This means that all parts
  // of the resulting code will operate on the same global object.
  // This also prevents cross context leaks for asm.js code, where we could
  // inline functions from a different context and hold on to that context (and
  // closure) from the code object.
  // TODO(turbofan): We might want to revisit this restriction later when we
  // have a need for this, and we know how to model different native contexts
  // in the same graph in a compositional way.
  if (function->context()->native_context() !=
      info_->context()->native_context()) {
    TRACE("Not inlining %s into %s because of different native contexts\n",
          shared_info->DebugName()->ToCString().get(),
          info_->shared_info()->DebugName()->ToCString().get());
    return NoChange();
  }

  // TODO(turbofan): TranslatedState::GetAdaptedArguments() currently relies on
  // not inlining recursive functions. We might want to relax that at some
  // point.
  for (Node* frame_state = call.frame_state();
       frame_state->opcode() == IrOpcode::kFrameState;
       frame_state = frame_state->InputAt(kFrameStateOuterStateInput)) {
    FrameStateInfo const& frame_info = OpParameter<FrameStateInfo>(frame_state);
    Handle<SharedFunctionInfo> frame_shared_info;
    if (frame_info.shared_info().ToHandle(&frame_shared_info) &&
        *frame_shared_info == *shared_info) {
      TRACE("Not inlining %s into %s because call is recursive\n",
            shared_info->DebugName()->ToCString().get(),
            info_->shared_info()->DebugName()->ToCString().get());
      return NoChange();
    }
  }

  // Find the IfException node, if any.
  Node* exception_target = nullptr;
  for (Edge edge : node->use_edges()) {
    if (NodeProperties::IsControlEdge(edge) &&
        edge.from()->opcode() == IrOpcode::kIfException) {
      DCHECK_NULL(exception_target);
      exception_target = edge.from();
    }
  }

  NodeVector uncaught_subcalls(local_zone_);

  if (exception_target != nullptr) {
    if (!FLAG_inline_into_try) {
      TRACE(
          "Try block surrounds #%d:%s and --no-inline-into-try active, so not "
          "inlining %s into %s.\n",
          exception_target->id(), exception_target->op()->mnemonic(),
          shared_info->DebugName()->ToCString().get(),
          info_->shared_info()->DebugName()->ToCString().get());
      return NoChange();
    } else {
      TRACE(
          "Inlining %s into %s regardless of surrounding try-block to catcher "
          "#%d:%s\n",
          shared_info->DebugName()->ToCString().get(),
          info_->shared_info()->DebugName()->ToCString().get(),
          exception_target->id(), exception_target->op()->mnemonic());
    }
  }

  Zone zone(info_->isolate()->allocator(), ZONE_NAME);
  ParseInfo parse_info(&zone, shared_info);
  CompilationInfo info(&parse_info, Handle<JSFunction>::null());
  if (info_->is_deoptimization_enabled()) info.MarkAsDeoptimizationEnabled();
  info.MarkAsOptimizeFromBytecode();

  if (!Compiler::EnsureBytecode(&info)) {
    TRACE("Not inlining %s into %s because bytecode generation failed\n",
          shared_info->DebugName()->ToCString().get(),
          info_->shared_info()->DebugName()->ToCString().get());
    if (info_->isolate()->has_pending_exception()) {
      info_->isolate()->clear_pending_exception();
    }
    return NoChange();
  }

  // Remember that we inlined this function. This needs to be called right
  // after we ensure deoptimization support so that the code flusher
  // does not remove the code with the deoptimization support.
  int inlining_id = info_->AddInlinedFunction(
      shared_info, source_positions_->GetSourcePosition(node));

  // ----------------------------------------------------------------
  // After this point, we've made a decision to inline this function.
  // We shall not bailout from inlining if we got here.

  TRACE("Inlining %s into %s\n",
        shared_info->DebugName()->ToCString().get(),
        info_->shared_info()->DebugName()->ToCString().get());

  // If function was lazily compiled, its literals array may not yet be set up.
  JSFunction::EnsureLiterals(function);

  // Create the subgraph for the inlinee.
  Node* start;
  Node* end;
  {
    // Run the BytecodeGraphBuilder to create the subgraph.
    Graph::SubgraphScope scope(graph());
    BytecodeGraphBuilder graph_builder(
        &zone, shared_info, handle(function->feedback_vector()),
        BailoutId::None(), jsgraph(), call.frequency(), source_positions_,
        inlining_id);
    graph_builder.CreateGraph(false);

    // Extract the inlinee start/end nodes.
    start = graph()->start();
    end = graph()->end();
  }

  if (exception_target != nullptr) {
    // Find all uncaught 'calls' in the inlinee.
    AllNodes inlined_nodes(local_zone_, end, graph());
    for (Node* subnode : inlined_nodes.reachable) {
      // Every possibly throwing node with an IfSuccess should get an
      // IfException.
      if (subnode->op()->HasProperty(Operator::kNoThrow)) {
        continue;
      }
      bool hasIfException = false;
      for (Node* use : subnode->uses()) {
        if (use->opcode() == IrOpcode::kIfException) {
          hasIfException = true;
          break;
        }
      }
      if (!hasIfException) {
        DCHECK_EQ(2, subnode->op()->ControlOutputCount());
        uncaught_subcalls.push_back(subnode);
      }
    }
  }

  Node* frame_state = call.frame_state();
  Node* new_target = jsgraph()->UndefinedConstant();

  // Inline {JSCallConstruct} requires some additional magic.
  if (node->opcode() == IrOpcode::kJSCallConstruct) {
    // Insert nodes around the call that model the behavior required for a
    // constructor dispatch (allocate implicit receiver and check return value).
    // This models the behavior usually accomplished by our {JSConstructStub}.
    // Note that the context has to be the callers context (input to call node).
    Node* receiver = jsgraph()->TheHoleConstant();  // Implicit receiver.
    if (NeedsImplicitReceiver(shared_info)) {
      Node* frame_state_before = NodeProperties::FindFrameStateBefore(node);
      Node* effect = NodeProperties::GetEffectInput(node);
      Node* context = NodeProperties::GetContextInput(node);
      Node* create = graph()->NewNode(javascript()->Create(), call.target(),
                                      call.new_target(), context,
                                      frame_state_before, effect);
      NodeProperties::ReplaceEffectInput(node, create);
      // Insert a check of the return value to determine whether the return
      // value or the implicit receiver should be selected as a result of the
      // call.
      Node* check = graph()->NewNode(simplified()->ObjectIsReceiver(), node);
      Node* select =
          graph()->NewNode(common()->Select(MachineRepresentation::kTagged),
                           check, node, create);
      NodeProperties::ReplaceUses(node, select, node, node, node);
      // Fix-up inputs that have been mangled by the {ReplaceUses} call above.
      NodeProperties::ReplaceValueInput(select, node, 1);  // Fix-up input.
      NodeProperties::ReplaceValueInput(check, node, 0);   // Fix-up input.
      receiver = create;  // The implicit receiver.
    }

    // Swizzle the inputs of the {JSCallConstruct} node to look like inputs to a
    // normal {JSCallFunction} node so that the rest of the inlining machinery
    // behaves as if we were dealing with a regular function invocation.
    new_target = call.new_target();  // Retrieve new target value input.
    node->RemoveInput(call.formal_arguments() + 1);  // Drop new target.
    node->InsertInput(graph()->zone(), 1, receiver);

    // Insert a construct stub frame into the chain of frame states. This will
    // reconstruct the proper frame when deoptimizing within the constructor.
    frame_state = CreateArtificialFrameState(
        node, frame_state, call.formal_arguments(),
        FrameStateType::kConstructStub, info.shared_info());
  }

  // The inlinee specializes to the context from the JSFunction object.
  // TODO(turbofan): We might want to load the context from the JSFunction at
  // runtime in case we only know the SharedFunctionInfo once we have dynamic
  // type feedback in the compiler.
  Node* context = jsgraph()->Constant(handle(function->context()));

  // Insert a JSConvertReceiver node for sloppy callees. Note that the context
  // passed into this node has to be the callees context (loaded above). Note
  // that the frame state passed to the JSConvertReceiver must be the frame
  // state _before_ the call; it is not necessary to fiddle with the receiver
  // in that frame state tho, as the conversion of the receiver can be repeated
  // any number of times, it's not observable.
  if (node->opcode() == IrOpcode::kJSCallFunction &&
      is_sloppy(shared_info->language_mode()) && !shared_info->native()) {
    Node* effect = NodeProperties::GetEffectInput(node);
    if (NeedsConvertReceiver(call.receiver(), effect)) {
      const CallFunctionParameters& p = CallFunctionParametersOf(node->op());
      Node* frame_state_before = NodeProperties::FindFrameStateBefore(node);
      Node* convert = effect = graph()->NewNode(
          javascript()->ConvertReceiver(p.convert_mode()), call.receiver(),
          context, frame_state_before, effect, start);
      NodeProperties::ReplaceValueInput(node, convert, 1);
      NodeProperties::ReplaceEffectInput(node, effect);
    }
  }

  // If we are inlining a JS call at tail position then we have to pop current
  // frame state and its potential arguments adaptor frame state in order to
  // make the call stack be consistent with non-inlining case.
  // After that we add a tail caller frame state which lets deoptimizer handle
  // the case when the outermost function inlines a tail call (it should remove
  // potential arguments adaptor frame that belongs to outermost function when
  // deopt happens).
  if (node->opcode() == IrOpcode::kJSCallFunction) {
    const CallFunctionParameters& p = CallFunctionParametersOf(node->op());
    if (p.tail_call_mode() == TailCallMode::kAllow) {
      frame_state = CreateTailCallerFrameState(node, frame_state);
    }
  }

  // Insert argument adaptor frame if required. The callees formal parameter
  // count (i.e. value outputs of start node minus target, receiver, new target,
  // arguments count and context) have to match the number of arguments passed
  // to the call.
  int parameter_count = shared_info->internal_formal_parameter_count();
  DCHECK_EQ(parameter_count, start->op()->ValueOutputCount() - 5);
  if (call.formal_arguments() != parameter_count) {
    frame_state = CreateArtificialFrameState(
        node, frame_state, call.formal_arguments(),
        FrameStateType::kArgumentsAdaptor, shared_info);
  }

  return InlineCall(node, new_target, context, frame_state, start, end,
                    exception_target, uncaught_subcalls);
}

Graph* JSInliner::graph() const { return jsgraph()->graph(); }

JSOperatorBuilder* JSInliner::javascript() const {
  return jsgraph()->javascript();
}

CommonOperatorBuilder* JSInliner::common() const { return jsgraph()->common(); }

SimplifiedOperatorBuilder* JSInliner::simplified() const {
  return jsgraph()->simplified();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
