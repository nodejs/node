#ifndef TEMPORAL_RS_TransitionDirection_HPP
#define TEMPORAL_RS_TransitionDirection_HPP

#include "TransitionDirection.d.hpp"

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

inline temporal_rs::capi::TransitionDirection temporal_rs::TransitionDirection::AsFFI() const {
    return static_cast<temporal_rs::capi::TransitionDirection>(value);
}

inline temporal_rs::TransitionDirection temporal_rs::TransitionDirection::FromFFI(temporal_rs::capi::TransitionDirection c_enum) {
    switch (c_enum) {
        case temporal_rs::capi::TransitionDirection_Next:
        case temporal_rs::capi::TransitionDirection_Previous:
            return static_cast<temporal_rs::TransitionDirection::Value>(c_enum);
        default:
            std::abort();
    }
}
#endif // TEMPORAL_RS_TransitionDirection_HPP
