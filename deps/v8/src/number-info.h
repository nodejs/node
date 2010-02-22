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

class NumberInfo : public AllStatic {
 public:
  enum Type {
    kUnknown = 0,
    kNumber = 1,
    kSmi = 3,
    kHeapNumber = 5,
    kUninitialized = 7
  };

  // Return the weakest (least precise) common type.
  static Type Combine(Type a, Type b) {
    // Make use of the order of enum values.
    return static_cast<Type>(a & b);
  }

  static bool IsNumber(Type a) {
    ASSERT(a != kUninitialized);
    return ((a & kNumber) != 0);
  }

  static const char* ToString(Type a) {
    switch (a) {
      case kUnknown: return "UnknownType";
      case kNumber: return "NumberType";
      case kSmi: return "SmiType";
      case kHeapNumber: return "HeapNumberType";
      case kUninitialized:
        UNREACHABLE();
        return "UninitializedType";
    }
    UNREACHABLE();
    return "Unreachable code";
  }
};

} }  // namespace v8::internal

#endif  // V8_NUMBER_INFO_H_
