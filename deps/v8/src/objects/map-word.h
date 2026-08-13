// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_MAP_WORD_H_
#define V8_OBJECTS_MAP_WORD_H_

#include "src/common/globals.h"
#include "src/objects/tagged.h"

namespace v8::internal {

class Map;
class HeapObject;

// Heap objects typically have a map pointer in their first word.  However,
// during GC other data (e.g. mark bits, forwarding addresses) is sometimes
// encoded in the first word.  The class MapWord is an abstraction of the
// value in a heap object's first word.
//
// When external code space is enabled forwarding pointers are encoded as
// Smi values representing a diff from the source or map word host object
// address in kObjectAlignment chunks. Such a representation has the following
// properties:
// a) it can hold both positive an negative diffs for full pointer compression
//    cage size (HeapObject address has only valuable 30 bits while Smis have
//    31 bits),
// b) it's independent of the pointer compression base and pointer compression
//    scheme.
class MapWord {
 public:
  // Normal state: the map word contains a map pointer.

  // Create a map word from a map pointer.
  static inline MapWord FromMap(const Tagged<Map> map);

  // View this map word as a map pointer.
  inline Tagged<Map> ToMap() const;

  // Scavenge collection: the map word of live objects in the from space
  // contains a forwarding address (a heap object pointer in the to space).

  // True if this map word is a forwarding address for a scavenge
  // collection.  Only valid during a scavenge collection (specifically,
  // when all map words are heap object pointers, i.e. not during a full GC).
  inline bool IsForwardingAddress() const;

  V8_EXPORT_PRIVATE static bool IsMapOrForwarded(Tagged<Map> map);

  // Create a map word from a forwarding address.
  static inline MapWord FromForwardingAddress(Tagged<HeapObject> map_word_host,
                                              Tagged<HeapObject> object);

  // View this map word as a forwarding address.
  inline Tagged<HeapObject> ToForwardingAddress(
      Tagged<HeapObject> map_word_host) const;

  constexpr inline Address ptr() const { return value_; }

  // When pointer compression is enabled, MapWord is uniquely identified by
  // the lower 32 bits. On the other hand full-value comparison is not correct
  // because map word in a forwarding state might have corrupted upper part.
  constexpr bool operator==(MapWord other) const {
    return static_cast<Tagged_t>(ptr()) == static_cast<Tagged_t>(other.ptr());
  }
  constexpr bool operator!=(MapWord other) const {
    return static_cast<Tagged_t>(ptr()) != static_cast<Tagged_t>(other.ptr());
  }

#ifdef V8_MAP_PACKING
  static constexpr Address Pack(Address map) {
    return map ^ Internals::kMapWordXorMask;
  }
  static constexpr Address Unpack(Address mapword) {
    // TODO(wenyuzhao): Clear header metadata.
    return mapword ^ Internals::kMapWordXorMask;
  }
  static constexpr bool IsPacked(Address mapword) {
    return (static_cast<intptr_t>(mapword) & Internals::kMapWordXorMask) ==
               Internals::kMapWordSignature &&
           (0xffffffff00000000 & static_cast<intptr_t>(mapword)) != 0;
  }
#else
  static constexpr bool IsPacked(Address) { return false; }
#endif

 private:
  // HeapObject calls the private constructor and directly reads the value.
  friend class HeapObject;
  template <typename TFieldType, int kFieldOffset, typename CompressionScheme>
  friend class TaggedField;

  explicit constexpr MapWord(Address value) : value_(value) {}

  Address value_;
};

}  // namespace v8::internal

#endif  // V8_OBJECTS_MAP_WORD_H_
