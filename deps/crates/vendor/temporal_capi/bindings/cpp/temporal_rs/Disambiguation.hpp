#ifndef TEMPORAL_RS_Disambiguation_HPP
#define TEMPORAL_RS_Disambiguation_HPP

#include "Disambiguation.d.hpp"

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

inline temporal_rs::capi::Disambiguation temporal_rs::Disambiguation::AsFFI() const {
    return static_cast<temporal_rs::capi::Disambiguation>(value);
}

inline temporal_rs::Disambiguation temporal_rs::Disambiguation::FromFFI(temporal_rs::capi::Disambiguation c_enum) {
    switch (c_enum) {
        case temporal_rs::capi::Disambiguation_Compatible:
        case temporal_rs::capi::Disambiguation_Earlier:
        case temporal_rs::capi::Disambiguation_Later:
        case temporal_rs::capi::Disambiguation_Reject:
            return static_cast<temporal_rs::Disambiguation::Value>(c_enum);
        default:
            std::abort();
    }
}
#endif // TEMPORAL_RS_Disambiguation_HPP
