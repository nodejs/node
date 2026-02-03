#ifndef TEMPORAL_RS_Unit_D_HPP
#define TEMPORAL_RS_Unit_D_HPP

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
    enum Unit {
      Unit_Auto = 0,
      Unit_Nanosecond = 1,
      Unit_Microsecond = 2,
      Unit_Millisecond = 3,
      Unit_Second = 4,
      Unit_Minute = 5,
      Unit_Hour = 6,
      Unit_Day = 7,
      Unit_Week = 8,
      Unit_Month = 9,
      Unit_Year = 10,
    };

    typedef struct Unit_option {union { Unit ok; }; bool is_ok; } Unit_option;
} // namespace capi
} // namespace

namespace temporal_rs {
class Unit {
public:
    enum Value {
        Auto = 0,
        Nanosecond = 1,
        Microsecond = 2,
        Millisecond = 3,
        Second = 4,
        Minute = 5,
        Hour = 6,
        Day = 7,
        Week = 8,
        Month = 9,
        Year = 10,
    };

    Unit(): value(Value::Auto) {}

    // Implicit conversions between enum and ::Value
    constexpr Unit(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

    inline temporal_rs::capi::Unit AsFFI() const;
    inline static temporal_rs::Unit FromFFI(temporal_rs::capi::Unit c_enum);
private:
    Value value;
};

} // namespace
#endif // TEMPORAL_RS_Unit_D_HPP
