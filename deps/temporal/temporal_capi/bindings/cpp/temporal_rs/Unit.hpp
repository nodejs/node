#ifndef TEMPORAL_RS_Unit_HPP
#define TEMPORAL_RS_Unit_HPP

#include "Unit.d.hpp"

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

inline temporal_rs::capi::Unit temporal_rs::Unit::AsFFI() const {
    return static_cast<temporal_rs::capi::Unit>(value);
}

inline temporal_rs::Unit temporal_rs::Unit::FromFFI(temporal_rs::capi::Unit c_enum) {
    switch (c_enum) {
        case temporal_rs::capi::Unit_Auto:
        case temporal_rs::capi::Unit_Nanosecond:
        case temporal_rs::capi::Unit_Microsecond:
        case temporal_rs::capi::Unit_Millisecond:
        case temporal_rs::capi::Unit_Second:
        case temporal_rs::capi::Unit_Minute:
        case temporal_rs::capi::Unit_Hour:
        case temporal_rs::capi::Unit_Day:
        case temporal_rs::capi::Unit_Week:
        case temporal_rs::capi::Unit_Month:
        case temporal_rs::capi::Unit_Year:
            return static_cast<temporal_rs::Unit::Value>(c_enum);
        default:
            std::abort();
    }
}
#endif // TEMPORAL_RS_Unit_HPP
