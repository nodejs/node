#ifndef TEMPORAL_RS_DisplayOffset_HPP
#define TEMPORAL_RS_DisplayOffset_HPP

#include "DisplayOffset.d.hpp"

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

inline temporal_rs::capi::DisplayOffset temporal_rs::DisplayOffset::AsFFI() const {
    return static_cast<temporal_rs::capi::DisplayOffset>(value);
}

inline temporal_rs::DisplayOffset temporal_rs::DisplayOffset::FromFFI(temporal_rs::capi::DisplayOffset c_enum) {
    switch (c_enum) {
        case temporal_rs::capi::DisplayOffset_Auto:
        case temporal_rs::capi::DisplayOffset_Never:
            return static_cast<temporal_rs::DisplayOffset::Value>(c_enum);
        default:
            std::abort();
    }
}
#endif // TEMPORAL_RS_DisplayOffset_HPP
