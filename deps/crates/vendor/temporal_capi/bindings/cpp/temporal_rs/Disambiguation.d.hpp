#ifndef TEMPORAL_RS_Disambiguation_D_HPP
#define TEMPORAL_RS_Disambiguation_D_HPP

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
    enum Disambiguation {
      Disambiguation_Compatible = 0,
      Disambiguation_Earlier = 1,
      Disambiguation_Later = 2,
      Disambiguation_Reject = 3,
    };

    typedef struct Disambiguation_option {union { Disambiguation ok; }; bool is_ok; } Disambiguation_option;
} // namespace capi
} // namespace

namespace temporal_rs {
class Disambiguation {
public:
    enum Value {
        Compatible = 0,
        Earlier = 1,
        Later = 2,
        Reject = 3,
    };

    Disambiguation(): value(Value::Compatible) {}

    // Implicit conversions between enum and ::Value
    constexpr Disambiguation(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

    inline temporal_rs::capi::Disambiguation AsFFI() const;
    inline static temporal_rs::Disambiguation FromFFI(temporal_rs::capi::Disambiguation c_enum);
private:
    Value value;
};

} // namespace
#endif // TEMPORAL_RS_Disambiguation_D_HPP
