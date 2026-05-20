#ifndef TEMPORAL_RS_PartialDateTime_HPP
#define TEMPORAL_RS_PartialDateTime_HPP

#include "PartialDateTime.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "PartialDate.hpp"
#include "PartialTime.hpp"
#include "diplomat_runtime.hpp"


namespace temporal_rs {
namespace capi {
    extern "C" {

    } // extern "C"
} // namespace capi
} // namespace


inline temporal_rs::capi::PartialDateTime temporal_rs::PartialDateTime::AsFFI() const {
    return temporal_rs::capi::PartialDateTime {
        /* .date = */ date.AsFFI(),
        /* .time = */ time.AsFFI(),
    };
}

inline temporal_rs::PartialDateTime temporal_rs::PartialDateTime::FromFFI(temporal_rs::capi::PartialDateTime c_struct) {
    return temporal_rs::PartialDateTime {
        /* .date = */ temporal_rs::PartialDate::FromFFI(c_struct.date),
        /* .time = */ temporal_rs::PartialTime::FromFFI(c_struct.time),
    };
}


#endif // TEMPORAL_RS_PartialDateTime_HPP
