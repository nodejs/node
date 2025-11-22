#ifndef TEMPORAL_RS_ErrorKind_HPP
#define TEMPORAL_RS_ErrorKind_HPP

#include "ErrorKind.d.hpp"

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

inline temporal_rs::capi::ErrorKind temporal_rs::ErrorKind::AsFFI() const {
    return static_cast<temporal_rs::capi::ErrorKind>(value);
}

inline temporal_rs::ErrorKind temporal_rs::ErrorKind::FromFFI(temporal_rs::capi::ErrorKind c_enum) {
    switch (c_enum) {
        case temporal_rs::capi::ErrorKind_Generic:
        case temporal_rs::capi::ErrorKind_Type:
        case temporal_rs::capi::ErrorKind_Range:
        case temporal_rs::capi::ErrorKind_Syntax:
        case temporal_rs::capi::ErrorKind_Assert:
            return static_cast<temporal_rs::ErrorKind::Value>(c_enum);
        default:
            std::abort();
    }
}
#endif // TEMPORAL_RS_ErrorKind_HPP
