// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_TRUSTED_POINTER_TABLE_INL_H_
#define V8_SANDBOX_TRUSTED_POINTER_TABLE_INL_H_

#include "src/sandbox/external-entity-table-inl.h"
#include "src/sandbox/sandbox.h"
#include "src/sandbox/trusted-pointer-table.h"

#ifdef V8_ENABLE_SANDBOX

namespace v8 {
namespace internal {

void TrustedPointerTableEntry::MakeTrustedPointerEntry(Address content,
                                                       bool mark_as_alive) {
  DCHECK_EQ(content & kMarkingBit, 0);
  if (mark_as_alive) content |= kMarkingBit;
  content_.store(content, std::memory_order_relaxed);
}

void TrustedPointerTableEntry::MakeFreelistEntry(uint32_t next_entry_index) {
  Address content = kFreeEntryTag | next_entry_index;
  content_.store(content, std::memory_order_relaxed);
}

Address TrustedPointerTableEntry::GetContent() const {
  DCHECK(!IsFreelistEntry());
  // We reuse the heap object tag bit as marking bit, so we need to explicitly
  // set it here when accessing the pointer.
  return content_.load(std::memory_order_relaxed) | kMarkingBit;
}

void TrustedPointerTableEntry::SetContent(Address new_content) {
  DCHECK(!IsFreelistEntry());
  // SetContent shouldn't change the marking state of the entry. Currently this
  // is always automatically the case, but if this ever fails, we might need to
  // manually copy the marking bit.
  DCHECK_EQ(content_ & kMarkingBit, new_content & kMarkingBit);
  content_.store(new_content, std::memory_order_relaxed);
}

bool TrustedPointerTableEntry::IsFreelistEntry() const {
  auto content = content_.load(std::memory_order_relaxed);
  return (content & kFreeEntryTag) == kFreeEntryTag;
}

uint32_t TrustedPointerTableEntry::GetNextFreelistEntryIndex() const {
  return static_cast<uint32_t>(content_.load(std::memory_order_relaxed));
}

void TrustedPointerTableEntry::Mark() {
  Address old_value = content_.load(std::memory_order_relaxed);
  Address new_value = old_value | kMarkingBit;

  // We don't need to perform the CAS in a loop since it can only fail if a new
  // value has been written into the entry. This, however, will also have set
  // the marking bit.
  bool success = content_.compare_exchange_strong(old_value, new_value,
                                                  std::memory_order_relaxed);
  DCHECK(success || (old_value & kMarkingBit) == kMarkingBit);
  USE(success);
}

void TrustedPointerTableEntry::Unmark() {
  Address content = content_.load(std::memory_order_relaxed);
  content &= ~kMarkingBit;
  content_.store(content, std::memory_order_relaxed);
}

bool TrustedPointerTableEntry::IsMarked() const {
  Address value = content_.load(std::memory_order_relaxed);
  return value & kMarkingBit;
}

Address TrustedPointerTable::Get(TrustedPointerHandle handle) const {
  uint32_t index = HandleToIndex(handle);
  return at(index).GetContent();
}

void TrustedPointerTable::Set(TrustedPointerHandle handle, Address pointer,
                              IndirectPointerTag tag) {
  DCHECK_NE(kNullTrustedPointerHandle, handle);
  Validate(pointer, tag);
  uint32_t index = HandleToIndex(handle);
  at(index).SetContent(pointer);
}

TrustedPointerHandle TrustedPointerTable::AllocateAndInitializeEntry(
    Space* space, Address pointer, IndirectPointerTag tag) {
  DCHECK(space->BelongsTo(this));
  Validate(pointer, tag);
  uint32_t index = AllocateEntry(space);
  at(index).MakeTrustedPointerEntry(pointer, space->allocate_black());
  return IndexToHandle(index);
}

void TrustedPointerTable::Mark(Space* space, TrustedPointerHandle handle) {
  DCHECK(space->BelongsTo(this));
  // The null entry is immortal and immutable, so no need to mark it as alive.
  if (handle == kNullTrustedPointerHandle) return;

  uint32_t index = HandleToIndex(handle);
  DCHECK(space->Contains(index));

  at(index).Mark();
}

template <typename Callback>
void TrustedPointerTable::IterateActiveEntriesIn(Space* space,
                                                 Callback callback) {
  IterateEntriesIn(space, [&](uint32_t index) {
    if (!at(index).IsFreelistEntry()) {
      callback(IndexToHandle(index), at(index).GetContent());
    }
  });
}

uint32_t TrustedPointerTable::HandleToIndex(TrustedPointerHandle handle) const {
  uint32_t index = handle >> kTrustedPointerHandleShift;
  DCHECK_EQ(handle, index << kTrustedPointerHandleShift);
  return index;
}

TrustedPointerHandle TrustedPointerTable::IndexToHandle(uint32_t index) const {
  TrustedPointerHandle handle = index << kTrustedPointerHandleShift;
  DCHECK_EQ(index, handle >> kTrustedPointerHandleShift);
  return handle;
}

void TrustedPointerTable::Validate(Address pointer, IndirectPointerTag tag) {
  if (IsTrustedSpaceMigrationInProgressForObjectsWithTag(tag)) {
    // This CHECK is mostly just here to force tags to be taken out of the
    // IsTrustedSpaceMigrationInProgressForObjectsWithTag function once the
    // objects are fully migrated into trusted space.
    DCHECK(GetProcessWideSandbox()->Contains(pointer));
    return;
  }

  // Entries must never point into the sandbox, as they couldn't be trusted in
  // that case. This CHECK is a defense-in-depth mechanism to guarantee this.
  CHECK(!InsideSandbox(pointer));
}

}  // namespace internal
}  // namespace v8

#endif  // V8_ENABLE_SANDBOX

#endif  // V8_SANDBOX_TRUSTED_POINTER_TABLE_INL_H_
