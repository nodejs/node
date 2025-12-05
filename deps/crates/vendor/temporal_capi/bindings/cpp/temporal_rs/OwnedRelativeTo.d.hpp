#ifndef TEMPORAL_RS_OwnedRelativeTo_D_HPP
#define TEMPORAL_RS_OwnedRelativeTo_D_HPP

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
namespace capi { struct PlainDate; }
class PlainDate;
namespace capi { struct Provider; }
class Provider;
namespace capi { struct ZonedDateTime; }
class ZonedDateTime;
struct OwnedRelativeTo;
struct TemporalError;
} // namespace temporal_rs



namespace temporal_rs {
namespace capi {
    struct OwnedRelativeTo {
      temporal_rs::capi::PlainDate* date;
      temporal_rs::capi::ZonedDateTime* zoned;
    };

    typedef struct OwnedRelativeTo_option {union { OwnedRelativeTo ok; }; bool is_ok; } OwnedRelativeTo_option;
} // namespace capi
} // namespace


namespace temporal_rs {
/**
 * GetTemporalRelativeToOption can create fresh PlainDate/ZonedDateTimes by parsing them,
 * we need a way to produce that result.
 */
struct OwnedRelativeTo {
    std::unique_ptr<temporal_rs::PlainDate> date;
    std::unique_ptr<temporal_rs::ZonedDateTime> zoned;

  inline static temporal_rs::diplomat::result<temporal_rs::OwnedRelativeTo, temporal_rs::TemporalError> from_utf8(std::string_view s);

  inline static temporal_rs::diplomat::result<temporal_rs::OwnedRelativeTo, temporal_rs::TemporalError> from_utf8_with_provider(std::string_view s, const temporal_rs::Provider& p);

  inline static temporal_rs::diplomat::result<temporal_rs::OwnedRelativeTo, temporal_rs::TemporalError> from_utf16(std::u16string_view s);

  inline static temporal_rs::diplomat::result<temporal_rs::OwnedRelativeTo, temporal_rs::TemporalError> from_utf16_with_provider(std::u16string_view s, const temporal_rs::Provider& p);

  inline static temporal_rs::OwnedRelativeTo empty();

    inline temporal_rs::capi::OwnedRelativeTo AsFFI() const;
    inline static temporal_rs::OwnedRelativeTo FromFFI(temporal_rs::capi::OwnedRelativeTo c_struct);
};

} // namespace
#endif // TEMPORAL_RS_OwnedRelativeTo_D_HPP
