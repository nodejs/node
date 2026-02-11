// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_HEAP_WRITE_BARRIER_H_
#define V8_HEAP_HEAP_WRITE_BARRIER_H_

// Clients of this interface shouldn't depend on lots of heap internals.
// Do not include anything from src/heap here!

#include "include/v8-internal.h"
#include "src/common/globals.h"
#include "src/objects/cpp-heap-object-wrapper.h"
#include "src/objects/heap-object.h"
#include "src/objects/objects.h"

namespace v8::internal {

template <typename T, IndirectPointerTag kTag>
class TrustedPointerMember;

class ArrayBufferExtension;
class InstructionStream;
class DescriptorArray;
class EphemeronHashTable;
class FixedArray;
class Heap;
class JSArrayBuffer;
class Map;
class MarkCompactCollector;
class MarkingBarrier;
class MemoryChunk;
class RelocInfo;

// A scoped object that determines the write barrier mode for a given object.
// The mode is only valid for the lifetime of this object.
class V8_EXPORT_PRIVATE V8_NODISCARD WriteBarrierModeScope final {
 public:
  explicit WriteBarrierModeScope(WriteBarrierMode mode);
  explicit WriteBarrierModeScope(Tagged<HeapObject> object,
                                 WriteBarrierMode mode);

  ~WriteBarrierModeScope();

  WriteBarrierModeScope(const WriteBarrierModeScope&) = delete;
  WriteBarrierModeScope& operator=(const WriteBarrierModeScope&) = delete;

  WriteBarrierModeScope(WriteBarrierModeScope&& other) V8_NOEXCEPT
      : object_(other.object_),
        mode_(other.mode_) {
    other.mode_ = std::nullopt;
    other.object_ = Tagged<HeapObject>();
  }
  WriteBarrierModeScope& operator=(WriteBarrierModeScope&&) = delete;

  WriteBarrierMode operator*() { return *mode_; }

 private:
  Tagged<HeapObject> object_;
  std::optional<WriteBarrierMode> mode_;
};

// Write barrier interface. It's preferred to use the macros defined in
// `object-macros.h`.
//
// Refer to the `ForFoo()` versions which will dispatch to all relevant barriers
// instead of emiting marking, compaction, generational, and shared barriers
// separately.
class V8_EXPORT_PRIVATE WriteBarrier final {
 public:
  // Trampolines for generated code. Have to take raw addresses.
  static void EphemeronKeyWriteBarrierFromCode(Address raw_object,
                                               Address key_slot_address,
                                               Isolate* isolate);
  static int MarkingFromCode(Address raw_host, Address raw_slot);
  static int IndirectPointerMarkingFromCode(Address raw_host, Address raw_slot,
                                            Address raw_tag);
  static int SharedMarkingFromCode(Address raw_host, Address raw_slot);
  static int SharedFromCode(Address raw_host, Address raw_slot);

  static inline WriteBarrierModeScope GetWriteBarrierModeForObject(
      Tagged<HeapObject> object, const DisallowGarbageCollection& promise);

