// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_BYTECODE_BRANCH_ANALYSIS_H_
#define V8_COMPILER_BYTECODE_BRANCH_ANALYSIS_H_

#include "src/bit-vector.h"
#include "src/handles.h"

namespace v8 {
namespace internal {

class BytecodeArray;

namespace compiler {

// A class for identifying branch targets within a bytecode array.
// This information can be used to construct the local control flow
// logic for high-level IR graphs built from bytecode.
//
// N.B. If this class is used to determine loop headers, then such a
// usage relies on the only backwards branches in bytecode being jumps
// back to loop headers.
class BytecodeBranchAnalysis BASE_EMBEDDED {
 public:
  BytecodeBranchAnalysis(Handle<BytecodeArray> bytecode_array, Zone* zone);

  // Analyze the bytecodes to find the branch sites and their
  // targets. No other methods in this class return valid information
  // until this has been called.
  void Analyze();

  // Returns true if there are any forward branches to the bytecode at
  // |offset|.
  bool forward_branches_target(int offset) const {
    return is_forward_target_.Contains(offset);
  }

  // Returns true if there are any backward branches to the bytecode
  // at |offset|.
  bool backward_branches_target(int offset) const {
    return is_backward_target_.Contains(offset);
  }

 private:
  void AddBranch(int origin_offset, int target_offset);

  Zone* zone() const { return zone_; }
  Handle<BytecodeArray> bytecode_array() const { return bytecode_array_; }

  Handle<BytecodeArray> bytecode_array_;
  BitVector is_backward_target_;
  BitVector is_forward_target_;
  Zone* zone_;

  DISALLOW_COPY_AND_ASSIGN(BytecodeBranchAnalysis);
};


}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_BYTECODE_BRANCH_ANALYSIS_H_
