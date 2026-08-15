#ifndef TEMPORAL_RS_PartialDate_D_HPP
#define TEMPORAL_RS_PartialDate_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "AnyCalendarKind.d.hpp"
#include "diplomat_runtime.hpp"
namespace temporal_rs {
class AnyCalendarKind;
} // namespace temporal_rs



namespace temporal_rs {
namespace capi {
    struct PartialDate {
      temporal_rs::diplomat::capi::OptionI32 year;
      temporal_rs::diplomat::capi::OptionU8 month;
      temporal_rs::diplomat::capi::DiplomatStringView month_code;
      temporal_rs::diplomat::capi::OptionU8 day;
      temporal_rs::diplomat::capi::DiplomatStringView era;
      temporal_rs::diplomat::capi::OptionI32 era_year;
      temporal_rs::capi::AnyCalendarKind calendar;
    };

    typedef struct PartialDate_option {union { PartialDate ok; }; bool is_ok; } PartialDate_option;
} // namespace capi
} // namespace


namespace temporal_rs {
struct PartialDate {
    std::optional<int32_t> year;
    std::optional<uint8_t> month;
    std::string_view month_code;
    std::optional<uint8_t> day;
    std::string_view era;
    std::optional<int32_t> era_year;
    temporal_rs::AnyCalendarKind calendar;

    inline temporal_rs::capi::PartialDate AsFFI() const;
    inline static temporal_rs::PartialDate FromFFI(temporal_rs::capi::PartialDate c_struct);
};

} // namespace
#endif // TEMPORAL_RS_PartialDate_D_HPP
