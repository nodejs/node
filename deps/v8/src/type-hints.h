// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TYPE_HINTS_H_
#define V8_TYPE_HINTS_H_

#include "src/base/flags.h"
#include "src/utils.h"

namespace v8 {
namespace internal {

// Type hints for an binary operation.
enum class BinaryOperationHint : uint8_t {
  kNone,
  kSignedSmall,
  kSigned32,
  kNumberOrOddball,
  kString,
  kAny
};

inline size_t hash_value(BinaryOperationHint hint) {
  return static_cast<unsigned>(hint);
}

std::ostream& operator<<(std::ostream&, BinaryOperationHint);

// Type hints for an compare operation.
enum class CompareOperationHint : uint8_t {
  kNone,
  kSignedSmall,
  kNumber,
  kNumberOrOddball,
  kAny
};

inline size_t hash_value(CompareOperationHint hint) {
  return static_cast<unsigned>(hint);
}

std::ostream& operator<<(std::ostream&, CompareOperationHint);

// Type hints for the ToBoolean type conversion.
enum class ToBooleanHint : uint16_t {
  kNone = 0u,
  kUndefined = 1u << 0,
  kBoolean = 1u << 1,
  kNull = 1u << 2,
  kSmallInteger = 1u << 3,
  kReceiver = 1u << 4,
  kString = 1u << 5,
  kSymbol = 1u << 6,
  kHeapNumber = 1u << 7,
  kSimdValue = 1u << 8,
  kAny = kUndefined | kBoolean | kNull | kSmallInteger | kReceiver | kString |
         kSymbol | kHeapNumber | kSimdValue
};

std::ostream& operator<<(std::ostream&, ToBooleanHint);

typedef base::Flags<ToBooleanHint, uint16_t> ToBooleanHints;

std::ostream& operator<<(std::ostream&, ToBooleanHints);

DEFINE_OPERATORS_FOR_FLAGS(ToBooleanHints)

}  // namespace internal
}  // namespace v8

#endif  // V8_TYPE_HINTS_H_
