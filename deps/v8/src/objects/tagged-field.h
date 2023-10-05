// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_TAGGED_FIELD_H_
#define V8_OBJECTS_TAGGED_FIELD_H_

#include "src/common/globals.h"
#include "src/common/ptr-compr.h"
#include "src/objects/objects.h"
#include "src/objects/tagged-value.h"

namespace v8::internal {

// This helper static class represents a tagged field of type T at offset
// kFieldOffset inside some host HeapObject.
// For full-pointer mode this type adds no overhead but when pointer
// compression is enabled such class allows us to use proper decompression
// function depending on the field type.
template <typename T, int kFieldOffset = 0,
          typename CompressionScheme = V8HeapCompressionScheme>
class TaggedField : public AllStatic {
 public:
  static_assert(std::is_base_of<Object, T>::value ||
                    std::is_same<MapWord, T>::value ||
                    std::is_same<MaybeObject, T>::value,
                "T must be strong or weak tagged type or MapWord");

  // True for Smi fields.
  static constexpr bool kIsSmi = std::is_base_of<Smi, T>::value;

  // True for HeapObject and MapWord fields. The latter may look like a Smi
  // if it contains forwarding pointer but still requires tagged pointer
  // decompression.
  static constexpr bool kIsHeapObject =
      std::is_base_of<HeapObject, T>::value || std::is_same<MapWord, T>::value;

  // Object subclasses should be wrapped in Tagged<>, otherwise use T directly.
  // TODO(leszeks): Clean this up to be more uniform.
  using PtrType =
      std::conditional_t<std::is_base_of<Object, T>::value, Tagged<T>, T>;

  static inline Address address(Tagged<HeapObject> host, int offset = 0);

  static inline PtrType load(Tagged<HeapObject> host, int offset = 0);
  static inline PtrType load(PtrComprCageBase cage_base,
                             Tagged<HeapObject> host, int offset = 0);

  static inline void store(Tagged<HeapObject> host, PtrType value);
  static inline void store(Tagged<HeapObject> host, int offset, PtrType value);

  static inline PtrType Relaxed_Load(Tagged<HeapObject> host, int offset = 0);
  static inline PtrType Relaxed_Load(PtrComprCageBase cage_base,
                                     Tagged<HeapObject> host, int offset = 0);

  static inline void Relaxed_Store(Tagged<HeapObject> host, PtrType value);
  static inline void Relaxed_Store(Tagged<HeapObject> host, int offset,
                                   PtrType value);

  static inline PtrType Acquire_Load(Tagged<HeapObject> host, int offset = 0);
  static inline PtrType Acquire_Load_No_Unpack(PtrComprCageBase cage_base,
                                               Tagged<HeapObject> host,
                                               int offset = 0);
  static inline PtrType Acquire_Load(PtrComprCageBase cage_base,
                                     Tagged<HeapObject> host, int offset = 0);

  static inline PtrType SeqCst_Load(Tagged<HeapObject> host, int offset = 0);
  static inline PtrType SeqCst_Load(PtrComprCageBase cage_base,
                                    Tagged<HeapObject> host, int offset = 0);

  static inline void Release_Store(Tagged<HeapObject> host, PtrType value);
  static inline void Release_Store(Tagged<HeapObject> host, int offset,
                                   PtrType value);

  static inline void SeqCst_Store(Tagged<HeapObject> host, PtrType value);
  static inline void SeqCst_Store(Tagged<HeapObject> host, int offset,
                                  PtrType value);

  static inline PtrType SeqCst_Swap(Tagged<HeapObject> host, int offset,
                                    PtrType value);
  static inline PtrType SeqCst_Swap(PtrComprCageBase cage_base,
                                    Tagged<HeapObject> host, int offset,
                                    PtrType value);

  static inline Tagged_t Release_CompareAndSwap(Tagged<HeapObject> host,
                                                PtrType old, PtrType value);
  static inline PtrType SeqCst_CompareAndSwap(Tagged<HeapObject> host,
                                              int offset, PtrType old,
                                              PtrType value);

  // Note: Use these *_Map_Word methods only when loading a MapWord from a
  // MapField.
  static inline PtrType Relaxed_Load_Map_Word(PtrComprCageBase cage_base,
                                              Tagged<HeapObject> host);
  static inline void Relaxed_Store_Map_Word(Tagged<HeapObject> host,
                                            PtrType value);
  static inline void Release_Store_Map_Word(Tagged<HeapObject> host,
                                            PtrType value);

 private:
  static inline Tagged_t* location(Tagged<HeapObject> host, int offset = 0);

  template <typename TOnHeapAddress>
  static inline Address tagged_to_full(TOnHeapAddress on_heap_addr,
                                       Tagged_t tagged_value);

  static inline Tagged_t full_to_tagged(Address value);
};

template <typename T, int kFieldOffset, typename CompressionScheme>
class TaggedField<Tagged<T>, kFieldOffset, CompressionScheme>
    : public TaggedField<T, kFieldOffset, CompressionScheme> {};

}  // namespace v8::internal

#endif  // V8_OBJECTS_TAGGED_FIELD_H_
