#ifndef TEMPORAL_RS_DateDuration_D_HPP
#define TEMPORAL_RS_DateDuration_D_HPP

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
namespace capi { struct DateDuration; }
class DateDuration;
struct TemporalError;
class Sign;
} // namespace temporal_rs



namespace temporal_rs {
namespace capi {
    struct DateDuration;
} // namespace capi
} // namespace

namespace temporal_rs {
class DateDuration {
public:

  inline static temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::DateDuration>, temporal_rs::TemporalError> try_new(int64_t years, int64_t months, int64_t weeks, int64_t days);

  inline std::unique_ptr<temporal_rs::DateDuration> abs() const;

  inline std::unique_ptr<temporal_rs::DateDuration> negated() const;

  inline temporal_rs::Sign sign() const;

    inline const temporal_rs::capi::DateDuration* AsFFI() const;
    inline temporal_rs::capi::DateDuration* AsFFI();
    inline static const temporal_rs::DateDuration* FromFFI(const temporal_rs::capi::DateDuration* ptr);
    inline static temporal_rs::DateDuration* FromFFI(temporal_rs::capi::DateDuration* ptr);
    inline static void operator delete(void* ptr);
private:
    DateDuration() = delete;
    DateDuration(const temporal_rs::DateDuration&) = delete;
    DateDuration(temporal_rs::DateDuration&&) noexcept = delete;
    DateDuration operator=(const temporal_rs::DateDuration&) = delete;
    DateDuration operator=(temporal_rs::DateDuration&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // TEMPORAL_RS_DateDuration_D_HPP
