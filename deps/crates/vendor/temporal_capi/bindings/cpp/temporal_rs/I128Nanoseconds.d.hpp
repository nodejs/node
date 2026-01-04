#ifndef TEMPORAL_RS_I128Nanoseconds_D_HPP
#define TEMPORAL_RS_I128Nanoseconds_D_HPP

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
namespace capi {
    struct I128Nanoseconds {
      uint64_t high;
      uint64_t low;
    };

    typedef struct I128Nanoseconds_option {union { I128Nanoseconds ok; }; bool is_ok; } I128Nanoseconds_option;
} // namespace capi
} // namespace


namespace temporal_rs {
/**
 * For portability, we use two u64s instead of an i128.
 * The high bit of the u64 is the sign.
 * This cannot represent i128::MIN, and has a -0, but those are largely
 * irrelevant for this purpose.
 *
 * This could potentially instead be a bit-by-bit split, or something else
 */
struct I128Nanoseconds {
    uint64_t high;
    uint64_t low;

  inline bool is_valid() const;

    inline temporal_rs::capi::I128Nanoseconds AsFFI() const;
    inline static temporal_rs::I128Nanoseconds FromFFI(temporal_rs::capi::I128Nanoseconds c_struct);
};

} // namespace
#endif // TEMPORAL_RS_I128Nanoseconds_D_HPP
