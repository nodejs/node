#ifndef TEMPORAL_RS_DifferenceSettings_HPP
#define TEMPORAL_RS_DifferenceSettings_HPP

#include "DifferenceSettings.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "RoundingMode.hpp"
#include "Unit.hpp"
#include "diplomat_runtime.hpp"


namespace temporal_rs {
namespace capi {
    extern "C" {

    } // extern "C"
} // namespace capi
} // namespace


inline temporal_rs::capi::DifferenceSettings temporal_rs::DifferenceSettings::AsFFI() const {
    return temporal_rs::capi::DifferenceSettings {
        /* .largest_unit = */ largest_unit.has_value() ? (temporal_rs::capi::Unit_option{ { largest_unit.value().AsFFI() }, true }) : (temporal_rs::capi::Unit_option{ {}, false }),
        /* .smallest_unit = */ smallest_unit.has_value() ? (temporal_rs::capi::Unit_option{ { smallest_unit.value().AsFFI() }, true }) : (temporal_rs::capi::Unit_option{ {}, false }),
        /* .rounding_mode = */ rounding_mode.has_value() ? (temporal_rs::capi::RoundingMode_option{ { rounding_mode.value().AsFFI() }, true }) : (temporal_rs::capi::RoundingMode_option{ {}, false }),
        /* .increment = */ increment.has_value() ? (temporal_rs::diplomat::capi::OptionU32{ { increment.value() }, true }) : (temporal_rs::diplomat::capi::OptionU32{ {}, false }),
    };
}

inline temporal_rs::DifferenceSettings temporal_rs::DifferenceSettings::FromFFI(temporal_rs::capi::DifferenceSettings c_struct) {
    return temporal_rs::DifferenceSettings {
        /* .largest_unit = */ c_struct.largest_unit.is_ok ? std::optional(temporal_rs::Unit::FromFFI(c_struct.largest_unit.ok)) : std::nullopt,
        /* .smallest_unit = */ c_struct.smallest_unit.is_ok ? std::optional(temporal_rs::Unit::FromFFI(c_struct.smallest_unit.ok)) : std::nullopt,
        /* .rounding_mode = */ c_struct.rounding_mode.is_ok ? std::optional(temporal_rs::RoundingMode::FromFFI(c_struct.rounding_mode.ok)) : std::nullopt,
        /* .increment = */ c_struct.increment.is_ok ? std::optional(c_struct.increment.ok) : std::nullopt,
    };
}


#endif // TEMPORAL_RS_DifferenceSettings_HPP
