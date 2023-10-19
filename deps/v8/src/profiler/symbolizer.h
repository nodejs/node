// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PROFILER_SYMBOLIZER_H_
#define V8_PROFILER_SYMBOLIZER_H_

#include "src/base/macros.h"
#include "src/profiler/profile-generator.h"

namespace v8 {
namespace internal {

class CodeEntry;
class InstructionStreamMap;

class V8_EXPORT_PRIVATE Symbolizer {
 public:
  explicit Symbolizer(InstructionStreamMap* instruction_stream_map);
  Symbolizer(const Symbolizer&) = delete;
  Symbolizer& operator=(const Symbolizer&) = delete;

  struct SymbolizedSample {
    ProfileStackTrace stack_trace;
    int src_line;
  };

  // Use the InstructionStreamMap to turn the raw addresses recorded in the
  // sample into code/function names.
  SymbolizedSample SymbolizeTickSample(const TickSample& sample);

  InstructionStreamMap* instruction_stream_map() { return code_map_; }

 private:
  CodeEntry* FindEntry(Address address,
                       Address* out_instruction_start = nullptr);

  InstructionStreamMap* const code_map_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_PROFILER_SYMBOLIZER_H_
