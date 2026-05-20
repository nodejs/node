#ifndef TEMPORAL_RS_PartialZonedDateTime_HPP
#define TEMPORAL_RS_PartialZonedDateTime_HPP

#include "PartialZonedDateTime.d.hpp"

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
#include "TimeZone.hpp"
#include "diplomat_runtime.hpp"


namespace temporal_rs {
namespace capi {
    extern "C" {

    } // extern "C"
} // namespace capi
} // namespace


inline temporal_rs::capi::PartialZonedDateTime temporal_rs::PartialZonedDateTime::AsFFI() const {
    return temporal_rs::capi::PartialZonedDateTime {
        /* .date = */ date.AsFFI(),
        /* .time = */ time.AsFFI(),
        /* .offset = */ offset.has_value() ? (temporal_rs::diplomat::capi::OptionStringView{ { {offset.value().data(), offset.value().size()} }, true }) : (temporal_rs::diplomat::capi::OptionStringView{ {}, false }),
        /* .timezone = */ timezone.has_value() ? (temporal_rs::capi::TimeZone_option{ { timezone.value().AsFFI() }, true }) : (temporal_rs::capi::TimeZone_option{ {}, false }),
    };
}

inline temporal_rs::PartialZonedDateTime temporal_rs::PartialZonedDateTime::FromFFI(temporal_rs::capi::PartialZonedDateTime c_struct) {
    return temporal_rs::PartialZonedDateTime {
        /* .date = */ temporal_rs::PartialDate::FromFFI(c_struct.date),
        /* .time = */ temporal_rs::PartialTime::FromFFI(c_struct.time),
        /* .offset = */ c_struct.offset.is_ok ? std::optional(std::string_view(c_struct.offset.ok.data, c_struct.offset.ok.len)) : std::nullopt,
        /* .timezone = */ c_struct.timezone.is_ok ? std::optional(temporal_rs::TimeZone::FromFFI(c_struct.timezone.ok)) : std::nullopt,
    };
}


#endif // TEMPORAL_RS_PartialZonedDateTime_HPP
