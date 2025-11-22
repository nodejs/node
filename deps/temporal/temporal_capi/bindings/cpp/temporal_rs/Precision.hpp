#ifndef TEMPORAL_RS_Precision_HPP
#define TEMPORAL_RS_Precision_HPP

#include "Precision.d.hpp"

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


inline temporal_rs::capi::Precision temporal_rs::Precision::AsFFI() const {
    return temporal_rs::capi::Precision {
        /* .is_minute = */ is_minute,
        /* .precision = */ precision.has_value() ? (temporal_rs::diplomat::capi::OptionU8{ { precision.value() }, true }) : (temporal_rs::diplomat::capi::OptionU8{ {}, false }),
    };
}

inline temporal_rs::Precision temporal_rs::Precision::FromFFI(temporal_rs::capi::Precision c_struct) {
    return temporal_rs::Precision {
        /* .is_minute = */ c_struct.is_minute,
        /* .precision = */ c_struct.precision.is_ok ? std::optional(c_struct.precision.ok) : std::nullopt,
    };
}


#endif // TEMPORAL_RS_Precision_HPP
