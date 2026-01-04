#ifndef TEMPORAL_RS_OffsetDisambiguation_HPP
#define TEMPORAL_RS_OffsetDisambiguation_HPP

#include "OffsetDisambiguation.d.hpp"

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

inline temporal_rs::capi::OffsetDisambiguation temporal_rs::OffsetDisambiguation::AsFFI() const {
    return static_cast<temporal_rs::capi::OffsetDisambiguation>(value);
}

inline temporal_rs::OffsetDisambiguation temporal_rs::OffsetDisambiguation::FromFFI(temporal_rs::capi::OffsetDisambiguation c_enum) {
    switch (c_enum) {
        case temporal_rs::capi::OffsetDisambiguation_Use:
        case temporal_rs::capi::OffsetDisambiguation_Prefer:
        case temporal_rs::capi::OffsetDisambiguation_Ignore:
        case temporal_rs::capi::OffsetDisambiguation_Reject:
            return static_cast<temporal_rs::OffsetDisambiguation::Value>(c_enum);
        default:
            std::abort();
    }
}
#endif // TEMPORAL_RS_OffsetDisambiguation_HPP
