// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_NUMBER_STRING_CACHE_INL_H_
#define V8_OBJECTS_NUMBER_STRING_CACHE_INL_H_

#include "src/objects/number-string-cache.h"
// Include the non-inl header before the rest of the headers.

#include "src/base/bits.h"
#include "src/common/globals.h"
#include "src/handles/handles-inl.h"
#include "src/objects/fixed-array-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8::internal {

uint32_t SmiStringCache::capacity() const {
  return Super::capacity() / kEntrySize;
}

InternalIndex SmiStringCache::GetEntryFor(Tagged<Smi> number) const {
  uint32_t hash = static_cast<uint32_t>(number.value());
  uint32_t mask = capacity() - 1;
  return InternalIndex{hash & mask};
}

// static
InternalIndex SmiStringCache::GetEntryFor(Isolate* isolate,
                                          Tagged<Smi> number) {
  return isolate->factory()->smi_string_cache()->GetEntryFor(number);
}

// static
Handle<Object> SmiStringCache::Get(Isolate* isolate, InternalIndex entry,
                                   Tagged<Smi> number) {
  DisallowGarbageCollection no_gc;
  Factory* factory = isolate->factory();
  Tagged<SmiStringCache> cache = *factory->smi_string_cache();
  uint32_t entry_index = entry.as_uint32() * kEntrySize;

  Tagged<Object> key = cache->get(entry_index + kEntryKeyIndex);
  const bool match = (key == number);

  if (V8_UNLIKELY(v8_flags.native_code_counters)) {
    Counters* counters = isolate->counters();
    counters->number_string_cache_smi_probes()->Increment();
    if (!match) {
      counters->number_string_cache_smi_misses()->Increment();
    }
  }

  if (match) {
    auto string = Cast<String>(cache->get(entry_index + kEntryValueIndex));
    return handle(string, isolate);
  }
  return factory->undefined_value();
}

// static
void SmiStringCache::Set(Isolate* isolate, InternalIndex entry,
                         Tagged<Smi> number, DirectHandle<String> string) {
  // Smi::zero() must never be put into this cache, one must read the value
  // from preallocated_number_string_table instead.
  DCHECK_NE(number, kEmptySentinel);

  DirectHandle<SmiStringCache> cache = isolate->factory()->smi_string_cache();
  DCHECK_EQ(entry, cache->GetEntryFor(number));
  uint32_t entry_index = entry.as_uint32() * kEntrySize;

  // Switch to full-size mode if the entry is already occupied.
  if (cache->capacity() == kInitialSize &&
      cache->get(entry_index + kEntryKeyIndex) != kEmptySentinel &&
      !isolate->MemorySaverModeEnabled()) {
    // Allocate full-size cache, recompute the entry and proceed with adding
    // the entry.
    uint32_t full_size =
        base::bits::RoundUpToPowerOfTwo32(v8_flags.smi_string_cache_size);
    CHECK_LT(kInitialSize, full_size);
    CHECK_LE(full_size, kMaxCapacity);
    // TODO(ishell): consider copying entries from previous table.
    cache = SmiStringCache::New(isolate, full_size);
    isolate->heap()->SetSmiStringCache(*cache);

    entry = cache->GetEntryFor(number);
    entry_index = entry.as_uint32() * kEntrySize;
  }
  DisallowGarbageCollection no_gc;
  cache->set(entry_index + kEntryKeyIndex, number);
  cache->set(entry_index + kEntryValueIndex, *string);
}

// static
template <class IsolateT>
DirectHandle<SmiStringCache> SmiStringCache::New(IsolateT* isolate,
                                                 int capacity) {
  std::optional<DisallowGarbageCollection> no_gc;
  DirectHandle<SmiStringCache> result = Cast<SmiStringCache>(
      Allocate(isolate, capacity * kEntrySize, &no_gc, AllocationType::kOld));
  result->Clear();
  return result;
}

// Clears all entried in the table.
void SmiStringCache::Clear() {
  MemsetTagged(RawFieldOfFirstElement(), kEmptySentinel, Super::capacity());
}

