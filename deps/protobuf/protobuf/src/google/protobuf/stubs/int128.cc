// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
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

#include <google/protobuf/stubs/int128.h>

#include <iomanip>
#include <ostream>  // NOLINT(readability/streams)
#include <sstream>

#include <google/protobuf/stubs/logging.h>

#include <google/protobuf/port_def.inc>

namespace google {
namespace protobuf {

const uint128_pod kuint128max = {
    static_cast<uint64>(PROTOBUF_LONGLONG(0xFFFFFFFFFFFFFFFF)),
    static_cast<uint64>(PROTOBUF_LONGLONG(0xFFFFFFFFFFFFFFFF))
};

// Returns the 0-based position of the last set bit (i.e., most significant bit)
// in the given uint64. The argument may not be 0.
//
// For example:
//   Given: 5 (decimal) == 101 (binary)
//   Returns: 2
#define STEP(T, n, pos, sh)                   \
  do {                                        \
    if ((n) >= (static_cast<T>(1) << (sh))) { \
      (n) = (n) >> (sh);                      \
      (pos) |= (sh);                          \
    }                                         \
  } while (0)
static inline int Fls64(uint64 n) {
  GOOGLE_DCHECK_NE(0, n);
  int pos = 0;
  STEP(uint64, n, pos, 0x20);
  uint32 n32 = n;
  STEP(uint32, n32, pos, 0x10);
  STEP(uint32, n32, pos, 0x08);
  STEP(uint32, n32, pos, 0x04);
  return pos + ((PROTOBUF_ULONGLONG(0x3333333322221100) >> (n32 << 2)) & 0x3);
}
#undef STEP

// Like Fls64() above, but returns the 0-based position of the last set bit
// (i.e., most significant bit) in the given uint128. The argument may not be 0.
static inline int Fls128(uint128 n) {
  if (uint64 hi = Uint128High64(n)) {
    return Fls64(hi) + 64;
  }
  return Fls64(Uint128Low64(n));
}

void uint128::DivModImpl(uint128 dividend, uint128 divisor,
                         uint128* quotient_ret, uint128* remainder_ret) {
  if (divisor == 0) {
    GOOGLE_LOG(FATAL) << "Division or mod by zero: dividend.hi=" << dividend.hi_
                      << ", lo=" << dividend.lo_;
  } else if (dividend < divisor) {
    *quotient_ret = 0;
    *remainder_ret = dividend;
    return;
  } else {
    int dividend_bit_length = Fls128(dividend);
    int divisor_bit_length = Fls128(divisor);
    int difference = dividend_bit_length - divisor_bit_length;
    uint128 quotient = 0;
    while (difference >= 0) {
      quotient <<= 1;
      uint128 shifted_divisor = divisor << difference;
      if (shifted_divisor <= dividend) {
        dividend -= shifted_divisor;
        quotient += 1;
      }
      difference -= 1;
    }
    //record the final quotient and remainder
    *quotient_ret = quotient;
    *remainder_ret = dividend;
  }
}


uint128& uint128::operator/=(const uint128& divisor) {
  uint128 quotient = 0;
  uint128 remainder = 0;
  DivModImpl(*this, divisor, &quotient, &remainder);
  *this = quotient;
  return *this;
}
uint128& uint128::operator%=(const uint128& divisor) {
  uint128 quotient = 0;
  uint128 remainder = 0;
  DivModImpl(*this, divisor, &quotient, &remainder);
  *this = remainder;
  return *this;
}

std::ostream& operator<<(std::ostream& o, const uint128& b) {
  std::ios_base::fmtflags flags = o.flags();

  // Select a divisor which is the largest power of the base < 2^64.
  uint128 div;
  std::streamsize div_base_log;
  switch (flags & std::ios::basefield) {
    case std::ios::hex:
      div =
          static_cast<uint64>(PROTOBUF_ULONGLONG(0x1000000000000000));  // 16^15
      div_base_log = 15;
      break;
    case std::ios::oct:
      div = static_cast<uint64>(
          PROTOBUF_ULONGLONG(01000000000000000000000));  // 8^21
      div_base_log = 21;
      break;
    default:  // std::ios::dec
      div = static_cast<uint64>(
          PROTOBUF_ULONGLONG(10000000000000000000));  // 10^19
      div_base_log = 19;
      break;
  }

  // Now piece together the uint128 representation from three chunks of
  // the original value, each less than "div" and therefore representable
  // as a uint64.
  std::ostringstream os;
  std::ios_base::fmtflags copy_mask =
      std::ios::basefield | std::ios::showbase | std::ios::uppercase;
  os.setf(flags & copy_mask, copy_mask);
  uint128 high = b;
  uint128 low;
  uint128::DivModImpl(high, div, &high, &low);
  uint128 mid;
  uint128::DivModImpl(high, div, &high, &mid);
  if (high.lo_ != 0) {
    os << high.lo_;
    os << std::noshowbase << std::setfill('0') << std::setw(div_base_log);
    os << mid.lo_;
    os << std::setw(div_base_log);
  } else if (mid.lo_ != 0) {
    os << mid.lo_;
    os << std::noshowbase << std::setfill('0') << std::setw(div_base_log);
  }
  os << low.lo_;
  std::string rep = os.str();

  // Add the requisite padding.
  std::streamsize width = o.width(0);
  if (width > rep.size()) {
    if ((flags & std::ios::adjustfield) == std::ios::left) {
      rep.append(width - rep.size(), o.fill());
    } else {
      rep.insert(static_cast<std::string::size_type>(0),
                 width - rep.size(), o.fill());
    }
  }

  // Stream the final representation in a single "<<" call.
  return o << rep;
}

}  // namespace protobuf
}  // namespace google
