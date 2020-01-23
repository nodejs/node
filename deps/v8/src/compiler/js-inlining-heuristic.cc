// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-inlining-heuristic.h"

#include "src/codegen/optimized-compilation-info.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/compiler-source-position-table.h"
#include "src/compiler/js-heap-broker.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/simplified-operator.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

#define TRACE(...)                                      \
  do {                                                  \
    if (FLAG_trace_turbo_inlining) PrintF(__VA_ARGS__); \
  } while (false)

namespace {
bool IsSmall(BytecodeArrayRef const& bytecode) {
  return bytecode.length() <= FLAG_max_inlined_bytecode_size_small;
}

bool CanConsiderForInlining(JSHeapBroker* broker,
                            SharedFunctionInfoRef const& shared,
                            FeedbackVectorRef const& feedback_vector) {
  if (!shared.IsInlineable()) return false;
  DCHECK(shared.HasBytecodeArray());
  if (!shared.IsSerializedForCompilation(feedback_vector)) {
    TRACE_BROKER_MISSING(
        broker, "data for " << shared << " (not serialized for compilation)");
    return false;
  }
  return true;
}

bool CanConsiderForInlining(JSHeapBroker* broker,
                            JSFunctionRef const& function) {
  if (!function.has_feedback_vector()) return false;
  if (!function.serialized()) {
    TRACE_BROKER_MISSING(
        broker, "data for " << function << " (cannot consider for inlining)");
    return false;
  }
  return CanConsiderForInlining(broker, function.shared(),
                                function.feedback_vector());
}

}  // namespace

JSInliningHeuristic::Candidate JSInliningHeuristic::CollectFunctions(
    Node* node, int functions_size) {
  DCHECK_NE(0, functions_size);
  Node* callee = node->InputAt(0);
  Candidate out;
  out.node = node;

  HeapObjectMatcher m(callee);
  if (m.HasValue() && m.Ref(broker()).IsJSFunction()) {
    out.functions[0] = m.Ref(broker()).AsJSFunction();
    JSFunctionRef function = out.functions[0].value();
    if (CanConsiderForInlining(broker(), function)) {
      out.bytecode[0] = function.shared().GetBytecodeArray();
      out.num_functions = 1;
      return out;
    }
  }
  if (m.IsPhi()) {
    int const value_input_count = m.node()->op()->ValueInputCount();
    if (value_input_count > functions_size) {
      out.num_functions = 0;
      return out;
    }
    for (int n = 0; n < value_input_count; ++n) {
      HeapObjectMatcher m(callee->InputAt(n));
      if (!m.HasValue() || !m.Ref(broker()).IsJSFunction()) {
        out.num_functions = 0;
        return out;
      }

      out.functions[n] = m.Ref(broker()).AsJSFunction();
      JSFunctionRef function = out.functions[n].value();
      if (CanConsiderForInlining(broker(), function)) {
        out.bytecode[n] = function.shared().GetBytecodeArray();
      }
    }
    out.num_functions = value_input_count;
    return out;
  }
  if (m.IsJSCreateClosure()) {
    DCHECK(!out.functions[0].has_value());
    CreateClosureParameters const& p = CreateClosureParametersOf(m.op());
    FeedbackCellRef feedback_cell(broker(), p.feedback_cell());
    SharedFunctionInfoRef shared_info(broker(), p.shared_info());
    out.shared_info = shared_info;
    if (feedback_cell.value().IsFeedbackVector() &&
        CanConsiderForInlining(broker(), shared_info,
                               feedback_cell.value().AsFeedbackVector())) {
      out.bytecode[0] = shared_info.GetBytecodeArray();
    }
    out.num_functions = 1;
    return out;
  }
  out.num_functions = 0;
  return out;
}

