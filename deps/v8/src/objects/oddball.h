// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_ODDBALL_H_
#define V8_OBJECTS_ODDBALL_H_

#include "src/objects/primitive-heap-object.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/oddball-tq.inc"

// The Oddball describes objects null, undefined, true, and false.
class Oddball : public PrimitiveHeapObject {
 public:
  // [to_number_raw]: Cached raw to_number computed at startup.
  DECL_PRIMITIVE_ACCESSORS(to_number_raw, double)
  inline void set_to_number_raw_as_bits(uint64_t bits);

  // [to_string]: Cached to_string computed at startup.
  DECL_ACCESSORS(to_string, Tagged<String>)

  // [to_number]: Cached to_number computed at startup.
  DECL_ACCESSORS(to_number, Tagged<Object>)

  // [typeof]: Cached type_of computed at startup.
  DECL_ACCESSORS(type_of, Tagged<String>)

  inline uint8_t kind() const;
  inline void set_kind(uint8_t kind);

  // ES6 section 7.1.3 ToNumber for Boolean, Null, Undefined.
  V8_WARN_UNUSED_RESULT static inline Handle<Object> ToNumber(
      Isolate* isolate, Handle<Oddball> input);

  DECL_CAST(Oddball)

  // Dispatched behavior.
  DECL_VERIFIER(Oddball)

  // Initialize the fields.
  static void Initialize(Isolate* isolate, Handle<Oddball> oddball,
                         const char* to_string, Handle<Object> to_number,
                         const char* type_of, uint8_t kind);

  // Layout description.
  DECL_FIELD_OFFSET_TQ(ToNumberRaw, HeapObject::kHeaderSize, "float64")
  DECL_FIELD_OFFSET_TQ(ToString, kToNumberRawOffset + kDoubleSize, "String")
  DECL_FIELD_OFFSET_TQ(ToNumber, kToStringOffset + kTaggedSize, "Number")
  DECL_FIELD_OFFSET_TQ(TypeOf, kToNumberOffset + kTaggedSize, "String")
  DECL_FIELD_OFFSET_TQ(Kind, kTypeOfOffset + kTaggedSize, "Smi")
  static const int kSize = kKindOffset + kTaggedSize;
  static const int kHeaderSize = kSize;

  static const uint8_t kFalse = 0;
  static const uint8_t kTrue = 1;
  static const uint8_t kNotBooleanMask = static_cast<uint8_t>(~1);
  static const uint8_t kNull = 3;
  static const uint8_t kArgumentsMarker = 4;
  static const uint8_t kUndefined = 5;
  static const uint8_t kUninitialized = 6;
  static const uint8_t kOther = 7;
  static const uint8_t kException = 8;
  static const uint8_t kOptimizedOut = 9;
  static const uint8_t kStaleRegister = 10;
  static const uint8_t kSelfReferenceMarker = 10;
  static const uint8_t kBasicBlockCountersMarker = 11;

  using BodyDescriptor =
      FixedBodyDescriptor<kToStringOffset, kKindOffset, kSize>;

  static_assert(kKindOffset == Internals::kOddballKindOffset);
  static_assert(kNull == Internals::kNullOddballKind);
  static_assert(kUndefined == Internals::kUndefinedOddballKind);

  DECL_PRINTER(Oddball)

  OBJECT_CONSTRUCTORS(Oddball, PrimitiveHeapObject);
};

class Null : public Oddball {
 public:
  DECL_CAST(Null)
  OBJECT_CONSTRUCTORS(Null, Oddball);
};

class Undefined : public Oddball {
 public:
  DECL_CAST(Undefined)
  OBJECT_CONSTRUCTORS(Undefined, Oddball);
};

class Boolean : public Oddball {
 public:
  DECL_CAST(Boolean)

  V8_INLINE bool ToBool(Isolate* isolate) const;

  OBJECT_CONSTRUCTORS(Boolean, Oddball);
};

class True : public Boolean {
 public:
  DECL_CAST(True)
  OBJECT_CONSTRUCTORS(True, Boolean);
};

class False : public Boolean {
 public:
  DECL_CAST(False)
  OBJECT_CONSTRUCTORS(False, Boolean);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_ODDBALL_H_
