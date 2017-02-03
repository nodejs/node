// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-inlining-heuristic.h"

#include "src/compilation-info.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/simplified-operator.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

#define TRACE(...)                                      \
  do {                                                  \
    if (FLAG_trace_turbo_inlining) PrintF(__VA_ARGS__); \
  } while (false)

namespace {

int CollectFunctions(Node* node, Handle<JSFunction>* functions,
                     int functions_size) {
  DCHECK_NE(0u, functions_size);
  HeapObjectMatcher m(node);
  if (m.HasValue() && m.Value()->IsJSFunction()) {
    functions[0] = Handle<JSFunction>::cast(m.Value());
    return 1;
  }
  if (m.IsPhi()) {
    int const value_input_count = m.node()->op()->ValueInputCount();
    if (value_input_count > functions_size) return 0;
    for (int n = 0; n < value_input_count; ++n) {
      HeapObjectMatcher m(node->InputAt(n));
      if (!m.HasValue() || !m.Value()->IsJSFunction()) return 0;
      functions[n] = Handle<JSFunction>::cast(m.Value());
    }
    return value_input_count;
  }
  return 0;
}

bool CanInlineFunction(Handle<JSFunction> function) {
  // Built-in functions are handled by the JSBuiltinReducer.
  if (function->shared()->HasBuiltinFunctionId()) return false;

  // Don't inline builtins.
  if (function->shared()->IsBuiltin()) return false;

  // Quick check on the size of the AST to avoid parsing large candidate.
  if (function->shared()->ast_node_count() > FLAG_max_inlined_nodes) {
    return false;
  }

  // Avoid inlining across the boundary of asm.js code.
  if (function->shared()->asm_function()) return false;
  return true;
}

}  // namespace

Reduction JSInliningHeuristic::Reduce(Node* node) {
  if (!IrOpcode::IsInlineeOpcode(node->opcode())) return NoChange();

  // Check if we already saw that {node} before, and if so, just skip it.
  if (seen_.find(node->id()) != seen_.end()) return NoChange();
  seen_.insert(node->id());

  // Check if the {node} is an appropriate candidate for inlining.
  Node* callee = node->InputAt(0);
  Candidate candidate;
  candidate.node = node;
  candidate.num_functions =
      CollectFunctions(callee, candidate.functions, kMaxCallPolymorphism);
  if (candidate.num_functions == 0) {
    return NoChange();
  } else if (candidate.num_functions > 1 && !FLAG_polymorphic_inlining) {
    TRACE(
        "Not considering call site #%d:%s, because polymorphic inlining "
        "is disabled\n",
        node->id(), node->op()->mnemonic());
    return NoChange();
  }

  // Functions marked with %SetForceInlineFlag are immediately inlined.
  bool can_inline = false, force_inline = true;
  for (int i = 0; i < candidate.num_functions; ++i) {
    Handle<JSFunction> function = candidate.functions[i];
    if (!function->shared()->force_inline()) {
      force_inline = false;
    }
    if (CanInlineFunction(function)) {
      can_inline = true;
    }
  }
  if (force_inline) return InlineCandidate(candidate);
  if (!can_inline) return NoChange();

  // Stop inlining once the maximum allowed level is reached.
  int level = 0;
  for (Node* frame_state = NodeProperties::GetFrameStateInput(node);
       frame_state->opcode() == IrOpcode::kFrameState;
       frame_state = NodeProperties::GetFrameStateInput(frame_state)) {
    FrameStateInfo const& frame_info = OpParameter<FrameStateInfo>(frame_state);
    if (FrameStateFunctionInfo::IsJSFunctionType(frame_info.type())) {
      if (++level > FLAG_max_inlining_levels) {
        TRACE(
            "Not considering call site #%d:%s, because inlining depth "
            "%d exceeds maximum allowed level %d\n",
            node->id(), node->op()->mnemonic(), level,
            FLAG_max_inlining_levels);
        return NoChange();
      }
    }
  }

  // Gather feedback on how often this call site has been hit before.
  if (node->opcode() == IrOpcode::kJSCallFunction) {
    CallFunctionParameters const p = CallFunctionParametersOf(node->op());
    candidate.frequency = p.frequency();
  } else {
    CallConstructParameters const p = CallConstructParametersOf(node->op());
    candidate.frequency = p.frequency();
  }

  // Handling of special inlining modes right away:
  //  - For restricted inlining: stop all handling at this point.
  //  - For stressing inlining: immediately handle all functions.
  switch (mode_) {
    case kRestrictedInlining:
      return NoChange();
    case kStressInlining:
      return InlineCandidate(candidate);
    case kGeneralInlining:
      break;
  }

  // In the general case we remember the candidate for later.
  candidates_.insert(candidate);
  return NoChange();
}

void JSInliningHeuristic::Finalize() {
  if (candidates_.empty()) return;  // Nothing to do without candidates.
  if (FLAG_trace_turbo_inlining) PrintCandidates();

  // We inline at most one candidate in every iteration of the fixpoint.
  // This is to ensure that we don't consume the full inlining budget
  // on things that aren't called very often.
  // TODO(bmeurer): Use std::priority_queue instead of std::set here.
  while (!candidates_.empty()) {
    if (cumulative_count_ > FLAG_max_inlined_nodes_cumulative) return;
    auto i = candidates_.begin();
    Candidate candidate = *i;
    candidates_.erase(i);
    // Make sure we don't try to inline dead candidate nodes.
    if (!candidate.node->IsDead()) {
      Reduction const reduction = InlineCandidate(candidate);
      if (reduction.Changed()) return;
    }
  }
}