Reduction JSInliningHeuristic::Reduce(Node* node) {
  DisallowHeapAccessIf no_heap_acess(FLAG_concurrent_inlining);

  if (!IrOpcode::IsInlineeOpcode(node->opcode())) return NoChange();

  if (total_inlined_bytecode_size_ >= FLAG_max_inlined_bytecode_size_absolute &&
      mode_ != kStressInlining) {
    return NoChange();
  }

  // Check if we already saw that {node} before, and if so, just skip it.
  if (seen_.find(node->id()) != seen_.end()) return NoChange();
  seen_.insert(node->id());

  // Check if the {node} is an appropriate candidate for inlining.
  Candidate candidate = CollectFunctions(node, kMaxCallPolymorphism);
  if (candidate.num_functions == 0) {
    return NoChange();
  } else if (candidate.num_functions > 1 && !FLAG_polymorphic_inlining) {
    TRACE(
        "Not considering call site #%d:%s, because polymorphic inlining "
        "is disabled\n",
        node->id(), node->op()->mnemonic());
    return NoChange();
  }

  bool can_inline_candidate = false, candidate_is_small = true;
  candidate.total_size = 0;
  Node* frame_state = NodeProperties::GetFrameStateInput(node);
  FrameStateInfo const& frame_info = FrameStateInfoOf(frame_state->op());
  Handle<SharedFunctionInfo> frame_shared_info;
  for (int i = 0; i < candidate.num_functions; ++i) {
    if (!candidate.bytecode[i].has_value()) {
      // Can't inline without bytecode.
      // TODO(neis): Should this even be a broker message?
      if (candidate.functions[i].has_value()) {
        TRACE_BROKER(broker(),
                     "Missing bytecode array trying to inline JSFunction "
                         << *candidate.functions[i]);
      } else {
        TRACE_BROKER(
            broker(),
            "Missing bytecode array trying to inline SharedFunctionInfo "
                << *candidate.shared_info);
      }
      // Those functions that don't have their bytecode serialized probably
      // don't have the SFI either, so we exit the loop early.
      candidate.can_inline_function[i] = false;
      continue;
    }

    SharedFunctionInfoRef shared = candidate.functions[i].has_value()
                                       ? candidate.functions[i].value().shared()
                                       : candidate.shared_info.value();
    candidate.can_inline_function[i] = candidate.bytecode[i].has_value();
    CHECK_IMPLIES(candidate.can_inline_function[i], shared.IsInlineable());
    // Do not allow direct recursion i.e. f() -> f(). We still allow indirect
    // recurion like f() -> g() -> f(). The indirect recursion is helpful in
    // cases where f() is a small dispatch function that calls the appropriate
    // function. In the case of direct recursion, we only have some static
    // information for the first level of inlining and it may not be that useful
    // to just inline one level in recursive calls. In some cases like tail
    // recursion we may benefit from recursive inlining, if we have additional
    // analysis that converts them to iterative implementations. Though it is
    // not obvious if such an anlysis is needed.
    if (frame_info.shared_info().ToHandle(&frame_shared_info) &&
        frame_shared_info.equals(shared.object())) {
      TRACE("Not considering call site #%d:%s, because of recursive inlining\n",
            node->id(), node->op()->mnemonic());
      candidate.can_inline_function[i] = false;
    }
    if (candidate.can_inline_function[i]) {
      can_inline_candidate = true;
      BytecodeArrayRef bytecode = candidate.bytecode[i].value();
      candidate.total_size += bytecode.length();
      candidate_is_small = candidate_is_small && IsSmall(bytecode);
    }
  }
  if (!can_inline_candidate) return NoChange();

  // Gather feedback on how often this call site has been hit before.
  if (node->opcode() == IrOpcode::kJSCall) {
    CallParameters const p = CallParametersOf(node->op());
    candidate.frequency = p.frequency();
  } else {
    ConstructParameters const p = ConstructParametersOf(node->op());
    candidate.frequency = p.frequency();
  }

  // Handling of special inlining modes right away:
  //  - For restricted inlining: stop all handling at this point.
  //  - For stressing inlining: immediately handle all functions.
  switch (mode_) {
    case kRestrictedInlining:
      return NoChange();
    case kStressInlining:
      return InlineCandidate(candidate, false);
    case kGeneralInlining:
      break;
  }

  // Don't consider a {candidate} whose frequency is below the
  // threshold, i.e. a call site that is only hit once every N
  // invocations of the caller.
  if (candidate.frequency.IsKnown() &&
      candidate.frequency.value() < FLAG_min_inlining_frequency) {
    return NoChange();
  }

  // Forcibly inline small functions here. In the case of polymorphic inlining
  // candidate_is_small is set only when all functions are small.
  if (candidate_is_small) {
    TRACE("Inlining small function(s) at call site #%d:%s\n", node->id(),
          node->op()->mnemonic());
    return InlineCandidate(candidate, true);
  }

  // In the general case we remember the candidate for later.
  candidates_.insert(candidate);
  return NoChange();
}

