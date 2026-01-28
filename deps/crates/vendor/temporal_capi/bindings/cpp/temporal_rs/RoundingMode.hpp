#ifndef TEMPORAL_RS_RoundingMode_HPP
#define TEMPORAL_RS_RoundingMode_HPP

#include "RoundingMode.d.hpp"

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

inline temporal_rs::capi::RoundingMode temporal_rs::RoundingMode::AsFFI() const {
    return static_cast<temporal_rs::capi::RoundingMode>(value);
}

inline temporal_rs::RoundingMode temporal_rs::RoundingMode::FromFFI(temporal_rs::capi::RoundingMode c_enum) {
    switch (c_enum) {
        case temporal_rs::capi::RoundingMode_Ceil:
        case temporal_rs::capi::RoundingMode_Floor:
        case temporal_rs::capi::RoundingMode_Expand:
        case temporal_rs::capi::RoundingMode_Trunc:
        case temporal_rs::capi::RoundingMode_HalfCeil:
        case temporal_rs::capi::RoundingMode_HalfFloor:
        case temporal_rs::capi::RoundingMode_HalfExpand:
        case temporal_rs::capi::RoundingMode_HalfTrunc:
        case temporal_rs::capi::RoundingMode_HalfEven:
            return static_cast<temporal_rs::RoundingMode::Value>(c_enum);
        default:
            std::abort();
    }
}
#endif // TEMPORAL_RS_RoundingMode_HPP
