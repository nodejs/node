#ifndef TEMPORAL_RS_PlainDateTime_D_HPP
#define TEMPORAL_RS_PlainDateTime_D_HPP

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
namespace capi { struct Calendar; }
class Calendar;
namespace capi { struct Duration; }
class Duration;
namespace capi { struct ParsedDateTime; }
class ParsedDateTime;
namespace capi { struct PlainDate; }
class PlainDate;
namespace capi { struct PlainDateTime; }
class PlainDateTime;
namespace capi { struct PlainTime; }
class PlainTime;
namespace capi { struct Provider; }
class Provider;
namespace capi { struct ZonedDateTime; }
class ZonedDateTime;
struct DifferenceSettings;
struct I128Nanoseconds;
struct PartialDateTime;
struct RoundingOptions;
struct TemporalError;
struct TimeZone;
struct ToStringRoundingOptions;
class AnyCalendarKind;
class ArithmeticOverflow;
class Disambiguation;
class DisplayCalendar;
} // namespace temporal_rs



namespace temporal_rs {
namespace capi {
    struct PlainDateTime;
} // namespace capi
} // namespace

namespace temporal_rs {
class PlainDateTime {
public:

  inline static temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError> try_new_constrain(int32_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second, uint16_t millisecond, uint16_t microsecond, uint16_t nanosecond, temporal_rs::AnyCalendarKind calendar);

  inline static temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError> try_new(int32_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second, uint16_t millisecond, uint16_t microsecond, uint16_t nanosecond, temporal_rs::AnyCalendarKind calendar);

  inline static temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError> from_partial(temporal_rs::PartialDateTime partial, std::optional<temporal_rs::ArithmeticOverflow> overflow);

  inline static temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError> from_parsed(const temporal_rs::ParsedDateTime& parsed);

  inline static temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError> from_epoch_milliseconds(int64_t ms, temporal_rs::TimeZone tz);

  inline static temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError> from_epoch_milliseconds_with_provider(int64_t ms, temporal_rs::TimeZone tz, const temporal_rs::Provider& p);

  inline static temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError> from_epoch_nanoseconds(temporal_rs::I128Nanoseconds ns, temporal_rs::TimeZone tz);

  inline static temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError> from_epoch_nanoseconds_with_provider(temporal_rs::I128Nanoseconds ns, temporal_rs::TimeZone tz, const temporal_rs::Provider& p);

  inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError> with(temporal_rs::PartialDateTime partial, std::optional<temporal_rs::ArithmeticOverflow> overflow) const;

  inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError> with_time(const temporal_rs::PlainTime* time) const;

  inline std::unique_ptr<temporal_rs::PlainDateTime> with_calendar(temporal_rs::AnyCalendarKind calendar) const;

  inline static temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError> from_utf8(std::string_view s);

  inline static temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError> from_utf16(std::u16string_view s);

  inline uint8_t hour() const;

  inline uint8_t minute() const;

  inline uint8_t second() const;

  inline uint16_t millisecond() const;

  inline uint16_t microsecond() const;

  inline uint16_t nanosecond() const;

  inline const temporal_rs::Calendar& calendar() const;

  inline int32_t year() const;

  inline uint8_t month() const;

  inline std::string month_code() const;
  template<typename W>
  inline void month_code_write(W& writeable_output) const;

  inline uint8_t day() const;

  inline uint16_t day_of_week() const;

  inline uint16_t day_of_year() const;

  inline std::optional<uint8_t> week_of_year() const;

  inline std::optional<int32_t> year_of_week() const;

  inline uint16_t days_in_week() const;

  inline uint16_t days_in_month() const;

  inline uint16_t days_in_year() const;

  inline uint16_t months_in_year() const;

  inline bool in_leap_year() const;

  inline std::string era() const;
  template<typename W>
  inline void era_write(W& writeable_output) const;

  inline std::optional<int32_t> era_year() const;

  inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError> add(const temporal_rs::Duration& duration, std::optional<temporal_rs::ArithmeticOverflow> overflow) const;

  inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError> subtract(const temporal_rs::Duration& duration, std::optional<temporal_rs::ArithmeticOverflow> overflow) const;

  inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> until(const temporal_rs::PlainDateTime& other, temporal_rs::DifferenceSettings settings) const;

  inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> since(const temporal_rs::PlainDateTime& other, temporal_rs::DifferenceSettings settings) const;

  inline bool equals(const temporal_rs::PlainDateTime& other) const;

  inline static int8_t compare(const temporal_rs::PlainDateTime& one, const temporal_rs::PlainDateTime& two);

  inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError> round(temporal_rs::RoundingOptions options) const;

  inline std::unique_ptr<temporal_rs::PlainDate> to_plain_date() const;

  inline std::unique_ptr<temporal_rs::PlainTime> to_plain_time() const;

  inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> to_zoned_date_time(temporal_rs::TimeZone time_zone, temporal_rs::Disambiguation disambiguation) const;

  inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> to_zoned_date_time_with_provider(temporal_rs::TimeZone time_zone, temporal_rs::Disambiguation disambiguation, const temporal_rs::Provider& p) const;

  inline temporal_rs::diplomat::result<std::string, temporal_rs::TemporalError> to_ixdtf_string(temporal_rs::ToStringRoundingOptions options, temporal_rs::DisplayCalendar display_calendar) const;
  template<typename W>
  inline temporal_rs::diplomat::result<std::monostate, temporal_rs::TemporalError> to_ixdtf_string_write(temporal_rs::ToStringRoundingOptions options, temporal_rs::DisplayCalendar display_calendar, W& writeable_output) const;

  inline std::unique_ptr<temporal_rs::PlainDateTime> clone() const;

    inline const temporal_rs::capi::PlainDateTime* AsFFI() const;
    inline temporal_rs::capi::PlainDateTime* AsFFI();
    inline static const temporal_rs::PlainDateTime* FromFFI(const temporal_rs::capi::PlainDateTime* ptr);
    inline static temporal_rs::PlainDateTime* FromFFI(temporal_rs::capi::PlainDateTime* ptr);
    inline static void operator delete(void* ptr);
private:
    PlainDateTime() = delete;
    PlainDateTime(const temporal_rs::PlainDateTime&) = delete;
    PlainDateTime(temporal_rs::PlainDateTime&&) noexcept = delete;
    PlainDateTime operator=(const temporal_rs::PlainDateTime&) = delete;
    PlainDateTime operator=(temporal_rs::PlainDateTime&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // TEMPORAL_RS_PlainDateTime_D_HPP