void JSInliningHeuristic::Finalize() {
  DisallowHeapAccessIf no_heap_acess(FLAG_concurrent_inlining);

  if (candidates_.empty()) return;  // Nothing to do without candidates.
  if (FLAG_trace_turbo_inlining) PrintCandidates();

  // We inline at most one candidate in every iteration of the fixpoint.
  // This is to ensure that we don't consume the full inlining budget
  // on things that aren't called very often.
  // TODO(bmeurer): Use std::priority_queue instead of std::set here.
  while (!candidates_.empty()) {
    auto i = candidates_.begin();
    Candidate candidate = *i;
    candidates_.erase(i);

    // Make sure we don't try to inline dead candidate nodes.
    if (candidate.node->IsDead()) {
      continue;
    }

    // Make sure we have some extra budget left, so that any small functions
    // exposed by this function would be given a chance to inline.
    double size_of_candidate =
        candidate.total_size * FLAG_reserve_inline_budget_scale_factor;
    int total_size =
        total_inlined_bytecode_size_ + static_cast<int>(size_of_candidate);
    if (total_size > FLAG_max_inlined_bytecode_size_cumulative) {
      // Try if any smaller functions are available to inline.
      continue;
    }

    Reduction const reduction = InlineCandidate(candidate, false);
    if (reduction.Changed()) return;
  }
}

namespace {

struct NodeAndIndex {
  Node* node;
  int index;
};

bool CollectStateValuesOwnedUses(Node* node, Node* state_values,
                                 NodeAndIndex* uses_buffer, size_t* use_count,
                                 size_t max_uses) {
  // Only accumulate states that are not shared with other users.
  if (state_values->UseCount() > 1) return true;
  for (int i = 0; i < state_values->InputCount(); i++) {
    Node* input = state_values->InputAt(i);
    if (input->opcode() == IrOpcode::kStateValues) {
      if (!CollectStateValuesOwnedUses(node, input, uses_buffer, use_count,
                                       max_uses)) {
        return false;
      }
    } else if (input == node) {
      if (*use_count >= max_uses) return false;
      uses_buffer[*use_count] = {state_values, i};
      (*use_count)++;
    }
  }
  return true;
}

}  // namespace

Node* JSInliningHeuristic::DuplicateStateValuesAndRename(Node* state_values,
                                                         Node* from, Node* to,
                                                         StateCloneMode mode) {
  // Only rename in states that are not shared with other users. This needs to
  // be in sync with the condition in {CollectStateValuesOwnedUses}.
  if (state_values->UseCount() > 1) return state_values;
  Node* copy = mode == kChangeInPlace ? state_values : nullptr;
  for (int i = 0; i < state_values->InputCount(); i++) {
    Node* input = state_values->InputAt(i);
    Node* processed;
    if (input->opcode() == IrOpcode::kStateValues) {
      processed = DuplicateStateValuesAndRename(input, from, to, mode);
    } else if (input == from) {
      processed = to;
    } else {
      processed = input;
    }
    if (processed != input) {
      if (!copy) {
        copy = graph()->CloneNode(state_values);
      }
      copy->ReplaceInput(i, processed);
    }
  }
  return copy ? copy : state_values;
}

namespace {

bool CollectFrameStateUniqueUses(Node* node, Node* frame_state,
                                 NodeAndIndex* uses_buffer, size_t* use_count,
                                 size_t max_uses) {
  // Only accumulate states that are not shared with other users.
  if (frame_state->UseCount() > 1) return true;
  if (frame_state->InputAt(kFrameStateStackInput) == node) {
    if (*use_count >= max_uses) return false;
    uses_buffer[*use_count] = {frame_state, kFrameStateStackInput};
    (*use_count)++;
  }
  if (!CollectStateValuesOwnedUses(node,
                                   frame_state->InputAt(kFrameStateLocalsInput),
                                   uses_buffer, use_count, max_uses)) {
    return false;
  }
  return true;
}

}  // namespace

