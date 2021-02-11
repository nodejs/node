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
class CodeMap;

class V8_EXPORT_PRIVATE Symbolizer {
 public:
  explicit Symbolizer(CodeMap* code_map);

  struct SymbolizedSample {
    ProfileStackTrace stack_trace;
    int src_line;
  };

  // Use the CodeMap to turn the raw addresses recorded in the sample into
  // code/function names.
  SymbolizedSample SymbolizeTickSample(const TickSample& sample);

  CodeMap* code_map() { return code_map_; }

 private:
  CodeEntry* FindEntry(Address address,
                       Address* out_instruction_start = nullptr);

  CodeMap* const code_map_;

  DISALLOW_COPY_AND_ASSIGN(Symbolizer);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_PROFILER_SYMBOLIZER_H_
