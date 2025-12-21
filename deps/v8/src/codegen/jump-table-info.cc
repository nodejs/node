// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/jump-table-info.h"

#include "src/base/memory.h"
#include "src/codegen/assembler-inl.h"
#include "src/common/globals.h"

namespace v8 {
namespace internal {

void JumpTableInfoWriter::Add(uint32_t pc_offset, int32_t target) {
  entries_.emplace_back(pc_offset, target);
}

size_t JumpTableInfoWriter::entry_count() const { return entries_.size(); }

uint32_t JumpTableInfoWriter::size_in_bytes() const {
  return static_cast<uint32_t>(entry_count() * JumpTableInfoEntry::kSize);
}

void JumpTableInfoWriter::Emit(Assembler* assm) {
  for (auto i = entries_.begin(); i != entries_.end(); ++i) {
    static_assert(JumpTableInfoEntry::kPCOffsetSize == kUInt32Size);
    assm->dd(i->pc_offset);
    static_assert(JumpTableInfoEntry::kTargetSize == kInt32Size);
    assm->dd(i->target);
    static_assert(JumpTableInfoEntry::kSize ==
                  JumpTableInfoEntry::kPCOffsetSize +
                      JumpTableInfoEntry::kTargetSize);
  }
}

JumpTableInfoIterator::JumpTableInfoIterator(Address start, uint32_t size)
    : start_(start), size_(size), cursor_(start_) {
  DCHECK_NE(kNullAddress, start);
}

uint32_t JumpTableInfoIterator::GetPCOffset() const {
  return base::ReadUnalignedValue<uint32_t>(
      cursor_ + offsetof(JumpTableInfoEntry, pc_offset));
}

int32_t JumpTableInfoIterator::GetTarget() const {
  return base::ReadUnalignedValue<int32_t>(
      cursor_ + offsetof(JumpTableInfoEntry, target));
}

void JumpTableInfoIterator::Next() { cursor_ += JumpTableInfoEntry::kSize; }

bool JumpTableInfoIterator::HasCurrent() const {
  return cursor_ < start_ + size_;
}

}  // namespace internal
}  // namespace v8