Node* JSInliningHeuristic::DuplicateFrameStateAndRename(Node* frame_state,
                                                        Node* from, Node* to,
                                                        StateCloneMode mode) {
  // Only rename in states that are not shared with other users. This needs to
  // be in sync with the condition in {DuplicateFrameStateAndRename}.
  if (frame_state->UseCount() > 1) return frame_state;
  Node* copy = mode == kChangeInPlace ? frame_state : nullptr;
  if (frame_state->InputAt(kFrameStateStackInput) == from) {
    if (!copy) {
      copy = graph()->CloneNode(frame_state);
    }
    copy->ReplaceInput(kFrameStateStackInput, to);
  }
  Node* locals = frame_state->InputAt(kFrameStateLocalsInput);
  Node* new_locals = DuplicateStateValuesAndRename(locals, from, to, mode);
  if (new_locals != locals) {
    if (!copy) {
      copy = graph()->CloneNode(frame_state);
    }
    copy->ReplaceInput(kFrameStateLocalsInput, new_locals);
  }
  return copy ? copy : frame_state;
}

bool JSInliningHeuristic::TryReuseDispatch(Node* node, Node* callee,
                                           Node** if_successes, Node** calls,
                                           Node** inputs, int input_count) {
  // We will try to reuse the control flow branch created for computing
  // the {callee} target of the call. We only reuse the branch if there
  // is no side-effect between the call and the branch, and if the callee is
  // only used as the target (and possibly also in the related frame states).

  // We are trying to match the following pattern:
  //
  //         C1     C2
  //          .     .
  //          |     |
  //         Merge(merge)  <-----------------+
  //           ^    ^                        |
  //  V1  V2   |    |         E1  E2         |
  //   .  .    |    +----+     .  .          |
  //   |  |    |         |     |  |          |
  //  Phi(callee)      EffectPhi(effect_phi) |
  //     ^                    ^              |
  //     |                    |              |
  //     +----+               |              |
  //     |    |               |              |
  //     |   StateValues      |              |
  //     |       ^            |              |
  //     +----+  |            |              |
  //     |    |  |            |              |
  //     |    FrameState      |              |
  //     |           ^        |              |
  //     |           |        |          +---+
  //     |           |        |          |   |
  //     +----+     Checkpoint(checkpoint)   |
  //     |    |           ^                  |
  //     |    StateValues |    +-------------+
  //     |        |       |    |
  //     +-----+  |       |    |
  //     |     |  |       |    |
  //     |     FrameState |    |
  //     |             ^  |    |
  //     +-----------+ |  |    |
  //                  Call(node)
  //                   |
  //                   |
  //                   .
  //
  // The {callee} here is a phi that merges the possible call targets, {node}
  // is the actual call that we will try to duplicate and connect to the
  // control that comes into {merge}. There can be a {checkpoint} between
  // the call and the calle phi.
  //
  // The idea is to get rid of the merge, effect phi and phi, then duplicate
  // the call (with all the frame states and such), and connect the duplicated
  // calls and states directly to the inputs of the ex-phi, ex-effect-phi and
  // ex-merge. The tricky part is to make sure that there is no interference
  // from the outside. In particular, there should not be any unaccounted uses
  // of the  phi, effect-phi and merge because we will remove them from
  // the graph.
  //
  //     V1              E1   C1  V2   E2               C2
  //     .                .    .  .    .                .
  //     |                |    |  |    |                |
  //     +----+           |    |  +----+                |
  //     |    |           |    |  |    |                |
  //     |   StateValues  |    |  |   StateValues       |
  //     |       ^        |    |  |       ^             |
  //     +----+  |        |    |  +----+  |             |
  //     |    |  |        |    |  |    |  |             |
  //     |    FrameState  |    |  |    FrameState       |
  //     |           ^    |    |  |           ^         |
  //     |           |    |    |  |           |         |
  //     |           |    |    |  |           |         |
  //     +----+     Checkpoint |  +----+     Checkpoint |
  //     |    |           ^    |  |    |           ^    |
  //     |    StateValues |    |  |    StateValues |    |
  //     |        |       |    |  |        |       |    |
  //     +-----+  |       |    |  +-----+  |       |    |
  //     |     |  |       |    |  |     |  |       |    |
  //     |     FrameState |    |  |     FrameState |    |
  //     |              ^ |    |  |              ^ |    |
  //     +-------------+| |    |  +-------------+| |    |
  //                   Call----+                Call----+
  //                     |                       |
  //                     +-------+  +------------+
  //                             |  |
  //                             Merge
  //                             EffectPhi
  //                             Phi
  //                              |
  //                             ...

  // Bailout if the call is not polymorphic anymore (other reducers might
  // have replaced the callee phi with a constant).
  if (callee->opcode() != IrOpcode::kPhi) return false;
  int const num_calls = callee->op()->ValueInputCount();

  // If there is a control node between the callee computation
  // and the call, bail out.
  Node* merge = NodeProperties::GetControlInput(callee);
  if (NodeProperties::GetControlInput(node) != merge) return false;

  // If there is a non-checkpoint effect node between the callee computation
  // and the call, bail out. We will drop any checkpoint between the call and
  // the callee phi because the callee computation should have its own
  // checkpoint that the call can fall back to.
  Node* checkpoint = nullptr;
  Node* effect = NodeProperties::GetEffectInput(node);
  if (effect->opcode() == IrOpcode::kCheckpoint) {
    checkpoint = effect;
    if (NodeProperties::GetControlInput(checkpoint) != merge) return false;
    effect = NodeProperties::GetEffectInput(effect);
  }
  if (effect->opcode() != IrOpcode::kEffectPhi) return false;
  if (NodeProperties::GetControlInput(effect) != merge) return false;
  Node* effect_phi = effect;

  // The effect phi, the callee, the call and the checkpoint must be the only
  // users of the merge.
  for (Node* merge_use : merge->uses()) {
    if (merge_use != effect_phi && merge_use != callee && merge_use != node &&
        merge_use != checkpoint) {
      return false;
    }
  }

  // The effect phi must be only used by the checkpoint or the call.
  for (Node* effect_phi_use : effect_phi->uses()) {
    if (effect_phi_use != node && effect_phi_use != checkpoint) return false;
  }

  // We must replace the callee phi with the appropriate constant in
  // the entire subgraph reachable by inputs from the call (terminating
  // at phis and merges). Since we do not want to walk (and later duplicate)
  // the subgraph here, we limit the possible uses to this set:
  //
  // 1. In the call (as a target).
  // 2. The checkpoint between the call and the callee computation merge.
  // 3. The lazy deoptimization frame state.
  //
  // This corresponds to the most common pattern, where the function is
  // called with only local variables or constants as arguments.
  //
  // To check the uses, we first collect all the occurrences of callee in 1, 2
  // and 3, and then we check that all uses of callee are in the collected
  // occurrences. If there is an unaccounted use, we do not try to rewire
  // the control flow.
  //
  // Note: With CFG, this would be much easier and more robust - we would just
  // duplicate all the nodes between the merge and the call, replacing all
  // occurrences of the {callee} phi with the appropriate constant.

  // First compute the set of uses that are only reachable from 2 and 3.
  const size_t kMaxUses = 8;
  NodeAndIndex replaceable_uses[kMaxUses];
  size_t replaceable_uses_count = 0;

  // Collect the uses to check case 2.
  Node* checkpoint_state = nullptr;
  if (checkpoint) {
    checkpoint_state = checkpoint->InputAt(0);
    if (!CollectFrameStateUniqueUses(callee, checkpoint_state, replaceable_uses,
                                     &replaceable_uses_count, kMaxUses)) {
      return false;
    }
  }

  // Collect the uses to check case 3.
  Node* frame_state = NodeProperties::GetFrameStateInput(node);
  if (!CollectFrameStateUniqueUses(callee, frame_state, replaceable_uses,
                                   &replaceable_uses_count, kMaxUses)) {
    return false;
  }

  // Bail out if there is a use of {callee} that is not reachable from 1, 2
  // and 3.
  for (Edge edge : callee->use_edges()) {
    // Case 1 (use by the call as a target).
    if (edge.from() == node && edge.index() == 0) continue;
    // Case 2 and 3 - used in checkpoint and/or lazy deopt frame states.
    bool found = false;
    for (size_t i = 0; i < replaceable_uses_count; i++) {
      if (replaceable_uses[i].node == edge.from() &&
          replaceable_uses[i].index == edge.index()) {
        found = true;
        break;
      }
    }
    if (!found) return false;
  }

  // Clone the call and the framestate, including the uniquely reachable
  // state values, making sure that we replace the phi with the constant.
  for (int i = 0; i < num_calls; ++i) {
    // Clone the calls for each branch.
    // We need to specialize the calls to the correct target, effect, and
    // control. We also need to duplicate the checkpoint and the lazy
    // frame state, and change all the uses of the callee to the constant
    // callee.
    Node* target = callee->InputAt(i);
    Node* effect = effect_phi->InputAt(i);
    Node* control = merge->InputAt(i);

    if (checkpoint) {
      // Duplicate the checkpoint.
      Node* new_checkpoint_state = DuplicateFrameStateAndRename(
          checkpoint_state, callee, target,
          (i == num_calls - 1) ? kChangeInPlace : kCloneState);
      effect = graph()->NewNode(checkpoint->op(), new_checkpoint_state, effect,
                                control);
    }

    // Duplicate the call.
    Node* new_lazy_frame_state = DuplicateFrameStateAndRename(
        frame_state, callee, target,
        (i == num_calls - 1) ? kChangeInPlace : kCloneState);
    inputs[0] = target;
    inputs[input_count - 3] = new_lazy_frame_state;
    inputs[input_count - 2] = effect;
    inputs[input_count - 1] = control;
    calls[i] = if_successes[i] =
        graph()->NewNode(node->op(), input_count, inputs);
  }

  // Mark the control inputs dead, so that we can kill the merge.
  node->ReplaceInput(input_count - 1, jsgraph()->Dead());
  callee->ReplaceInput(num_calls, jsgraph()->Dead());
  effect_phi->ReplaceInput(num_calls, jsgraph()->Dead());
  if (checkpoint) {
    checkpoint->ReplaceInput(2, jsgraph()->Dead());
  }

  merge->Kill();
  return true;
}

