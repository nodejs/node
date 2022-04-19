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
  DECL_ACCESSORS(to_string, String)

  // [to_number]: Cached to_number computed at startup.
  DECL_ACCESSORS(to_number, Object)

  // [typeof]: Cached type_of computed at startup.
  DECL_ACCESSORS(type_of, String)

  inline byte kind() const;
  inline void set_kind(byte kind);

  // ES6 section 7.1.3 ToNumber for Boolean, Null, Undefined.
  V8_WARN_UNUSED_RESULT static inline Handle<Object> ToNumber(
      Isolate* isolate, Handle<Oddball> input);

  V8_INLINE bool ToBool(Isolate* isolate) const;

  DECL_CAST(Oddball)

  // Dispatched behavior.
  DECL_VERIFIER(Oddball)

  // Initialize the fields.
  static void Initialize(Isolate* isolate, Handle<Oddball> oddball,
                         const char* to_string, Handle<Object> to_number,
                         const char* type_of, byte kind);

  // Layout description.
  DECL_FIELD_OFFSET_TQ(ToNumberRaw, HeapObject::kHeaderSize, "float64")
  DECL_FIELD_OFFSET_TQ(ToString, kToNumberRawOffset + kDoubleSize, "String")
  DECL_FIELD_OFFSET_TQ(ToNumber, kToStringOffset + kTaggedSize, "Number")
  DECL_FIELD_OFFSET_TQ(TypeOf, kToNumberOffset + kTaggedSize, "String")
  DECL_FIELD_OFFSET_TQ(Kind, kTypeOfOffset + kTaggedSize, "Smi")
  static const int kSize = kKindOffset + kTaggedSize;

  static const byte kFalse = 0;
  static const byte kTrue = 1;
  static const byte kNotBooleanMask = static_cast<byte>(~1);
  static const byte kTheHole = 2;
  static const byte kNull = 3;
  static const byte kArgumentsMarker = 4;
  static const byte kUndefined = 5;
  static const byte kUninitialized = 6;
  static const byte kOther = 7;
  static const byte kException = 8;
  static const byte kOptimizedOut = 9;
  static const byte kStaleRegister = 10;
  static const byte kSelfReferenceMarker = 10;
  static const byte kBasicBlockCountersMarker = 11;

  using BodyDescriptor =
      FixedBodyDescriptor<kToStringOffset, kKindOffset, kSize>;

  STATIC_ASSERT(kKindOffset == Internals::kOddballKindOffset);
  STATIC_ASSERT(kNull == Internals::kNullOddballKind);
  STATIC_ASSERT(kUndefined == Internals::kUndefinedOddballKind);

  DECL_PRINTER(Oddball)

  OBJECT_CONSTRUCTORS(Oddball, PrimitiveHeapObject);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_ODDBALL_H_
