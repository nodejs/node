#ifndef TEMPORAL_RS_PlainMonthDay_HPP
#define TEMPORAL_RS_PlainMonthDay_HPP

#include "PlainMonthDay.d.hpp"

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
#include "DisplayCalendar.hpp"
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

    typedef struct temporal_rs_PlainMonthDay_try_new_with_overflow_result {union {temporal_rs::capi::PlainMonthDay* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainMonthDay_try_new_with_overflow_result;
    temporal_rs_PlainMonthDay_try_new_with_overflow_result temporal_rs_PlainMonthDay_try_new_with_overflow(uint8_t month, uint8_t day, temporal_rs::capi::AnyCalendarKind calendar, temporal_rs::capi::ArithmeticOverflow overflow, temporal_rs::diplomat::capi::OptionI32 ref_year);

    typedef struct temporal_rs_PlainMonthDay_from_partial_result {union {temporal_rs::capi::PlainMonthDay* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainMonthDay_from_partial_result;
    temporal_rs_PlainMonthDay_from_partial_result temporal_rs_PlainMonthDay_from_partial(temporal_rs::capi::PartialDate partial, temporal_rs::capi::ArithmeticOverflow_option overflow);

    typedef struct temporal_rs_PlainMonthDay_from_parsed_result {union {temporal_rs::capi::PlainMonthDay* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainMonthDay_from_parsed_result;
    temporal_rs_PlainMonthDay_from_parsed_result temporal_rs_PlainMonthDay_from_parsed(const temporal_rs::capi::ParsedDate* parsed);

    typedef struct temporal_rs_PlainMonthDay_with_result {union {temporal_rs::capi::PlainMonthDay* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainMonthDay_with_result;
    temporal_rs_PlainMonthDay_with_result temporal_rs_PlainMonthDay_with(const temporal_rs::capi::PlainMonthDay* self, temporal_rs::capi::PartialDate partial, temporal_rs::capi::ArithmeticOverflow_option overflow);

    bool temporal_rs_PlainMonthDay_equals(const temporal_rs::capi::PlainMonthDay* self, const temporal_rs::capi::PlainMonthDay* other);

    typedef struct temporal_rs_PlainMonthDay_from_utf8_result {union {temporal_rs::capi::PlainMonthDay* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainMonthDay_from_utf8_result;
    temporal_rs_PlainMonthDay_from_utf8_result temporal_rs_PlainMonthDay_from_utf8(temporal_rs::diplomat::capi::DiplomatStringView s);

    typedef struct temporal_rs_PlainMonthDay_from_utf16_result {union {temporal_rs::capi::PlainMonthDay* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainMonthDay_from_utf16_result;
    temporal_rs_PlainMonthDay_from_utf16_result temporal_rs_PlainMonthDay_from_utf16(temporal_rs::diplomat::capi::DiplomatString16View s);

    uint8_t temporal_rs_PlainMonthDay_day(const temporal_rs::capi::PlainMonthDay* self);

    const temporal_rs::capi::Calendar* temporal_rs_PlainMonthDay_calendar(const temporal_rs::capi::PlainMonthDay* self);

    void temporal_rs_PlainMonthDay_month_code(const temporal_rs::capi::PlainMonthDay* self, temporal_rs::diplomat::capi::DiplomatWrite* write);

    typedef struct temporal_rs_PlainMonthDay_to_plain_date_result {union {temporal_rs::capi::PlainDate* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainMonthDay_to_plain_date_result;
    temporal_rs_PlainMonthDay_to_plain_date_result temporal_rs_PlainMonthDay_to_plain_date(const temporal_rs::capi::PlainMonthDay* self, temporal_rs::capi::PartialDate_option year);

    typedef struct temporal_rs_PlainMonthDay_epoch_ms_for_result {union {int64_t ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainMonthDay_epoch_ms_for_result;
    temporal_rs_PlainMonthDay_epoch_ms_for_result temporal_rs_PlainMonthDay_epoch_ms_for(const temporal_rs::capi::PlainMonthDay* self, temporal_rs::capi::TimeZone time_zone);

    typedef struct temporal_rs_PlainMonthDay_epoch_ms_for_with_provider_result {union {int64_t ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainMonthDay_epoch_ms_for_with_provider_result;
    temporal_rs_PlainMonthDay_epoch_ms_for_with_provider_result temporal_rs_PlainMonthDay_epoch_ms_for_with_provider(const temporal_rs::capi::PlainMonthDay* self, temporal_rs::capi::TimeZone time_zone, const temporal_rs::capi::Provider* p);

    void temporal_rs_PlainMonthDay_to_ixdtf_string(const temporal_rs::capi::PlainMonthDay* self, temporal_rs::capi::DisplayCalendar display_calendar, temporal_rs::diplomat::capi::DiplomatWrite* write);

    temporal_rs::capi::PlainMonthDay* temporal_rs_PlainMonthDay_clone(const temporal_rs::capi::PlainMonthDay* self);

    void temporal_rs_PlainMonthDay_destroy(PlainMonthDay* self);

    } // extern "C"
} // namespace capi
} // namespace

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainMonthDay>, temporal_rs::TemporalError> temporal_rs::PlainMonthDay::try_new_with_overflow(uint8_t month, uint8_t day, temporal_rs::AnyCalendarKind calendar, temporal_rs::ArithmeticOverflow overflow, std::optional<int32_t> ref_year) {
    auto result = temporal_rs::capi::temporal_rs_PlainMonthDay_try_new_with_overflow(month,
        day,
        calendar.AsFFI(),
        overflow.AsFFI(),
        ref_year.has_value() ? (temporal_rs::diplomat::capi::OptionI32{ { ref_year.value() }, true }) : (temporal_rs::diplomat::capi::OptionI32{ {}, false }));
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainMonthDay>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::PlainMonthDay>>(std::unique_ptr<temporal_rs::PlainMonthDay>(temporal_rs::PlainMonthDay::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainMonthDay>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainMonthDay>, temporal_rs::TemporalError> temporal_rs::PlainMonthDay::from_partial(temporal_rs::PartialDate partial, std::optional<temporal_rs::ArithmeticOverflow> overflow) {
    auto result = temporal_rs::capi::temporal_rs_PlainMonthDay_from_partial(partial.AsFFI(),
        overflow.has_value() ? (temporal_rs::capi::ArithmeticOverflow_option{ { overflow.value().AsFFI() }, true }) : (temporal_rs::capi::ArithmeticOverflow_option{ {}, false }));
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainMonthDay>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::PlainMonthDay>>(std::unique_ptr<temporal_rs::PlainMonthDay>(temporal_rs::PlainMonthDay::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainMonthDay>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainMonthDay>, temporal_rs::TemporalError> temporal_rs::PlainMonthDay::from_parsed(const temporal_rs::ParsedDate& parsed) {
    auto result = temporal_rs::capi::temporal_rs_PlainMonthDay_from_parsed(parsed.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainMonthDay>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::PlainMonthDay>>(std::unique_ptr<temporal_rs::PlainMonthDay>(temporal_rs::PlainMonthDay::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainMonthDay>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainMonthDay>, temporal_rs::TemporalError> temporal_rs::PlainMonthDay::with(temporal_rs::PartialDate partial, std::optional<temporal_rs::ArithmeticOverflow> overflow) const {
    auto result = temporal_rs::capi::temporal_rs_PlainMonthDay_with(this->AsFFI(),
        partial.AsFFI(),
        overflow.has_value() ? (temporal_rs::capi::ArithmeticOverflow_option{ { overflow.value().AsFFI() }, true }) : (temporal_rs::capi::ArithmeticOverflow_option{ {}, false }));
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainMonthDay>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::PlainMonthDay>>(std::unique_ptr<temporal_rs::PlainMonthDay>(temporal_rs::PlainMonthDay::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainMonthDay>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline bool temporal_rs::PlainMonthDay::equals(const temporal_rs::PlainMonthDay& other) const {
    auto result = temporal_rs::capi::temporal_rs_PlainMonthDay_equals(this->AsFFI(),
        other.AsFFI());
    return result;
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainMonthDay>, temporal_rs::TemporalError> temporal_rs::PlainMonthDay::from_utf8(std::string_view s) {
    auto result = temporal_rs::capi::temporal_rs_PlainMonthDay_from_utf8({s.data(), s.size()});
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainMonthDay>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::PlainMonthDay>>(std::unique_ptr<temporal_rs::PlainMonthDay>(temporal_rs::PlainMonthDay::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainMonthDay>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainMonthDay>, temporal_rs::TemporalError> temporal_rs::PlainMonthDay::from_utf16(std::u16string_view s) {
    auto result = temporal_rs::capi::temporal_rs_PlainMonthDay_from_utf16({s.data(), s.size()});
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainMonthDay>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::PlainMonthDay>>(std::unique_ptr<temporal_rs::PlainMonthDay>(temporal_rs::PlainMonthDay::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainMonthDay>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline uint8_t temporal_rs::PlainMonthDay::day() const {
    auto result = temporal_rs::capi::temporal_rs_PlainMonthDay_day(this->AsFFI());
    return result;
}

inline const temporal_rs::Calendar& temporal_rs::PlainMonthDay::calendar() const {
    auto result = temporal_rs::capi::temporal_rs_PlainMonthDay_calendar(this->AsFFI());
    return *temporal_rs::Calendar::FromFFI(result);
}

inline std::string temporal_rs::PlainMonthDay::month_code() const {
    std::string output;
    temporal_rs::diplomat::capi::DiplomatWrite write = temporal_rs::diplomat::WriteFromString(output);
    temporal_rs::capi::temporal_rs_PlainMonthDay_month_code(this->AsFFI(),
        &write);
    return output;
}
template<typename W>
inline void temporal_rs::PlainMonthDay::month_code_write(W& writeable) const {
    temporal_rs::diplomat::capi::DiplomatWrite write = temporal_rs::diplomat::WriteTrait<W>::Construct(writeable);
    temporal_rs::capi::temporal_rs_PlainMonthDay_month_code(this->AsFFI(),
        &write);
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError> temporal_rs::PlainMonthDay::to_plain_date(std::optional<temporal_rs::PartialDate> year) const {
    auto result = temporal_rs::capi::temporal_rs_PlainMonthDay_to_plain_date(this->AsFFI(),
        year.has_value() ? (temporal_rs::capi::PartialDate_option{ { year.value().AsFFI() }, true }) : (temporal_rs::capi::PartialDate_option{ {}, false }));
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::PlainDate>>(std::unique_ptr<temporal_rs::PlainDate>(temporal_rs::PlainDate::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDate>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<int64_t, temporal_rs::TemporalError> temporal_rs::PlainMonthDay::epoch_ms_for(temporal_rs::TimeZone time_zone) const {
    auto result = temporal_rs::capi::temporal_rs_PlainMonthDay_epoch_ms_for(this->AsFFI(),
        time_zone.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<int64_t, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<int64_t>(result.ok)) : temporal_rs::diplomat::result<int64_t, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<int64_t, temporal_rs::TemporalError> temporal_rs::PlainMonthDay::epoch_ms_for_with_provider(temporal_rs::TimeZone time_zone, const temporal_rs::Provider& p) const {
    auto result = temporal_rs::capi::temporal_rs_PlainMonthDay_epoch_ms_for_with_provider(this->AsFFI(),
        time_zone.AsFFI(),
        p.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<int64_t, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<int64_t>(result.ok)) : temporal_rs::diplomat::result<int64_t, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline std::string temporal_rs::PlainMonthDay::to_ixdtf_string(temporal_rs::DisplayCalendar display_calendar) const {
    std::string output;
    temporal_rs::diplomat::capi::DiplomatWrite write = temporal_rs::diplomat::WriteFromString(output);
    temporal_rs::capi::temporal_rs_PlainMonthDay_to_ixdtf_string(this->AsFFI(),
        display_calendar.AsFFI(),
        &write);
    return output;
}
template<typename W>
inline void temporal_rs::PlainMonthDay::to_ixdtf_string_write(temporal_rs::DisplayCalendar display_calendar, W& writeable) const {
    temporal_rs::diplomat::capi::DiplomatWrite write = temporal_rs::diplomat::WriteTrait<W>::Construct(writeable);
    temporal_rs::capi::temporal_rs_PlainMonthDay_to_ixdtf_string(this->AsFFI(),
        display_calendar.AsFFI(),
        &write);
}

inline std::unique_ptr<temporal_rs::PlainMonthDay> temporal_rs::PlainMonthDay::clone() const {
    auto result = temporal_rs::capi::temporal_rs_PlainMonthDay_clone(this->AsFFI());
    return std::unique_ptr<temporal_rs::PlainMonthDay>(temporal_rs::PlainMonthDay::FromFFI(result));
}

inline const temporal_rs::capi::PlainMonthDay* temporal_rs::PlainMonthDay::AsFFI() const {
    return reinterpret_cast<const temporal_rs::capi::PlainMonthDay*>(this);
}

inline temporal_rs::capi::PlainMonthDay* temporal_rs::PlainMonthDay::AsFFI() {
    return reinterpret_cast<temporal_rs::capi::PlainMonthDay*>(this);
}

inline const temporal_rs::PlainMonthDay* temporal_rs::PlainMonthDay::FromFFI(const temporal_rs::capi::PlainMonthDay* ptr) {
    return reinterpret_cast<const temporal_rs::PlainMonthDay*>(ptr);
}

inline temporal_rs::PlainMonthDay* temporal_rs::PlainMonthDay::FromFFI(temporal_rs::capi::PlainMonthDay* ptr) {
    return reinterpret_cast<temporal_rs::PlainMonthDay*>(ptr);
}

inline void temporal_rs::PlainMonthDay::operator delete(void* ptr) {
    temporal_rs::capi::temporal_rs_PlainMonthDay_destroy(reinterpret_cast<temporal_rs::capi::PlainMonthDay*>(ptr));
}


#endif // TEMPORAL_RS_PlainMonthDay_HPP
