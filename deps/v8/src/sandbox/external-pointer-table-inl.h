// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_EXTERNAL_POINTER_TABLE_INL_H_
#define V8_SANDBOX_EXTERNAL_POINTER_TABLE_INL_H_

#include "src/sandbox/compactible-external-entity-table-inl.h"
#include "src/sandbox/external-pointer-table.h"
#include "src/sandbox/external-pointer.h"

#ifdef V8_COMPRESS_POINTERS

namespace v8 {
namespace internal {

void ExternalPointerTableEntry::MakeExternalPointerEntry(
    Address value, ExternalPointerTag tag) {
  DCHECK_EQ(0, value & kExternalPointerTagMask);
  DCHECK(tag & kExternalPointerMarkBit);
  DCHECK_NE(tag, kExternalPointerFreeEntryTag);
  DCHECK_NE(tag, kExternalPointerEvacuationEntryTag);

  Payload new_payload(value, tag);
  payload_.store(new_payload, std::memory_order_relaxed);
  MaybeUpdateRawPointerForLSan(value);
}

Address ExternalPointerTableEntry::GetExternalPointer(
    ExternalPointerTag tag) const {
  auto payload = payload_.load(std::memory_order_relaxed);
  DCHECK(payload.ContainsExternalPointer());
  return payload.Untag(tag);
}

void ExternalPointerTableEntry::SetExternalPointer(Address value,
                                                   ExternalPointerTag tag) {
  DCHECK_EQ(0, value & kExternalPointerTagMask);
  DCHECK(tag & kExternalPointerMarkBit);
  DCHECK(payload_.load(std::memory_order_relaxed).ContainsExternalPointer());

  Payload new_payload(value, tag);
  payload_.store(new_payload, std::memory_order_relaxed);
  MaybeUpdateRawPointerForLSan(value);
}

bool ExternalPointerTableEntry::HasExternalPointer(
    ExternalPointerTag tag) const {
  auto payload = payload_.load(std::memory_order_relaxed);
  return tag == kAnyExternalPointerTag || payload.IsTaggedWith(tag);
}

Address ExternalPointerTableEntry::ExchangeExternalPointer(
    Address value, ExternalPointerTag tag) {
  DCHECK_EQ(0, value & kExternalPointerTagMask);
  DCHECK(tag & kExternalPointerMarkBit);

  Payload new_payload(value, tag);
  Payload old_payload =
      payload_.exchange(new_payload, std::memory_order_relaxed);
  DCHECK(old_payload.ContainsExternalPointer());
  MaybeUpdateRawPointerForLSan(value);
  return old_payload.Untag(tag);
}

ExternalPointerTag ExternalPointerTableEntry::GetExternalPointerTag() const {
  auto payload = payload_.load(std::memory_order_relaxed);
  DCHECK(payload.ContainsExternalPointer());
  return payload.ExtractTag();
}

Address ExternalPointerTableEntry::ExtractManagedResourceOrNull() const {
  auto payload = payload_.load(std::memory_order_relaxed);
  ExternalPointerTag tag = payload.ExtractTag();
  if (IsManagedExternalPointerType(tag)) {
    return payload.Untag(tag);
  }
  return kNullAddress;
}

void ExternalPointerTableEntry::MakeZappedEntry() {
  Payload new_payload(kNullAddress, kExternalPointerZappedEntryTag);
  payload_.store(new_payload, std::memory_order_relaxed);
}

void ExternalPointerTableEntry::MakeFreelistEntry(uint32_t next_entry_index) {
  // The next freelist entry is stored in the lower bits of the entry.
  static_assert(kMaxExternalPointers <= std::numeric_limits<uint32_t>::max());
  Payload new_payload(next_entry_index, kExternalPointerFreeEntryTag);
  payload_.store(new_payload, std::memory_order_relaxed);
}

uint32_t ExternalPointerTableEntry::GetNextFreelistEntryIndex() const {
  auto payload = payload_.load(std::memory_order_relaxed);
  return payload.ExtractFreelistLink();
}

void ExternalPointerTableEntry::Mark() {
  auto old_payload = payload_.load(std::memory_order_relaxed);
  DCHECK(old_payload.ContainsExternalPointer());

  auto new_payload = old_payload;
  new_payload.SetMarkBit();

  // We don't need to perform the CAS in a loop: if the new value is not equal
  // to the old value, then the mutator must've just written a new value into
  // the entry. This in turn must've set the marking bit already (see e.g.
  // StoreExternalPointer), so we don't need to do it again.
  bool success = payload_.compare_exchange_strong(old_payload, new_payload,
                                                  std::memory_order_relaxed);
  DCHECK(success || old_payload.HasMarkBitSet());
  USE(success);
}

void ExternalPointerTableEntry::MakeEvacuationEntry(Address handle_location) {
  Payload new_payload(handle_location, kExternalPointerEvacuationEntryTag);
  payload_.store(new_payload, std::memory_order_relaxed);
}

bool ExternalPointerTableEntry::HasEvacuationEntry() const {
  auto payload = payload_.load(std::memory_order_relaxed);
  return payload.ContainsEvacuationEntry();
}

void ExternalPointerTableEntry::MigrateInto(ExternalPointerTableEntry& other) {
  auto payload = payload_.load(std::memory_order_relaxed);
  // We expect to only migrate entries containing external pointers.
  DCHECK(payload.ContainsExternalPointer());

  other.payload_.store(payload, std::memory_order_relaxed);
#if defined(LEAK_SANITIZER)
  other.raw_pointer_for_lsan_ = raw_pointer_for_lsan_;
#endif  // LEAK_SANITIZER

#ifdef DEBUG
  // In debug builds, we clobber this old entry so that any sharing of table
  // entries is easily detected. Shared entries would require write barriers,
  // so we'd like to avoid them. See the compaction algorithm explanation in
  // external-pointer-table.h for more details.
  constexpr Address kClobberedEntryMarker = static_cast<Address>(-1);
  Payload clobbered(kClobberedEntryMarker, kExternalPointerNullTag);
  DCHECK_NE(payload, clobbered);
  payload_.store(clobbered, std::memory_order_relaxed);
#endif  // DEBUG
}

Address ExternalPointerTable::Get(ExternalPointerHandle handle,
                                  ExternalPointerTag tag) const {
  uint32_t index = HandleToIndex(handle);
#if defined(V8_USE_ADDRESS_SANITIZER)
  // We rely on the tagging scheme to produce non-canonical addresses when an
  // entry isn't tagged with the expected tag. Such "safe" crashes can then be
  // filtered out by our sandbox crash filter. However, when ASan is active, it
  // may perform its shadow memory access prior to the actual memory access.
  // For a non-canonical address, this can lead to a segfault at a _canonical_
  // address, which our crash filter can then not distinguish from a "real"
  // crash. Therefore, in ASan builds, we perform an additional CHECK here that
  // the entry is tagged with the expected tag. The resulting CHECK failure
  // will then be ignored by the crash filter.
  // This check is, however, not needed when accessing the null entry, as that
  // is always valid (it just contains nullptr).
  CHECK(index == 0 || at(index).HasExternalPointer(tag));
#else
  // Otherwise, this is just a DCHECK.
  DCHECK(index == 0 || at(index).HasExternalPointer(tag));
#endif
  return at(index).GetExternalPointer(tag);
}

void ExternalPointerTable::Set(ExternalPointerHandle handle, Address value,
                               ExternalPointerTag tag) {
  DCHECK_NE(kNullExternalPointerHandle, handle);
  uint32_t index = HandleToIndex(handle);
  at(index).SetExternalPointer(value, tag);
}

Address ExternalPointerTable::Exchange(ExternalPointerHandle handle,
                                       Address value, ExternalPointerTag tag) {
  DCHECK_NE(kNullExternalPointerHandle, handle);
  uint32_t index = HandleToIndex(handle);
  return at(index).ExchangeExternalPointer(value, tag);
}

ExternalPointerTag ExternalPointerTable::GetTag(
    ExternalPointerHandle handle) const {
  uint32_t index = HandleToIndex(handle);
  return at(index).GetExternalPointerTag();
}

void ExternalPointerTable::Zap(ExternalPointerHandle handle) {
  // Zapping the null entry is a nop. This is useful as we reset the handle of
  // managed resources to the kNullExternalPointerHandle when the entry is
  // deleted. See SweepAndCompact.
  if (handle == kNullExternalPointerHandle) return;
  uint32_t index = HandleToIndex(handle);
  at(index).MakeZappedEntry();
}

ExternalPointerHandle ExternalPointerTable::AllocateAndInitializeEntry(
    Space* space, Address initial_value, ExternalPointerTag tag) {
  DCHECK(space->BelongsTo(this));
  uint32_t index = AllocateEntry(space);
  at(index).MakeExternalPointerEntry(initial_value, tag);

  ExternalPointerHandle handle = IndexToHandle(index);

  // If we allocated the entry for a managed resource, we need to also
  // initialize that resource's back reference to the table entry.
  if (IsManagedExternalPointerType(tag)) {
    ManagedResource* resource =
        reinterpret_cast<ManagedResource*>(initial_value);
    DCHECK_EQ(resource->ept_entry_, kNullExternalPointerHandle);
    resource->owning_table_ = this;
    resource->ept_entry_ = handle;
  }

  return handle;
}

void ExternalPointerTable::Mark(Space* space, ExternalPointerHandle handle,
                                Address handle_location) {
  DCHECK(space->BelongsTo(this));

  // The handle_location must always contain the given handle. Except:
  // - If the slot is lazily-initialized, the handle may transition from the
  //   null handle to a valid handle. In that case, we'll return from this
  //   function early (see below), which is fine since the newly-allocated
  //   entry will already have been marked as alive during allocation.
  // - If the slot is de-initialized, i.e. reset to the null handle. In that
  //   case, we'll still mark the old entry as alive and potentially mark it for
  //   evacuation. Both of these things are fine though: the entry is just kept
  //   alive a little longer and compaction will detect that the slot has been
  //   de-initialized and not perform the evacuation.
#ifdef DEBUG
  ExternalPointerHandle current_handle = base::AsAtomic32::Acquire_Load(
      reinterpret_cast<ExternalPointerHandle*>(handle_location));
  DCHECK(handle == kNullExternalPointerHandle ||
         current_handle == kNullExternalPointerHandle ||
         handle == current_handle);
#endif

  // The null entry is immortal and immutable, so no need to mark it as alive.
  if (handle == kNullExternalPointerHandle) return;

  uint32_t index = HandleToIndex(handle);
  DCHECK(space->Contains(index));

  // If the table is being compacted and the entry is inside the evacuation
  // area, then allocate and set up an evacuation entry for it.
  MaybeCreateEvacuationEntry(space, index, handle_location);

  // Even if the entry is marked for evacuation, it still needs to be marked as
  // alive as it may be visited during sweeping before being evacuation.
  at(index).Mark();
}

// static
bool ExternalPointerTable::IsValidHandle(ExternalPointerHandle handle) {
  uint32_t index = handle >> kExternalPointerIndexShift;
  return handle == index << kExternalPointerIndexShift;
}

// static
uint32_t ExternalPointerTable::HandleToIndex(ExternalPointerHandle handle) {
  DCHECK(IsValidHandle(handle));
  uint32_t index = handle >> kExternalPointerIndexShift;
#if defined(LEAK_SANITIZER)
  // When LSan is active, we use "fat" entries that also store the raw pointer
  // to that LSan can find live references. However, we do this transparently:
  // we simply multiply the handle by two so that `(handle >> index_shift) * 8`
  // still produces the correct offset of the entry in the table. However, this
  // is not secure as an attacker could reference the raw pointer instead of
  // the encoded pointer in an entry, thereby bypassing the type checks. As
  // such, this mode must only be used in testing environments. Alternatively,
  // all places that access external pointer table entries must be made aware
  // that the entries are 16 bytes large when LSan is active.
  index /= 2;
#endif  // LEAK_SANITIZER
  DCHECK_LE(index, kMaxExternalPointers);
  return index;
}

// static
ExternalPointerHandle ExternalPointerTable::IndexToHandle(uint32_t index) {
  DCHECK_LE(index, kMaxExternalPointers);
  ExternalPointerHandle handle = index << kExternalPointerIndexShift;
#if defined(LEAK_SANITIZER)
  handle *= 2;
#endif  // LEAK_SANITIZER
  DCHECK_NE(handle, kNullExternalPointerHandle);
  return handle;
}

void ExternalPointerTable::Space::NotifyExternalPointerFieldInvalidated(
    Address field_address) {
#ifdef DEBUG
  ExternalPointerHandle handle = base::AsAtomic32::Acquire_Load(
      reinterpret_cast<ExternalPointerHandle*>(field_address));
  DCHECK(Contains(HandleToIndex(handle)));
#endif
  AddInvalidatedField(field_address);
}

void ExternalPointerTable::ManagedResource::ZapExternalPointerTableEntry() {
  if (owning_table_) {
    owning_table_->Zap(ept_entry_);
  }
  ept_entry_ = kNullExternalPointerHandle;
}

}  // namespace internal
}  // namespace v8

#endif  // V8_COMPRESS_POINTERS

#endif  // V8_SANDBOX_EXTERNAL_POINTER_TABLE_INL_H_
