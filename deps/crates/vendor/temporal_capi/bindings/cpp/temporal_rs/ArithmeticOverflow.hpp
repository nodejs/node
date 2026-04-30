#ifndef TEMPORAL_RS_ArithmeticOverflow_HPP
#define TEMPORAL_RS_ArithmeticOverflow_HPP

#include "ArithmeticOverflow.d.hpp"

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

inline temporal_rs::capi::ArithmeticOverflow temporal_rs::ArithmeticOverflow::AsFFI() const {
    return static_cast<temporal_rs::capi::ArithmeticOverflow>(value);
}

inline temporal_rs::ArithmeticOverflow temporal_rs::ArithmeticOverflow::FromFFI(temporal_rs::capi::ArithmeticOverflow c_enum) {
    switch (c_enum) {
        case temporal_rs::capi::ArithmeticOverflow_Constrain:
        case temporal_rs::capi::ArithmeticOverflow_Reject:
            return static_cast<temporal_rs::ArithmeticOverflow::Value>(c_enum);
        default:
            std::abort();
    }
}
#endif // TEMPORAL_RS_ArithmeticOverflow_HPP
