#ifndef TEMPORAL_RS_DisplayCalendar_D_HPP
#define TEMPORAL_RS_DisplayCalendar_D_HPP

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
    enum DisplayCalendar {
      DisplayCalendar_Auto = 0,
      DisplayCalendar_Always = 1,
      DisplayCalendar_Never = 2,
      DisplayCalendar_Critical = 3,
    };

    typedef struct DisplayCalendar_option {union { DisplayCalendar ok; }; bool is_ok; } DisplayCalendar_option;
} // namespace capi
} // namespace

namespace temporal_rs {
class DisplayCalendar {
public:
    enum Value {
        Auto = 0,
        Always = 1,
        Never = 2,
        Critical = 3,
    };

    DisplayCalendar(): value(Value::Auto) {}

    // Implicit conversions between enum and ::Value
    constexpr DisplayCalendar(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

    inline temporal_rs::capi::DisplayCalendar AsFFI() const;
    inline static temporal_rs::DisplayCalendar FromFFI(temporal_rs::capi::DisplayCalendar c_enum);
private:
    Value value;
};

} // namespace
#endif // TEMPORAL_RS_DisplayCalendar_D_HPP
