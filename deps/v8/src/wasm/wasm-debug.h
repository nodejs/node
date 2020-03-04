// Copyright 2019 the V8 project authors. All rights reserved.  Use of
// this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_DEBUG_H_
#define V8_WASM_WASM_DEBUG_H_

#include <algorithm>
#include <vector>

#include "src/base/logging.h"

namespace v8 {
namespace internal {

template <typename T>
class Handle;
class JSObject;
class WasmInstanceObject;

namespace wasm {

// Side table storing information used to inspect Liftoff frames at runtime.
// This table is only created on demand for debugging, so it is not optimized
// for memory size.
class DebugSideTable {
 public:
  class Entry {
   public:
    struct Constant {
      int index;
      int32_t i32_const;
    };

    Entry(int pc_offset, int stack_height, std::vector<Constant> constants)
        : pc_offset_(pc_offset),
          stack_height_(stack_height),
          constants_(std::move(constants)) {
      DCHECK(std::is_sorted(constants_.begin(), constants_.end(),
                            ConstantIndexLess{}));
    }

    int pc_offset() const { return pc_offset_; }
    int stack_height() const { return stack_height_; }
    bool IsConstant(int index) const {
      return std::binary_search(constants_.begin(), constants_.end(),
                                Constant{index, 0}, ConstantIndexLess{});
    }
    int32_t GetConstant(int index) const {
      DCHECK(IsConstant(index));
      auto it = std::lower_bound(constants_.begin(), constants_.end(),
                                 Constant{index, 0}, ConstantIndexLess{});
      DCHECK_NE(it, constants_.end());
      DCHECK_EQ(it->index, index);
      return it->i32_const;
    }

   private:
    struct ConstantIndexLess {
      bool operator()(const Constant& a, const Constant& b) const {
        return a.index < b.index;
      }
    };

    int pc_offset_;
    int stack_height_;
    std::vector<Constant> constants_;
  };

  explicit DebugSideTable(std::vector<Entry> entries)
      : entries_(std::move(entries)) {
    DCHECK(
        std::is_sorted(entries_.begin(), entries_.end(), EntryPositionLess{}));
  }

  const Entry* GetEntry(int pc_offset) const {
    auto it = std::lower_bound(entries_.begin(), entries_.end(),
                               Entry{pc_offset, 0, {}}, EntryPositionLess{});
    if (it == entries_.end() || it->pc_offset() != pc_offset) return nullptr;
    return &*it;
  }

 private:
  struct EntryPositionLess {
    bool operator()(const Entry& a, const Entry& b) const {
      return a.pc_offset() < b.pc_offset();
    }
  };

  std::vector<Entry> entries_;
};

// Get the global scope for a given instance. This will contain the wasm memory
// (if the instance has a memory) and the values of all globals.
Handle<JSObject> GetGlobalScopeObject(Handle<WasmInstanceObject>);

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_DEBUG_H_
