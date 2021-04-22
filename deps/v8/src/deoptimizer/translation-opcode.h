// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DEOPTIMIZER_TRANSLATION_OPCODE_H_
#define V8_DEOPTIMIZER_TRANSLATION_OPCODE_H_

namespace v8 {
namespace internal {

// V(name, operand_count)
#define TRANSLATION_OPCODE_LIST(V)                        \
  V(ARGUMENTS_ADAPTOR_FRAME, 2)                           \
  V(ARGUMENTS_ELEMENTS, 1)                                \
  V(ARGUMENTS_LENGTH, 0)                                  \
  V(BEGIN, 3)                                             \
  V(BOOL_REGISTER, 1)                                     \
  V(BOOL_STACK_SLOT, 1)                                   \
  V(BUILTIN_CONTINUATION_FRAME, 3)                        \
  V(CAPTURED_OBJECT, 1)                                   \
  V(CONSTRUCT_STUB_FRAME, 3)                              \
  V(DOUBLE_REGISTER, 1)                                   \
  V(DOUBLE_STACK_SLOT, 1)                                 \
  V(DUPLICATED_OBJECT, 1)                                 \
  V(FLOAT_REGISTER, 1)                                    \
  V(FLOAT_STACK_SLOT, 1)                                  \
  V(INT32_REGISTER, 1)                                    \
  V(INT32_STACK_SLOT, 1)                                  \
  V(INT64_REGISTER, 1)                                    \
  V(INT64_STACK_SLOT, 1)                                  \
  V(INTERPRETED_FRAME, 5)                                 \
  V(JAVA_SCRIPT_BUILTIN_CONTINUATION_FRAME, 3)            \
  V(JAVA_SCRIPT_BUILTIN_CONTINUATION_WITH_CATCH_FRAME, 3) \
  V(JS_TO_WASM_BUILTIN_CONTINUATION_FRAME, 4)             \
  V(LITERAL, 1)                                           \
  V(REGISTER, 1)                                          \
  V(STACK_SLOT, 1)                                        \
  V(UINT32_REGISTER, 1)                                   \
  V(UINT32_STACK_SLOT, 1)                                 \
  V(UPDATE_FEEDBACK, 2)

enum class TranslationOpcode {
#define CASE(name, ...) name,
  TRANSLATION_OPCODE_LIST(CASE)
#undef CASE
};

constexpr TranslationOpcode TranslationOpcodeFromInt(int i) {
  return static_cast<TranslationOpcode>(i);
}

inline int TranslationOpcodeOperandCount(TranslationOpcode o) {
#define CASE(name, operand_count) operand_count,
  static const int counts[] = {TRANSLATION_OPCODE_LIST(CASE)};
#undef CASE
  return counts[static_cast<int>(o)];
}

inline const char* TranslationOpcodeToString(TranslationOpcode o) {
#define CASE(name, ...) #name,
  static const char* const names[] = {TRANSLATION_OPCODE_LIST(CASE)};
#undef CASE
  return names[static_cast<int>(o)];
}

#undef TRANSLATION_OPCODE_LIST

}  // namespace internal
}  // namespace v8

#endif  // V8_DEOPTIMIZER_TRANSLATION_OPCODE_H_
