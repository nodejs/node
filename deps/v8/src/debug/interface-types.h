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
  bool is_empty_;
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
  kDebugAsyncFunctionPromiseCreated,
  kDebugPromiseThen,
  kDebugPromiseCatch,
  kDebugPromiseFinally,
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

class ConsoleContext {
 public:
  ConsoleContext(int id, v8::Local<v8::String> name) : id_(id), name_(name) {}
  ConsoleContext() : id_(0) {}

  int id() const { return id_; }
  v8::Local<v8::String> name() const { return name_; }

 private:
  int id_;
  v8::Local<v8::String> name_;
};

class ConsoleDelegate {
 public:
  virtual void Debug(const ConsoleCallArguments& args,
                     const ConsoleContext& context) {}
  virtual void Error(const ConsoleCallArguments& args,
                     const ConsoleContext& context) {}
  virtual void Info(const ConsoleCallArguments& args,
                    const ConsoleContext& context) {}
  virtual void Log(const ConsoleCallArguments& args,
                   const ConsoleContext& context) {}
  virtual void Warn(const ConsoleCallArguments& args,
                    const ConsoleContext& context) {}
  virtual void Dir(const ConsoleCallArguments& args,
                   const ConsoleContext& context) {}
  virtual void DirXml(const ConsoleCallArguments& args,
                      const ConsoleContext& context) {}
  virtual void Table(const ConsoleCallArguments& args,
                     const ConsoleContext& context) {}
  virtual void Trace(const ConsoleCallArguments& args,
                     const ConsoleContext& context) {}
  virtual void Group(const ConsoleCallArguments& args,
                     const ConsoleContext& context) {}
  virtual void GroupCollapsed(const ConsoleCallArguments& args,
                              const ConsoleContext& context) {}
  virtual void GroupEnd(const ConsoleCallArguments& args,
                        const ConsoleContext& context) {}
  virtual void Clear(const ConsoleCallArguments& args,
                     const ConsoleContext& context) {}
  virtual void Count(const ConsoleCallArguments& args,
                     const ConsoleContext& context) {}
  virtual void Assert(const ConsoleCallArguments& args,
                      const ConsoleContext& context) {}
  virtual void MarkTimeline(const ConsoleCallArguments& args,
                            const ConsoleContext& context) {}
  virtual void Profile(const ConsoleCallArguments& args,
                       const ConsoleContext& context) {}
  virtual void ProfileEnd(const ConsoleCallArguments& args,
                          const ConsoleContext& context) {}
  virtual void Timeline(const ConsoleCallArguments& args,
                        const ConsoleContext& context) {}
  virtual void TimelineEnd(const ConsoleCallArguments& args,
                           const ConsoleContext& context) {}
  virtual void Time(const ConsoleCallArguments& args,
                    const ConsoleContext& context) {}
  virtual void TimeEnd(const ConsoleCallArguments& args,
                       const ConsoleContext& context) {}
  virtual void TimeStamp(const ConsoleCallArguments& args,
                         const ConsoleContext& context) {}
  virtual ~ConsoleDelegate() = default;
};

typedef int BreakpointId;

}  // namespace debug
}  // namespace v8

#endif  // V8_DEBUG_INTERFACE_TYPES_H_
