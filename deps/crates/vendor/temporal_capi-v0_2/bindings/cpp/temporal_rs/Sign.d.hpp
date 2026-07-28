#ifndef TEMPORAL_RS_Sign_D_HPP
#define TEMPORAL_RS_Sign_D_HPP

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
    enum Sign {
      Sign_Positive = 1,
      Sign_Zero = 0,
      Sign_Negative = -1,
    };

    typedef struct Sign_option {union { Sign ok; }; bool is_ok; } Sign_option;
} // namespace capi
} // namespace

namespace temporal_rs {
class Sign {
public:
    enum Value {
        Positive = 1,
        Zero = 0,
        Negative = -1,
    };

    Sign(): value(Value::Zero) {}

    // Implicit conversions between enum and ::Value
    constexpr Sign(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

    inline temporal_rs::capi::Sign AsFFI() const;
    inline static temporal_rs::Sign FromFFI(temporal_rs::capi::Sign c_enum);
private:
    Value value;
};

} // namespace
#endif // TEMPORAL_RS_Sign_D_HPP