void JSInliningHeuristic::CreateOrReuseDispatch(Node* node, Node* callee,
                                                Candidate const& candidate,
                                                Node** if_successes,
                                                Node** calls, Node** inputs,
                                                int input_count) {
  SourcePositionTable::Scope position(
      source_positions_, source_positions_->GetSourcePosition(node));
  if (TryReuseDispatch(node, callee, if_successes, calls, inputs,
                       input_count)) {
    return;
  }

  Node* fallthrough_control = NodeProperties::GetControlInput(node);
  int const num_calls = candidate.num_functions;

  // Create the appropriate control flow to dispatch to the cloned calls.
  for (int i = 0; i < num_calls; ++i) {
    // TODO(2206): Make comparison be based on underlying SharedFunctionInfo
    // instead of the target JSFunction reference directly.
    Node* target = jsgraph()->Constant(candidate.functions[i].value());
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

    // Clone the calls for each branch.
    // The first input to the call is the actual target (which we specialize
    // to the known {target}); the last input is the control dependency.
    // We also specialize the new.target of JSConstruct {node}s if it refers
    // to the same node as the {node}'s target input, so that we can later
    // properly inline the JSCreate operations.
    if (node->opcode() == IrOpcode::kJSConstruct && inputs[0] == inputs[1]) {
      inputs[1] = target;
    }
    inputs[0] = target;
    inputs[input_count - 1] = if_successes[i];
    calls[i] = if_successes[i] =
        graph()->NewNode(node->op(), input_count, inputs);
  }
}

