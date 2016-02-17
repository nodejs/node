// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_JS_INLINING_HEURISTIC_H_
#define V8_COMPILER_JS_INLINING_HEURISTIC_H_

#include "src/compiler/js-inlining.h"

namespace v8 {
namespace internal {
namespace compiler {

class JSInliningHeuristic final : public AdvancedReducer {
 public:
  enum Mode { kGeneralInlining, kRestrictedInlining, kStressInlining };
  JSInliningHeuristic(Editor* editor, Mode mode, Zone* local_zone,
                      CompilationInfo* info, JSGraph* jsgraph)
      : AdvancedReducer(editor),
        mode_(mode),
        inliner_(editor, local_zone, info, jsgraph),
        candidates_(local_zone),
        seen_(local_zone),
        info_(info) {}

  Reduction Reduce(Node* node) final;

  // Processes the list of candidates gathered while the reducer was running,
  // and inlines call sites that the heuristic determines to be important.
  void Finalize() final;

 private:
  struct Candidate {
    Handle<JSFunction> function;  // The call target being inlined.
    Node* node;                   // The call site at which to inline.
    int calls;                    // Number of times the call site was hit.
  };

  // Comparator for candidates.
  struct CandidateCompare {
    bool operator()(const Candidate& left, const Candidate& right) const;
  };

  // Candidates are kept in a sorted set of unique candidates.
  typedef ZoneSet<Candidate, CandidateCompare> Candidates;

  // Dumps candidates to console.
  void PrintCandidates();

  Mode const mode_;
  JSInliner inliner_;
  Candidates candidates_;
  ZoneSet<NodeId> seen_;
  CompilationInfo* info_;
  int cumulative_count_ = 0;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_JS_INLINING_HEURISTIC_H_
