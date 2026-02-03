#ifndef TEMPORAL_RS_AnyCalendarKind_D_HPP
#define TEMPORAL_RS_AnyCalendarKind_D_HPP

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
class AnyCalendarKind;
} // namespace temporal_rs



namespace temporal_rs {
namespace capi {
    enum AnyCalendarKind {
      AnyCalendarKind_Buddhist = 0,
      AnyCalendarKind_Chinese = 1,
      AnyCalendarKind_Coptic = 2,
      AnyCalendarKind_Dangi = 3,
      AnyCalendarKind_Ethiopian = 4,
      AnyCalendarKind_EthiopianAmeteAlem = 5,
      AnyCalendarKind_Gregorian = 6,
      AnyCalendarKind_Hebrew = 7,
      AnyCalendarKind_Indian = 8,
      AnyCalendarKind_HijriTabularTypeIIFriday = 9,
      AnyCalendarKind_HijriSimulatedMecca = 10,
      AnyCalendarKind_HijriTabularTypeIIThursday = 11,
      AnyCalendarKind_HijriUmmAlQura = 12,
      AnyCalendarKind_Iso = 13,
      AnyCalendarKind_Japanese = 14,
      AnyCalendarKind_JapaneseExtended = 15,
      AnyCalendarKind_Persian = 16,
      AnyCalendarKind_Roc = 17,
    };

    typedef struct AnyCalendarKind_option {union { AnyCalendarKind ok; }; bool is_ok; } AnyCalendarKind_option;
} // namespace capi
} // namespace

namespace temporal_rs {
class AnyCalendarKind {
public:
    enum Value {
        Buddhist = 0,
        Chinese = 1,
        Coptic = 2,
        Dangi = 3,
        Ethiopian = 4,
        EthiopianAmeteAlem = 5,
        Gregorian = 6,
        Hebrew = 7,
        Indian = 8,
        HijriTabularTypeIIFriday = 9,
        HijriSimulatedMecca = 10,
        HijriTabularTypeIIThursday = 11,
        HijriUmmAlQura = 12,
        Iso = 13,
        Japanese = 14,
        JapaneseExtended = 15,
        Persian = 16,
        Roc = 17,
    };

    AnyCalendarKind(): value(Value::Buddhist) {}

    // Implicit conversions between enum and ::Value
    constexpr AnyCalendarKind(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

  inline static std::optional<temporal_rs::AnyCalendarKind> get_for_str(std::string_view s);

  inline static std::optional<temporal_rs::AnyCalendarKind> parse_temporal_calendar_string(std::string_view s);

    inline temporal_rs::capi::AnyCalendarKind AsFFI() const;
    inline static temporal_rs::AnyCalendarKind FromFFI(temporal_rs::capi::AnyCalendarKind c_enum);
private:
    Value value;
};

} // namespace
#endif // TEMPORAL_RS_AnyCalendarKind_D_HPP