Reduction JSInliningHeuristic::InlineCandidate(Candidate const& candidate) {
  int const num_calls = candidate.num_functions;
  Node* const node = candidate.node;
  if (num_calls == 1) {
    Handle<JSFunction> function = candidate.functions[0];
    Reduction const reduction = inliner_.ReduceJSCall(node, function);
    if (reduction.Changed()) {
      cumulative_count_ += function->shared()->ast_node_count();
    }
    return reduction;
  }

  // Expand the JSCallFunction/JSCallConstruct node to a subgraph first if
  // we have multiple known target functions.
  DCHECK_LT(1, num_calls);
  Node* calls[kMaxCallPolymorphism + 1];
  Node* if_successes[kMaxCallPolymorphism];
  Node* callee = NodeProperties::GetValueInput(node, 0);
  Node* fallthrough_control = NodeProperties::GetControlInput(node);

  // Setup the inputs for the cloned call nodes.
  int const input_count = node->InputCount();
  Node** inputs = graph()->zone()->NewArray<Node*>(input_count);
  for (int i = 0; i < input_count; ++i) {
    inputs[i] = node->InputAt(i);
  }

  // Create the appropriate control flow to dispatch to the cloned calls.
  for (int i = 0; i < num_calls; ++i) {
    Node* target = jsgraph()->HeapConstant(candidate.functions[i]);
    if (i != (num_calls - 1)) {
      Node* check =
          graph()->NewNode(simplified()->ReferenceEqual(), callee, target);
      Node* branch =
          graph()->NewNode(common()->Branch(), check, fallthrough_control);
      fallthrough_control = graph()->NewNode(common()->IfFalse(), branch);
      if_successes[i] = graph()->NewNode(common()->IfTrue(), branch);
    } else {
      if_successes[i] = fallthrough_control;
    }

    // The first input to the call is the actual target (which we specialize
    // to the known {target}); the last input is the control dependency.
    inputs[0] = target;
    inputs[input_count - 1] = if_successes[i];
    calls[i] = graph()->NewNode(node->op(), input_count, inputs);
    if_successes[i] = graph()->NewNode(common()->IfSuccess(), calls[i]);
  }

  // Check if we have an exception projection for the call {node}.
  Node* if_exception = nullptr;
  for (Edge const edge : node->use_edges()) {
    if (NodeProperties::IsControlEdge(edge) &&
        edge.from()->opcode() == IrOpcode::kIfException) {
      if_exception = edge.from();
      break;
    }
  }
  if (if_exception != nullptr) {
    // Morph the {if_exception} projection into a join.
    Node* if_exceptions[kMaxCallPolymorphism + 1];
    for (int i = 0; i < num_calls; ++i) {
      if_exceptions[i] =
          graph()->NewNode(common()->IfException(), calls[i], calls[i]);
    }
    Node* exception_control =
        graph()->NewNode(common()->Merge(num_calls), num_calls, if_exceptions);
    if_exceptions[num_calls] = exception_control;
    Node* exception_effect = graph()->NewNode(common()->EffectPhi(num_calls),
                                              num_calls + 1, if_exceptions);
    Node* exception_value = graph()->NewNode(
        common()->Phi(MachineRepresentation::kTagged, num_calls), num_calls + 1,
        if_exceptions);
    ReplaceWithValue(if_exception, exception_value, exception_effect,
                     exception_control);
  }

  // Morph the call site into the dispatched call sites.
  Node* control =
      graph()->NewNode(common()->Merge(num_calls), num_calls, if_successes);
  calls[num_calls] = control;
  Node* effect =
      graph()->NewNode(common()->EffectPhi(num_calls), num_calls + 1, calls);
  Node* value =
      graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, num_calls),
                       num_calls + 1, calls);
  ReplaceWithValue(node, value, effect, control);

  // Inline the individual, cloned call sites.
  for (int i = 0; i < num_calls; ++i) {
    Handle<JSFunction> function = candidate.functions[i];
    Node* node = calls[i];
    Reduction const reduction = inliner_.ReduceJSCall(node, function);
    if (reduction.Changed()) {
      cumulative_count_ += function->shared()->ast_node_count();
    }
  }

  return Replace(value);
}

bool JSInliningHeuristic::CandidateCompare::operator()(
    const Candidate& left, const Candidate& right) const {
  if (left.frequency > right.frequency) {
    return true;
  } else if (left.frequency < right.frequency) {
    return false;
  } else {
    return left.node->id() > right.node->id();
  }
}

void JSInliningHeuristic::PrintCandidates() {
  PrintF("Candidates for inlining (size=%zu):\n", candidates_.size());
  for (const Candidate& candidate : candidates_) {
    PrintF("  #%d:%s, frequency:%g\n", candidate.node->id(),
           candidate.node->op()->mnemonic(), candidate.frequency);
    for (int i = 0; i < candidate.num_functions; ++i) {
      Handle<JSFunction> function = candidate.functions[i];
      PrintF("  - size:%d, name: %s\n", function->shared()->ast_node_count(),
             function->shared()->DebugName()->ToCString().get());
    }
  }
}

Graph* JSInliningHeuristic::graph() const { return jsgraph()->graph(); }

CommonOperatorBuilder* JSInliningHeuristic::common() const {
  return jsgraph()->common();
}

SimplifiedOperatorBuilder* JSInliningHeuristic::simplified() const {
  return jsgraph()->simplified();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
