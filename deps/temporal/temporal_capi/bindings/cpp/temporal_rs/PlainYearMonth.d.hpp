#ifndef TEMPORAL_RS_PlainYearMonth_D_HPP
#define TEMPORAL_RS_PlainYearMonth_D_HPP

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
namespace capi { struct PlainYearMonth; }
class PlainYearMonth;
namespace capi { struct Provider; }
class Provider;
struct DifferenceSettings;
struct PartialDate;
struct TemporalError;
struct TimeZone;
class AnyCalendarKind;
class ArithmeticOverflow;
class DisplayCalendar;
} // namespace temporal_rs



namespace temporal_rs {
namespace capi {
    struct PlainYearMonth;
} // namespace capi
} // namespace

namespace temporal_rs {
class PlainYearMonth {
public:

  inline static temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainYearMonth>, temporal_rs::TemporalError> try_new_with_overflow(int32_t year, uint8_t month, std::optional<uint8_t> reference_day, temporal_rs::AnyCalendarKind calendar, temporal_rs::ArithmeticOverflow overflow);

  inline static temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainYearMonth>, temporal_rs::TemporalError> from_partial(temporal_rs::PartialDate partial, std::optional<temporal_rs::ArithmeticOverflow> overflow);

  inline static temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainYearMonth>, temporal_rs::TemporalError> from_parsed(const temporal_rs::ParsedDate& parsed);

  inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainYearMonth>, temporal_rs::TemporalError> with(temporal_rs::PartialDate partial, std::optional<temporal_rs::ArithmeticOverflow> overflow) const;

  inline static temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainYearMonth>, temporal_rs::TemporalError> from_utf8(std::string_view s);

  inline static temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainYearMonth>, temporal_rs::TemporalError> from_utf16(std::u16string_view s);

  inline int32_t year() const;

  inline uint8_t month() const;

  inline std::string month_code() const;
  template<typename W>
  inline void month_code_write(W& writeable_output) const;

  inline bool in_leap_year() const;

  inline uint16_t days_in_month() const;

  inline uint16_t days_in_year() const;

  inline uint16_t months_in_year() const;

  inline std::string era() const;
  template<typename W>
  inline void era_write(W& writeable_output) const;

  inline std::optional<int32_t> era_year() const;

  inline const temporal_rs::Calendar& calendar() const;

  inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainYearMonth>, temporal_rs::TemporalError> add(const temporal_rs::Duration& duration, temporal_rs::ArithmeticOverflow overflow) const;

  inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainYearMonth>, temporal_rs::TemporalError> subtract(const temporal_rs::Duration& duration, temporal_rs::ArithmeticOverflow overflow) const;

  inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> until(const temporal_rs::PlainYearMonth& other, temporal_rs::DifferenceSettings settings) const;

  inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> since(const temporal_rs::PlainYearMonth& other, temporal_rs::DifferenceSettings settings) const;

  inline bool equals(const temporal_rs::PlainYearMonth& other) const;

  inline static int8_t compare(const temporal_rs::PlainYearMonth& one, const temporal_rs::PlainYearMonth& two);

  inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError> to_plain_date(std::optional<temporal_rs::PartialDate> day) const;

  inline temporal_rs::diplomat::result<int64_t, temporal_rs::TemporalError> epoch_ms_for(temporal_rs::TimeZone time_zone) const;

  inline temporal_rs::diplomat::result<int64_t, temporal_rs::TemporalError> epoch_ms_for_with_provider(temporal_rs::TimeZone time_zone, const temporal_rs::Provider& p) const;

  inline std::string to_ixdtf_string(temporal_rs::DisplayCalendar display_calendar) const;
  template<typename W>
  inline void to_ixdtf_string_write(temporal_rs::DisplayCalendar display_calendar, W& writeable_output) const;

  inline std::unique_ptr<temporal_rs::PlainYearMonth> clone() const;

    inline const temporal_rs::capi::PlainYearMonth* AsFFI() const;
    inline temporal_rs::capi::PlainYearMonth* AsFFI();
    inline static const temporal_rs::PlainYearMonth* FromFFI(const temporal_rs::capi::PlainYearMonth* ptr);
    inline static temporal_rs::PlainYearMonth* FromFFI(temporal_rs::capi::PlainYearMonth* ptr);
    inline static void operator delete(void* ptr);
private:
    PlainYearMonth() = delete;
    PlainYearMonth(const temporal_rs::PlainYearMonth&) = delete;
    PlainYearMonth(temporal_rs::PlainYearMonth&&) noexcept = delete;
    PlainYearMonth operator=(const temporal_rs::PlainYearMonth&) = delete;
    PlainYearMonth operator=(temporal_rs::PlainYearMonth&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // TEMPORAL_RS_PlainYearMonth_D_HPP
