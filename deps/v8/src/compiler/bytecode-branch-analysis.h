// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_BYTECODE_BRANCH_ANALYSIS_H_
#define V8_COMPILER_BYTECODE_BRANCH_ANALYSIS_H_

#include "src/bit-vector.h"
#include "src/handles.h"
#include "src/zone-containers.h"

namespace v8 {
namespace internal {

class BytecodeArray;

namespace compiler {

class BytecodeBranchInfo;

// A class for identifying the branch targets and their branch sites
// within a bytecode array and also identifying which bytecodes are
// reachable. This information can be used to construct the local
// control flow logic for high-level IR graphs built from bytecode.
//
// NB This class relies on the only backwards branches in bytecode
// being jumps back to loop headers.
class BytecodeBranchAnalysis BASE_EMBEDDED {
 public:
  BytecodeBranchAnalysis(Handle<BytecodeArray> bytecode_array, Zone* zone);

  // Analyze the bytecodes to find the branch sites and their
  // targets. No other methods in this class return valid information
  // until this has been called.
  void Analyze();

  // Offsets of bytecodes having a backward branch to the bytecode at |offset|.
  const ZoneVector<int>* BackwardBranchesTargetting(int offset) const;

  // Offsets of bytecodes having a forward branch to the bytecode at |offset|.
  const ZoneVector<int>* ForwardBranchesTargetting(int offset) const;

  // Returns true if the bytecode at |offset| is reachable.
  bool is_reachable(int offset) const { return reachable_.Contains(offset); }

  // Returns true if there are any forward branches to the bytecode at
  // |offset|.
  bool forward_branches_target(int offset) const {
    const ZoneVector<int>* sites = ForwardBranchesTargetting(offset);
    return sites != nullptr && sites->size() > 0;
  }

  // Returns true if there are any backward branches to the bytecode
  // at |offset|.
  bool backward_branches_target(int offset) const {
    const ZoneVector<int>* sites = BackwardBranchesTargetting(offset);
    return sites != nullptr && sites->size() > 0;
  }

 private:
  void AddBranch(int origin_offset, int target_offset);

  Zone* zone() const { return zone_; }
  Handle<BytecodeArray> bytecode_array() const { return bytecode_array_; }

  ZoneMap<int, BytecodeBranchInfo*> branch_infos_;
  Handle<BytecodeArray> bytecode_array_;
  BitVector reachable_;
  Zone* zone_;

  DISALLOW_COPY_AND_ASSIGN(BytecodeBranchAnalysis);
};


}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_BYTECODE_BRANCH_ANALYSIS_H_
