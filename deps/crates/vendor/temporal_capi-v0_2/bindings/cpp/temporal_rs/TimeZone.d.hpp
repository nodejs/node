#ifndef TEMPORAL_RS_TimeZone_D_HPP
#define TEMPORAL_RS_TimeZone_D_HPP

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
namespace capi { struct Provider; }
class Provider;
struct TemporalError;
struct TimeZone;
} // namespace temporal_rs



namespace temporal_rs {
namespace capi {
    struct TimeZone {
      int16_t offset_minutes;
      size_t resolved_id;
      size_t normalized_id;
      bool is_iana_id;
    };

    typedef struct TimeZone_option {union { TimeZone ok; }; bool is_ok; } TimeZone_option;
} // namespace capi
} // namespace


namespace temporal_rs {
/**
 * A type representing a time zone over FFI.
 *
 * It is not recommended to directly manipulate the fields of this type.
 */
struct TimeZone {
    int16_t offset_minutes;
    size_t resolved_id;
    size_t normalized_id;
    bool is_iana_id;

  inline static temporal_rs::diplomat::result<temporal_rs::TimeZone, temporal_rs::TemporalError> try_from_identifier_str(std::string_view ident);

  inline static temporal_rs::diplomat::result<temporal_rs::TimeZone, temporal_rs::TemporalError> try_from_identifier_str_with_provider(std::string_view ident, const temporal_rs::Provider& p);

  inline static temporal_rs::diplomat::result<temporal_rs::TimeZone, temporal_rs::TemporalError> try_from_offset_str(std::string_view ident);

  inline static temporal_rs::diplomat::result<temporal_rs::TimeZone, temporal_rs::TemporalError> try_from_str(std::string_view ident);

  inline static temporal_rs::diplomat::result<temporal_rs::TimeZone, temporal_rs::TemporalError> try_from_str_with_provider(std::string_view ident, const temporal_rs::Provider& p);

  inline std::string identifier() const;
  template<typename W>
  inline void identifier_write(W& writeable_output) const;

  inline temporal_rs::diplomat::result<std::string, temporal_rs::TemporalError> identifier_with_provider(const temporal_rs::Provider& p) const;
  template<typename W>
  inline temporal_rs::diplomat::result<std::monostate, temporal_rs::TemporalError> identifier_with_provider_write(const temporal_rs::Provider& p, W& writeable_output) const;

  inline static temporal_rs::TimeZone utc();

  inline static temporal_rs::diplomat::result<temporal_rs::TimeZone, temporal_rs::TemporalError> utc_with_provider(const temporal_rs::Provider& p);

  /**
   * Create a TimeZone that represents +00:00
   *
   * This is the only way to infallibly make a TimeZone without compiled_data,
   * and can be used as a fallback.
   */
  inline static temporal_rs::TimeZone zero();

  /**
   * Get the primary time zone identifier corresponding to this time zone
   */
  inline temporal_rs::diplomat::result<temporal_rs::TimeZone, temporal_rs::TemporalError> primary_identifier() const;

  inline temporal_rs::diplomat::result<temporal_rs::TimeZone, temporal_rs::TemporalError> primary_identifier_with_provider(const temporal_rs::Provider& p) const;

    inline temporal_rs::capi::TimeZone AsFFI() const;
    inline static temporal_rs::TimeZone FromFFI(temporal_rs::capi::TimeZone c_struct);
};

} // namespace
#endif // TEMPORAL_RS_TimeZone_D_HPP
