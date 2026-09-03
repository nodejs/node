#ifndef TEMPORAL_RS_Calendar_D_HPP
#define TEMPORAL_RS_Calendar_D_HPP

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
namespace capi { struct Calendar; }
class Calendar;
struct TemporalError;
class AnyCalendarKind;
} // namespace temporal_rs



namespace temporal_rs {
namespace capi {
    struct Calendar;
} // namespace capi
} // namespace

namespace temporal_rs {
/**
 * By and large one should not need to construct this type; it mostly exists
 * to represent calendar data found on other Temporal types, obtained via `.calendar()`.
 */
class Calendar {
public:

  inline static std::unique_ptr<temporal_rs::Calendar> try_new_constrain(temporal_rs::AnyCalendarKind kind);

  inline static temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Calendar>, temporal_rs::TemporalError> from_utf8(std::string_view s);

  inline bool is_iso() const;

  inline std::string_view identifier() const;

  /**
   * Returns the kind of this calendar
   */
  inline temporal_rs::AnyCalendarKind kind() const;

    inline const temporal_rs::capi::Calendar* AsFFI() const;
    inline temporal_rs::capi::Calendar* AsFFI();
    inline static const temporal_rs::Calendar* FromFFI(const temporal_rs::capi::Calendar* ptr);
    inline static temporal_rs::Calendar* FromFFI(temporal_rs::capi::Calendar* ptr);
    inline static void operator delete(void* ptr);
private:
    Calendar() = delete;
    Calendar(const temporal_rs::Calendar&) = delete;
    Calendar(temporal_rs::Calendar&&) noexcept = delete;
    Calendar operator=(const temporal_rs::Calendar&) = delete;
    Calendar operator=(temporal_rs::Calendar&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // TEMPORAL_RS_Calendar_D_HPP
