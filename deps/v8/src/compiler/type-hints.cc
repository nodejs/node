// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/type-hints.h"

namespace v8 {
namespace internal {
namespace compiler {

std::ostream& operator<<(std::ostream& os, BinaryOperationHints::Hint hint) {
  switch (hint) {
    case BinaryOperationHints::kNone:
      return os << "None";
    case BinaryOperationHints::kSignedSmall:
      return os << "SignedSmall";
    case BinaryOperationHints::kSigned32:
      return os << "Signed32";
    case BinaryOperationHints::kNumber:
      return os << "Number";
    case BinaryOperationHints::kString:
      return os << "String";
    case BinaryOperationHints::kAny:
      return os << "Any";
  }
  UNREACHABLE();
  return os;
}


std::ostream& operator<<(std::ostream& os, BinaryOperationHints hints) {
  return os << hints.left() << "*" << hints.right() << "->" << hints.result();
}


std::ostream& operator<<(std::ostream& os, ToBooleanHint hint) {
  switch (hint) {
    case ToBooleanHint::kNone:
      return os << "None";
    case ToBooleanHint::kUndefined:
      return os << "Undefined";
    case ToBooleanHint::kBoolean:
      return os << "Boolean";
    case ToBooleanHint::kNull:
      return os << "Null";
    case ToBooleanHint::kSmallInteger:
      return os << "SmallInteger";
    case ToBooleanHint::kReceiver:
      return os << "Receiver";
    case ToBooleanHint::kString:
      return os << "String";
    case ToBooleanHint::kSymbol:
      return os << "Symbol";
    case ToBooleanHint::kHeapNumber:
      return os << "HeapNumber";
    case ToBooleanHint::kSimdValue:
      return os << "SimdValue";
    case ToBooleanHint::kAny:
      return os << "Any";
  }
  UNREACHABLE();
  return os;
}


std::ostream& operator<<(std::ostream& os, ToBooleanHints hints) {
  if (hints == ToBooleanHint::kAny) return os << "Any";
  if (hints == ToBooleanHint::kNone) return os << "None";
  bool first = true;
  for (ToBooleanHints::mask_type i = 0; i < sizeof(i) * CHAR_BIT; ++i) {
    ToBooleanHint const hint = static_cast<ToBooleanHint>(1u << i);
    if (hints & hint) {
      if (!first) os << "|";
      first = false;
      os << hint;
    }
  }
  return os;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
