#ifndef TEMPORAL_RS_ArithmeticOverflow_D_HPP
#define TEMPORAL_RS_ArithmeticOverflow_D_HPP

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
    enum ArithmeticOverflow {
      ArithmeticOverflow_Constrain = 0,
      ArithmeticOverflow_Reject = 1,
    };

    typedef struct ArithmeticOverflow_option {union { ArithmeticOverflow ok; }; bool is_ok; } ArithmeticOverflow_option;
} // namespace capi
} // namespace

namespace temporal_rs {
class ArithmeticOverflow {
public:
    enum Value {
        Constrain = 0,
        Reject = 1,
    };

    ArithmeticOverflow(): value(Value::Constrain) {}

    // Implicit conversions between enum and ::Value
    constexpr ArithmeticOverflow(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

    inline temporal_rs::capi::ArithmeticOverflow AsFFI() const;
    inline static temporal_rs::ArithmeticOverflow FromFFI(temporal_rs::capi::ArithmeticOverflow c_enum);
private:
    Value value;
};

} // namespace
#endif // TEMPORAL_RS_ArithmeticOverflow_D_HPP
