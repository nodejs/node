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
  kSignedSmallInputs,
  kSigned32,
  kNumber,
  kNumberOrOddball,
  kString,
  kBigInt,
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
  kInternalizedString,
  kString,
  kSymbol,
  kBigInt,
  kReceiver,
  kReceiverOrNullOrUndefined,
  kAny
};

inline size_t hash_value(CompareOperationHint hint) {
  return static_cast<unsigned>(hint);
}

std::ostream& operator<<(std::ostream&, CompareOperationHint);

// Type hints for for..in statements.
enum class ForInHint : uint8_t {
  kNone,
  kEnumCacheKeysAndIndices,
  kEnumCacheKeys,
  kAny
};

std::ostream& operator<<(std::ostream&, ForInHint);

enum StringAddFlags {
  // Omit both parameter checks.
  STRING_ADD_CHECK_NONE,
  // Convert parameters when check fails (instead of throwing an exception).
  STRING_ADD_CONVERT_LEFT,
  STRING_ADD_CONVERT_RIGHT,
};

std::ostream& operator<<(std::ostream& os, const StringAddFlags& flags);

}  // namespace internal
}  // namespace v8

#endif  // V8_TYPE_HINTS_H_
