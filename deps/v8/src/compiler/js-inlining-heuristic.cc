// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-inlining-heuristic.h"

#include "src/compiler.h"
#include "src/compiler/node-matchers.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

Reduction JSInliningHeuristic::Reduce(Node* node) {
  if (!IrOpcode::IsInlineeOpcode(node->opcode())) return NoChange();

  // Check if we already saw that {node} before, and if so, just skip it.
  if (seen_.find(node->id()) != seen_.end()) return NoChange();
  seen_.insert(node->id());

  Node* callee = node->InputAt(0);
  HeapObjectMatcher match(callee);
  if (!match.HasValue() || !match.Value()->IsJSFunction()) return NoChange();
  Handle<JSFunction> function = Handle<JSFunction>::cast(match.Value());

  // Functions marked with %SetForceInlineFlag are immediately inlined.
  if (function->shared()->force_inline()) {
    return inliner_.ReduceJSCall(node, function);
  }

  // Handling of special inlining modes right away:
  //  - For restricted inlining: stop all handling at this point.
  //  - For stressing inlining: immediately handle all functions.
  switch (mode_) {
    case kRestrictedInlining:
      return NoChange();
    case kStressInlining:
      return inliner_.ReduceJSCall(node, function);
    case kGeneralInlining:
      break;
  }

  // ---------------------------------------------------------------------------
  // Everything below this line is part of the inlining heuristic.
  // ---------------------------------------------------------------------------

  // Built-in functions are handled by the JSBuiltinReducer.
  if (function->shared()->HasBuiltinFunctionId()) return NoChange();

  // Don't inline builtins.
  if (function->shared()->IsBuiltin()) return NoChange();

  // Quick check on source code length to avoid parsing large candidate.
  if (function->shared()->SourceSize() > FLAG_max_inlined_source_size) {
    return NoChange();
  }

  // Quick check on the size of the AST to avoid parsing large candidate.
  if (function->shared()->ast_node_count() > FLAG_max_inlined_nodes) {
    return NoChange();
  }

  // Avoid inlining within or across the boundary of asm.js code.
  if (info_->shared_info()->asm_function()) return NoChange();
  if (function->shared()->asm_function()) return NoChange();

  // Stop inlinining once the maximum allowed level is reached.
  int level = 0;
  for (Node* frame_state = NodeProperties::GetFrameStateInput(node, 0);
       frame_state->opcode() == IrOpcode::kFrameState;
       frame_state = NodeProperties::GetFrameStateInput(frame_state, 0)) {
    if (++level > FLAG_max_inlining_levels) return NoChange();
  }

  // Gather feedback on how often this call site has been hit before.
  int calls = -1;  // Same default as CallICNexus::ExtractCallCount.
  // TODO(turbofan): We also want call counts for constructor calls.
  if (node->opcode() == IrOpcode::kJSCallFunction) {
    CallFunctionParameters p = CallFunctionParametersOf(node->op());
    if (p.feedback().IsValid()) {
      CallICNexus nexus(p.feedback().vector(), p.feedback().slot());
      calls = nexus.ExtractCallCount();
    }
  }

  // ---------------------------------------------------------------------------
  // Everything above this line is part of the inlining heuristic.
  // ---------------------------------------------------------------------------

  // In the general case we remember the candidate for later.
  candidates_.insert({function, node, calls});
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
      Reduction r = inliner_.ReduceJSCall(candidate.node, candidate.function);
      if (r.Changed()) {
        cumulative_count_ += candidate.function->shared()->ast_node_count();
        return;
      }
    }
  }
}


bool JSInliningHeuristic::CandidateCompare::operator()(
    const Candidate& left, const Candidate& right) const {
  return left.node != right.node && left.calls >= right.calls;
}


void JSInliningHeuristic::PrintCandidates() {
  PrintF("Candidates for inlining (size=%zu):\n", candidates_.size());
  for (const Candidate& candidate : candidates_) {
    PrintF("  id:%d, calls:%d, size[source]:%d, size[ast]:%d / %s\n",
           candidate.node->id(), candidate.calls,
           candidate.function->shared()->SourceSize(),
           candidate.function->shared()->ast_node_count(),
           candidate.function->shared()->DebugName()->ToCString().get());
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
