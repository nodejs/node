#ifndef TEMPORAL_RS_UnsignedRoundingMode_D_HPP
#define TEMPORAL_RS_UnsignedRoundingMode_D_HPP

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
    enum UnsignedRoundingMode {
      UnsignedRoundingMode_Infinity = 0,
      UnsignedRoundingMode_Zero = 1,
      UnsignedRoundingMode_HalfInfinity = 2,
      UnsignedRoundingMode_HalfZero = 3,
      UnsignedRoundingMode_HalfEven = 4,
    };

    typedef struct UnsignedRoundingMode_option {union { UnsignedRoundingMode ok; }; bool is_ok; } UnsignedRoundingMode_option;
} // namespace capi
} // namespace

namespace temporal_rs {
class UnsignedRoundingMode {
public:
    enum Value {
        Infinity = 0,
        Zero = 1,
        HalfInfinity = 2,
        HalfZero = 3,
        HalfEven = 4,
    };

    UnsignedRoundingMode(): value(Value::Infinity) {}

    // Implicit conversions between enum and ::Value
    constexpr UnsignedRoundingMode(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

    inline temporal_rs::capi::UnsignedRoundingMode AsFFI() const;
    inline static temporal_rs::UnsignedRoundingMode FromFFI(temporal_rs::capi::UnsignedRoundingMode c_enum);
private:
    Value value;
};

} // namespace
#endif // TEMPORAL_RS_UnsignedRoundingMode_D_HPP
