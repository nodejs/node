// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_HEAP_WRITE_BARRIER_H_
#define V8_HEAP_HEAP_WRITE_BARRIER_H_

#include "include/v8-internal.h"
#include "src/common/globals.h"

namespace v8 {
namespace internal {

class ArrayBufferExtension;
class Code;
class FixedArray;
class Heap;
class RelocInfo;
class EphemeronHashTable;

// Note: In general it is preferred to use the macros defined in
// object-macros.h.

// Combined write barriers.
void WriteBarrierForCode(Code host, RelocInfo* rinfo, Object value);
void WriteBarrierForCode(Code host, RelocInfo* rinfo, HeapObject value);
void WriteBarrierForCode(Code host);

// Generational write barrier.
void GenerationalBarrier(HeapObject object, ObjectSlot slot, Object value);
void GenerationalBarrier(HeapObject object, ObjectSlot slot, HeapObject value);
void GenerationalBarrier(HeapObject object, MaybeObjectSlot slot,
                         MaybeObject value);
void GenerationalEphemeronKeyBarrier(EphemeronHashTable table, ObjectSlot slot,
                                     Object value);
void GenerationalBarrierForCode(Code host, RelocInfo* rinfo, HeapObject object);

// Marking write barrier.
void MarkingBarrier(HeapObject object, ObjectSlot slot, Object value);
void MarkingBarrier(HeapObject object, ObjectSlot slot, HeapObject value);
void MarkingBarrier(HeapObject object, MaybeObjectSlot slot, MaybeObject value);
void MarkingBarrierForCode(Code host, RelocInfo* rinfo, HeapObject object);

void MarkingBarrierForArrayBufferExtension(HeapObject object,
                                           ArrayBufferExtension* extension);

void MarkingBarrierForDescriptorArray(Heap* heap, HeapObject host,
                                      HeapObject descriptor_array,
                                      int number_of_own_descriptors);

inline bool IsReadOnlyHeapObject(HeapObject object);

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_HEAP_WRITE_BARRIER_H_
