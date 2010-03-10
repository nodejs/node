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

#ifndef V8_NUMBER_INFO_H_
#define V8_NUMBER_INFO_H_

namespace v8 {
namespace internal {

//        Unknown
//           |
//         Number
//         /    |
//  HeapNumber Integer32
//        |      |
//        |     Smi
//        |     /
//      Uninitialized.

class NumberInfo {
 public:
  NumberInfo() { }

  static inline NumberInfo Unknown();
  // We know it's a number of some sort.
  static inline NumberInfo Number();
  // We know it's signed or unsigned 32 bit integer.
  static inline NumberInfo Integer32();
  // We know it's a Smi.
  static inline NumberInfo Smi();
  // We know it's a heap number.
  static inline NumberInfo HeapNumber();
  // We haven't started collecting info yet.
  static inline NumberInfo Uninitialized();

  // Return compact representation.  Very sensitive to enum values below!
  int ThreeBitRepresentation() {
    ASSERT(type_ != kUninitializedType);
    int answer = type_ > 6 ? type_ -2 : type_;
    ASSERT(answer >= 0);
    ASSERT(answer <= 7);
    return answer;
  }

  // Decode compact representation.  Very sensitive to enum values below!
  static NumberInfo ExpandedRepresentation(int three_bit_representation) {
    Type t = static_cast<Type>(three_bit_representation >= 6 ?
                               three_bit_representation + 2 :
                               three_bit_representation);
    ASSERT(t == kUnknownType ||
           t == kNumberType ||
           t == kInteger32Type ||
           t == kSmiType ||
           t == kHeapNumberType);
    return NumberInfo(t);
  }

  int ToInt() {
    return type_;
  }

  static NumberInfo FromInt(int bit_representation) {
    Type t = static_cast<Type>(bit_representation);
    ASSERT(t == kUnknownType ||
           t == kNumberType ||
           t == kInteger32Type ||
           t == kSmiType ||
           t == kHeapNumberType);
    return NumberInfo(t);
  }

  // Return the weakest (least precise) common type.
  static NumberInfo Combine(NumberInfo a, NumberInfo b) {
    return NumberInfo(static_cast<Type>(a.type_ & b.type_));
  }

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

  inline bool IsHeapNumber() {
    ASSERT(type_ != kUninitializedType);
    return ((type_ & kHeapNumberType) == kHeapNumberType);
  }

  inline bool IsUninitialized() {
    return type_ == kUninitializedType;
  }

  const char* ToString() {
    switch (type_) {
      case kUnknownType: return "UnknownType";
      case kNumberType: return "NumberType";
      case kSmiType: return "SmiType";
      case kHeapNumberType: return "HeapNumberType";
      case kInteger32Type: return "Integer32Type";
      case kUninitializedType:
        UNREACHABLE();
        return "UninitializedType";
    }
    UNREACHABLE();
    return "Unreachable code";
  }

 private:
  enum Type {
    kUnknownType = 0,
    kNumberType = 1,
    kInteger32Type = 3,
    kSmiType = 7,
    kHeapNumberType = 9,
    kUninitializedType = 15
  };
  explicit inline NumberInfo(Type t) : type_(t) { }

  Type type_;
};


NumberInfo NumberInfo::Unknown() {
  return NumberInfo(kUnknownType);
}


NumberInfo NumberInfo::Number() {
  return NumberInfo(kNumberType);
}


NumberInfo NumberInfo::Integer32() {
  return NumberInfo(kInteger32Type);
}


NumberInfo NumberInfo::Smi() {
  return NumberInfo(kSmiType);
}


NumberInfo NumberInfo::HeapNumber() {
  return NumberInfo(kHeapNumberType);
}


NumberInfo NumberInfo::Uninitialized() {
  return NumberInfo(kUninitializedType);
}

} }  // namespace v8::internal

#endif  // V8_NUMBER_INFO_H_
