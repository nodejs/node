// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_TAGGED_FIELD_H_
#define V8_OBJECTS_TAGGED_FIELD_H_

#include "src/common/globals.h"

#include "src/objects/objects.h"
#include "src/objects/tagged-value.h"

namespace v8 {
namespace internal {

// This helper static class represents a tagged field of type T at offset
// kFieldOffset inside some host HeapObject.
// For full-pointer mode this type adds no overhead but when pointer
// compression is enabled such class allows us to use proper decompression
// function depending on the field type.
template <typename T, int kFieldOffset = 0>
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

  static inline Address address(HeapObject host, int offset = 0);

  static inline T load(HeapObject host, int offset = 0);
  static inline T load(Isolate* isolate, HeapObject host, int offset = 0);

  static inline void store(HeapObject host, T value);
  static inline void store(HeapObject host, int offset, T value);

  static inline T Relaxed_Load(HeapObject host, int offset = 0);
  static inline T Relaxed_Load(Isolate* isolate, HeapObject host,
                               int offset = 0);

  static inline void Relaxed_Store(HeapObject host, T value);
  static inline void Relaxed_Store(HeapObject host, int offset, T value);

  static inline T Acquire_Load(HeapObject host, int offset = 0);
  static inline T Acquire_Load(Isolate* isolate, HeapObject host,
                               int offset = 0);

  static inline void Release_Store(HeapObject host, T value);
  static inline void Release_Store(HeapObject host, int offset, T value);

  static inline Tagged_t Release_CompareAndSwap(HeapObject host, T old,
                                                T value);

 private:
  static inline Tagged_t* location(HeapObject host, int offset = 0);

  template <typename TOnHeapAddress>
  static inline Address tagged_to_full(TOnHeapAddress on_heap_addr,
                                       Tagged_t tagged_value);

  static inline Tagged_t full_to_tagged(Address value);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_TAGGED_FIELD_H_
