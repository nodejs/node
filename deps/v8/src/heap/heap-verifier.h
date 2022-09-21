// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_HEAP_VERIFIER_H_
#define V8_HEAP_HEAP_VERIFIER_H_

#include "src/common/globals.h"
#include "src/flags/flags.h"
#include "src/heap/read-only-heap.h"
#include "src/objects/map.h"

namespace v8 {
namespace internal {

class Heap;
class ReadOnlyHeap;

class HeapVerifier final {
 public:
#ifdef VERIFY_HEAP
  // Verify the heap is in its normal state before or after a GC.
  V8_EXPORT_PRIVATE static void VerifyHeap(Heap* heap);

  // Verify the read-only heap after all read-only heap objects have been
  // created.
  static void VerifyReadOnlyHeap(Heap* heap);

  // Verify the shared heap, initiating from a client heap. This performs a
  // global safepoint, then the normal heap verification.
  static void VerifySharedHeap(Heap* heap, Isolate* initiator);

  // Verifies OLD_TO_NEW and OLD_TO_SHARED remembered sets for this object.
  static void VerifyRememberedSetFor(Heap* heap, HeapObject object);

  // Checks that this is a safe map transition.
  V8_EXPORT_PRIVATE static void VerifySafeMapTransition(Heap* heap,
                                                        HeapObject object,
                                                        Map new_map);

  // This function checks that either
  // - the map transition is safe,
  // - or it was communicated to GC using NotifyObjectLayoutChange.
  V8_EXPORT_PRIVATE static void VerifyObjectLayoutChange(Heap* heap,
                                                         HeapObject object,
                                                         Map new_map);

#else
  static void VerifyHeap(Heap* heap) {}
  static void VerifyReadOnlyHeap(Heap* heap) {}
  static void VerifySharedHeap(Heap* heap, Isolate* initiator) {}
  static void VerifyRememberedSetFor(Heap* heap, HeapObject object) {}
  static void VerifySafeMapTransition(Heap* heap, HeapObject object,
                                      Map new_map) {}
  static void VerifyObjectLayoutChange(Heap* heap, HeapObject object,
                                       Map new_map) {}
#endif

  V8_INLINE static void VerifyHeapIfEnabled(Heap* heap) {
    if (v8_flags.verify_heap) {
      VerifyHeap(heap);
    }
  }

 private:
  HeapVerifier();
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_HEAP_VERIFIER_H_
