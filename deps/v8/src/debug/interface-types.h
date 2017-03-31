// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DEBUG_INTERFACE_TYPES_H_
#define V8_DEBUG_INTERFACE_TYPES_H_

#include <cstdint>
#include <string>
#include <vector>

namespace v8 {
namespace debug {

/**
 * Defines location inside script.
 * Lines and columns are 0-based.
 */
class Location {
 public:
  Location(int line_number, int column_number);
  /**
   * Create empty location.
   */
  Location();

  int GetLineNumber() const;
  int GetColumnNumber() const;
  bool IsEmpty() const;

 private:
  int line_number_;
  int column_number_;
};

/**
 * The result of disassembling a wasm function.
 * Consists of the disassembly string and an offset table mapping wasm byte
 * offsets to line and column in the disassembly.
 * The offset table entries are ordered by the byte_offset.
 * All numbers are 0-based.
 */
struct WasmDisassemblyOffsetTableEntry {
  WasmDisassemblyOffsetTableEntry(uint32_t byte_offset, int line, int column)
      : byte_offset(byte_offset), line(line), column(column) {}

  uint32_t byte_offset;
  int line;
  int column;
};
struct WasmDisassembly {
  using OffsetTable = std::vector<WasmDisassemblyOffsetTableEntry>;
  WasmDisassembly() {}
  WasmDisassembly(std::string disassembly, OffsetTable offset_table)
      : disassembly(std::move(disassembly)),
        offset_table(std::move(offset_table)) {}

  std::string disassembly;
  OffsetTable offset_table;
};

enum PromiseDebugActionType {
  kDebugEnqueueAsyncFunction,
  kDebugEnqueuePromiseResolve,
  kDebugEnqueuePromiseReject,
  kDebugEnqueuePromiseResolveThenableJob,
  kDebugPromiseCollected,
  kDebugWillHandle,
  kDebugDidHandle,
};

}  // namespace debug
}  // namespace v8

#endif  // V8_DEBUG_INTERFACE_TYPES_H_
