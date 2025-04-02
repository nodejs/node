// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_REGEXP_EXPERIMENTAL_EXPERIMENTAL_H_
#define V8_REGEXP_EXPERIMENTAL_EXPERIMENTAL_H_

#include "src/regexp/regexp-flags.h"
#include "src/regexp/regexp.h"

namespace v8 {
namespace internal {

class ExperimentalRegExp final : public AllStatic {
 public:
  // Initialization & Compilation
  // -------------------------------------------------------------------------
  // Check whether a parsed regexp pattern can be compiled and executed by the
  // EXPERIMENTAL engine.
  // TODO(mbid, v8:10765): This walks the RegExpTree, but it could also be
  // checked on the fly in the parser.  Not done currently because walking the
  // AST again is more flexible and less error prone (but less performant).
  static bool CanBeHandled(RegExpTree* tree, Handle<String> pattern,
                           RegExpFlags flags, int capture_count);
  static void Initialize(Isolate* isolate, DirectHandle<JSRegExp> re,
                         DirectHandle<String> pattern, RegExpFlags flags,
                         int capture_count);
  static bool IsCompiled(DirectHandle<IrRegExpData> re_data, Isolate* isolate);
  V8_WARN_UNUSED_RESULT
  static bool Compile(Isolate* isolate, DirectHandle<IrRegExpData> re_data);

  // Execution:
  static int32_t MatchForCallFromJs(Address subject, int32_t start_position,
                                    Address input_start, Address input_end,
                                    int* output_registers,
                                    int32_t output_register_count,
                                    RegExp::CallOrigin call_origin,
                                    Isolate* isolate, Address regexp_data);
  static std::optional<int> Exec(Isolate* isolate,
                                 DirectHandle<IrRegExpData> regexp_data,
                                 Handle<String> subject, int index,
                                 int32_t* result_offsets_vector,
                                 uint32_t result_offsets_vector_length);
  static int32_t ExecRaw(Isolate* isolate, RegExp::CallOrigin call_origin,
                         Tagged<IrRegExpData> regexp_data,
                         Tagged<String> subject, int32_t* output_registers,
                         int32_t output_register_count, int32_t subject_index);

  // Compile and execute a regexp with the experimental engine, regardless of
  // its type tag.  The regexp itself is not changed (apart from lastIndex).
  static std::optional<int> OneshotExec(Isolate* isolate,
                                        DirectHandle<IrRegExpData> regexp_data,
                                        DirectHandle<String> subject, int index,
                                        int32_t* result_offsets_vector,
                                        uint32_t result_offsets_vector_length);
  static int32_t OneshotExecRaw(Isolate* isolate,
                                DirectHandle<IrRegExpData> regexp_data,
                                DirectHandle<String> subject,
                                int32_t* output_registers,
                                int32_t output_register_count,
                                int32_t subject_index);

  static constexpr bool kSupportsUnicode = false;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_REGEXP_EXPERIMENTAL_EXPERIMENTAL_H_
