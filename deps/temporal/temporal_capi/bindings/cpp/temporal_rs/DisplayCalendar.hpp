#ifndef TEMPORAL_RS_DisplayCalendar_HPP
#define TEMPORAL_RS_DisplayCalendar_HPP

#include "DisplayCalendar.d.hpp"

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

inline temporal_rs::capi::DisplayCalendar temporal_rs::DisplayCalendar::AsFFI() const {
    return static_cast<temporal_rs::capi::DisplayCalendar>(value);
}

inline temporal_rs::DisplayCalendar temporal_rs::DisplayCalendar::FromFFI(temporal_rs::capi::DisplayCalendar c_enum) {
    switch (c_enum) {
        case temporal_rs::capi::DisplayCalendar_Auto:
        case temporal_rs::capi::DisplayCalendar_Always:
        case temporal_rs::capi::DisplayCalendar_Never:
        case temporal_rs::capi::DisplayCalendar_Critical:
            return static_cast<temporal_rs::DisplayCalendar::Value>(c_enum);
        default:
            std::abort();
    }
}
#endif // TEMPORAL_RS_DisplayCalendar_HPP
