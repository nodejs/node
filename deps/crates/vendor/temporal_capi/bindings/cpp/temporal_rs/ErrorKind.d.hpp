#ifndef TEMPORAL_RS_ErrorKind_D_HPP
#define TEMPORAL_RS_ErrorKind_D_HPP

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
    enum ErrorKind {
      ErrorKind_Generic = 0,
      ErrorKind_Type = 1,
      ErrorKind_Range = 2,
      ErrorKind_Syntax = 3,
      ErrorKind_Assert = 4,
    };

    typedef struct ErrorKind_option {union { ErrorKind ok; }; bool is_ok; } ErrorKind_option;
} // namespace capi
} // namespace

namespace temporal_rs {
class ErrorKind {
public:
    enum Value {
        Generic = 0,
        Type = 1,
        Range = 2,
        Syntax = 3,
        Assert = 4,
    };

    ErrorKind(): value(Value::Generic) {}

    // Implicit conversions between enum and ::Value
    constexpr ErrorKind(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

    inline temporal_rs::capi::ErrorKind AsFFI() const;
    inline static temporal_rs::ErrorKind FromFFI(temporal_rs::capi::ErrorKind c_enum);
private:
    Value value;
};

} // namespace
#endif // TEMPORAL_RS_ErrorKind_D_HPP
