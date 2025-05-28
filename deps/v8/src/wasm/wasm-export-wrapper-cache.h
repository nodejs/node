// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_EXPORT_WRAPPER_CACHE_H_
#define V8_WASM_WASM_EXPORT_WRAPPER_CACHE_H_

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#include "src/objects/code.h"
#include "src/objects/fixed-array.h"
#include "src/objects/internal-index.h"
#include "src/wasm/canonical-types.h"
#include "src/wasm/value-type.h"

namespace v8::internal::wasm {

// Provides functionality for caching JS-to-Wasm wrappers in an {Isolate}.
// Since these wrappers are JS-compatible {Code} objects living in managed
// code space, the cache is also Isolate-specific. It uses a WeakFixedArray
// as backing store in order to not leak unused wrappers, and a HashTable-
// inspired indexing scheme rather than a dense array layout because we expect
// only a minority of canonical type indices to have a corresponding wrapper.
// For sandbox purposes, the cache itself is untrusted, but calls through the
// wrappers are protected by our regular signature verification scheme.
class WasmExportWrapperCache : public AllStatic {
 public:
  // DCHECK-fails if a caller attempts to overwrite an existing wrapper.
  static void Put(Isolate* isolate, CanonicalTypeIndex sig_index,
                  bool receiver_is_first_param, DirectHandle<Code> code);

  // Returns an empty Tagged if no cache entry was found.
  static Tagged<CodeWrapper> Get(Isolate* isolate, CanonicalTypeIndex sig_index,
                                 bool receiver_is_first_param);

#ifdef VERIFY_HEAP
  static void Verify(Heap* heap);
#endif  // VERIFY_HEAP

  // Defined in cctest/test-run-wasm-wrappers.cc to not bloat shipping binaries.
  static int CountWrappersForTesting(Isolate* isolate);

 private:
  static constexpr int kUnused = -1;

  static constexpr int kNumUsedElementsIndex = 0;
  static constexpr int kReservedSlots = 1;

  // Key (Smi), value (Weak<CodeWrapper>).
  static constexpr int kSlotsPerEntry = 2;

  static DirectHandle<WeakFixedArray> New(Isolate* isolate, int capacity);

  static uint32_t Capacity(Tagged<WeakFixedArray> cache);

  static uint32_t Hash(CanonicalTypeIndex sig_index,
                       bool receiver_is_first_param) {
    // We expect most signatures to get only one wrapper, so putting the
    // {receiver_is_first_param} bit at the top should give good distribution
    // of hash values.
    static_assert(kMaxCanonicalTypes < (1u << 20));
    return ((receiver_is_first_param ? 1 : 0) << 20) | sig_index.index;
  }

  // Maps the n-th entry to an index in the underlying WeakFixedArray.
  static int ToArraySlot(InternalIndex entry) {
    return kReservedSlots + entry.as_int() * kSlotsPerEntry;
  }

  static InternalIndex FirstProbe(uint32_t hash, uint32_t capacity) {
    // {New()} ensures that {capacity} is always a power of two.
    return InternalIndex(hash & (capacity - 1));
  }

  static InternalIndex NextProbe(InternalIndex last, uint32_t number,
                                 uint32_t capacity) {
    // {New()} ensures that {capacity} is always a power of two.
    return InternalIndex((last.as_uint32() + number) & (capacity - 1));
  }

  V8_WARN_UNUSED_RESULT static Tagged<WeakFixedArray> EnsureCapacity(
      Isolate* isolate);

  template <bool entry_may_exist>
  static void PutInternal(Tagged<WeakFixedArray> cache, uint32_t hash,
                          Tagged<MaybeWeak<Object>> value);
};

}  // namespace v8::internal::wasm

#endif  // V8_WASM_WASM_EXPORT_WRAPPER_CACHE_H_
