// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_EMBEDDER_DATA_SLOT_H_
#define V8_OBJECTS_EMBEDDER_DATA_SLOT_H_

#include <type_traits>
#include <utility>

#include "include/cppgc/allocation.h"
#include "include/cppgc/garbage-collected.h"
#include "include/cppgc/visitor.h"
#include "src/common/assert-scope.h"
#include "src/common/globals.h"
#include "src/objects/slots.h"
#include "src/sandbox/isolate.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class EmbedderDataArray;
class JSObject;
class Object;

// A wrapper CppGC object referenced by the CppHeapPointerTable for entries in
// EmbedderDataSlot that store arbitrary C++ pointers.
class EmbedderDataSlotWrapper final
    : public cppgc::GarbageCollected<EmbedderDataSlotWrapper> {
 public:
  EmbedderDataSlotWrapper(void* pointer, ExternalPointerTag tag)
      : pointer_(pointer), tag_(tag) {}

  void* pointer() const { return pointer_; }
  ExternalPointerTag tag() const { return tag_; }

  void Trace(cppgc::Visitor*) const {}

 private:
  void* const pointer_;
  const ExternalPointerTag tag_;
};

// An EmbedderDataSlot instance describes a kEmbedderDataSlotSize field ("slot")
// holding an embedder data which may contain raw aligned pointer or a tagged
// pointer (smi or heap object).
// Its address() is the address of the slot.
// The slot's contents can be read and written using respective load_XX() and
// store_XX() methods.
// Storing heap object through this slot may require triggering write barriers
// so this operation must be done via static store_tagged() methods.
class EmbedderDataSlot
    : public SlotBase<EmbedderDataSlot, Address, kTaggedSize> {
 public:
  // When the sandbox is enabled, an EmbedderDataSlot always contains a valid
  // CppHeap pointer table index (initially, zero) in its "raw" part and a
  // valid tagged value in its 32-bit "tagged" part.
  // When pointer compression is disabled, an EmbedderDataSlot similarly
  // contains two separate fields: a tagged value in its "tagged" part and a
  // raw external pointer in its "raw" part.
  //
  // Layout (sandbox or no pointer compression):
  // +-----------------------------------+-----------------------------------+
  // | Tagged (Smi/Pointer)              | CppHeap Pointer Table Index /     |
  // |                                   | CppHeap Pointer                   |
  // +-----------------------------------+-----------------------------------+
  // ^                                   ^
  // kTaggedPayloadOffset                kCppHeapPointerOffset
  static constexpr int kTaggedPayloadOffset = 0;
  static constexpr int kCppHeapPointerOffset = kTaggedSize;

  static constexpr int kRequiredPtrAlignment = kSmiTagSize;

  EmbedderDataSlot() : SlotBase(kNullAddress) {}
  V8_INLINE EmbedderDataSlot(Tagged<EmbedderDataArray> array, int entry_index);
  V8_INLINE EmbedderDataSlot(Tagged<JSObject> object, int embedder_field_index);

  // Opaque type used for storing raw embedder data.
#ifdef V8_COMPRESS_POINTERS
  using RawData = uint64_t;
#else
  struct RawData {
    Address pointer;
    Address tagged;
    constexpr RawData() : pointer(0), tagged(0) {}
    constexpr RawData(Address pointer, Address tagged)
        : pointer(pointer), tagged(tagged) {}
    constexpr RawData(Address addr) : pointer(addr), tagged(0) {}
  };
#endif

  V8_INLINE void Initialize(Tagged<Object> initial_value);

  V8_INLINE Tagged<Object> load_tagged() const;
  V8_INLINE void store_smi(Tagged<Smi> value);

  // Setting an arbitrary tagged value requires triggering a write barrier
  // which requires separate object and offset values, therefore these static
  // functions also has the target object parameter.
  static V8_INLINE void store_tagged(Tagged<EmbedderDataArray> array,
                                     int entry_index, Tagged<Object> value);
  static V8_INLINE void store_tagged(Tagged<JSObject> object,
                                     int embedder_field_index,
                                     Tagged<Object> value);

  // Tries reinterpret the value as an aligned pointer and sets *out_result to
  // the pointer-like value. Note, that some Smis could still look like an
  // aligned pointers.
  // Returns true on success.
  // When the sandbox is enabled, calling this method when the raw part of the
  // slot does not contain valid external pointer table index is undefined
  // behaviour and most likely result in crashes.
  V8_INLINE bool ToAlignedPointer(IsolateForPointerCompression isolate,
                                  void** out_result,
                                  ExternalPointerTagRange tag_range) const;
  V8_INLINE bool ToAlignedPointer(IsolateForPointerCompression isolate,
                                  void** out_result,
                                  CppHeapPointerTagRange tag_range) const;

  V8_INLINE bool ToGenericAlignedPointer(IsolateForPointerCompression isolate,
                                         void** out_result) const;

  // Deprecated, either use ToAlignedPointer with a `tag_range`, or use
  // `ToGenericAlignedPointer to indicate that the read pointer will not be
  // dereferenced.
  V8_INLINE bool DeprecatedToAlignedPointer(
      IsolateForPointerCompression isolate, void** out_result) const;

  // Returns true if the pointer was successfully stored or false if the pointer
  // was improperly aligned.
  V8_INLINE V8_WARN_UNUSED_RESULT bool store_aligned_pointer(
      Isolate* isolate, Tagged<HeapObject> host, void* ptr,
      CppHeapPointerTag tag);
  template <typename T>
    requires std::is_same_v<EmbedderDataArray, T> || std::is_same_v<JSObject, T>
  static V8_INLINE V8_WARN_UNUSED_RESULT bool store_aligned_pointer(
      Isolate* isolate, DirectHandle<T> host, int entry_or_embedder_field_index,
      void* ptr, ExternalPointerTag tag);

#ifdef V8_COMPRESS_POINTERS
  V8_INLINE void store_tagged_without_barrier(Tagged<Object> value);

  V8_INLINE V8_WARN_UNUSED_RESULT bool store_handle_without_barrier(
      IsolateForPointerCompression isolate, CppHeapPointerHandle handle);
#endif  // V8_COMPRESS_POINTERS

  V8_INLINE bool MustClearDuringSerialization(
      const DisallowGarbageCollection& no_gc);

  // IMPORTANT: load_raw and store_raw are strictly intended for temporary
  // in-place save-and-restore of an object's embedder slots during snapshot
  // serialization (see ContextSerializer::SerializeObjectWithEmbedderFields).
  // They must NEVER be used to copy or duplicate embedder slot contents across
  // different objects or slots, as doing so would duplicate raw
  // CppHeapPointerHandles without updating the CppHeapPointerTable.
  V8_INLINE RawData load_raw(IsolateForPointerCompression isolate,
                             const DisallowGarbageCollection& no_gc) const;

  // IMPORTANT: load_raw and store_raw are strictly intended for temporary
  // in-place save-and-restore of an object's embedder slots during snapshot
  // serialization (see ContextSerializer::SerializeObjectWithEmbedderFields).
  // They must NEVER be used to copy or duplicate embedder slot contents across
  // different objects or slots, as doing so would duplicate raw
  // CppHeapPointerHandles without updating the CppHeapPointerTable.
  V8_INLINE void store_raw(IsolateForPointerCompression isolate, RawData data,
                           const DisallowGarbageCollection& no_gc);

 private:
  static V8_INLINE void clear_cpp_heap_pointer_field(Address slot_address);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_EMBEDDER_DATA_SLOT_H_
