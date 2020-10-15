// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_REGEXP_EXPERIMENTAL_EXPERIMENTAL_H_
#define V8_REGEXP_EXPERIMENTAL_EXPERIMENTAL_H_

#include "src/regexp/regexp.h"

namespace v8 {
namespace internal {

class ExperimentalRegExp final : public AllStatic {
 public:
  // Initialization & Compilation:
  static void Initialize(Isolate* isolate, Handle<JSRegExp> re,
                         Handle<String> pattern, JSRegExp::Flags flags,
                         int capture_count);
  static bool IsCompiled(Handle<JSRegExp> re);
  static void Compile(Isolate* isolate, Handle<JSRegExp> re);

  // Execution:
  static int32_t MatchForCallFromJs(Address subject, int32_t start_position,
                                    Address input_start, Address input_end,
                                    int* output_registers,
                                    int32_t output_register_count,
                                    Address backtrack_stack,
                                    RegExp::CallOrigin call_origin,
                                    Isolate* isolate, Address regexp);
  static MaybeHandle<Object> Exec(Isolate* isolate, Handle<JSRegExp> regexp,
                                  Handle<String> subject, int index,
                                  Handle<RegExpMatchInfo> last_match_info);
  static int32_t ExecRaw(JSRegExp regexp, String subject,
                         int32_t* output_registers,
                         int32_t output_register_count, int32_t subject_index);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_REGEXP_EXPERIMENTAL_EXPERIMENTAL_H_