Reduction JSInliningHeuristic::InlineCandidate(Candidate const& candidate,
                                               bool small_function) {
  int const num_calls = candidate.num_functions;
  Node* const node = candidate.node;
  if (num_calls == 1) {
    Reduction const reduction = inliner_.ReduceJSCall(node);
    if (reduction.Changed()) {
      total_inlined_bytecode_size_ += candidate.bytecode[0].value().length();
    }
    return reduction;
  }

  // Expand the JSCall/JSConstruct node to a subgraph first if
  // we have multiple known target functions.
  DCHECK_LT(1, num_calls);
  Node* calls[kMaxCallPolymorphism + 1];
  Node* if_successes[kMaxCallPolymorphism];
  Node* callee = NodeProperties::GetValueInput(node, 0);

  // Setup the inputs for the cloned call nodes.
  int const input_count = node->InputCount();
  Node** inputs = graph()->zone()->NewArray<Node*>(input_count);
  for (int i = 0; i < input_count; ++i) {
    inputs[i] = node->InputAt(i);
  }

  // Create the appropriate control flow to dispatch to the cloned calls.
  CreateOrReuseDispatch(node, callee, candidate, if_successes, calls, inputs,
                        input_count);

  // Check if we have an exception projection for the call {node}.
  Node* if_exception = nullptr;
  if (NodeProperties::IsExceptionalCall(node, &if_exception)) {
    Node* if_exceptions[kMaxCallPolymorphism + 1];
    for (int i = 0; i < num_calls; ++i) {
      if_successes[i] = graph()->NewNode(common()->IfSuccess(), calls[i]);
      if_exceptions[i] =
          graph()->NewNode(common()->IfException(), calls[i], calls[i]);
    }

    // Morph the {if_exception} projection into a join.
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

  // Morph the original call site into a join of the dispatched call sites.
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
  for (int i = 0; i < num_calls && total_inlined_bytecode_size_ <
                                       FLAG_max_inlined_bytecode_size_absolute;
       ++i) {
    if (candidate.can_inline_function[i] &&
        (small_function || total_inlined_bytecode_size_ <
                               FLAG_max_inlined_bytecode_size_cumulative)) {
      Node* node = calls[i];
      Reduction const reduction = inliner_.ReduceJSCall(node);
      if (reduction.Changed()) {
        total_inlined_bytecode_size_ += candidate.bytecode[i]->length();
        // Killing the call node is not strictly necessary, but it is safer to
        // make sure we do not resurrect the node.
        node->Kill();
      }
    }
  }

  return Replace(value);
}

bool JSInliningHeuristic::CandidateCompare::operator()(
    const Candidate& left, const Candidate& right) const {
  if (right.frequency.IsUnknown()) {
    if (left.frequency.IsUnknown()) {
      // If left and right are both unknown then the ordering is indeterminate,
      // which breaks strict weak ordering requirements, so we fall back to the
      // node id as a tie breaker.
      return left.node->id() > right.node->id();
    }
    return true;
  } else if (left.frequency.IsUnknown()) {
    return false;
  } else if (left.frequency.value() > right.frequency.value()) {
    return true;
  } else if (left.frequency.value() < right.frequency.value()) {
    return false;
  } else {
    return left.node->id() > right.node->id();
  }
}

void JSInliningHeuristic::PrintCandidates() {
  StdoutStream os;
  os << candidates_.size() << " candidate(s) for inlining:" << std::endl;
  for (const Candidate& candidate : candidates_) {
    os << "- candidate: " << candidate.node->op()->mnemonic() << " node #"
       << candidate.node->id() << " with frequency " << candidate.frequency
       << ", " << candidate.num_functions << " target(s):" << std::endl;
    for (int i = 0; i < candidate.num_functions; ++i) {
      SharedFunctionInfoRef shared = candidate.functions[i].has_value()
                                         ? candidate.functions[i]->shared()
                                         : candidate.shared_info.value();
      os << "  - target: " << shared;
      if (candidate.bytecode[i].has_value()) {
        os << ", bytecode size: " << candidate.bytecode[i]->length();
      } else {
        os << ", no bytecode";
      }
      os << std::endl;
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

#undef TRACE

}  // namespace compiler
}  // namespace internal
}  // namespace v8
