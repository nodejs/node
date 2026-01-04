#ifndef TEMPORAL_RS_RelativeTo_HPP
#define TEMPORAL_RS_RelativeTo_HPP

#include "RelativeTo.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "PlainDate.hpp"
#include "ZonedDateTime.hpp"
#include "diplomat_runtime.hpp"


namespace temporal_rs {
namespace capi {
    extern "C" {

    } // extern "C"
} // namespace capi
} // namespace


inline temporal_rs::capi::RelativeTo temporal_rs::RelativeTo::AsFFI() const {
    return temporal_rs::capi::RelativeTo {
        /* .date = */ date ? date->AsFFI() : nullptr,
        /* .zoned = */ zoned ? zoned->AsFFI() : nullptr,
    };
}

inline temporal_rs::RelativeTo temporal_rs::RelativeTo::FromFFI(temporal_rs::capi::RelativeTo c_struct) {
    return temporal_rs::RelativeTo {
        /* .date = */ temporal_rs::PlainDate::FromFFI(c_struct.date),
        /* .zoned = */ temporal_rs::ZonedDateTime::FromFFI(c_struct.zoned),
    };
}


#endif // TEMPORAL_RS_RelativeTo_HPP
