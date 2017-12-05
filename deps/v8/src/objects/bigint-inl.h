// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_BIGINT_INL_H_
#define V8_OBJECTS_BIGINT_INL_H_

#include "src/objects/bigint.h"

#include "src/objects.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

int BigInt::length() const {
  intptr_t bitfield = READ_INTPTR_FIELD(this, kBitfieldOffset);
  return LengthBits::decode(static_cast<uint32_t>(bitfield));
}
void BigInt::set_length(int new_length) {
  intptr_t bitfield = READ_INTPTR_FIELD(this, kBitfieldOffset);
  bitfield = LengthBits::update(static_cast<uint32_t>(bitfield), new_length);
  WRITE_INTPTR_FIELD(this, kBitfieldOffset, bitfield);
}

bool BigInt::sign() const {
  intptr_t bitfield = READ_INTPTR_FIELD(this, kBitfieldOffset);
  return SignBits::decode(static_cast<uint32_t>(bitfield));
}
void BigInt::set_sign(bool new_sign) {
  intptr_t bitfield = READ_INTPTR_FIELD(this, kBitfieldOffset);
  bitfield = SignBits::update(static_cast<uint32_t>(bitfield), new_sign);
  WRITE_INTPTR_FIELD(this, kBitfieldOffset, bitfield);
}

BigInt::digit_t BigInt::digit(int n) const {
  SLOW_DCHECK(0 <= n && n < length());
  const byte* address = FIELD_ADDR_CONST(this, kDigitsOffset + n * kDigitSize);
  return *reinterpret_cast<digit_t*>(reinterpret_cast<intptr_t>(address));
}
void BigInt::set_digit(int n, digit_t value) {
  SLOW_DCHECK(0 <= n && n < length());
  byte* address = FIELD_ADDR(this, kDigitsOffset + n * kDigitSize);
  (*reinterpret_cast<digit_t*>(reinterpret_cast<intptr_t>(address))) = value;
}

TYPE_CHECKER(BigInt, BIGINT_TYPE)

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_BIGINT_INL_H_
