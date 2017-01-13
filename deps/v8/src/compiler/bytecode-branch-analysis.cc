// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/bytecode-branch-analysis.h"

#include "src/interpreter/bytecode-array-iterator.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

BytecodeBranchAnalysis::BytecodeBranchAnalysis(
    Handle<BytecodeArray> bytecode_array, Zone* zone)
    : bytecode_array_(bytecode_array),
      is_backward_target_(bytecode_array->length(), zone),
      is_forward_target_(bytecode_array->length(), zone),
      zone_(zone) {}

void BytecodeBranchAnalysis::Analyze() {
  interpreter::BytecodeArrayIterator iterator(bytecode_array());
  while (!iterator.done()) {
    interpreter::Bytecode bytecode = iterator.current_bytecode();
    int current_offset = iterator.current_offset();
    if (interpreter::Bytecodes::IsJump(bytecode)) {
      AddBranch(current_offset, iterator.GetJumpTargetOffset());
    }
    iterator.Advance();
  }
}

void BytecodeBranchAnalysis::AddBranch(int source_offset, int target_offset) {
  if (source_offset < target_offset) {
    is_forward_target_.Add(target_offset);
  } else {
    is_backward_target_.Add(target_offset);
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
