#ifndef TEMPORAL_RS_ParsedDateTime_D_HPP
#define TEMPORAL_RS_ParsedDateTime_D_HPP

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
namespace capi { struct ParsedDateTime; }
class ParsedDateTime;
struct TemporalError;
} // namespace temporal_rs



namespace temporal_rs {
namespace capi {
    struct ParsedDateTime;
} // namespace capi
} // namespace

namespace temporal_rs {
class ParsedDateTime {
public:

  inline static temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ParsedDateTime>, temporal_rs::TemporalError> from_utf8(std::string_view s);

  inline static temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ParsedDateTime>, temporal_rs::TemporalError> from_utf16(std::u16string_view s);

    inline const temporal_rs::capi::ParsedDateTime* AsFFI() const;
    inline temporal_rs::capi::ParsedDateTime* AsFFI();
    inline static const temporal_rs::ParsedDateTime* FromFFI(const temporal_rs::capi::ParsedDateTime* ptr);
    inline static temporal_rs::ParsedDateTime* FromFFI(temporal_rs::capi::ParsedDateTime* ptr);
    inline static void operator delete(void* ptr);
private:
    ParsedDateTime() = delete;
    ParsedDateTime(const temporal_rs::ParsedDateTime&) = delete;
    ParsedDateTime(temporal_rs::ParsedDateTime&&) noexcept = delete;
    ParsedDateTime operator=(const temporal_rs::ParsedDateTime&) = delete;
    ParsedDateTime operator=(temporal_rs::ParsedDateTime&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // TEMPORAL_RS_ParsedDateTime_D_HPP
