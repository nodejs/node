// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_NUMBERS_INTEGER_LITERAL_INL_H_
#define V8_NUMBERS_INTEGER_LITERAL_INL_H_

#include "src/numbers/integer-literal.h"
// Include the non-inl header before the rest of the headers.

namespace v8 {
namespace internal {

inline std::string IntegerLiteral::ToString() const {
  if (negative_) return std::string("-") + std::to_string(absolute_value_);
  return std::to_string(absolute_value_);
}

inline IntegerLiteral operator<<(const IntegerLiteral& x,
                                 const IntegerLiteral& y) {
  DCHECK(!y.is_negative());
  DCHECK_LT(y.absolute_value(), sizeof(uint64_t) * kBitsPerByte);
  return IntegerLiteral(x.is_negative(), x.absolute_value()
                                             << y.absolute_value());
}

inline IntegerLiteral operator+(const IntegerLiteral& x,
                                const IntegerLiteral& y) {
  if (x.is_negative() == y.is_negative()) {
    DCHECK_GE(x.absolute_value() + y.absolute_value(), x.absolute_value());
    return IntegerLiteral(x.is_negative(),
                          x.absolute_value() + y.absolute_value());
  }
  if (x.absolute_value() >= y.absolute_value()) {
    return IntegerLiteral(x.is_negative(),
                          x.absolute_value() - y.absolute_value());
  }
  return IntegerLiteral(!x.is_negative(),
                        y.absolute_value() - x.absolute_value());
}

}  // namespace internal
}  // namespace v8
#endif  // V8_NUMBERS_INTEGER_LITERAL_INL_H_
