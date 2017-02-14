// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_BYTECODE_LOOP_ANALYSIS_H_
#define V8_COMPILER_BYTECODE_LOOP_ANALYSIS_H_

#include "src/handles.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {

class BytecodeArray;

namespace compiler {

class BytecodeBranchAnalysis;

class BytecodeLoopAnalysis BASE_EMBEDDED {
 public:
  BytecodeLoopAnalysis(Handle<BytecodeArray> bytecode_array,
                       const BytecodeBranchAnalysis* branch_analysis,
                       Zone* zone);

  // Analyze the bytecodes to find the branch sites and their
  // targets. No other methods in this class return valid information
  // until this has been called.
  void Analyze();

  // Get the loop header offset of the containing loop for arbitrary
  // {offset}, or -1 if the {offset} is not inside any loop.
  int GetLoopOffsetFor(int offset) const;
  // Gets the loop header offset of the parent loop of the loop header
  // at {header_offset}, or -1 for outer-most loops.
  int GetParentLoopFor(int header_offset) const;

 private:
  void AddLoopEntry(int entry_offset);
  void AddBranch(int origin_offset, int target_offset);

  Zone* zone() const { return zone_; }
  Handle<BytecodeArray> bytecode_array() const { return bytecode_array_; }

  Handle<BytecodeArray> bytecode_array_;
  const BytecodeBranchAnalysis* branch_analysis_;
  Zone* zone_;

  int current_loop_offset_;
  bool found_current_backedge_;

  // Map from the offset of a backedge jump to the offset of the corresponding
  // loop header. There might be multiple backedges for do-while loops.
  ZoneMap<int, int> backedge_to_header_;
  // Map from the offset of a loop header to the offset of its parent's loop
  // header. This map will have as many entries as there are loops in the
  // function.
  ZoneMap<int, int> loop_header_to_parent_;

  DISALLOW_COPY_AND_ASSIGN(BytecodeLoopAnalysis);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_BYTECODE_LOOP_ANALYSIS_H_
