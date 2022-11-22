// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#ifndef V8_WASM_STRING_BUILDER_MULTILINE_H_
#define V8_WASM_STRING_BUILDER_MULTILINE_H_

#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "src/wasm/string-builder.h"

namespace v8 {

namespace debug {
class DisassemblyCollector;
}  // namespace debug

namespace internal {
namespace wasm {

struct LabelInfo {
  LabelInfo(size_t line_number, size_t offset,
            uint32_t index_by_occurrence_order)
      : name_section_index(index_by_occurrence_order),
        line_number(line_number),
        offset(offset) {}
  uint32_t name_section_index;
  size_t line_number;
  size_t offset;
  const char* start{nullptr};
  size_t length{0};
};

class MultiLineStringBuilder : public StringBuilder {
 public:
  MultiLineStringBuilder() : StringBuilder(kKeepOldChunks) {}

  void NextLine(uint32_t byte_offset) {
    *allocate(1) = '\n';
    size_t len = length();
    lines_.emplace_back(start(), len, pending_bytecode_offset_);
    start_here();
    pending_bytecode_offset_ = byte_offset;
  }
  size_t line_number() { return lines_.size(); }

  void set_current_line_bytecode_offset(uint32_t offset) {
    pending_bytecode_offset_ = offset;
  }

  // Label backpatching support. Parameters:
  // {label}: Information about where to insert the label. Fields {line_number},
  // {offset}, and {length} must already be populated; {start} will be populated
  // with the location where the inserted label was written in memory. Note that
  // this will become stale/invalid if the same line is patched again!
  // {label_source}: Pointer to the characters forming the snippet that is to
  // be inserted into the position described by {label}. The length of this
  // snippet is passed in {label.length}.
  void PatchLabel(LabelInfo& label, const char* label_source) {
    DCHECK_GT(label.length, 0);
    DCHECK_LT(label.line_number, lines_.size());

    // Step 1: Patching a line makes it longer, and we can't grow it in-place
    // because it's boxed in, so allocate space for its patched copy.
    char* patched_line;
    Line& l = lines_[label.line_number];
    // +1 because we add a space before the label: "block" -> "block $label0",
    // "block i32" -> "block $label0 i32".
    size_t patched_length = l.len + label.length + 1;
    if (length() == 0) {
      // No current unfinished line. Allocate the patched line as if it was
      // the next line.
      patched_line = allocate(patched_length);
      start_here();
    } else {
      // Shift the current unfinished line out of the way.
      // TODO(jkummerow): This approach ends up being O(n²) for a `br_table`
      // with `n` labels. If that ever becomes a problem, we could allocate a
      // separate new chunk for patched copies of old lines, then we wouldn't
      // need to shift the unfinished line around.
      const char* unfinished_start = start();  // Remember the unfinished
      size_t unfinished_length = length();     // line, and...
      rewind_to_start();                       // ...free up its space.
      patched_line = allocate(patched_length);
      // Write the unfinished line into its new location.
      start_here();
      char* new_location = allocate(unfinished_length);
      memmove(new_location, unfinished_start, unfinished_length);
      if (label_source >= unfinished_start &&
          label_source < unfinished_start + unfinished_length) {
        label_source = new_location + (label_source - unfinished_start);
      }
    }

    // Step 2: Write the patched copy of the line to be patched.
    char* cursor = patched_line;
    memcpy(cursor, l.data, label.offset);
    cursor += label.offset;
    *(cursor++) = ' ';
    label.start = cursor;
    memcpy(cursor, label_source, label.length);
    cursor += label.length;
    memcpy(cursor, l.data + label.offset, l.len - label.offset);
    l.data = patched_line;
    l.len = patched_length;
  }

  // Note: implemented in wasm-disassembler.cc (which is also the only user).
  void ToDisassemblyCollector(v8::debug::DisassemblyCollector* collector);

  void WriteTo(std::ostream& out) {
    if (length() != 0) NextLine(0);

    // In the name of speed, batch up lines that happen to be stored
    // consecutively.
    if (lines_.size() == 0) return;
    const Line& first = lines_[0];
    const char* last_start = first.data;
    size_t len = first.len;
    for (size_t i = 1; i < lines_.size(); i++) {
      const Line& l = lines_[i];
      if (last_start + len == l.data) {
        len += l.len;
      } else {
        out.write(last_start, len);
        last_start = l.data;
        len = l.len;
      }
    }
    out.write(last_start, len);
  }

  size_t ApproximateSizeMB() { return approximate_size_mb(); }

 private:
  struct Line {
    Line(const char* d, size_t length, uint32_t bytecode_offset)
        : data(d), len(length), bytecode_offset(bytecode_offset) {}
    const char* data;
    size_t len;
    uint32_t bytecode_offset;
  };

  std::vector<Line> lines_;
  uint32_t pending_bytecode_offset_ = 0;
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_STRING_BUILDER_MULTILINE_H_
