#ifndef TEMPORAL_RS_Duration_D_HPP
#define TEMPORAL_RS_Duration_D_HPP

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
namespace capi { struct Duration; }
class Duration;
namespace capi { struct Provider; }
class Provider;
struct PartialDuration;
struct RelativeTo;
struct RoundingOptions;
struct TemporalError;
struct ToStringRoundingOptions;
class Sign;
class Unit;
} // namespace temporal_rs



namespace temporal_rs {
namespace capi {
    struct Duration;
} // namespace capi
} // namespace

namespace temporal_rs {
class Duration {
public:

  /**
   * Temporary API until v8 can move off of it
   */
  inline static temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> create(int64_t years, int64_t months, int64_t weeks, int64_t days, int64_t hours, int64_t minutes, int64_t seconds, int64_t milliseconds, double microseconds, double nanoseconds);

  inline static temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> try_new(int64_t years, int64_t months, int64_t weeks, int64_t days, int64_t hours, int64_t minutes, int64_t seconds, int64_t milliseconds, double microseconds, double nanoseconds);

  inline static temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> from_partial_duration(temporal_rs::PartialDuration partial);

  inline static temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> from_utf8(std::string_view s);

  inline static temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> from_utf16(std::u16string_view s);

  inline bool is_time_within_range() const;

  inline int64_t years() const;

  inline int64_t months() const;

  inline int64_t weeks() const;

  inline int64_t days() const;

  inline int64_t hours() const;

  inline int64_t minutes() const;

  inline int64_t seconds() const;

  inline int64_t milliseconds() const;

  inline double microseconds() const;

  inline double nanoseconds() const;

  inline temporal_rs::Sign sign() const;

  inline bool is_zero() const;

  inline std::unique_ptr<temporal_rs::Duration> abs() const;

  inline std::unique_ptr<temporal_rs::Duration> negated() const;

  inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> add(const temporal_rs::Duration& other) const;

  inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> subtract(const temporal_rs::Duration& other) const;

  inline temporal_rs::diplomat::result<std::string, temporal_rs::TemporalError> to_string(temporal_rs::ToStringRoundingOptions options) const;
  template<typename W>
  inline temporal_rs::diplomat::result<std::monostate, temporal_rs::TemporalError> to_string_write(temporal_rs::ToStringRoundingOptions options, W& writeable_output) const;

  inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> round(temporal_rs::RoundingOptions options, temporal_rs::RelativeTo relative_to) const;

  inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> round_with_provider(temporal_rs::RoundingOptions options, temporal_rs::RelativeTo relative_to, const temporal_rs::Provider& p) const;

  inline temporal_rs::diplomat::result<int8_t, temporal_rs::TemporalError> compare(const temporal_rs::Duration& other, temporal_rs::RelativeTo relative_to) const;

  inline temporal_rs::diplomat::result<int8_t, temporal_rs::TemporalError> compare_with_provider(const temporal_rs::Duration& other, temporal_rs::RelativeTo relative_to, const temporal_rs::Provider& p) const;

  inline temporal_rs::diplomat::result<double, temporal_rs::TemporalError> total(temporal_rs::Unit unit, temporal_rs::RelativeTo relative_to) const;

  inline temporal_rs::diplomat::result<double, temporal_rs::TemporalError> total_with_provider(temporal_rs::Unit unit, temporal_rs::RelativeTo relative_to, const temporal_rs::Provider& p) const;

  inline std::unique_ptr<temporal_rs::Duration> clone() const;

    inline const temporal_rs::capi::Duration* AsFFI() const;
    inline temporal_rs::capi::Duration* AsFFI();
    inline static const temporal_rs::Duration* FromFFI(const temporal_rs::capi::Duration* ptr);
    inline static temporal_rs::Duration* FromFFI(temporal_rs::capi::Duration* ptr);
    inline static void operator delete(void* ptr);
private:
    Duration() = delete;
    Duration(const temporal_rs::Duration&) = delete;
    Duration(temporal_rs::Duration&&) noexcept = delete;
    Duration operator=(const temporal_rs::Duration&) = delete;
    Duration operator=(temporal_rs::Duration&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // TEMPORAL_RS_Duration_D_HPP
