// Copyright 2019 the V8 project authors. All rights reserved.  Use of
// this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_DEBUG_H_
#define V8_WASM_WASM_DEBUG_H_

#include <algorithm>
#include <memory>
#include <vector>

#include "include/v8-internal.h"
#include "src/base/iterator.h"
#include "src/base/logging.h"
#include "src/base/macros.h"
#include "src/wasm/value-type.h"

namespace v8 {
namespace internal {

template <typename T>
class Handle;
class JSObject;
template <typename T>
class Vector;
class WasmCompiledFrame;
class WasmInstanceObject;

namespace wasm {

class DebugInfoImpl;
class LocalNames;
class NativeModule;
class WasmCode;
class WireBytesRef;

// Side table storing information used to inspect Liftoff frames at runtime.
// This table is only created on demand for debugging, so it is not optimized
// for memory size.
class DebugSideTable {
 public:
  class Entry {
   public:
    enum ValueKind : int8_t { kConstant, kRegister, kStack };
    struct Value {
      ValueType type;
      ValueKind kind;
      union {
        int32_t i32_const;  // if kind == kConstant
        int reg_code;       // if kind == kRegister
        int stack_offset;   // if kind == kStack
      };
    };

    Entry(int pc_offset, std::vector<Value> values)
        : pc_offset_(pc_offset), values_(std::move(values)) {}

    // Constructor for map lookups (only initializes the {pc_offset_}).
    explicit Entry(int pc_offset) : pc_offset_(pc_offset) {}

    int pc_offset() const { return pc_offset_; }

    int num_values() const { return static_cast<int>(values_.size()); }
    ValueType value_type(int index) const { return values_[index].type; }

    auto values() const {
      return base::make_iterator_range(values_.begin(), values_.end());
    }

    int stack_offset(int index) const {
      DCHECK_EQ(kStack, values_[index].kind);
      return values_[index].stack_offset;
    }

    bool is_constant(int index) const {
      return values_[index].kind == kConstant;
    }

    bool is_register(int index) const {
      return values_[index].kind == kRegister;
    }

    int32_t i32_constant(int index) const {
      DCHECK_EQ(kConstant, values_[index].kind);
      return values_[index].i32_const;
    }

    int32_t register_code(int index) const {
      DCHECK_EQ(kRegister, values_[index].kind);
      return values_[index].reg_code;
    }

   private:
    int pc_offset_;
    std::vector<Value> values_;
  };

  // Technically it would be fine to copy this class, but there should not be a
  // reason to do so, hence mark it move only.
  MOVE_ONLY_NO_DEFAULT_CONSTRUCTOR(DebugSideTable);

  explicit DebugSideTable(int num_locals, std::vector<Entry> entries)
      : num_locals_(num_locals), entries_(std::move(entries)) {
    DCHECK(
        std::is_sorted(entries_.begin(), entries_.end(), EntryPositionLess{}));
  }

  const Entry* GetEntry(int pc_offset) const {
    auto it = std::lower_bound(entries_.begin(), entries_.end(),
                               Entry{pc_offset}, EntryPositionLess{});
    if (it == entries_.end() || it->pc_offset() != pc_offset) return nullptr;
    DCHECK_LE(num_locals_, it->num_values());
    return &*it;
  }

  auto entries() const {
    return base::make_iterator_range(entries_.begin(), entries_.end());
  }

  int num_locals() const { return num_locals_; }

 private:
  struct EntryPositionLess {
    bool operator()(const Entry& a, const Entry& b) const {
      return a.pc_offset() < b.pc_offset();
    }
  };

  int num_locals_;
  std::vector<Entry> entries_;
};

// Get the global scope for a given instance. This will contain the wasm memory
// (if the instance has a memory) and the values of all globals.
Handle<JSObject> GetGlobalScopeObject(Handle<WasmInstanceObject>);

// Debug info per NativeModule, created lazily on demand.
// Implementation in {wasm-debug.cc} using PIMPL.
class DebugInfo {
 public:
  explicit DebugInfo(NativeModule*);
  ~DebugInfo();

  // {fp} is the frame pointer of the Liftoff frame, {debug_break_fp} that of
  // the {WasmDebugBreak} frame (if any).
  Handle<JSObject> GetLocalScopeObject(Isolate*, Address pc, Address fp,
                                       Address debug_break_fp);

  Handle<JSObject> GetStackScopeObject(Isolate*, Address pc, Address fp,
                                       Address debug_break_fp);

  WireBytesRef GetLocalName(int func_index, int local_index);

  void SetBreakpoint(int func_index, int offset, Isolate* current_isolate);

  void PrepareStep(Isolate*, StackFrameId);

  void ClearStepping();

  bool IsStepping(WasmCompiledFrame*);

  void RemoveBreakpoint(int func_index, int offset, Isolate* current_isolate);

  void RemoveDebugSideTables(Vector<WasmCode* const>);

 private:
  std::unique_ptr<DebugInfoImpl> impl_;
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_DEBUG_H_
