// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-export-wrapper-cache.h"

#include "src/handles/handles-inl.h"
#include "src/objects/fixed-array-inl.h"

namespace v8::internal::wasm {

// static
DirectHandle<WeakFixedArray> WasmExportWrapperCache::New(Isolate* isolate,
                                                         int capacity) {
  // Probing depends on bit-masking the hashes, so the capacity needs to be
  // a power of two.
  DCHECK(base::bits::IsPowerOfTwo(capacity));
  capacity = capacity * kSlotsPerEntry + kReservedSlots;
  DirectHandle<WeakFixedArray> cache =
      WeakFixedArray::New(isolate, capacity, AllocationType::kOld,
                          direct_handle(Smi::FromInt(kUnused), isolate));
  cache->set(kNumUsedElementsIndex, Smi::zero());
  return cache;
}

// static
void WasmExportWrapperCache::Put(Isolate* isolate, CanonicalTypeIndex sig_index,
                                 bool receiver_is_first_param,
                                 DirectHandle<Code> code) {
  uint32_t hash = Hash(sig_index, receiver_is_first_param);
  Tagged<WeakFixedArray> cache = EnsureCapacity(isolate);
  PutInternal<true>(cache, hash, code->wrapper());
}

// static
Tagged<CodeWrapper> WasmExportWrapperCache::Get(Isolate* isolate,
                                                CanonicalTypeIndex sig_index,
                                                bool receiver_is_first_param) {
  Tagged<WeakFixedArray> cache = isolate->heap()->js_to_wasm_wrappers();
  uint32_t hash = Hash(sig_index, receiver_is_first_param);
  uint32_t capacity = Capacity(cache);
  if (capacity == 0) return {};  // Not found (cache is uninitialized).
  uint32_t count = 1;
  for (InternalIndex entry = FirstProbe(hash, capacity);;
       entry = NextProbe(entry, count++, capacity)) {
    DCHECK(count < capacity);
    int index = ToArraySlot(entry);
    Tagged<MaybeWeak<Object>> raw_key = cache->get(index);
    DCHECK(raw_key.IsSmi());
    int key = raw_key.ToSmi().value();
    if (key == kUnused) return {};  // Not found.
    DCHECK_GE(key, 0);
    if (static_cast<uint32_t>(key) != hash) continue;
    Tagged<MaybeWeak<Object>> value = cache->get(index + 1);
    if (value.IsCleared()) return {};  // Not found.
    return Cast<CodeWrapper>(value.GetHeapObjectAssumeWeak());
  }
}

// static
uint32_t WasmExportWrapperCache::Capacity(Tagged<WeakFixedArray> cache) {
  // If we ever add more reserved slots, we'll have to subtract them properly
  // rather than just shifting them out implicitly.
  static_assert((kReservedSlots >> 1) == 0);
  return cache->length() >> 1;
}

// static
Tagged<WeakFixedArray> WasmExportWrapperCache::EnsureCapacity(
    Isolate* isolate) {
  Tagged<WeakFixedArray> cache = isolate->heap()->js_to_wasm_wrappers();
  if (cache->length() == 0) {
    // Lazy initialization.
    static constexpr int kInitialCapacity = 16;
    DirectHandle<WeakFixedArray> new_cache = New(isolate, kInitialCapacity);
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
  DirectHandle<WeakFixedArray> old_cache(cache, isolate);
  Tagged<WeakFixedArray> grown = *New(isolate, capacity * 2);
  DisallowGarbageCollection no_gc;
  cache = *old_cache;
  int num_used_entries = 0;
  for (int i = kReservedSlots; i < cache->length(); i += kSlotsPerEntry) {
    Tagged<MaybeWeak<Object>> raw_key = cache->get(i);
    DCHECK(raw_key.IsSmi());
    int key = raw_key.ToSmi().value();
    if (key == kUnused) continue;
    Tagged<MaybeWeak<Object>> value = cache->get(i + 1);
    if (value.IsCleared()) continue;
    num_used_entries++;
    PutInternal<false>(grown, key, value);
  }
  grown->set(kNumUsedElementsIndex, Smi::FromInt(num_used_entries));
  isolate->heap()->SetJSToWasmWrappers(grown);
  return grown;
}

// static
template <bool entry_may_exist>
void WasmExportWrapperCache::PutInternal(Tagged<WeakFixedArray> cache,
                                         uint32_t hash,
                                         Tagged<MaybeWeak<Object>> value) {
  uint32_t capacity = Capacity(cache);
  uint32_t count = 1;
  for (InternalIndex entry = FirstProbe(hash, capacity);;
       entry = NextProbe(entry, count++, capacity)) {
    DCHECK(count < capacity);
    int index = ToArraySlot(entry);
    Tagged<MaybeWeak<Object>> raw_key = cache->get(index);
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
    cache->set(index + 1, MakeWeak(value));
    return;
  }
}

#ifdef VERIFY_HEAP

// static
void WasmExportWrapperCache::Verify(Heap* heap) {
  Tagged<WeakFixedArray> cache = heap->js_to_wasm_wrappers();
  if (cache->length() == 0) return;
  CHECK(cache->get(kNumUsedElementsIndex).IsSmi());
  for (int i = kReservedSlots; i < cache->length(); i += kSlotsPerEntry) {
    Tagged<MaybeObject> key = cache->get(i);
    Tagged<MaybeObject> value = cache->get(i + 1);
    CHECK(key.IsSmi());
    if (value.IsSmi()) {
      CHECK_EQ(key.ToSmi().value(), kUnused);
      continue;
    }
    if (value.IsCleared()) continue;
    CHECK(value.IsWeak());
    CHECK(IsCodeWrapper(value.GetHeapObjectAssumeWeak()));
  }
}

#endif  // VERIFY_HEAP

}  // namespace v8::internal::wasm
