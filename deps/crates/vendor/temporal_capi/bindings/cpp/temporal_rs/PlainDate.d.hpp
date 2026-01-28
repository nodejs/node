#ifndef TEMPORAL_RS_PlainDate_D_HPP
#define TEMPORAL_RS_PlainDate_D_HPP

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
namespace capi { struct ParsedDate; }
class ParsedDate;
namespace capi { struct PlainDate; }
class PlainDate;
namespace capi { struct PlainDateTime; }
class PlainDateTime;
namespace capi { struct PlainMonthDay; }
class PlainMonthDay;
namespace capi { struct PlainTime; }
class PlainTime;
namespace capi { struct PlainYearMonth; }
class PlainYearMonth;
namespace capi { struct Provider; }
class Provider;
namespace capi { struct ZonedDateTime; }
class ZonedDateTime;
struct DifferenceSettings;
struct I128Nanoseconds;
struct PartialDate;
struct TemporalError;
struct TimeZone;
class AnyCalendarKind;
class ArithmeticOverflow;
class DisplayCalendar;
} // namespace temporal_rs



namespace temporal_rs {
namespace capi {
    struct PlainDate;
} // namespace capi
} // namespace

namespace temporal_rs {
class PlainDate {
public:

  inline static temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError> try_new_constrain(int32_t year, uint8_t month, uint8_t day, temporal_rs::AnyCalendarKind calendar);

  inline static temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError> try_new(int32_t year, uint8_t month, uint8_t day, temporal_rs::AnyCalendarKind calendar);

  inline static temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError> try_new_with_overflow(int32_t year, uint8_t month, uint8_t day, temporal_rs::AnyCalendarKind calendar, temporal_rs::ArithmeticOverflow overflow);

  inline static temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError> from_partial(temporal_rs::PartialDate partial, std::optional<temporal_rs::ArithmeticOverflow> overflow);

  inline static temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError> from_parsed(const temporal_rs::ParsedDate& parsed);

  inline static temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError> from_epoch_milliseconds(int64_t ms, temporal_rs::TimeZone tz);

  inline static temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError> from_epoch_milliseconds_with_provider(int64_t ms, temporal_rs::TimeZone tz, const temporal_rs::Provider& p);

  inline static temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError> from_epoch_nanoseconds(temporal_rs::I128Nanoseconds ns, temporal_rs::TimeZone tz);

  inline static temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError> from_epoch_nanoseconds_with_provider(temporal_rs::I128Nanoseconds ns, temporal_rs::TimeZone tz, const temporal_rs::Provider& p);

  inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError> with(temporal_rs::PartialDate partial, std::optional<temporal_rs::ArithmeticOverflow> overflow) const;

  inline std::unique_ptr<temporal_rs::PlainDate> with_calendar(temporal_rs::AnyCalendarKind calendar) const;

  inline static temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError> from_utf8(std::string_view s);

  inline static temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError> from_utf16(std::u16string_view s);

  inline const temporal_rs::Calendar& calendar() const;

  inline bool is_valid() const;

  inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError> add(const temporal_rs::Duration& duration, std::optional<temporal_rs::ArithmeticOverflow> overflow) const;

  inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError> subtract(const temporal_rs::Duration& duration, std::optional<temporal_rs::ArithmeticOverflow> overflow) const;

  inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> until(const temporal_rs::PlainDate& other, temporal_rs::DifferenceSettings settings) const;

  inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> since(const temporal_rs::PlainDate& other, temporal_rs::DifferenceSettings settings) const;

  inline bool equals(const temporal_rs::PlainDate& other) const;

  inline static int8_t compare(const temporal_rs::PlainDate& one, const temporal_rs::PlainDate& two);

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

  inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError> to_plain_date_time(const temporal_rs::PlainTime* time) const;

  inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainMonthDay>, temporal_rs::TemporalError> to_plain_month_day() const;

  inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainYearMonth>, temporal_rs::TemporalError> to_plain_year_month() const;

  inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> to_zoned_date_time(temporal_rs::TimeZone time_zone, const temporal_rs::PlainTime* time) const;

  inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> to_zoned_date_time_with_provider(temporal_rs::TimeZone time_zone, const temporal_rs::PlainTime* time, const temporal_rs::Provider& p) const;

  inline std::string to_ixdtf_string(temporal_rs::DisplayCalendar display_calendar) const;
  template<typename W>
  inline void to_ixdtf_string_write(temporal_rs::DisplayCalendar display_calendar, W& writeable_output) const;

  inline std::unique_ptr<temporal_rs::PlainDate> clone() const;

    inline const temporal_rs::capi::PlainDate* AsFFI() const;
    inline temporal_rs::capi::PlainDate* AsFFI();
    inline static const temporal_rs::PlainDate* FromFFI(const temporal_rs::capi::PlainDate* ptr);
    inline static temporal_rs::PlainDate* FromFFI(temporal_rs::capi::PlainDate* ptr);
    inline static void operator delete(void* ptr);
private:
    PlainDate() = delete;
    PlainDate(const temporal_rs::PlainDate&) = delete;
    PlainDate(temporal_rs::PlainDate&&) noexcept = delete;
    PlainDate operator=(const temporal_rs::PlainDate&) = delete;
    PlainDate operator=(temporal_rs::PlainDate&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // TEMPORAL_RS_PlainDate_D_HPP
