// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>

#include "include/v8config.h"
#include "src/base/macros.h"
#include "src/codegen/arm64/constants-arm64.h"

namespace v8 {
namespace internal {

// ISA constants. --------------------------------------------------------------

// The following code initializes float/double variables with bit patterns.
//
// TODO(mostynb): replace these with std::numeric_limits constexpr's where
// possible, and figure out how to replace *DefaultNaN with something clean,
// then move this code back into instructions-arm64.cc with the same types
// that client code uses.

namespace integer_constants {
constexpr uint16_t kFP16PositiveInfinity = 0x7C00;
constexpr uint16_t kFP16NegativeInfinity = 0xFC00;
constexpr uint32_t kFP32PositiveInfinity = 0x7F800000;
constexpr uint32_t kFP32NegativeInfinity = 0xFF800000;
constexpr uint64_t kFP64PositiveInfinity = 0x7FF0000000000000UL;
constexpr uint64_t kFP64NegativeInfinity = 0xFFF0000000000000UL;

// This value is a signalling NaN as both a double and as a float (taking the
// least-significant word).
constexpr uint64_t kFP64SignallingNaN = 0x7FF000007F800001;
constexpr uint32_t kFP32SignallingNaN = 0x7F800001;

// A similar value, but as a quiet NaN.
constexpr uint64_t kFP64QuietNaN = 0x7FF800007FC00001;
constexpr uint32_t kFP32QuietNaN = 0x7FC00001;

// The default NaN values (for FPCR.DN=1).
constexpr uint64_t kFP64DefaultNaN = 0x7FF8000000000000UL;
constexpr uint32_t kFP32DefaultNaN = 0x7FC00000;
extern const uint16_t kFP16DefaultNaN = 0x7E00;
}  // namespace integer_constants

#if defined(V8_OS_WIN)
extern "C" {
#endif

extern const float16 kFP16PositiveInfinity =
    base::bit_cast<float16>(integer_constants::kFP16PositiveInfinity);
extern const float16 kFP16NegativeInfinity =
    base::bit_cast<float16>(integer_constants::kFP16NegativeInfinity);
V8_EXPORT_PRIVATE extern const float kFP32PositiveInfinity =
    base::bit_cast<float>(integer_constants::kFP32PositiveInfinity);
V8_EXPORT_PRIVATE extern const float kFP32NegativeInfinity =
    base::bit_cast<float>(integer_constants::kFP32NegativeInfinity);
V8_EXPORT_PRIVATE extern const double kFP64PositiveInfinity =
    base::bit_cast<double>(integer_constants::kFP64PositiveInfinity);
V8_EXPORT_PRIVATE extern const double kFP64NegativeInfinity =
    base::bit_cast<double>(integer_constants::kFP64NegativeInfinity);

V8_EXPORT_PRIVATE extern const double kFP64SignallingNaN =
    base::bit_cast<double>(integer_constants::kFP64SignallingNaN);
V8_EXPORT_PRIVATE extern const float kFP32SignallingNaN =
    base::bit_cast<float>(integer_constants::kFP32SignallingNaN);

V8_EXPORT_PRIVATE extern const double kFP64QuietNaN =
    base::bit_cast<double>(integer_constants::kFP64QuietNaN);
V8_EXPORT_PRIVATE extern const float kFP32QuietNaN =
    base::bit_cast<float>(integer_constants::kFP32QuietNaN);

V8_EXPORT_PRIVATE extern const double kFP64DefaultNaN =
    base::bit_cast<double>(integer_constants::kFP64DefaultNaN);
V8_EXPORT_PRIVATE extern const float kFP32DefaultNaN =
    base::bit_cast<float>(integer_constants::kFP32DefaultNaN);
extern const float16 kFP16DefaultNaN =
    base::bit_cast<float16>(integer_constants::kFP16DefaultNaN);

#if defined(V8_OS_WIN)
}  // end of extern "C"
#endif

}  // namespace internal
}  // namespace v8
