// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CRANKSHAFT_HYDROGEN_TYPES_H_
#define V8_CRANKSHAFT_HYDROGEN_TYPES_H_

#include <climits>
#include <iosfwd>

#include "src/base/macros.h"

namespace v8 {
namespace internal {

// Forward declarations.
template <typename T> class Handle;
class Object;

#define HTYPE_LIST(V)                               \
  V(Any, 0x0)             /* 0000 0000 0000 0000 */ \
  V(Tagged, 0x1)          /* 0000 0000 0000 0001 */ \
  V(TaggedPrimitive, 0x5) /* 0000 0000 0000 0101 */ \
  V(TaggedNumber, 0xd)    /* 0000 0000 0000 1101 */ \
  V(Smi, 0x1d)            /* 0000 0000 0001 1101 */ \
  V(HeapObject, 0x21)     /* 0000 0000 0010 0001 */ \
  V(HeapPrimitive, 0x25)  /* 0000 0000 0010 0101 */ \
  V(Null, 0x27)           /* 0000 0000 0010 0111 */ \
  V(HeapNumber, 0x2d)     /* 0000 0000 0010 1101 */ \
  V(String, 0x65)         /* 0000 0000 0110 0101 */ \
  V(Boolean, 0xa5)        /* 0000 0000 1010 0101 */ \
  V(Undefined, 0x125)     /* 0000 0001 0010 0101 */ \
  V(JSReceiver, 0x221)    /* 0000 0010 0010 0001 */ \
  V(JSObject, 0x621)      /* 0000 0110 0010 0001 */ \
  V(JSArray, 0xe21)       /* 0000 1110 0010 0001 */ \
  V(None, 0xfff)          /* 0000 1111 1111 1111 */

class HType final {
 public:
  #define DECLARE_CONSTRUCTOR(Name, mask) \
    static HType Name() WARN_UNUSED_RESULT { return HType(k##Name); }
  HTYPE_LIST(DECLARE_CONSTRUCTOR)
  #undef DECLARE_CONSTRUCTOR

  // Return the weakest (least precise) common type.
  HType Combine(HType other) const WARN_UNUSED_RESULT {
    return HType(static_cast<Kind>(kind_ & other.kind_));
  }

  bool Equals(HType other) const WARN_UNUSED_RESULT {
    return kind_ == other.kind_;
  }

  bool IsSubtypeOf(HType other) const WARN_UNUSED_RESULT {
    return Combine(other).Equals(other);
  }

  #define DECLARE_IS_TYPE(Name, mask)               \
    bool Is##Name() const WARN_UNUSED_RESULT {   \
      return IsSubtypeOf(HType::Name());            \
    }
  HTYPE_LIST(DECLARE_IS_TYPE)
  #undef DECLARE_IS_TYPE

  template <class T>
  static HType FromType(typename T::TypeHandle type) WARN_UNUSED_RESULT;
  static HType FromValue(Handle<Object> value) WARN_UNUSED_RESULT;

  friend std::ostream& operator<<(std::ostream& os, const HType& t);

 private:
  enum Kind {
    #define DECLARE_TYPE(Name, mask) k##Name = mask,
    HTYPE_LIST(DECLARE_TYPE)
    #undef DECLARE_TYPE
    LAST_KIND = kNone
  };

  // Make sure type fits in int16.
  STATIC_ASSERT(LAST_KIND < (1 << (CHAR_BIT * sizeof(int16_t))));

  explicit HType(Kind kind) : kind_(kind) { }

  int16_t kind_;
};


std::ostream& operator<<(std::ostream& os, const HType& t);
}  // namespace internal
}  // namespace v8

#endif  // V8_CRANKSHAFT_HYDROGEN_TYPES_H_
