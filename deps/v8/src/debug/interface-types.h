// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DEBUG_INTERFACE_TYPES_H_
#define V8_DEBUG_INTERFACE_TYPES_H_

#include <cstdint>
#include <string>
#include <vector>

#include "include/v8.h"
#include "src/globals.h"

namespace v8 {

namespace internal {
class BuiltinArguments;
}  // internal

namespace debug {

/**
 * Defines location inside script.
 * Lines and columns are 0-based.
 */
class V8_EXPORT_PRIVATE Location {
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
  kDebugPromiseCreated,
  kDebugEnqueueAsyncFunction,
  kDebugEnqueuePromiseResolve,
  kDebugEnqueuePromiseReject,
  kDebugWillHandle,
  kDebugDidHandle,
};

enum BreakLocationType {
  kCallBreakLocation,
  kReturnBreakLocation,
  kDebuggerStatementBreakLocation,
  kCommonBreakLocation
};

class V8_EXPORT_PRIVATE BreakLocation : public Location {
 public:
  BreakLocation(int line_number, int column_number, BreakLocationType type)
      : Location(line_number, column_number), type_(type) {}

  BreakLocationType type() const { return type_; }

 private:
  BreakLocationType type_;
};

class ConsoleCallArguments : private v8::FunctionCallbackInfo<v8::Value> {
 public:
  int Length() const { return v8::FunctionCallbackInfo<v8::Value>::Length(); }
  V8_INLINE Local<Value> operator[](int i) const {
    return v8::FunctionCallbackInfo<v8::Value>::operator[](i);
  }

  explicit ConsoleCallArguments(const v8::FunctionCallbackInfo<v8::Value>&);
  explicit ConsoleCallArguments(internal::BuiltinArguments&);
};

// v8::FunctionCallbackInfo could be used for getting arguments only. Calling
// of any other getter will produce a crash.
class ConsoleDelegate {
 public:
  virtual void Debug(const ConsoleCallArguments& args) {}
  virtual void Error(const ConsoleCallArguments& args) {}
  virtual void Info(const ConsoleCallArguments& args) {}
  virtual void Log(const ConsoleCallArguments& args) {}
  virtual void Warn(const ConsoleCallArguments& args) {}
  virtual void Dir(const ConsoleCallArguments& args) {}
  virtual void DirXml(const ConsoleCallArguments& args) {}
  virtual void Table(const ConsoleCallArguments& args) {}
  virtual void Trace(const ConsoleCallArguments& args) {}
  virtual void Group(const ConsoleCallArguments& args) {}
  virtual void GroupCollapsed(const ConsoleCallArguments& args) {}
  virtual void GroupEnd(const ConsoleCallArguments& args) {}
  virtual void Clear(const ConsoleCallArguments& args) {}
  virtual void Count(const ConsoleCallArguments& args) {}
  virtual void Assert(const ConsoleCallArguments& args) {}
  virtual void MarkTimeline(const ConsoleCallArguments& args) {}
  virtual void Profile(const ConsoleCallArguments& args) {}
  virtual void ProfileEnd(const ConsoleCallArguments& args) {}
  virtual void Timeline(const ConsoleCallArguments& args) {}
  virtual void TimelineEnd(const ConsoleCallArguments& args) {}
  virtual void Time(const ConsoleCallArguments& args) {}
  virtual void TimeEnd(const ConsoleCallArguments& args) {}
  virtual void TimeStamp(const ConsoleCallArguments& args) {}
  virtual ~ConsoleDelegate() = default;
};

}  // namespace debug
}  // namespace v8

#endif  // V8_DEBUG_INTERFACE_TYPES_H_
