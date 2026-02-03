#ifndef TEMPORAL_RS_Provider_D_HPP
#define TEMPORAL_RS_Provider_D_HPP

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
} // namespace temporal_rs



namespace temporal_rs {
namespace capi {
    struct Provider;
} // namespace capi
} // namespace

namespace temporal_rs {
/**
 * A time zone data provider
 */
class Provider {
public:

  /**
   * Construct a provider backed by a zoneinfo64.res file
   *
   * This failing to construct is not a Temporal error, so it just returns ()
   */
  inline static temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Provider>, std::monostate> new_zoneinfo64(temporal_rs::diplomat::span<const uint32_t> data);

  inline static std::unique_ptr<temporal_rs::Provider> new_compiled();

  /**
   * Fallback type in case construction does not work.
   */
  inline static std::unique_ptr<temporal_rs::Provider> empty();

    inline const temporal_rs::capi::Provider* AsFFI() const;
    inline temporal_rs::capi::Provider* AsFFI();
    inline static const temporal_rs::Provider* FromFFI(const temporal_rs::capi::Provider* ptr);
    inline static temporal_rs::Provider* FromFFI(temporal_rs::capi::Provider* ptr);
    inline static void operator delete(void* ptr);
private:
    Provider() = delete;
    Provider(const temporal_rs::Provider&) = delete;
    Provider(temporal_rs::Provider&&) noexcept = delete;
    Provider operator=(const temporal_rs::Provider&) = delete;
    Provider operator=(temporal_rs::Provider&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // TEMPORAL_RS_Provider_D_HPP
