// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/builtin-snapshot-utils.h"

namespace v8 {
namespace internal {

// static
bool BuiltinSnapshotUtils::IsBuiltinIndex(int maybe_index) {
  return (kFirstBuiltinIndex <= maybe_index &&
          maybe_index < kFirstBuiltinIndex + kNumberOfBuiltins);
}

// static
bool BuiltinSnapshotUtils::IsHandlerIndex(int maybe_index) {
  return (kFirstHandlerIndex <= maybe_index &&
          maybe_index < kFirstHandlerIndex + kNumberOfHandlers);
}

// static
int BuiltinSnapshotUtils::BytecodeToIndex(Bytecode bytecode,
                                          OperandScale operand_scale) {
  int index =
      BuiltinSnapshotUtils::kNumberOfBuiltins + static_cast<int>(bytecode);
  switch (operand_scale) {  // clang-format off
    case OperandScale::kSingle: return index;
    case OperandScale::kDouble: return index + Bytecodes::kBytecodeCount;
    case OperandScale::kQuadruple: return index + 2 * Bytecodes::kBytecodeCount;
  }  // clang-format on
  UNREACHABLE();
}

// static
std::pair<interpreter::Bytecode, interpreter::OperandScale>
BuiltinSnapshotUtils::BytecodeFromIndex(int index) {
  DCHECK(IsHandlerIndex(index));

  const int x = index - BuiltinSnapshotUtils::kNumberOfBuiltins;
  Bytecode bytecode = Bytecodes::FromByte(x % Bytecodes::kBytecodeCount);
  switch (x / Bytecodes::kBytecodeCount) {  // clang-format off
    case 0: return {bytecode, OperandScale::kSingle};
    case 1: return {bytecode, OperandScale::kDouble};
    case 2: return {bytecode, OperandScale::kQuadruple};
    default: UNREACHABLE();
  }  // clang-format on
}

// static
void BuiltinSnapshotUtils::ForEachBytecode(
    std::function<void(Bytecode, OperandScale)> f) {
  static const OperandScale kOperandScales[] = {
#define VALUE(Name, _) OperandScale::k##Name,
      OPERAND_SCALE_LIST(VALUE)
#undef VALUE
  };

  for (OperandScale operand_scale : kOperandScales) {
    for (int i = 0; i < Bytecodes::kBytecodeCount; i++) {
      f(Bytecodes::FromByte(i), operand_scale);
    }
  }
}

}  // namespace internal
}  // namespace v8
