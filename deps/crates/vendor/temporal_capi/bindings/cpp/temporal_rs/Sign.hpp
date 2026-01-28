#ifndef TEMPORAL_RS_Sign_HPP
#define TEMPORAL_RS_Sign_HPP

#include "Sign.d.hpp"

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

inline temporal_rs::capi::Sign temporal_rs::Sign::AsFFI() const {
    return static_cast<temporal_rs::capi::Sign>(value);
}

inline temporal_rs::Sign temporal_rs::Sign::FromFFI(temporal_rs::capi::Sign c_enum) {
    switch (c_enum) {
        case temporal_rs::capi::Sign_Positive:
        case temporal_rs::capi::Sign_Zero:
        case temporal_rs::capi::Sign_Negative:
            return static_cast<temporal_rs::Sign::Value>(c_enum);
        default:
            std::abort();
    }
}
#endif // TEMPORAL_RS_Sign_HPP
