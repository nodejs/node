// Copyright 2006-2008 the V8 project authors. All rights reserved.
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

#ifndef V8_CONVERSIONS_INL_H_
#define V8_CONVERSIONS_INL_H_

#include <math.h>
#include <float.h>         // required for DBL_MAX and on Win32 for finite()
#include <stdarg.h>

// ----------------------------------------------------------------------------
// Extra POSIX/ANSI functions for Win32/MSVC.

#include "conversions.h"
#include "platform.h"

namespace v8 { namespace internal {

// The fast double-to-int conversion routine does not guarantee
// rounding towards zero.
static inline int FastD2I(double x) {
#ifdef __USE_ISOC99
  // The ISO C99 standard defines the lrint() function which rounds a
  // double to an integer according to the current rounding direction.
  return lrint(x);
#else
  // This is incredibly slow on Intel x86. The reason is that rounding
  // towards zero is implied by the C standard. This means that the
  // status register of the FPU has to be changed with the 'fldcw'
  // instruction. This completely stalls the pipeline and takes many
  // hundreds of clock cycles.
  return static_cast<int>(x);
#endif
}


static inline double DoubleToInteger(double x) {
  if (isnan(x)) return 0;
  if (!isfinite(x) || x == 0) return x;
  return (x >= 0) ? floor(x) : ceil(x);
}


int32_t NumberToInt32(Object* number) {
  if (number->IsSmi()) return Smi::cast(number)->value();
  return DoubleToInt32(number->Number());
}


uint32_t NumberToUint32(Object* number) {
  if (number->IsSmi()) return Smi::cast(number)->value();
  return DoubleToUint32(number->Number());
}


int32_t DoubleToInt32(double x) {
  int32_t i = FastD2I(x);
  if (FastI2D(i) == x) return i;
  static const double two32 = 4294967296.0;
  static const double two31 = 2147483648.0;
  if (!isfinite(x) || x == 0) return 0;
  if (x < 0 || x >= two32) x = fmod(x, two32);
  x = (x >= 0) ? floor(x) : ceil(x) + two32;
  return (int32_t) ((x >= two31) ? x - two32 : x);
}


} }  // namespace v8::internal

#endif  // V8_CONVERSIONS_INL_H_
