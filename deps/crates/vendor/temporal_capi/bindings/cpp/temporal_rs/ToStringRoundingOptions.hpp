#ifndef TEMPORAL_RS_ToStringRoundingOptions_HPP
#define TEMPORAL_RS_ToStringRoundingOptions_HPP

#include "ToStringRoundingOptions.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "Precision.hpp"
#include "RoundingMode.hpp"
#include "Unit.hpp"
#include "diplomat_runtime.hpp"


namespace temporal_rs {
namespace capi {
    extern "C" {

    } // extern "C"
} // namespace capi
} // namespace


inline temporal_rs::capi::ToStringRoundingOptions temporal_rs::ToStringRoundingOptions::AsFFI() const {
    return temporal_rs::capi::ToStringRoundingOptions {
        /* .precision = */ precision.AsFFI(),
        /* .smallest_unit = */ smallest_unit.has_value() ? (temporal_rs::capi::Unit_option{ { smallest_unit.value().AsFFI() }, true }) : (temporal_rs::capi::Unit_option{ {}, false }),
        /* .rounding_mode = */ rounding_mode.has_value() ? (temporal_rs::capi::RoundingMode_option{ { rounding_mode.value().AsFFI() }, true }) : (temporal_rs::capi::RoundingMode_option{ {}, false }),
    };
}

inline temporal_rs::ToStringRoundingOptions temporal_rs::ToStringRoundingOptions::FromFFI(temporal_rs::capi::ToStringRoundingOptions c_struct) {
    return temporal_rs::ToStringRoundingOptions {
        /* .precision = */ temporal_rs::Precision::FromFFI(c_struct.precision),
        /* .smallest_unit = */ c_struct.smallest_unit.is_ok ? std::optional(temporal_rs::Unit::FromFFI(c_struct.smallest_unit.ok)) : std::nullopt,
        /* .rounding_mode = */ c_struct.rounding_mode.is_ok ? std::optional(temporal_rs::RoundingMode::FromFFI(c_struct.rounding_mode.ok)) : std::nullopt,
    };
}


#endif // TEMPORAL_RS_ToStringRoundingOptions_HPP
