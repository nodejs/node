// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_HEAP_WRITE_BARRIER_H_
#define V8_HEAP_HEAP_WRITE_BARRIER_H_

#include "include/v8-internal.h"
#include "src/base/optional.h"
#include "src/common/globals.h"

namespace v8 {
namespace internal {

class ArrayBufferExtension;
class Code;
class DescriptorArray;
class EphemeronHashTable;
class FixedArray;
class Heap;
class JSArrayBuffer;
class Map;
class MarkCompactCollector;
class MarkingBarrier;
class RelocInfo;

// Note: In general it is preferred to use the macros defined in
// object-macros.h.

// Combined write barriers.
void WriteBarrierForCode(Code host, RelocInfo* rinfo, Object value);
void WriteBarrierForCode(Code host, RelocInfo* rinfo, HeapObject value);
void WriteBarrierForCode(Code host);

// Generational write barrier.
void GenerationalBarrier(HeapObject object, ObjectSlot slot, Object value);
void GenerationalBarrier(HeapObject object, ObjectSlot slot, Code value);
void GenerationalBarrier(HeapObject object, ObjectSlot slot, HeapObject value);
void GenerationalBarrier(HeapObject object, MaybeObjectSlot slot,
                         MaybeObject value);
void GenerationalEphemeronKeyBarrier(EphemeronHashTable table, ObjectSlot slot,
                                     Object value);
void GenerationalBarrierForCode(Code host, RelocInfo* rinfo, HeapObject object);

inline bool IsReadOnlyHeapObject(HeapObject object);

class V8_EXPORT_PRIVATE WriteBarrier {
 public:
  static inline void Marking(HeapObject host, ObjectSlot, Object value);
  static inline void Marking(HeapObject host, HeapObjectSlot, HeapObject value);
  static inline void Marking(HeapObject host, MaybeObjectSlot,
                             MaybeObject value);
  static inline void Marking(Code host, RelocInfo*, HeapObject value);
  static inline void Marking(JSArrayBuffer host, ArrayBufferExtension*);
  static inline void Marking(DescriptorArray, int number_of_own_descriptors);
  // It is invoked from generated code and has to take raw addresses.
  static int MarkingFromCode(Address raw_host, Address raw_slot);

  static void SetForThread(MarkingBarrier*);
  static void ClearForThread(MarkingBarrier*);

  static MarkingBarrier* CurrentMarkingBarrier(Heap* heap);

 private:
  static void MarkingSlow(Heap* heap, HeapObject host, HeapObjectSlot,
                          HeapObject value);
  static void MarkingSlow(Heap* heap, Code host, RelocInfo*, HeapObject value);
  static void MarkingSlow(Heap* heap, JSArrayBuffer host,
                          ArrayBufferExtension*);
  static void MarkingSlow(Heap* heap, DescriptorArray,
                          int number_of_own_descriptors);
  static inline base::Optional<Heap*> GetHeapIfMarking(HeapObject object);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_HEAP_WRITE_BARRIER_H_
