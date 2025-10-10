// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_JUMP_TABLE_INFO_H_
#define V8_CODEGEN_JUMP_TABLE_INFO_H_

#include <vector>

#include "include/v8-internal.h"
#include "src/base/macros.h"
#include "src/common/globals.h"

namespace v8 {
namespace internal {

class Assembler;

// The jump table info is a part of code metadata, used by the disassembler. The
// layout is:
//
// byte count       content
// ----------------------------------------------------------------
// [Inline array of JumpTableInfoEntry in increasing pc_offset order]
// ┌ 4              pc_offset of entry as uint32_t
// └ 4              target of entry as int32_t

struct JumpTableInfoEntry {
  constexpr JumpTableInfoEntry(uint32_t pc_offset, int32_t target)
      : pc_offset(pc_offset), target(target) {}
  uint32_t pc_offset;
  int32_t target;

  static constexpr int kPCOffsetSize = kUInt32Size;
  static constexpr int kTargetSize = kInt32Size;
  static constexpr int kSize = kPCOffsetSize + kTargetSize;
};
static_assert(sizeof(JumpTableInfoEntry) == JumpTableInfoEntry::kSize);

// Used during codegen.
class JumpTableInfoWriter {
 public:
  V8_EXPORT_PRIVATE void Add(uint32_t pc_offset, int32_t target);
  void Emit(Assembler* assm);

  size_t entry_count() const;
  uint32_t size_in_bytes() const;

 private:
  std::vector<JumpTableInfoEntry> entries_;
};

// Used during disassembly.
class V8_EXPORT_PRIVATE JumpTableInfoIterator {
 public:
  JumpTableInfoIterator(Address start, uint32_t size);
  uint32_t GetPCOffset() const;
  int32_t GetTarget() const;
  void Next();
  bool HasCurrent() const;

 private:
  const Address start_;
  const uint32_t size_;
  Address cursor_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_JUMP_TABLE_INFO_H_
