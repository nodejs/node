// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_BUILTIN_SNAPSHOT_UTILS_H_
#define V8_SNAPSHOT_BUILTIN_SNAPSHOT_UTILS_H_

#include <functional>

#include "src/interpreter/interpreter.h"

namespace v8 {
namespace internal {

// Constants and utility methods used by builtin and bytecode handler
// (de)serialization.
class BuiltinSnapshotUtils : public AllStatic {
  using Bytecode = interpreter::Bytecode;
  using BytecodeOperands = interpreter::BytecodeOperands;
  using Bytecodes = interpreter::Bytecodes;
  using Interpreter = interpreter::Interpreter;
  using OperandScale = interpreter::OperandScale;

 public:
  static const int kFirstBuiltinIndex = 0;
  static const int kNumberOfBuiltins = Builtins::builtin_count;

  static const int kFirstHandlerIndex = kFirstBuiltinIndex + kNumberOfBuiltins;
  static const int kNumberOfHandlers =
      Bytecodes::kBytecodeCount * BytecodeOperands::kOperandScaleCount;

  // The number of code objects in the builtin snapshot.
  // TODO(jgruber): This could be reduced by a bit since not every
  // {bytecode, operand_scale} combination has an associated handler
  // (see Bytecodes::BytecodeHasHandler).
  static const int kNumberOfCodeObjects = kNumberOfBuiltins + kNumberOfHandlers;

  // Indexes into the offsets vector contained in snapshot.
  // See e.g. BuiltinSerializer::code_offsets_.
  static bool IsBuiltinIndex(int maybe_index);
  static bool IsHandlerIndex(int maybe_index);
  static int BytecodeToIndex(Bytecode bytecode, OperandScale operand_scale);

  // Converts an index back into the {bytecode,operand_scale} tuple. This is the
  // inverse operation of BytecodeToIndex().
  static std::pair<Bytecode, OperandScale> BytecodeFromIndex(int index);

  // Iteration over all {bytecode,operand_scale} pairs. Implemented here since
  // (de)serialization depends on the iteration order.
  static void ForEachBytecode(std::function<void(Bytecode, OperandScale)> f);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_BUILTIN_SNAPSHOT_UTILS_H_
