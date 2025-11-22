#ifndef TEMPORAL_RS_Instant_D_HPP
#define TEMPORAL_RS_Instant_D_HPP

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
namespace capi { struct Instant; }
class Instant;
namespace capi { struct Provider; }
class Provider;
namespace capi { struct ZonedDateTime; }
class ZonedDateTime;
struct DifferenceSettings;
struct I128Nanoseconds;
struct RoundingOptions;
struct TemporalError;
struct TimeZone;
struct ToStringRoundingOptions;
} // namespace temporal_rs



namespace temporal_rs {
namespace capi {
    struct Instant;
} // namespace capi
} // namespace

namespace temporal_rs {
class Instant {
public:

  inline static temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError> try_new(temporal_rs::I128Nanoseconds ns);

  inline static temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError> from_epoch_milliseconds(int64_t epoch_milliseconds);

  inline static temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError> from_utf8(std::string_view s);

  inline static temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError> from_utf16(std::u16string_view s);

  inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError> add(const temporal_rs::Duration& duration) const;

  inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError> subtract(const temporal_rs::Duration& duration) const;

  inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> since(const temporal_rs::Instant& other, temporal_rs::DifferenceSettings settings) const;

  inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> until(const temporal_rs::Instant& other, temporal_rs::DifferenceSettings settings) const;

  inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError> round(temporal_rs::RoundingOptions options) const;

  inline int8_t compare(const temporal_rs::Instant& other) const;

  inline bool equals(const temporal_rs::Instant& other) const;

  inline int64_t epoch_milliseconds() const;

  inline temporal_rs::I128Nanoseconds epoch_nanoseconds() const;

  inline temporal_rs::diplomat::result<std::string, temporal_rs::TemporalError> to_ixdtf_string_with_compiled_data(std::optional<temporal_rs::TimeZone> zone, temporal_rs::ToStringRoundingOptions options) const;
  template<typename W>
  inline temporal_rs::diplomat::result<std::monostate, temporal_rs::TemporalError> to_ixdtf_string_with_compiled_data_write(std::optional<temporal_rs::TimeZone> zone, temporal_rs::ToStringRoundingOptions options, W& writeable_output) const;

  inline temporal_rs::diplomat::result<std::string, temporal_rs::TemporalError> to_ixdtf_string_with_provider(std::optional<temporal_rs::TimeZone> zone, temporal_rs::ToStringRoundingOptions options, const temporal_rs::Provider& p) const;
  template<typename W>
  inline temporal_rs::diplomat::result<std::monostate, temporal_rs::TemporalError> to_ixdtf_string_with_provider_write(std::optional<temporal_rs::TimeZone> zone, temporal_rs::ToStringRoundingOptions options, const temporal_rs::Provider& p, W& writeable_output) const;

  inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> to_zoned_date_time_iso(temporal_rs::TimeZone zone) const;

  inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> to_zoned_date_time_iso_with_provider(temporal_rs::TimeZone zone, const temporal_rs::Provider& p) const;

  inline std::unique_ptr<temporal_rs::Instant> clone() const;

    inline const temporal_rs::capi::Instant* AsFFI() const;
    inline temporal_rs::capi::Instant* AsFFI();
    inline static const temporal_rs::Instant* FromFFI(const temporal_rs::capi::Instant* ptr);
    inline static temporal_rs::Instant* FromFFI(temporal_rs::capi::Instant* ptr);
    inline static void operator delete(void* ptr);
private:
    Instant() = delete;
    Instant(const temporal_rs::Instant&) = delete;
    Instant(temporal_rs::Instant&&) noexcept = delete;
    Instant operator=(const temporal_rs::Instant&) = delete;
    Instant operator=(temporal_rs::Instant&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // TEMPORAL_RS_Instant_D_HPP
