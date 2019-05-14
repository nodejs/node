// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fstream>
#include <iostream>

#include "src/interpreter/bytecodes.h"

namespace v8 {
namespace internal {
namespace interpreter {

void WriteBytecode(std::ofstream& out, Bytecode bytecode,
                   OperandScale operand_scale, int* count, int offset_table[],
                   int table_index) {
  DCHECK_NOT_NULL(count);
  if (Bytecodes::BytecodeHasHandler(bytecode, operand_scale)) {
    out << " \\\n  V(" << Bytecodes::ToString(bytecode, operand_scale, "")
        << "Handler, interpreter::OperandScale::k" << operand_scale
        << ", interpreter::Bytecode::k" << Bytecodes::ToString(bytecode) << ")";
    offset_table[table_index] = *count;
    (*count)++;
  } else {
    offset_table[table_index] = -1;
  }
}

void WriteHeader(const char* header_filename) {
  std::ofstream out(header_filename);

  out << "// Automatically generated from interpreter/bytecodes.h\n"
      << "// The following list macro is used to populate the builtins list\n"
      << "// with the bytecode handlers\n\n"
      << "#ifndef V8_BUILTINS_GENERATED_BYTECODES_BUILTINS_LIST\n"
      << "#define V8_BUILTINS_GENERATED_BYTECODES_BUILTINS_LIST\n\n"
      << "namespace v8 {\n"
      << "namespace internal {\n\n"
      << "#define BUILTIN_LIST_BYTECODE_HANDLERS(V)";

  constexpr int kTableSize =
      BytecodeOperands::kOperandScaleCount * Bytecodes::kBytecodeCount;
  int offset_table[kTableSize];
  int count = 0;
  int index = 0;

#define ADD_BYTECODES(Name, ...)                                             \
  WriteBytecode(out, Bytecode::k##Name, operand_scale, &count, offset_table, \
                index++);
  OperandScale operand_scale = OperandScale::kSingle;
  BYTECODE_LIST(ADD_BYTECODES)
  int single_count = count;
  operand_scale = OperandScale::kDouble;
  BYTECODE_LIST(ADD_BYTECODES)
  int wide_count = count - single_count;
  operand_scale = OperandScale::kQuadruple;
  BYTECODE_LIST(ADD_BYTECODES)
#undef ADD_BYTECODES
  int extra_wide_count = count - wide_count - single_count;
  CHECK_GT(single_count, wide_count);
  CHECK_EQ(single_count, Bytecodes::kBytecodeCount);
  CHECK_EQ(wide_count, extra_wide_count);
  out << "\n\nconst int kNumberOfBytecodeHandlers = " << single_count << ";\n"
      << "const int kNumberOfWideBytecodeHandlers = " << wide_count << ";\n\n"
      << "// Mapping from (Bytecode + OperandScaleAsIndex * |Bytecodes|) to\n"
      << "// a dense form with all the illegal Bytecode/OperandScale\n"
      << "// combinations removed. Used to index into the builtins table.\n"
      << "constexpr int kBytecodeToBuiltinsMapping[" << kTableSize << "] = {\n"
      << "    ";

  for (int i = 0; i < kTableSize; ++i) {
    if (i == single_count || i == 2 * single_count) {
      out << "\n    ";
    }
    out << offset_table[i] << ", ";
  }

  out << "};\n\n"
      << "}  // namespace internal\n"
      << "}  // namespace v8\n"
      << "#endif  // V8_BUILTINS_GENERATED_BYTECODES_BUILTINS_LIST\n";
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

int main(int argc, const char* argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <output filename>\n";
    std::exit(1);
  }

  v8::internal::interpreter::WriteHeader(argv[1]);

  return 0;
}
