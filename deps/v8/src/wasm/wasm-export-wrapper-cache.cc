// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-export-wrapper-cache.h"

#include "src/handles/handles-inl.h"
#include "src/objects/code-inl.h"
#include "src/objects/fixed-array-inl.h"

namespace v8::internal::wasm {

// static
DirectHandle<ProtectedWeakFixedArray> WasmExportWrapperCache::New(
    Isolate* isolate, uint32_t capacity) {
  // Probing depends on bit-masking the hashes, so the capacity needs to be
  // a power of two.
  DCHECK(base::bits::IsPowerOfTwo(capacity));
  capacity = capacity * kSlotsPerEntry + kReservedSlots;
  DirectHandle<ProtectedWeakFixedArray> cache =
      ProtectedWeakFixedArray::New(isolate, capacity);
  cache->set(kNumUsedElementsIndex, Smi::zero());
  Tagged<Smi> unused = Smi::FromInt(kUnused);
  for (uint32_t i = kReservedSlots; i < capacity; ++i) {
    cache->set(i, unused);
  }
  return cache;
}

// static
void WasmExportWrapperCache::Put(Isolate* isolate, CanonicalTypeIndex sig_index,
                                 DirectHandle<Code> code) {
  DCHECK_EQ(code->kind(), CodeKind::JS_TO_WASM_FUNCTION);
  uint32_t hash = Hash(sig_index);
  Tagged<ProtectedWeakFixedArray> cache = EnsureCapacity(isolate);
  PutInternal<true>(cache, hash, MakeWeak(*code));
}

// static
Tagged<Code> WasmExportWrapperCache::Get(Isolate* isolate,
                                         CanonicalTypeIndex sig_index) {
  Tagged<ProtectedWeakFixedArray> cache =
      isolate->heap()->js_to_wasm_wrappers();
  uint32_t hash = Hash(sig_index);
  uint32_t capacity = Capacity(cache);
  if (capacity == 0) return {};  // Not found (cache is uninitialized).
  uint32_t count = 1;
  for (InternalIndex entry = FirstProbe(hash, capacity);;
       entry = NextProbe(entry, count++, capacity)) {
    DCHECK(count < capacity);
    int index = ToArraySlot(entry);
    Tagged<ProtectedWeakFixedArray::ElementT> raw_key = cache->get(index);
    DCHECK(raw_key.IsSmi());
    int key = raw_key.ToSmi().value();
    if (key == kUnused) return {};  // Not found.
    DCHECK_GE(key, 0);
    if (static_cast<uint32_t>(key) != hash) continue;
    Tagged<ProtectedWeakFixedArray::ElementT> value = cache->get(index + 1);
    if (value.IsCleared()) return {};  // Not found.
    return TrustedCast<Code>(value.GetHeapObjectAssumeWeak());
  }
}

// static
uint32_t WasmExportWrapperCache::Capacity(
    Tagged<ProtectedWeakFixedArray> cache) {
  // If we ever add more reserved slots, we'll have to subtract them properly
  // rather than just shifting them out implicitly.
  static_assert((kReservedSlots >> 1) == 0);
  return cache->ulength().value() >> 1;
}

// static
Tagged<ProtectedWeakFixedArray> WasmExportWrapperCache::EnsureCapacity(
    Isolate* isolate) {
  Tagged<ProtectedWeakFixedArray> cache =
      isolate->heap()->js_to_wasm_wrappers();
  uint32_t cache_len = cache->ulength().value();
  if (cache_len == 0) {
    // Lazy initialization.
    static constexpr uint32_t kInitialCapacity = 16;
    DirectHandle<ProtectedWeakFixedArray> new_cache =
        New(isolate, kInitialCapacity);
    isolate->heap()->SetJSToWasmWrappers(*new_cache);
    return *new_cache;
  }
  uint32_t capacity = Capacity(cache);
  uint32_t used = cache->get(kNumUsedElementsIndex).ToSmi().value();
  // We allow a maximum usage of 50% plus one entry, otherwise grow.
  // It can happen that we find many cleared entries during growing, so the
  // returned grown cache may be nearly empty. That's unlikely to happen
  // often, and if it happens a few times that's not a problem.
  if (used < capacity / 2) return cache;
  DirectHandle<ProtectedWeakFixedArray> old_cache(cache, isolate);
  Tagged<ProtectedWeakFixedArray> grown = *New(isolate, capacity * 2);
  DisallowGarbageCollection no_gc;
  cache = *old_cache;
  uint32_t num_used_entries = 0;
  for (uint32_t i = kReservedSlots; i < cache_len; i += kSlotsPerEntry) {
    Tagged<ProtectedWeakFixedArray::ElementT> raw_key = cache->get(i);
    DCHECK(raw_key.IsSmi());
    int key = raw_key.ToSmi().value();
    if (key == kUnused) continue;
    Tagged<ProtectedWeakFixedArray::ElementT> value = cache->get(i + 1);
    if (value.IsCleared()) continue;
    num_used_entries++;
    PutInternal<false>(grown, key, value);
  }
  grown->set(kNumUsedElementsIndex, Smi::FromUInt(num_used_entries));
  isolate->heap()->SetJSToWasmWrappers(grown);
  return grown;
}

// static
template <bool entry_may_exist>
void WasmExportWrapperCache::PutInternal(
    Tagged<ProtectedWeakFixedArray> cache, uint32_t hash,
    Tagged<ProtectedWeakFixedArray::ElementT> value) {
  DCHECK(value.IsWeak());
  uint32_t capacity = Capacity(cache);
  uint32_t count = 1;
  for (InternalIndex entry = FirstProbe(hash, capacity);;
       entry = NextProbe(entry, count++, capacity)) {
    DCHECK(count < capacity);
    int index = ToArraySlot(entry);
    Tagged<ProtectedWeakFixedArray::ElementT> raw_key = cache->get(index);
    DCHECK(raw_key.IsSmi());
    int current_key = raw_key.ToSmi().value();
    if (current_key == kUnused) {
      cache->set(index, Smi::FromInt(hash));
      int used_elements = cache->get(kNumUsedElementsIndex).ToSmi().value();
      cache->set(kNumUsedElementsIndex, Smi::FromInt(used_elements + 1));
    } else if constexpr (entry_may_exist) {
      if (static_cast<uint32_t>(current_key) != hash) continue;
      // Key exists; value must be cleared, otherwise we shouldn't be
      // trying to put a new value.
      DCHECK(cache->get(index + 1).IsCleared());
    } else {
      DCHECK_NE(static_cast<uint32_t>(current_key), hash);
      continue;
    }

    // We get here when we've found the right entry (unused or cleared).
    cache->set(index + 1, value);
    return;
  }
}

#ifdef VERIFY_HEAP

// static
void WasmExportWrapperCache::Verify(Heap* heap) {
  Tagged<ProtectedWeakFixedArray> cache = heap->js_to_wasm_wrappers();
  uint32_t cache_len = cache->ulength().value();
  if (cache_len == 0) return;
  CHECK(cache->get(kNumUsedElementsIndex).IsSmi());
  for (uint32_t i = kReservedSlots; i < cache_len; i += kSlotsPerEntry) {
    Tagged<ProtectedWeakFixedArray::ElementT> key = cache->get(i);
    Tagged<ProtectedWeakFixedArray::ElementT> value = cache->get(i + 1);
    CHECK(key.IsSmi());
    if (value.IsSmi()) {
      CHECK_EQ(key.ToSmi().value(), kUnused);
      continue;
    }
    if (value.IsCleared()) continue;
    CHECK(value.IsWeak());
    Tagged<HeapObject> obj = value.GetHeapObjectAssumeWeak();
    CHECK(IsCode(obj));
    CHECK_EQ(TrustedCast<Code>(obj)->kind(), CodeKind::JS_TO_WASM_FUNCTION);
  }
}

#endif  // VERIFY_HEAP

}  // namespace v8::internal::wasm
