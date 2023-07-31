// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_CODE_POINTER_TABLE_INL_H_
#define V8_SANDBOX_CODE_POINTER_TABLE_INL_H_

#include "src/sandbox/code-pointer-table.h"
#include "src/sandbox/external-entity-table-inl.h"

#ifdef V8_COMPRESS_POINTERS

namespace v8 {
namespace internal {

void CodePointerTableEntry::MakeCodePointerEntry(Address value) {
  pointer_.store(value, std::memory_order_relaxed);
}

Address CodePointerTableEntry::GetCodePointer() const {
  auto pointer = pointer_.load(std::memory_order_relaxed);
  DCHECK_NE(pointer & kFreeEntryTag, kFreeEntryTag);
  return pointer;
}

void CodePointerTableEntry::SetCodePointer(Address value) {
  DCHECK_NE(pointer_.load(std::memory_order_relaxed) & kFreeEntryTag,
            kFreeEntryTag);
  pointer_.store(value, std::memory_order_relaxed);
}

void CodePointerTableEntry::MakeFreelistEntry(uint32_t next_entry_index) {
  Address value = kFreeEntryTag | next_entry_index;
  pointer_.store(value, std::memory_order_relaxed);
}

uint32_t CodePointerTableEntry::GetNextFreelistEntryIndex() const {
  return static_cast<uint32_t>(pointer_.load(std::memory_order_relaxed));
}

Address CodePointerTable::Get(CodePointerHandle handle) const {
  uint32_t index = HandleToIndex(handle);
  return at(index).GetCodePointer();
}

void CodePointerTable::Set(CodePointerHandle handle, Address value) {
  DCHECK_NE(kNullCodePointerHandle, handle);
  uint32_t index = HandleToIndex(handle);
  at(index).SetCodePointer(value);
}

CodePointerHandle CodePointerTable::AllocateAndInitializeEntry(
    Address initial_value) {
  uint32_t index = AllocateEntry();
  at(index).MakeCodePointerEntry(initial_value);
  return IndexToHandle(index);
}

uint32_t CodePointerTable::HandleToIndex(CodePointerHandle handle) const {
  uint32_t index = handle >> kCodePointerIndexShift;
  DCHECK_EQ(handle, index << kCodePointerIndexShift);
  DCHECK_LT(index, capacity());
  return index;
}

CodePointerHandle CodePointerTable::IndexToHandle(uint32_t index) const {
  CodePointerHandle handle = index << kCodePointerIndexShift;
  DCHECK_EQ(index, handle >> kCodePointerIndexShift);
  return handle;
}

}  // namespace internal
}  // namespace v8

#endif  // V8_COMPRESS_POINTERS

#endif  // V8_SANDBOX_CODE_POINTER_TABLE_INL_H_