// static
InternalIndex DoubleStringCache::GetEntryFor(uint64_t number_bits,
                                             uint32_t capacity) {
  uint32_t hash = static_cast<uint32_t>(number_bits) ^
                  static_cast<uint32_t>(number_bits >> 32);
  uint32_t mask = capacity - 1;
  return InternalIndex{hash & mask};
}

InternalIndex DoubleStringCache::GetEntryFor(uint64_t number_bits) const {
  return GetEntryFor(number_bits, capacity());
}

// static
InternalIndex DoubleStringCache::GetEntryFor(Isolate* isolate,
                                             uint64_t number_bits) {
  return isolate->factory()->double_string_cache()->GetEntryFor(number_bits);
}

// static
Handle<Object> DoubleStringCache::Get(Isolate* isolate,
                                      InternalIndex entry_index,
                                      uint64_t number_bits) {
  DisallowGarbageCollection no_gc;
  Factory* factory = isolate->factory();
  DirectHandle<DoubleStringCache> cache = factory->double_string_cache();
  auto& entry = cache->entries()[entry_index.as_uint32()];

  const bool match = entry.key_.value_as_bits() == number_bits;

  if (V8_UNLIKELY(v8_flags.native_code_counters)) {
    Counters* counters = isolate->counters();
    counters->number_string_cache_double_probes()->Increment();
    if (!match) {
      counters->number_string_cache_double_misses()->Increment();
    }
  }

  if (match) {
    auto string = Cast<String>(entry.value_.load());
    return handle(string, isolate);
  }
  return factory->undefined_value();
}

// static
void DoubleStringCache::Set(Isolate* isolate, InternalIndex entry_index,
                            uint64_t number_bits, DirectHandle<String> string) {
  DirectHandle<DoubleStringCache> cache =
      isolate->factory()->double_string_cache();
  DCHECK_EQ(entry_index, cache->GetEntryFor(number_bits));

  auto* entry = &cache->entries()[entry_index.as_uint32()];

  // Switch to full-size mode if the entry is already occupied.
  if (cache->capacity() == kInitialSize &&
      entry->value_.load() != kEmptySentinel &&
      !isolate->MemorySaverModeEnabled()) {
    // Allocate full-size cache, recompute the entry and proceed with adding
    // the entry.
    uint32_t full_size =
        base::bits::RoundUpToPowerOfTwo32(v8_flags.double_string_cache_size);
    CHECK_LT(kInitialSize, full_size);
    CHECK_LE(full_size, kMaxCapacity);
    cache = DoubleStringCache::New(isolate, full_size);
    isolate->heap()->SetDoubleStringCache(*cache);

    entry_index = cache->GetEntryFor(number_bits);
    entry = &cache->entries()[entry_index.as_uint32()];
  }
  DisallowGarbageCollection no_gc;
  entry->key_.set_value_as_bits(number_bits);
  entry->value_.store(&**cache, *string);
}

// static
template <class IsolateT>
DirectHandle<DoubleStringCache> DoubleStringCache::New(IsolateT* isolate,
                                                       int capacity) {
  DCHECK_GE(capacity, kInitialSize);
  DCHECK_LE(capacity, kMaxCapacity);

  Tagged<DoubleStringCache> xs =
      UncheckedCast<DoubleStringCache>(isolate->factory()->AllocateRawArray(
          SizeFor(capacity), AllocationType::kOld));

  ReadOnlyRoots roots{isolate};
  xs->set_map_after_allocation(isolate, roots.double_string_cache_map(),
                               SKIP_WRITE_BARRIER);
  xs->capacity_ = capacity;
#if TAGGED_SIZE_8_BYTES
  xs->optional_padding_ = 0;
#endif
  xs->Clear();

  return handle(xs, isolate);
}

void DoubleStringCache::Clear() {
  static_assert(sizeof(Entry) % kTaggedSize == 0);
  auto* data = reinterpret_cast<Tagged_t*>(begin());
  MemsetTagged(data, kEmptySentinel,
               capacity() * (sizeof(Entry) / kTaggedSize));
}

}  // namespace v8::internal

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_NUMBER_STRING_CACHE_INL_H_
