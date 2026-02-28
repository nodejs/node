#ifndef TEMPORAL_RS_PlainYearMonth_HPP
#define TEMPORAL_RS_PlainYearMonth_HPP

#include "PlainYearMonth.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "AnyCalendarKind.hpp"
#include "ArithmeticOverflow.hpp"
#include "Calendar.hpp"
#include "DifferenceSettings.hpp"
#include "DisplayCalendar.hpp"
#include "Duration.hpp"
#include "ParsedDate.hpp"
#include "PartialDate.hpp"
#include "PlainDate.hpp"
#include "Provider.hpp"
#include "TemporalError.hpp"
#include "TimeZone.hpp"
#include "diplomat_runtime.hpp"


namespace temporal_rs {
namespace capi {
    extern "C" {

    typedef struct temporal_rs_PlainYearMonth_try_new_with_overflow_result {union {temporal_rs::capi::PlainYearMonth* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainYearMonth_try_new_with_overflow_result;
    temporal_rs_PlainYearMonth_try_new_with_overflow_result temporal_rs_PlainYearMonth_try_new_with_overflow(int32_t year, uint8_t month, temporal_rs::diplomat::capi::OptionU8 reference_day, temporal_rs::capi::AnyCalendarKind calendar, temporal_rs::capi::ArithmeticOverflow overflow);

    typedef struct temporal_rs_PlainYearMonth_from_partial_result {union {temporal_rs::capi::PlainYearMonth* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainYearMonth_from_partial_result;
    temporal_rs_PlainYearMonth_from_partial_result temporal_rs_PlainYearMonth_from_partial(temporal_rs::capi::PartialDate partial, temporal_rs::capi::ArithmeticOverflow_option overflow);

    typedef struct temporal_rs_PlainYearMonth_from_parsed_result {union {temporal_rs::capi::PlainYearMonth* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainYearMonth_from_parsed_result;
    temporal_rs_PlainYearMonth_from_parsed_result temporal_rs_PlainYearMonth_from_parsed(const temporal_rs::capi::ParsedDate* parsed);

    typedef struct temporal_rs_PlainYearMonth_with_result {union {temporal_rs::capi::PlainYearMonth* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainYearMonth_with_result;
    temporal_rs_PlainYearMonth_with_result temporal_rs_PlainYearMonth_with(const temporal_rs::capi::PlainYearMonth* self, temporal_rs::capi::PartialDate partial, temporal_rs::capi::ArithmeticOverflow_option overflow);

    typedef struct temporal_rs_PlainYearMonth_from_utf8_result {union {temporal_rs::capi::PlainYearMonth* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainYearMonth_from_utf8_result;
    temporal_rs_PlainYearMonth_from_utf8_result temporal_rs_PlainYearMonth_from_utf8(temporal_rs::diplomat::capi::DiplomatStringView s);

    typedef struct temporal_rs_PlainYearMonth_from_utf16_result {union {temporal_rs::capi::PlainYearMonth* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainYearMonth_from_utf16_result;
    temporal_rs_PlainYearMonth_from_utf16_result temporal_rs_PlainYearMonth_from_utf16(temporal_rs::diplomat::capi::DiplomatString16View s);

    int32_t temporal_rs_PlainYearMonth_year(const temporal_rs::capi::PlainYearMonth* self);

    uint8_t temporal_rs_PlainYearMonth_month(const temporal_rs::capi::PlainYearMonth* self);

    void temporal_rs_PlainYearMonth_month_code(const temporal_rs::capi::PlainYearMonth* self, temporal_rs::diplomat::capi::DiplomatWrite* write);

    bool temporal_rs_PlainYearMonth_in_leap_year(const temporal_rs::capi::PlainYearMonth* self);

    uint16_t temporal_rs_PlainYearMonth_days_in_month(const temporal_rs::capi::PlainYearMonth* self);

    uint16_t temporal_rs_PlainYearMonth_days_in_year(const temporal_rs::capi::PlainYearMonth* self);

    uint16_t temporal_rs_PlainYearMonth_months_in_year(const temporal_rs::capi::PlainYearMonth* self);

    void temporal_rs_PlainYearMonth_era(const temporal_rs::capi::PlainYearMonth* self, temporal_rs::diplomat::capi::DiplomatWrite* write);

    typedef struct temporal_rs_PlainYearMonth_era_year_result {union {int32_t ok; }; bool is_ok;} temporal_rs_PlainYearMonth_era_year_result;
    temporal_rs_PlainYearMonth_era_year_result temporal_rs_PlainYearMonth_era_year(const temporal_rs::capi::PlainYearMonth* self);

    const temporal_rs::capi::Calendar* temporal_rs_PlainYearMonth_calendar(const temporal_rs::capi::PlainYearMonth* self);

    typedef struct temporal_rs_PlainYearMonth_add_result {union {temporal_rs::capi::PlainYearMonth* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainYearMonth_add_result;
    temporal_rs_PlainYearMonth_add_result temporal_rs_PlainYearMonth_add(const temporal_rs::capi::PlainYearMonth* self, const temporal_rs::capi::Duration* duration, temporal_rs::capi::ArithmeticOverflow overflow);

    typedef struct temporal_rs_PlainYearMonth_subtract_result {union {temporal_rs::capi::PlainYearMonth* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainYearMonth_subtract_result;
    temporal_rs_PlainYearMonth_subtract_result temporal_rs_PlainYearMonth_subtract(const temporal_rs::capi::PlainYearMonth* self, const temporal_rs::capi::Duration* duration, temporal_rs::capi::ArithmeticOverflow overflow);

    typedef struct temporal_rs_PlainYearMonth_until_result {union {temporal_rs::capi::Duration* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainYearMonth_until_result;
    temporal_rs_PlainYearMonth_until_result temporal_rs_PlainYearMonth_until(const temporal_rs::capi::PlainYearMonth* self, const temporal_rs::capi::PlainYearMonth* other, temporal_rs::capi::DifferenceSettings settings);

    typedef struct temporal_rs_PlainYearMonth_since_result {union {temporal_rs::capi::Duration* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainYearMonth_since_result;
    temporal_rs_PlainYearMonth_since_result temporal_rs_PlainYearMonth_since(const temporal_rs::capi::PlainYearMonth* self, const temporal_rs::capi::PlainYearMonth* other, temporal_rs::capi::DifferenceSettings settings);

    bool temporal_rs_PlainYearMonth_equals(const temporal_rs::capi::PlainYearMonth* self, const temporal_rs::capi::PlainYearMonth* other);

    int8_t temporal_rs_PlainYearMonth_compare(const temporal_rs::capi::PlainYearMonth* one, const temporal_rs::capi::PlainYearMonth* two);

    typedef struct temporal_rs_PlainYearMonth_to_plain_date_result {union {temporal_rs::capi::PlainDate* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainYearMonth_to_plain_date_result;
    temporal_rs_PlainYearMonth_to_plain_date_result temporal_rs_PlainYearMonth_to_plain_date(const temporal_rs::capi::PlainYearMonth* self, temporal_rs::capi::PartialDate_option day);

    typedef struct temporal_rs_PlainYearMonth_epoch_ms_for_result {union {int64_t ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainYearMonth_epoch_ms_for_result;
    temporal_rs_PlainYearMonth_epoch_ms_for_result temporal_rs_PlainYearMonth_epoch_ms_for(const temporal_rs::capi::PlainYearMonth* self, temporal_rs::capi::TimeZone time_zone);

    typedef struct temporal_rs_PlainYearMonth_epoch_ms_for_with_provider_result {union {int64_t ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainYearMonth_epoch_ms_for_with_provider_result;
    temporal_rs_PlainYearMonth_epoch_ms_for_with_provider_result temporal_rs_PlainYearMonth_epoch_ms_for_with_provider(const temporal_rs::capi::PlainYearMonth* self, temporal_rs::capi::TimeZone time_zone, const temporal_rs::capi::Provider* p);

    void temporal_rs_PlainYearMonth_to_ixdtf_string(const temporal_rs::capi::PlainYearMonth* self, temporal_rs::capi::DisplayCalendar display_calendar, temporal_rs::diplomat::capi::DiplomatWrite* write);

    temporal_rs::capi::PlainYearMonth* temporal_rs_PlainYearMonth_clone(const temporal_rs::capi::PlainYearMonth* self);

    void temporal_rs_PlainYearMonth_destroy(PlainYearMonth* self);

    } // extern "C"
} // namespace capi
} // namespace

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainYearMonth>, temporal_rs::TemporalError> temporal_rs::PlainYearMonth::try_new_with_overflow(int32_t year, uint8_t month, std::optional<uint8_t> reference_day, temporal_rs::AnyCalendarKind calendar, temporal_rs::ArithmeticOverflow overflow) {
    auto result = temporal_rs::capi::temporal_rs_PlainYearMonth_try_new_with_overflow(year,
        month,
        reference_day.has_value() ? (temporal_rs::diplomat::capi::OptionU8{ { reference_day.value() }, true }) : (temporal_rs::diplomat::capi::OptionU8{ {}, false }),
        calendar.AsFFI(),
        overflow.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainYearMonth>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::PlainYearMonth>>(std::unique_ptr<temporal_rs::PlainYearMonth>(temporal_rs::PlainYearMonth::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainYearMonth>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainYearMonth>, temporal_rs::TemporalError> temporal_rs::PlainYearMonth::from_partial(temporal_rs::PartialDate partial, std::optional<temporal_rs::ArithmeticOverflow> overflow) {
    auto result = temporal_rs::capi::temporal_rs_PlainYearMonth_from_partial(partial.AsFFI(),
        overflow.has_value() ? (temporal_rs::capi::ArithmeticOverflow_option{ { overflow.value().AsFFI() }, true }) : (temporal_rs::capi::ArithmeticOverflow_option{ {}, false }));
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainYearMonth>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::PlainYearMonth>>(std::unique_ptr<temporal_rs::PlainYearMonth>(temporal_rs::PlainYearMonth::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainYearMonth>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainYearMonth>, temporal_rs::TemporalError> temporal_rs::PlainYearMonth::from_parsed(const temporal_rs::ParsedDate& parsed) {
    auto result = temporal_rs::capi::temporal_rs_PlainYearMonth_from_parsed(parsed.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainYearMonth>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::PlainYearMonth>>(std::unique_ptr<temporal_rs::PlainYearMonth>(temporal_rs::PlainYearMonth::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainYearMonth>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainYearMonth>, temporal_rs::TemporalError> temporal_rs::PlainYearMonth::with(temporal_rs::PartialDate partial, std::optional<temporal_rs::ArithmeticOverflow> overflow) const {
    auto result = temporal_rs::capi::temporal_rs_PlainYearMonth_with(this->AsFFI(),
        partial.AsFFI(),
        overflow.has_value() ? (temporal_rs::capi::ArithmeticOverflow_option{ { overflow.value().AsFFI() }, true }) : (temporal_rs::capi::ArithmeticOverflow_option{ {}, false }));
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainYearMonth>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::PlainYearMonth>>(std::unique_ptr<temporal_rs::PlainYearMonth>(temporal_rs::PlainYearMonth::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainYearMonth>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainYearMonth>, temporal_rs::TemporalError> temporal_rs::PlainYearMonth::from_utf8(std::string_view s) {
    auto result = temporal_rs::capi::temporal_rs_PlainYearMonth_from_utf8({s.data(), s.size()});
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainYearMonth>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::PlainYearMonth>>(std::unique_ptr<temporal_rs::PlainYearMonth>(temporal_rs::PlainYearMonth::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainYearMonth>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainYearMonth>, temporal_rs::TemporalError> temporal_rs::PlainYearMonth::from_utf16(std::u16string_view s) {
    auto result = temporal_rs::capi::temporal_rs_PlainYearMonth_from_utf16({s.data(), s.size()});
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainYearMonth>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::PlainYearMonth>>(std::unique_ptr<temporal_rs::PlainYearMonth>(temporal_rs::PlainYearMonth::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainYearMonth>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline int32_t temporal_rs::PlainYearMonth::year() const {
    auto result = temporal_rs::capi::temporal_rs_PlainYearMonth_year(this->AsFFI());
    return result;
}

inline uint8_t temporal_rs::PlainYearMonth::month() const {
    auto result = temporal_rs::capi::temporal_rs_PlainYearMonth_month(this->AsFFI());
    return result;
}

inline std::string temporal_rs::PlainYearMonth::month_code() const {
    std::string output;
    temporal_rs::diplomat::capi::DiplomatWrite write = temporal_rs::diplomat::WriteFromString(output);
    temporal_rs::capi::temporal_rs_PlainYearMonth_month_code(this->AsFFI(),
        &write);
    return output;
}
template<typename W>
inline void temporal_rs::PlainYearMonth::month_code_write(W& writeable) const {
    temporal_rs::diplomat::capi::DiplomatWrite write = temporal_rs::diplomat::WriteTrait<W>::Construct(writeable);
    temporal_rs::capi::temporal_rs_PlainYearMonth_month_code(this->AsFFI(),
        &write);
}

inline bool temporal_rs::PlainYearMonth::in_leap_year() const {
    auto result = temporal_rs::capi::temporal_rs_PlainYearMonth_in_leap_year(this->AsFFI());
    return result;
}

inline uint16_t temporal_rs::PlainYearMonth::days_in_month() const {
    auto result = temporal_rs::capi::temporal_rs_PlainYearMonth_days_in_month(this->AsFFI());
    return result;
}

inline uint16_t temporal_rs::PlainYearMonth::days_in_year() const {
    auto result = temporal_rs::capi::temporal_rs_PlainYearMonth_days_in_year(this->AsFFI());
    return result;
}

inline uint16_t temporal_rs::PlainYearMonth::months_in_year() const {
    auto result = temporal_rs::capi::temporal_rs_PlainYearMonth_months_in_year(this->AsFFI());
    return result;
}

inline std::string temporal_rs::PlainYearMonth::era() const {
    std::string output;
    temporal_rs::diplomat::capi::DiplomatWrite write = temporal_rs::diplomat::WriteFromString(output);
    temporal_rs::capi::temporal_rs_PlainYearMonth_era(this->AsFFI(),
        &write);
    return output;
}
template<typename W>
inline void temporal_rs::PlainYearMonth::era_write(W& writeable) const {
    temporal_rs::diplomat::capi::DiplomatWrite write = temporal_rs::diplomat::WriteTrait<W>::Construct(writeable);
    temporal_rs::capi::temporal_rs_PlainYearMonth_era(this->AsFFI(),
        &write);
}

inline std::optional<int32_t> temporal_rs::PlainYearMonth::era_year() const {
    auto result = temporal_rs::capi::temporal_rs_PlainYearMonth_era_year(this->AsFFI());
    return result.is_ok ? std::optional<int32_t>(result.ok) : std::nullopt;
}

inline const temporal_rs::Calendar& temporal_rs::PlainYearMonth::calendar() const {
    auto result = temporal_rs::capi::temporal_rs_PlainYearMonth_calendar(this->AsFFI());
    return *temporal_rs::Calendar::FromFFI(result);
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainYearMonth>, temporal_rs::TemporalError> temporal_rs::PlainYearMonth::add(const temporal_rs::Duration& duration, temporal_rs::ArithmeticOverflow overflow) const {
    auto result = temporal_rs::capi::temporal_rs_PlainYearMonth_add(this->AsFFI(),
        duration.AsFFI(),
        overflow.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainYearMonth>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::PlainYearMonth>>(std::unique_ptr<temporal_rs::PlainYearMonth>(temporal_rs::PlainYearMonth::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainYearMonth>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainYearMonth>, temporal_rs::TemporalError> temporal_rs::PlainYearMonth::subtract(const temporal_rs::Duration& duration, temporal_rs::ArithmeticOverflow overflow) const {
    auto result = temporal_rs::capi::temporal_rs_PlainYearMonth_subtract(this->AsFFI(),
        duration.AsFFI(),
        overflow.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainYearMonth>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::PlainYearMonth>>(std::unique_ptr<temporal_rs::PlainYearMonth>(temporal_rs::PlainYearMonth::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainYearMonth>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> temporal_rs::PlainYearMonth::until(const temporal_rs::PlainYearMonth& other, temporal_rs::DifferenceSettings settings) const {
    auto result = temporal_rs::capi::temporal_rs_PlainYearMonth_until(this->AsFFI(),
        other.AsFFI(),
        settings.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::Duration>>(std::unique_ptr<temporal_rs::Duration>(temporal_rs::Duration::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> temporal_rs::PlainYearMonth::since(const temporal_rs::PlainYearMonth& other, temporal_rs::DifferenceSettings settings) const {
    auto result = temporal_rs::capi::temporal_rs_PlainYearMonth_since(this->AsFFI(),
        other.AsFFI(),
        settings.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::Duration>>(std::unique_ptr<temporal_rs::Duration>(temporal_rs::Duration::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline bool temporal_rs::PlainYearMonth::equals(const temporal_rs::PlainYearMonth& other) const {
    auto result = temporal_rs::capi::temporal_rs_PlainYearMonth_equals(this->AsFFI(),
        other.AsFFI());
    return result;
}

inline int8_t temporal_rs::PlainYearMonth::compare(const temporal_rs::PlainYearMonth& one, const temporal_rs::PlainYearMonth& two) {
    auto result = temporal_rs::capi::temporal_rs_PlainYearMonth_compare(one.AsFFI(),
        two.AsFFI());
    return result;
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError> temporal_rs::PlainYearMonth::to_plain_date(std::optional<temporal_rs::PartialDate> day) const {
    auto result = temporal_rs::capi::temporal_rs_PlainYearMonth_to_plain_date(this->AsFFI(),
        day.has_value() ? (temporal_rs::capi::PartialDate_option{ { day.value().AsFFI() }, true }) : (temporal_rs::capi::PartialDate_option{ {}, false }));
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::PlainDate>>(std::unique_ptr<temporal_rs::PlainDate>(temporal_rs::PlainDate::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<int64_t, temporal_rs::TemporalError> temporal_rs::PlainYearMonth::epoch_ms_for(temporal_rs::TimeZone time_zone) const {
    auto result = temporal_rs::capi::temporal_rs_PlainYearMonth_epoch_ms_for(this->AsFFI(),
        time_zone.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<int64_t, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<int64_t>(result.ok)) : temporal_rs::diplomat::result<int64_t, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<int64_t, temporal_rs::TemporalError> temporal_rs::PlainYearMonth::epoch_ms_for_with_provider(temporal_rs::TimeZone time_zone, const temporal_rs::Provider& p) const {
    auto result = temporal_rs::capi::temporal_rs_PlainYearMonth_epoch_ms_for_with_provider(this->AsFFI(),
        time_zone.AsFFI(),
        p.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<int64_t, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<int64_t>(result.ok)) : temporal_rs::diplomat::result<int64_t, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline std::string temporal_rs::PlainYearMonth::to_ixdtf_string(temporal_rs::DisplayCalendar display_calendar) const {
    std::string output;
    temporal_rs::diplomat::capi::DiplomatWrite write = temporal_rs::diplomat::WriteFromString(output);
    temporal_rs::capi::temporal_rs_PlainYearMonth_to_ixdtf_string(this->AsFFI(),
        display_calendar.AsFFI(),
        &write);
    return output;
}
template<typename W>
inline void temporal_rs::PlainYearMonth::to_ixdtf_string_write(temporal_rs::DisplayCalendar display_calendar, W& writeable) const {
    temporal_rs::diplomat::capi::DiplomatWrite write = temporal_rs::diplomat::WriteTrait<W>::Construct(writeable);
    temporal_rs::capi::temporal_rs_PlainYearMonth_to_ixdtf_string(this->AsFFI(),
        display_calendar.AsFFI(),
        &write);
}

inline std::unique_ptr<temporal_rs::PlainYearMonth> temporal_rs::PlainYearMonth::clone() const {
    auto result = temporal_rs::capi::temporal_rs_PlainYearMonth_clone(this->AsFFI());
    return std::unique_ptr<temporal_rs::PlainYearMonth>(temporal_rs::PlainYearMonth::FromFFI(result));
}

inline const temporal_rs::capi::PlainYearMonth* temporal_rs::PlainYearMonth::AsFFI() const {
    return reinterpret_cast<const temporal_rs::capi::PlainYearMonth*>(this);
}

inline temporal_rs::capi::PlainYearMonth* temporal_rs::PlainYearMonth::AsFFI() {
    return reinterpret_cast<temporal_rs::capi::PlainYearMonth*>(this);
}

inline const temporal_rs::PlainYearMonth* temporal_rs::PlainYearMonth::FromFFI(const temporal_rs::capi::PlainYearMonth* ptr) {
    return reinterpret_cast<const temporal_rs::PlainYearMonth*>(ptr);
}

inline temporal_rs::PlainYearMonth* temporal_rs::PlainYearMonth::FromFFI(temporal_rs::capi::PlainYearMonth* ptr) {
    return reinterpret_cast<temporal_rs::PlainYearMonth*>(ptr);
}

inline void temporal_rs::PlainYearMonth::operator delete(void* ptr) {
    temporal_rs::capi::temporal_rs_PlainYearMonth_destroy(reinterpret_cast<temporal_rs::capi::PlainYearMonth*>(ptr));
}


#endif // TEMPORAL_RS_PlainYearMonth_HPP
