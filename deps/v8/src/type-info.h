// Copyright 2010 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_TYPE_INFO_H_
#define V8_TYPE_INFO_H_

#include "globals.h"

namespace v8 {
namespace internal {

//        Unknown
//           |
//      PrimitiveType
//           |   \--------|
//         Number      String
//         /    |         |
//    Double  Integer32   |
//        |      |       /
//        |     Smi     /
//        |     /      /
//        Uninitialized.

class TypeInfo {
 public:
  TypeInfo() : type_(kUnknownType) { }

  static inline TypeInfo Unknown();
  // We know it's a primitive type.
  static inline TypeInfo Primitive();
  // We know it's a number of some sort.
  static inline TypeInfo Number();
  // We know it's signed or unsigned 32 bit integer.
  static inline TypeInfo Integer32();
  // We know it's a Smi.
  static inline TypeInfo Smi();
  // We know it's a heap number.
  static inline TypeInfo Double();
  // We know it's a string.
  static inline TypeInfo String();
  // We haven't started collecting info yet.
  static inline TypeInfo Uninitialized();

  // Return compact representation.  Very sensitive to enum values below!
  // Compacting drops information about primtive types and strings types.
  // We use the compact representation when we only care about number types.
  int ThreeBitRepresentation() {
    ASSERT(type_ != kUninitializedType);
    int answer = type_ & 0xf;
    answer = answer > 6 ? answer - 2 : answer;
    ASSERT(answer >= 0);
    ASSERT(answer <= 7);
    return answer;
  }

  // Decode compact representation.  Very sensitive to enum values below!
  static TypeInfo ExpandedRepresentation(int three_bit_representation) {
    Type t = static_cast<Type>(three_bit_representation > 4 ?
                               three_bit_representation + 2 :
                               three_bit_representation);
    t = (t == kUnknownType) ? t : static_cast<Type>(t | kPrimitiveType);
    ASSERT(t == kUnknownType ||
           t == kNumberType ||
           t == kInteger32Type ||
           t == kSmiType ||
           t == kDoubleType);
    return TypeInfo(t);
  }

  int ToInt() {
    return type_;
  }

  static TypeInfo FromInt(int bit_representation) {
    Type t = static_cast<Type>(bit_representation);
    ASSERT(t == kUnknownType ||
           t == kPrimitiveType ||
           t == kNumberType ||
           t == kInteger32Type ||
           t == kSmiType ||
           t == kDoubleType ||
           t == kStringType);
    return TypeInfo(t);
  }

  // Return the weakest (least precise) common type.
  static TypeInfo Combine(TypeInfo a, TypeInfo b) {
    return TypeInfo(static_cast<Type>(a.type_ & b.type_));
  }


  // Integer32 is an integer that can be represented as either a signed
  // 32-bit integer or as an unsigned 32-bit integer. It has to be
  // in the range [-2^31, 2^32 - 1]. We also have to check for negative 0
  // as it is not an Integer32.
  static inline bool IsInt32Double(double value) {
    const DoubleRepresentation minus_zero(-0.0);
    DoubleRepresentation rep(value);
    if (rep.bits == minus_zero.bits) return false;
    if (value >= kMinInt && value <= kMaxUInt32) {
      if (value <= kMaxInt && value == static_cast<int32_t>(value)) {
        return true;
      }
      if (value == static_cast<uint32_t>(value)) return true;
    }
    return false;
  }

  static TypeInfo TypeFromValue(Handle<Object> value);

  inline bool IsUnknown() {
    return type_ == kUnknownType;
  }

  inline bool IsNumber() {
    ASSERT(type_ != kUninitializedType);
    return ((type_ & kNumberType) == kNumberType);
  }

  inline bool IsSmi() {
    ASSERT(type_ != kUninitializedType);
    return ((type_ & kSmiType) == kSmiType);
  }

  inline bool IsInteger32() {
    ASSERT(type_ != kUninitializedType);
    return ((type_ & kInteger32Type) == kInteger32Type);
  }

  inline bool IsDouble() {
    ASSERT(type_ != kUninitializedType);
    return ((type_ & kDoubleType) == kDoubleType);
  }

  inline bool IsString() {
    ASSERT(type_ != kUninitializedType);
    return ((type_ & kStringType) == kStringType);
  }

  inline bool IsUninitialized() {
    return type_ == kUninitializedType;
  }

  const char* ToString() {
    switch (type_) {
      case kUnknownType: return "UnknownType";
      case kPrimitiveType: return "PrimitiveType";
      case kNumberType: return "NumberType";
      case kInteger32Type: return "Integer32Type";
      case kSmiType: return "SmiType";
      case kDoubleType: return "DoubleType";
      case kStringType: return "StringType";
      case kUninitializedType:
        UNREACHABLE();
        return "UninitializedType";
    }
    UNREACHABLE();
    return "Unreachable code";
  }

 private:
  // We use 6 bits to represent the types.
  enum Type {
    kUnknownType = 0,          // 000000
    kPrimitiveType = 0x10,     // 010000
    kNumberType = 0x11,        // 010001
    kInteger32Type = 0x13,     // 010011
    kSmiType = 0x17,           // 010111
    kDoubleType = 0x19,        // 011001
    kStringType = 0x30,        // 110000
    kUninitializedType = 0x3f  // 111111
  };
  explicit inline TypeInfo(Type t) : type_(t) { }

  Type type_;
};


TypeInfo TypeInfo::Unknown() {
  return TypeInfo(kUnknownType);
}


TypeInfo TypeInfo::Primitive() {
  return TypeInfo(kPrimitiveType);
}


TypeInfo TypeInfo::Number() {
  return TypeInfo(kNumberType);
}


TypeInfo TypeInfo::Integer32() {
  return TypeInfo(kInteger32Type);
}


TypeInfo TypeInfo::Smi() {
  return TypeInfo(kSmiType);
}


TypeInfo TypeInfo::Double() {
  return TypeInfo(kDoubleType);
}


TypeInfo TypeInfo::String() {
  return TypeInfo(kStringType);
}


TypeInfo TypeInfo::Uninitialized() {
  return TypeInfo(kUninitializedType);
}

} }  // namespace v8::internal

#endif  // V8_TYPE_INFO_H_
