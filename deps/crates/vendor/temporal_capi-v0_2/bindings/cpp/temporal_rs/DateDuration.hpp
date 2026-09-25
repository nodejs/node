#ifndef TEMPORAL_RS_DateDuration_HPP
#define TEMPORAL_RS_DateDuration_HPP

#include "DateDuration.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "Sign.hpp"
#include "TemporalError.hpp"
#include "diplomat_runtime.hpp"


namespace temporal_rs {
namespace capi {
    extern "C" {

    typedef struct temporal_rs_DateDuration_try_new_result {union {temporal_rs::capi::DateDuration* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_DateDuration_try_new_result;
    temporal_rs_DateDuration_try_new_result temporal_rs_DateDuration_try_new(int64_t years, int64_t months, int64_t weeks, int64_t days);

    temporal_rs::capi::DateDuration* temporal_rs_DateDuration_abs(const temporal_rs::capi::DateDuration* self);

    temporal_rs::capi::DateDuration* temporal_rs_DateDuration_negated(const temporal_rs::capi::DateDuration* self);

    temporal_rs::capi::Sign temporal_rs_DateDuration_sign(const temporal_rs::capi::DateDuration* self);

    void temporal_rs_DateDuration_destroy(DateDuration* self);

    } // extern "C"
} // namespace capi
} // namespace

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::DateDuration>, temporal_rs::TemporalError> temporal_rs::DateDuration::try_new(int64_t years, int64_t months, int64_t weeks, int64_t days) {
    auto result = temporal_rs::capi::temporal_rs_DateDuration_try_new(years,
        months,
        weeks,
        days);
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::DateDuration>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::DateDuration>>(std::unique_ptr<temporal_rs::DateDuration>(temporal_rs::DateDuration::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::DateDuration>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline std::unique_ptr<temporal_rs::DateDuration> temporal_rs::DateDuration::abs() const {
    auto result = temporal_rs::capi::temporal_rs_DateDuration_abs(this->AsFFI());
    return std::unique_ptr<temporal_rs::DateDuration>(temporal_rs::DateDuration::FromFFI(result));
}

inline std::unique_ptr<temporal_rs::DateDuration> temporal_rs::DateDuration::negated() const {
    auto result = temporal_rs::capi::temporal_rs_DateDuration_negated(this->AsFFI());
    return std::unique_ptr<temporal_rs::DateDuration>(temporal_rs::DateDuration::FromFFI(result));
}

inline temporal_rs::Sign temporal_rs::DateDuration::sign() const {
    auto result = temporal_rs::capi::temporal_rs_DateDuration_sign(this->AsFFI());
    return temporal_rs::Sign::FromFFI(result);
}

inline const temporal_rs::capi::DateDuration* temporal_rs::DateDuration::AsFFI() const {
    return reinterpret_cast<const temporal_rs::capi::DateDuration*>(this);
}

inline temporal_rs::capi::DateDuration* temporal_rs::DateDuration::AsFFI() {
    return reinterpret_cast<temporal_rs::capi::DateDuration*>(this);
}

inline const temporal_rs::DateDuration* temporal_rs::DateDuration::FromFFI(const temporal_rs::capi::DateDuration* ptr) {
    return reinterpret_cast<const temporal_rs::DateDuration*>(ptr);
}

inline temporal_rs::DateDuration* temporal_rs::DateDuration::FromFFI(temporal_rs::capi::DateDuration* ptr) {
    return reinterpret_cast<temporal_rs::DateDuration*>(ptr);
}

inline void temporal_rs::DateDuration::operator delete(void* ptr) {
    temporal_rs::capi::temporal_rs_DateDuration_destroy(reinterpret_cast<temporal_rs::capi::DateDuration*>(ptr));
}


#endif // TEMPORAL_RS_DateDuration_HPP
