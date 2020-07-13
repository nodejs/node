// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_HEAP_OBJECT_HEADER_INL_H_
#define V8_HEAP_CPPGC_HEAP_OBJECT_HEADER_INL_H_

#include "include/cppgc/allocation.h"
#include "include/cppgc/internal/gc-info.h"
#include "src/base/atomic-utils.h"
#include "src/base/logging.h"
#include "src/base/macros.h"
#include "src/heap/cppgc/gc-info-table.h"
#include "src/heap/cppgc/globals.h"
#include "src/heap/cppgc/heap-object-header.h"

namespace cppgc {
namespace internal {

// static
HeapObjectHeader& HeapObjectHeader::FromPayload(void* payload) {
  return *reinterpret_cast<HeapObjectHeader*>(static_cast<Address>(payload) -
                                              sizeof(HeapObjectHeader));
}

// static
const HeapObjectHeader& HeapObjectHeader::FromPayload(const void* payload) {
  return *reinterpret_cast<const HeapObjectHeader*>(
      static_cast<ConstAddress>(payload) - sizeof(HeapObjectHeader));
}

HeapObjectHeader::HeapObjectHeader(size_t size, GCInfoIndex gc_info_index) {
#if defined(V8_TARGET_ARCH_64_BIT)
  USE(padding_);
#endif  // defined(V8_TARGET_ARCH_64_BIT)
  DCHECK_LT(gc_info_index, GCInfoTable::kMaxIndex);
  DCHECK_EQ(0u, size & (sizeof(HeapObjectHeader) - 1));
  DCHECK_GE(kMaxSize, size);
  encoded_high_ = GCInfoIndexField::encode(gc_info_index);
  encoded_low_ = EncodeSize(size);
  DCHECK(IsInConstruction());
#ifdef DEBUG
  CheckApiConstants();
#endif  // DEBUG
}

Address HeapObjectHeader::Payload() const {
  return reinterpret_cast<Address>(const_cast<HeapObjectHeader*>(this)) +
         sizeof(HeapObjectHeader);
}

template <HeapObjectHeader::AccessMode mode>
GCInfoIndex HeapObjectHeader::GetGCInfoIndex() const {
  const uint16_t encoded =
      LoadEncoded<mode, EncodedHalf::kHigh, std::memory_order_acquire>();
  return GCInfoIndexField::decode(encoded);
}

template <HeapObjectHeader::AccessMode mode>
size_t HeapObjectHeader::GetSize() const {
  // Size is immutable after construction while either marking or sweeping
  // is running so relaxed load (if mode == kAtomic) is enough.
  uint16_t encoded_low_value =
      LoadEncoded<mode, EncodedHalf::kLow, std::memory_order_relaxed>();
  const size_t size = DecodeSize(encoded_low_value);
  return size;
}

void HeapObjectHeader::SetSize(size_t size) {
  DCHECK(!IsMarked());
  encoded_low_ |= EncodeSize(size);
}

template <HeapObjectHeader::AccessMode mode>
bool HeapObjectHeader::IsLargeObject() const {
  return GetSize<mode>() == kLargeObjectSizeInHeader;
}

template <HeapObjectHeader::AccessMode mode>
bool HeapObjectHeader::IsInConstruction() const {
  const uint16_t encoded =
      LoadEncoded<mode, EncodedHalf::kHigh, std::memory_order_acquire>();
  return !FullyConstructedField::decode(encoded);
}

void HeapObjectHeader::MarkAsFullyConstructed() {
  MakeGarbageCollectedTraitInternal::MarkObjectAsFullyConstructed(Payload());
}

template <HeapObjectHeader::AccessMode mode>
bool HeapObjectHeader::IsMarked() const {
  const uint16_t encoded =
      LoadEncoded<mode, EncodedHalf::kLow, std::memory_order_relaxed>();
  return MarkBitField::decode(encoded);
}

template <HeapObjectHeader::AccessMode mode>
void HeapObjectHeader::Unmark() {
  DCHECK(IsMarked<mode>());
  StoreEncoded<mode, EncodedHalf::kLow, std::memory_order_relaxed>(
      MarkBitField::encode(false), MarkBitField::kMask);
}

bool HeapObjectHeader::TryMarkAtomic() {
  auto* atomic_encoded = v8::base::AsAtomicPtr(&encoded_low_);
  uint16_t old_value = atomic_encoded->load(std::memory_order_relaxed);
  const uint16_t new_value = old_value | MarkBitField::encode(true);
  if (new_value == old_value) {
    return false;
  }
  return atomic_encoded->compare_exchange_strong(old_value, new_value,
                                                 std::memory_order_relaxed);
}

template <HeapObjectHeader::AccessMode mode>
bool HeapObjectHeader::IsFree() const {
  return GetGCInfoIndex() == kFreeListGCInfoIndex;
}

bool HeapObjectHeader::IsFinalizable() const {
  const GCInfo& gc_info = GlobalGCInfoTable::GCInfoFromIndex(GetGCInfoIndex());
  return gc_info.finalize;
}

template <HeapObjectHeader::AccessMode mode, HeapObjectHeader::EncodedHalf part,
          std::memory_order memory_order>
uint16_t HeapObjectHeader::LoadEncoded() const {
  const uint16_t& half =
      part == EncodedHalf::kLow ? encoded_low_ : encoded_high_;
  if (mode == AccessMode::kNonAtomic) return half;
  return v8::base::AsAtomicPtr(&half)->load(memory_order);
}

template <HeapObjectHeader::AccessMode mode, HeapObjectHeader::EncodedHalf part,
          std::memory_order memory_order>
void HeapObjectHeader::StoreEncoded(uint16_t bits, uint16_t mask) {
  // Caveat: Not all changes to HeapObjectHeader's bitfields go through
  // StoreEncoded. The following have their own implementations and need to be
  // kept in sync:
  // - HeapObjectHeader::TryMarkAtomic
  // - MarkObjectAsFullyConstructed (API)
  DCHECK_EQ(0u, bits & ~mask);
  uint16_t& half = part == EncodedHalf::kLow ? encoded_low_ : encoded_high_;
  if (mode == AccessMode::kNonAtomic) {
    half = (half & ~mask) | bits;
    return;
  }
  // We don't perform CAS loop here assuming that only none of the info that
  // shares the same encoded halfs change at the same time.
  auto* atomic_encoded = v8::base::AsAtomicPtr(&half);
  uint16_t value = atomic_encoded->load(std::memory_order_relaxed);
  value = (value & ~mask) | bits;
  atomic_encoded->store(value, memory_order);
}

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_HEAP_OBJECT_HEADER_INL_H_
