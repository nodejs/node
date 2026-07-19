#ifndef TEMPORAL_RS_ParsedZonedDateTime_D_HPP
#define TEMPORAL_RS_ParsedZonedDateTime_D_HPP

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
namespace capi { struct ParsedZonedDateTime; }
class ParsedZonedDateTime;
namespace capi { struct Provider; }
class Provider;
struct TemporalError;
} // namespace temporal_rs



namespace temporal_rs {
namespace capi {
    struct ParsedZonedDateTime;
} // namespace capi
} // namespace

namespace temporal_rs {
class ParsedZonedDateTime {
public:

  inline static temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ParsedZonedDateTime>, temporal_rs::TemporalError> from_utf8(std::string_view s);

  inline static temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ParsedZonedDateTime>, temporal_rs::TemporalError> from_utf8_with_provider(std::string_view s, const temporal_rs::Provider& p);

  inline static temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ParsedZonedDateTime>, temporal_rs::TemporalError> from_utf16(std::u16string_view s);

  inline static temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ParsedZonedDateTime>, temporal_rs::TemporalError> from_utf16_with_provider(std::u16string_view s, const temporal_rs::Provider& p);

    inline const temporal_rs::capi::ParsedZonedDateTime* AsFFI() const;
    inline temporal_rs::capi::ParsedZonedDateTime* AsFFI();
    inline static const temporal_rs::ParsedZonedDateTime* FromFFI(const temporal_rs::capi::ParsedZonedDateTime* ptr);
    inline static temporal_rs::ParsedZonedDateTime* FromFFI(temporal_rs::capi::ParsedZonedDateTime* ptr);
    inline static void operator delete(void* ptr);
private:
    ParsedZonedDateTime() = delete;
    ParsedZonedDateTime(const temporal_rs::ParsedZonedDateTime&) = delete;
    ParsedZonedDateTime(temporal_rs::ParsedZonedDateTime&&) noexcept = delete;
    ParsedZonedDateTime operator=(const temporal_rs::ParsedZonedDateTime&) = delete;
    ParsedZonedDateTime operator=(temporal_rs::ParsedZonedDateTime&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // TEMPORAL_RS_ParsedZonedDateTime_D_HPP
