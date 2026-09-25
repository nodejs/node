#ifndef TEMPORAL_RS_PartialZonedDateTime_D_HPP
#define TEMPORAL_RS_PartialZonedDateTime_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "PartialDate.d.hpp"
#include "PartialTime.d.hpp"
#include "TimeZone.d.hpp"
#include "diplomat_runtime.hpp"
namespace temporal_rs {
struct PartialDate;
struct PartialTime;
struct TimeZone;
} // namespace temporal_rs



namespace temporal_rs {
namespace capi {
    struct PartialZonedDateTime {
      temporal_rs::capi::PartialDate date;
      temporal_rs::capi::PartialTime time;
      temporal_rs::diplomat::capi::OptionStringView offset;
      temporal_rs::capi::TimeZone_option timezone;
    };

    typedef struct PartialZonedDateTime_option {union { PartialZonedDateTime ok; }; bool is_ok; } PartialZonedDateTime_option;
} // namespace capi
} // namespace


namespace temporal_rs {
struct PartialZonedDateTime {
    temporal_rs::PartialDate date;
    temporal_rs::PartialTime time;
    std::optional<std::string_view> offset;
    std::optional<temporal_rs::TimeZone> timezone;

    inline temporal_rs::capi::PartialZonedDateTime AsFFI() const;
    inline static temporal_rs::PartialZonedDateTime FromFFI(temporal_rs::capi::PartialZonedDateTime c_struct);
};

} // namespace
#endif // TEMPORAL_RS_PartialZonedDateTime_D_HPP
