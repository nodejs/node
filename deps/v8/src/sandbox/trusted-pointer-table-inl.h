// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_TRUSTED_POINTER_TABLE_INL_H_
#define V8_SANDBOX_TRUSTED_POINTER_TABLE_INL_H_

#include "src/sandbox/trusted-pointer-table.h"
// Include the non-inl header before the rest of the headers.

#include "src/sandbox/external-entity-table-inl.h"
#include "src/sandbox/sandbox.h"
#include "src/sandbox/trusted-pointer-scope.h"

#ifdef V8_ENABLE_SANDBOX

namespace v8 {
namespace internal {

void TrustedPointerTableEntry::MakeTrustedPointerEntry(Address pointer,
                                                       IndirectPointerTag tag,
                                                       bool mark_as_alive) {
  auto payload = Payload(pointer, tag);
  if (mark_as_alive) payload.SetMarkBit();
  payload_.store(payload, std::memory_order_relaxed);
}

void TrustedPointerTableEntry::MakeFreelistEntry(uint32_t next_entry_index) {
  auto payload = Payload(next_entry_index, kIndirectPointerFreeEntryTag);
  payload_.store(payload, std::memory_order_relaxed);
}

void TrustedPointerTableEntry::MakeZappedEntry() {
  auto payload = Payload(kNullAddress, kIndirectPointerZappedEntryTag);
  payload_.store(payload, std::memory_order_relaxed);
}

Address TrustedPointerTableEntry::GetPointer(
    IndirectPointerTagRange tag_range) const {
  DCHECK(!IsFreelistEntry());
  return payload_.load(std::memory_order_relaxed).Untag(tag_range);
}

void TrustedPointerTableEntry::SetPointer(Address pointer,
                                          IndirectPointerTag tag) {
  DCHECK(!IsFreelistEntry());
  // Currently, this method is only used when the mark bit is unset. If this
  // ever changes, we'd need to check the marking state of the old entry and
  // set the marking state of the new entry accordingly.
  DCHECK(!payload_.load(std::memory_order_relaxed).HasMarkBitSet());
  auto new_payload = Payload(pointer, tag);
  DCHECK(!new_payload.HasMarkBitSet());
  payload_.store(new_payload, std::memory_order_relaxed);
}

bool TrustedPointerTableEntry::HasPointer(
    IndirectPointerTagRange tag_range) const {
  auto payload = payload_.load(std::memory_order_relaxed);
  if (!payload.ContainsPointer()) return false;
  return payload.IsTaggedWithTagIn(tag_range);
}

void TrustedPointerTableEntry::Unpublish() {
  auto old_payload = payload_.load(std::memory_order_relaxed);
  auto new_payload = old_payload;
  new_payload.SetTag(kUnpublishedIndirectPointerTag);
  payload_.store(new_payload, std::memory_order_relaxed);
}

void TrustedPointerTableEntry::Publish(IndirectPointerTag tag) {
  auto old_payload = payload_.load(std::memory_order_relaxed);
  CHECK(old_payload.IsTaggedWith(kUnpublishedIndirectPointerTag));
  auto new_payload = old_payload;
  new_payload.SetTag(tag);
  payload_.store(new_payload, std::memory_order_relaxed);
}

bool TrustedPointerTableEntry::IsFreelistEntry() const {
  auto payload = payload_.load(std::memory_order_relaxed);
  return payload.ContainsFreelistLink();
}

std::optional<uint32_t> TrustedPointerTableEntry::GetNextFreelistEntryIndex()
    const {
  return payload_.load(std::memory_order_relaxed).ExtractFreelistLink();
}

void TrustedPointerTableEntry::Mark() {
  auto old_payload = payload_.load(std::memory_order_relaxed);
  DCHECK(old_payload.ContainsPointer());

  auto new_payload = old_payload;
  new_payload.SetMarkBit();

  // We don't need to perform the CAS in a loop since it can only fail if a new
  // value has been written into the entry. This, however, will also have set
  // the marking bit.
  bool success = payload_.compare_exchange_strong(old_payload, new_payload,
                                                  std::memory_order_relaxed);
  DCHECK(success || old_payload.HasMarkBitSet());
  USE(success);
}

void TrustedPointerTableEntry::Unmark() {
  auto payload = payload_.load(std::memory_order_relaxed);
  payload.ClearMarkBit();
  payload_.store(payload, std::memory_order_relaxed);
}

bool TrustedPointerTableEntry::IsMarked() const {
  return payload_.load(std::memory_order_relaxed).HasMarkBitSet();
}

Address TrustedPointerTable::Get(TrustedPointerHandle handle,
                                 IndirectPointerTagRange tag_range) const {
  uint32_t index = HandleToIndex(handle);
  DCHECK(index == 0 || at(index).HasPointer(tag_range));
  return at(index).GetPointer(tag_range);
}

Address TrustedPointerTable::GetMaybeUnpublished(
    TrustedPointerHandle handle, IndirectPointerTagRange tag_range) const {
  uint32_t index = HandleToIndex(handle);
  const TrustedPointerTableEntry& entry = at(index);
  if (entry.HasPointer(kUnpublishedIndirectPointerTag)) {
    return entry.GetPointer(kUnpublishedIndirectPointerTag);
  }
  return Get(handle, tag_range);
}

void TrustedPointerTable::Set(TrustedPointerHandle handle, Address pointer,
                              IndirectPointerTag tag) {
  DCHECK_NE(kNullTrustedPointerHandle, handle);
  Validate(pointer, tag);
  uint32_t index = HandleToIndex(handle);
  at(index).SetPointer(pointer, tag);
}

TrustedPointerHandle TrustedPointerTable::AllocateAndInitializeEntry(
    Space* space, Address pointer, IndirectPointerTag tag,
    TrustedPointerPublishingScope* scope) {
  DCHECK(space->BelongsTo(this));
  Validate(pointer, tag);
  uint32_t index = AllocateEntry(space);
  at(index).MakeTrustedPointerEntry(pointer, tag, space->allocate_black());
  if (scope != nullptr) scope->TrackPointer(&at(index));
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

void TrustedPointerTable::Zap(TrustedPointerHandle handle) {
  uint32_t index = HandleToIndex(handle);
  at(index).MakeZappedEntry();
}

void TrustedPointerTable::Publish(TrustedPointerHandle handle,
                                  IndirectPointerTag tag) {
  uint32_t index = HandleToIndex(handle);
  at(index).Publish(tag);
}

bool TrustedPointerTable::IsUnpublished(TrustedPointerHandle handle) const {
  uint32_t index = HandleToIndex(handle);
  return at(index).HasPointer(kUnpublishedIndirectPointerTag);
}

template <typename Callback>
void TrustedPointerTable::IterateActiveEntriesIn(Space* space,
                                                 Callback callback) {
  IterateEntriesIn(space, [&](uint32_t index) {
    if (!at(index).IsFreelistEntry()) {
      // We might see unpublished entries here and also need to handle them.
      Address pointer =
          at(index).GetPointer(kAllIndirectPointerTagsIncludingUnpublished);
      callback(IndexToHandle(index), pointer);
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
    DCHECK(Sandbox::current()->Contains(pointer));
    return;
  }

  // Entries must never point into the sandbox, as they couldn't be trusted in
  // that case. This CHECK is a defense-in-depth mechanism to guarantee this.
  CHECK(OutsideSandbox(pointer));
}

}  // namespace internal
}  // namespace v8

#endif  // V8_ENABLE_SANDBOX

#endif  // V8_SANDBOX_TRUSTED_POINTER_TABLE_INL_H_
