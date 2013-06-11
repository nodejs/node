// Copyright 2013 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_HYDROGEN_GVN_H_
#define V8_HYDROGEN_GVN_H_

#include "hydrogen.h"
#include "hydrogen-instructions.h"
#include "compiler.h"
#include "zone.h"

namespace v8 {
namespace internal {

// Simple sparse set with O(1) add, contains, and clear.
class SparseSet {
 public:
  SparseSet(Zone* zone, int capacity)
      : capacity_(capacity),
        length_(0),
        dense_(zone->NewArray<int>(capacity)),
        sparse_(zone->NewArray<int>(capacity)) {
#ifndef NVALGRIND
    // Initialize the sparse array to make valgrind happy.
    memset(sparse_, 0, sizeof(sparse_[0]) * capacity);
#endif
  }

  bool Contains(int n) const {
    ASSERT(0 <= n && n < capacity_);
    int d = sparse_[n];
    return 0 <= d && d < length_ && dense_[d] == n;
  }

  bool Add(int n) {
    if (Contains(n)) return false;
    dense_[length_] = n;
    sparse_[n] = length_;
    ++length_;
    return true;
  }

  void Clear() { length_ = 0; }

 private:
  int capacity_;
  int length_;
  int* dense_;
  int* sparse_;

  DISALLOW_COPY_AND_ASSIGN(SparseSet);
};


class HGlobalValueNumberer BASE_EMBEDDED {
 public:
  HGlobalValueNumberer(HGraph* graph, CompilationInfo* info);

  // Returns true if values with side effects are removed.
  bool Analyze();

 private:
  GVNFlagSet CollectSideEffectsOnPathsToDominatedBlock(
      HBasicBlock* dominator,
      HBasicBlock* dominated);
  void AnalyzeGraph();
  void ComputeBlockSideEffects();
  void LoopInvariantCodeMotion();
  void ProcessLoopBlock(HBasicBlock* block,
                        HBasicBlock* before_loop,
                        GVNFlagSet loop_kills,
                        GVNFlagSet* accumulated_first_time_depends,
                        GVNFlagSet* accumulated_first_time_changes);
  bool AllowCodeMotion();
  bool ShouldMove(HInstruction* instr, HBasicBlock* loop_header);

  HGraph* graph() { return graph_; }
  CompilationInfo* info() { return info_; }
  Zone* zone() const { return graph_->zone(); }

  HGraph* graph_;
  CompilationInfo* info_;
  bool removed_side_effects_;

  // A map of block IDs to their side effects.
  ZoneList<GVNFlagSet> block_side_effects_;

  // A map of loop header block IDs to their loop's side effects.
  ZoneList<GVNFlagSet> loop_side_effects_;

  // Used when collecting side effects on paths from dominator to
  // dominated.
  SparseSet visited_on_paths_;
};


} }  // namespace v8::internal

#endif  // V8_HYDROGEN_GVN_H_
