#ifndef TEMPORAL_RS_TransitionDirection_D_HPP
#define TEMPORAL_RS_TransitionDirection_D_HPP

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
    enum TransitionDirection {
      TransitionDirection_Next = 0,
      TransitionDirection_Previous = 1,
    };

    typedef struct TransitionDirection_option {union { TransitionDirection ok; }; bool is_ok; } TransitionDirection_option;
} // namespace capi
} // namespace

namespace temporal_rs {
class TransitionDirection {
public:
    enum Value {
        Next = 0,
        Previous = 1,
    };

    TransitionDirection(): value(Value::Next) {}

    // Implicit conversions between enum and ::Value
    constexpr TransitionDirection(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

    inline temporal_rs::capi::TransitionDirection AsFFI() const;
    inline static temporal_rs::TransitionDirection FromFFI(temporal_rs::capi::TransitionDirection c_enum);
private:
    Value value;
};

} // namespace
#endif // TEMPORAL_RS_TransitionDirection_D_HPP
