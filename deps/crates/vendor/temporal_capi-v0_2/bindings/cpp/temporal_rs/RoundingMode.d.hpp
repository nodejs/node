#ifndef TEMPORAL_RS_RoundingMode_D_HPP
#define TEMPORAL_RS_RoundingMode_D_HPP

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
    enum RoundingMode {
      RoundingMode_Ceil = 0,
      RoundingMode_Floor = 1,
      RoundingMode_Expand = 2,
      RoundingMode_Trunc = 3,
      RoundingMode_HalfCeil = 4,
      RoundingMode_HalfFloor = 5,
      RoundingMode_HalfExpand = 6,
      RoundingMode_HalfTrunc = 7,
      RoundingMode_HalfEven = 8,
    };

    typedef struct RoundingMode_option {union { RoundingMode ok; }; bool is_ok; } RoundingMode_option;
} // namespace capi
} // namespace

namespace temporal_rs {
class RoundingMode {
public:
    enum Value {
        Ceil = 0,
        Floor = 1,
        Expand = 2,
        Trunc = 3,
        HalfCeil = 4,
        HalfFloor = 5,
        HalfExpand = 6,
        HalfTrunc = 7,
        HalfEven = 8,
    };

    RoundingMode(): value(Value::Ceil) {}

    // Implicit conversions between enum and ::Value
    constexpr RoundingMode(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

    inline temporal_rs::capi::RoundingMode AsFFI() const;
    inline static temporal_rs::RoundingMode FromFFI(temporal_rs::capi::RoundingMode c_enum);
private:
    Value value;
};

} // namespace
#endif // TEMPORAL_RS_RoundingMode_D_HPP
