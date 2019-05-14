// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include "include/v8config.h"

namespace v8 {
namespace internal {

// ISA constants. --------------------------------------------------------------

// The following code initializes float/double variables with bit patterns
// without using static initializers (which is surprisingly difficult in
// C++).  These variables are used by client code as extern float16,
// extern float and extern double types, which works because (I think) the
// linker ignores the types.  This is kept in a separate source file to
// avoid breaking jumbo builds.
//
// TODO(mostynb): replace these with std::numeric_limits constexpr's where
// possible, and figure out how to replace *DefaultNaN with something clean,
// then move this code back into instructions-arm64.cc with the same types
// that client code uses.

#if defined(V8_OS_WIN)
extern "C" {
#endif

extern const uint16_t kFP16PositiveInfinity = 0x7C00;
extern const uint16_t kFP16NegativeInfinity = 0xFC00;
extern const uint32_t kFP32PositiveInfinity = 0x7F800000;
extern const uint32_t kFP32NegativeInfinity = 0xFF800000;
extern const uint64_t kFP64PositiveInfinity = 0x7FF0000000000000UL;
extern const uint64_t kFP64NegativeInfinity = 0xFFF0000000000000UL;

// This value is a signalling NaN as both a double and as a float (taking the
// least-significant word).
extern const uint64_t kFP64SignallingNaN = 0x7FF000007F800001;
extern const uint32_t kFP32SignallingNaN = 0x7F800001;

// A similar value, but as a quiet NaN.
extern const uint64_t kFP64QuietNaN = 0x7FF800007FC00001;
extern const uint32_t kFP32QuietNaN = 0x7FC00001;

// The default NaN values (for FPCR.DN=1).
extern const uint64_t kFP64DefaultNaN = 0x7FF8000000000000UL;
extern const uint32_t kFP32DefaultNaN = 0x7FC00000;
extern const uint16_t kFP16DefaultNaN = 0x7E00;

#if defined(V8_OS_WIN)
}  // end of extern "C"
#endif

}  // namespace internal
}  // namespace v8
