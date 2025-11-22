#ifndef TEMPORAL_RS_Calendar_HPP
#define TEMPORAL_RS_Calendar_HPP

#include "Calendar.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "AnyCalendarKind.hpp"
#include "TemporalError.hpp"
#include "diplomat_runtime.hpp"


namespace temporal_rs {
namespace capi {
    extern "C" {

    temporal_rs::capi::Calendar* temporal_rs_Calendar_try_new_constrain(temporal_rs::capi::AnyCalendarKind kind);

    typedef struct temporal_rs_Calendar_from_utf8_result {union {temporal_rs::capi::Calendar* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Calendar_from_utf8_result;
    temporal_rs_Calendar_from_utf8_result temporal_rs_Calendar_from_utf8(temporal_rs::diplomat::capi::DiplomatStringView s);

    bool temporal_rs_Calendar_is_iso(const temporal_rs::capi::Calendar* self);

    temporal_rs::diplomat::capi::DiplomatStringView temporal_rs_Calendar_identifier(const temporal_rs::capi::Calendar* self);

    temporal_rs::capi::AnyCalendarKind temporal_rs_Calendar_kind(const temporal_rs::capi::Calendar* self);

    void temporal_rs_Calendar_destroy(Calendar* self);

    } // extern "C"
} // namespace capi
} // namespace

inline std::unique_ptr<temporal_rs::Calendar> temporal_rs::Calendar::try_new_constrain(temporal_rs::AnyCalendarKind kind) {
    auto result = temporal_rs::capi::temporal_rs_Calendar_try_new_constrain(kind.AsFFI());
    return std::unique_ptr<temporal_rs::Calendar>(temporal_rs::Calendar::FromFFI(result));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Calendar>, temporal_rs::TemporalError> temporal_rs::Calendar::from_utf8(std::string_view s) {
    auto result = temporal_rs::capi::temporal_rs_Calendar_from_utf8({s.data(), s.size()});
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Calendar>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::Calendar>>(std::unique_ptr<temporal_rs::Calendar>(temporal_rs::Calendar::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Calendar>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline bool temporal_rs::Calendar::is_iso() const {
    auto result = temporal_rs::capi::temporal_rs_Calendar_is_iso(this->AsFFI());
    return result;
}

inline std::string_view temporal_rs::Calendar::identifier() const {
    auto result = temporal_rs::capi::temporal_rs_Calendar_identifier(this->AsFFI());
    return std::string_view(result.data, result.len);
}

inline temporal_rs::AnyCalendarKind temporal_rs::Calendar::kind() const {
    auto result = temporal_rs::capi::temporal_rs_Calendar_kind(this->AsFFI());
    return temporal_rs::AnyCalendarKind::FromFFI(result);
}

inline const temporal_rs::capi::Calendar* temporal_rs::Calendar::AsFFI() const {
    return reinterpret_cast<const temporal_rs::capi::Calendar*>(this);
}

inline temporal_rs::capi::Calendar* temporal_rs::Calendar::AsFFI() {
    return reinterpret_cast<temporal_rs::capi::Calendar*>(this);
}

inline const temporal_rs::Calendar* temporal_rs::Calendar::FromFFI(const temporal_rs::capi::Calendar* ptr) {
    return reinterpret_cast<const temporal_rs::Calendar*>(ptr);
}

inline temporal_rs::Calendar* temporal_rs::Calendar::FromFFI(temporal_rs::capi::Calendar* ptr) {
    return reinterpret_cast<temporal_rs::Calendar*>(ptr);
}

inline void temporal_rs::Calendar::operator delete(void* ptr) {
    temporal_rs::capi::temporal_rs_Calendar_destroy(reinterpret_cast<temporal_rs::capi::Calendar*>(ptr));
}


#endif // TEMPORAL_RS_Calendar_HPP
