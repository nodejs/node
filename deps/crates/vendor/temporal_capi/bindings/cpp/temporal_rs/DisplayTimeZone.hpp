#ifndef TEMPORAL_RS_DisplayTimeZone_HPP
#define TEMPORAL_RS_DisplayTimeZone_HPP

#include "DisplayTimeZone.d.hpp"

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

inline temporal_rs::capi::DisplayTimeZone temporal_rs::DisplayTimeZone::AsFFI() const {
    return static_cast<temporal_rs::capi::DisplayTimeZone>(value);
}

inline temporal_rs::DisplayTimeZone temporal_rs::DisplayTimeZone::FromFFI(temporal_rs::capi::DisplayTimeZone c_enum) {
    switch (c_enum) {
        case temporal_rs::capi::DisplayTimeZone_Auto:
        case temporal_rs::capi::DisplayTimeZone_Never:
        case temporal_rs::capi::DisplayTimeZone_Critical:
            return static_cast<temporal_rs::DisplayTimeZone::Value>(c_enum);
        default:
            std::abort();
    }
}
#endif // TEMPORAL_RS_DisplayTimeZone_HPP