  template <typename T>
  static inline void ForValue(Tagged<HeapObject> host, MaybeObjectSlot slot,
                              Tagged<T> value, WriteBarrierMode mode);
  template <typename T>
  static inline void ForValue(HeapObjectLayout* host, TaggedMemberBase* slot,
                              Tagged<T> value, WriteBarrierMode mode);
  static inline void ForEphemeronHashTable(Tagged<EphemeronHashTable> host,
                                           ObjectSlot slot,
                                           Tagged<Object> value,
                                           WriteBarrierMode mode);
  static inline void ForRelocInfo(Tagged<InstructionStream> host,
                                  RelocInfo* rinfo, Tagged<HeapObject> value,
                                  WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  static inline void ForDescriptorArray(Tagged<DescriptorArray>,
                                        int number_of_own_descriptors);
  static inline void ForArrayBufferExtension(Tagged<JSArrayBuffer> host,
                                             ArrayBufferExtension* extension);
  static inline void ForExternalPointer(
      Tagged<HeapObject> host, ExternalPointerSlot slot,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  static inline void ForIndirectPointer(
      Tagged<HeapObject> host, IndirectPointerSlot slot,
      Tagged<HeapObject> value, WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  template <typename T, IndirectPointerTag kTag>
  static inline void ForIndirectPointer(
      HeapObjectLayout* host, TrustedPointerMember<T, kTag>* slot,
      Tagged<T> value, WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  static inline void ForProtectedPointer(
      Tagged<TrustedObject> host, ProtectedPointerSlot slot,
      Tagged<TrustedObject> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  static inline void ForCppHeapPointer(
      Tagged<CppHeapPointerWrapperObjectT> host, CppHeapPointerSlot slot,
      void* value);
  static inline void ForJSDispatchHandle(
      Tagged<HeapObject> host, JSDispatchHandle handle,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  // Executes generational and/or marking write barrier for a [start, end) range
  // of non-weak slots inside |object|.
  template <typename TSlot>
  static void ForRange(Heap* heap, Tagged<HeapObject> object, TSlot start,
                       TSlot end);

  static MarkingBarrier* SetForThread(MarkingBarrier* marking_barrier);
  static MarkingBarrier* CurrentMarkingBarrier(
      Tagged<HeapObject> verification_candidate);

  // Invoked from traced handles where no host object is available.
  static inline void MarkingFromTracedHandle(Tagged<Object> value);

  static inline void GenerationalForRelocInfo(Tagged<InstructionStream> host,
                                              RelocInfo* rinfo,
                                              Tagged<HeapObject> object);
  static inline void SharedForRelocInfo(Tagged<InstructionStream> host,
                                        RelocInfo*, Tagged<HeapObject> value);

  static inline void MarkingForTesting(Tagged<HeapObject> host, ObjectSlot,
                                       Tagged<Object> value);

#if V8_VERIFY_WRITE_BARRIERS
  template <typename T>
  static inline bool IsRequired(Tagged<HeapObject> host, T value);
  template <typename T>
  static inline bool IsRequired(const HeapObjectLayout* host, T value);

  static bool VerifyDispatchHandleMarkingState(Tagged<HeapObject> host,
                                               JSDispatchHandle value,
                                               WriteBarrierMode mode);
#endif  // V8_VERIFY_WRITE_BARRIERS

  // In native code we skip any further write barrier processing if the hosts
  // page does not have the kPointersFromHereAreInterestingMask. Users of this
  // variable rely on that fact.
  static constexpr bool kUninterestingPagesCanBeSkipped = true;

 private:
  static inline bool IsSkipWriteBarrierMode(WriteBarrierMode mode) {
    static_assert(SKIP_WRITE_BARRIER == 0 && SKIP_WRITE_BARRIER_SCOPE == 1 &&
                  SKIP_WRITE_BARRIER_FOR_GC == 2 &&
                  UNSAFE_SKIP_WRITE_BARRIER == 3);
    return mode <= UNSAFE_SKIP_WRITE_BARRIER;
  }

  static inline WriteBarrierMode ComputeWriteBarrierModeForObject(
      Tagged<HeapObject> object, const DisallowGarbageCollection& promise);

#if V8_VERIFY_WRITE_BARRIERS
  template <typename T>
  static void VerifySkipWriteBarrier(Tagged<HeapObject> host, Tagged<T> value,
                                     WriteBarrierMode mode);
#endif  // V8_VERIFY_WRITE_BARRIERS

#if V8_VERIFY_WRITE_BARRIERS
  static bool IsMostRecentYoungAllocation(Address object);

  template <typename HostType, typename ValueType>
  static inline bool IsRequiredCommon(HostType host, ValueType value);
#endif

  static bool PageFlagsAreConsistent(Tagged<HeapObject> object);

  static inline bool IsMarking(Tagged<HeapObject> object);

  static inline void Marking(Tagged<HeapObject> host, HeapObjectSlot,
                             Tagged<HeapObject> value);
  static inline void Marking(Tagged<HeapObject> host, MaybeObjectSlot,
                             Tagged<MaybeObject> value);
  static inline void MarkingForRelocInfo(Tagged<InstructionStream> host,
                                         RelocInfo*, Tagged<HeapObject> value);
  static inline void Marking(Tagged<HeapObject> host, ExternalPointerSlot slot);
  static inline void Marking(Tagged<HeapObject> host, IndirectPointerSlot slot);
  static inline void Marking(Tagged<TrustedObject> host,
                             ProtectedPointerSlot slot,
                             Tagged<TrustedObject> value);
  static inline void Marking(Tagged<HeapObject> host, JSDispatchHandle handle);

  template <RecordYoungSlot kRecordYoung = RecordYoungSlot::kNo>
  static void MarkingSlow(Tagged<HeapObject> host, HeapObjectSlot,
                          Tagged<HeapObject> value);
  static void MarkingSlow(Tagged<InstructionStream> host, RelocInfo*,
                          Tagged<HeapObject> value);
  static void MarkingSlow(Tagged<JSArrayBuffer> host, ArrayBufferExtension*);
  static void MarkingSlow(Tagged<DescriptorArray>,
                          int number_of_own_descriptors);
  static void MarkingSlow(Tagged<HeapObject> host, ExternalPointerSlot slot);
  static void MarkingSlow(Tagged<HeapObject> host, IndirectPointerSlot slot);
  static void MarkingSlow(Tagged<TrustedObject> host, ProtectedPointerSlot slot,
                          Tagged<TrustedObject> value);
  static void MarkingSlow(Tagged<HeapObject> host, JSDispatchHandle handle);
  static void MarkingSlowFromTracedHandle(Tagged<HeapObject> value);
  static void MarkingSlowFromCppHeapWrappable(
      Heap* heap, Tagged<CppHeapPointerWrapperObjectT> host,
      CppHeapPointerSlot slot, void* object);

  static void GenerationalBarrierSlow(Tagged<HeapObject> object, Address slot,
                                      Tagged<HeapObject> value);
  static inline void GenerationalBarrierForCppHeapPointer(
      Tagged<CppHeapPointerWrapperObjectT> host, void* value);

  static void SharedSlow(Tagged<TrustedObject> host, ProtectedPointerSlot slot,
                         Tagged<TrustedObject> value);
  static void SharedSlow(Tagged<InstructionStream> host, RelocInfo*,
                         Tagged<HeapObject> value);
  static void SharedHeapBarrierSlow(Tagged<HeapObject> object, Address slot);

  static inline void CombinedWriteBarrierInternal(Tagged<HeapObject> host,
                                                  HeapObjectSlot slot,
                                                  Tagged<HeapObject> value,
                                                  WriteBarrierMode mode);
  static inline void CombinedWriteBarrierInternalForStickyMarkbits(
      Tagged<HeapObject> host, HeapObjectSlot slot, Tagged<HeapObject> value,
      WriteBarrierMode mode);
  // Either marking is on, or we are dealing with an old (non-shared) to
  // young/shared write.
  static void CombinedWriteBarrierInternalSlow(Tagged<HeapObject> host,
                                               MemoryChunk* host_chunk,
                                               HeapObjectSlot slot,
                                               Tagged<HeapObject> value,
                                               MemoryChunk* value_chunk);
  static void CombinedGenerationalAndSharedBarrierSlow(
      Tagged<HeapObject> object, Address slot, Tagged<HeapObject> value);
  static void CombinedGenerationalAndSharedEphemeronBarrierSlow(
      Tagged<EphemeronHashTable> table, Address slot, Tagged<HeapObject> value);
  static void GenerationalBarrierForCodeSlow(Tagged<InstructionStream> host,
                                             RelocInfo* rinfo,
                                             Tagged<HeapObject> value);
};

}  // namespace v8::internal

#endif  // V8_HEAP_HEAP_WRITE_BARRIER_H_
