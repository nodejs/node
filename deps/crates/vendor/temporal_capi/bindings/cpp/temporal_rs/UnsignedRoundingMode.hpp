#ifndef TEMPORAL_RS_UnsignedRoundingMode_HPP
#define TEMPORAL_RS_UnsignedRoundingMode_HPP

#include "UnsignedRoundingMode.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "diplomat_runtime.hpp"


namespace temporal_rs {
namespace capi {
    extern "C" {

    } // extern "C"
} // namespace capi
} // namespace

inline temporal_rs::capi::UnsignedRoundingMode temporal_rs::UnsignedRoundingMode::AsFFI() const {
    return static_cast<temporal_rs::capi::UnsignedRoundingMode>(value);
}

inline temporal_rs::UnsignedRoundingMode temporal_rs::UnsignedRoundingMode::FromFFI(temporal_rs::capi::UnsignedRoundingMode c_enum) {
    switch (c_enum) {
        case temporal_rs::capi::UnsignedRoundingMode_Infinity:
        case temporal_rs::capi::UnsignedRoundingMode_Zero:
        case temporal_rs::capi::UnsignedRoundingMode_HalfInfinity:
        case temporal_rs::capi::UnsignedRoundingMode_HalfZero:
        case temporal_rs::capi::UnsignedRoundingMode_HalfEven:
            return static_cast<temporal_rs::UnsignedRoundingMode::Value>(c_enum);
        default:
            std::abort();
    }
}
#endif // TEMPORAL_RS_UnsignedRoundingMode_HPP
