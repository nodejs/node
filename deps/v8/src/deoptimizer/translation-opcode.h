// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DEOPTIMIZER_TRANSLATION_OPCODE_H_
#define V8_DEOPTIMIZER_TRANSLATION_OPCODE_H_

#include "src/base/macros.h"

namespace v8 {
namespace internal {

// V(name, operand_count)
#define TRANSLATION_OPCODE_LIST(V)                        \
  V(ARGUMENTS_ELEMENTS, 1)                                \
  V(ARGUMENTS_LENGTH, 0)                                  \
  V(BEGIN_WITHOUT_FEEDBACK, 3)                            \
  V(BEGIN_WITH_FEEDBACK, 3)                               \
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
  V(INLINED_EXTRA_ARGUMENTS, 2)                           \
  V(INT32_REGISTER, 1)                                    \
  V(INT32_STACK_SLOT, 1)                                  \
  V(INT64_REGISTER, 1)                                    \
  V(INT64_STACK_SLOT, 1)                                  \
  V(SIGNED_BIGINT64_REGISTER, 1)                          \
  V(SIGNED_BIGINT64_STACK_SLOT, 1)                        \
  V(UNSIGNED_BIGINT64_REGISTER, 1)                        \
  V(UNSIGNED_BIGINT64_STACK_SLOT, 1)                      \
  V(INTERPRETED_FRAME_WITH_RETURN, 5)                     \
  V(INTERPRETED_FRAME_WITHOUT_RETURN, 3)                  \
  V(JAVA_SCRIPT_BUILTIN_CONTINUATION_FRAME, 3)            \
  V(JAVA_SCRIPT_BUILTIN_CONTINUATION_WITH_CATCH_FRAME, 3) \
  IF_WASM(V, JS_TO_WASM_BUILTIN_CONTINUATION_FRAME, 4)    \
  V(OPTIMIZED_OUT, 0)                                     \
  V(LITERAL, 1)                                           \
  V(REGISTER, 1)                                          \
  V(STACK_SLOT, 1)                                        \
  V(UINT32_REGISTER, 1)                                   \
  V(UINT32_STACK_SLOT, 1)                                 \
  V(UPDATE_FEEDBACK, 2)                                   \
  V(MATCH_PREVIOUS_TRANSLATION, 1)

enum class TranslationOpcode {
#define CASE(name, ...) name,
  TRANSLATION_OPCODE_LIST(CASE)
#undef CASE
};

#define PLUS_ONE(...) +1
static constexpr int kNumTranslationOpcodes =
    0 TRANSLATION_OPCODE_LIST(PLUS_ONE);
#undef PLUS_ONE

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

constexpr int kMaxTranslationOperandCount = 5;
#define CASE(name, operand_count) \
  static_assert(operand_count <= kMaxTranslationOperandCount);
TRANSLATION_OPCODE_LIST(CASE)
#undef CASE

inline bool TranslationOpcodeIsBegin(TranslationOpcode o) {
  return o == TranslationOpcode::BEGIN_WITH_FEEDBACK ||
         o == TranslationOpcode::BEGIN_WITHOUT_FEEDBACK;
}

#undef TRANSLATION_OPCODE_LIST

}  // namespace internal
}  // namespace v8

#endif  // V8_DEOPTIMIZER_TRANSLATION_OPCODE_H_
