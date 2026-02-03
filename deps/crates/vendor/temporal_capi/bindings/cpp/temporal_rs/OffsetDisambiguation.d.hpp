#ifndef TEMPORAL_RS_OffsetDisambiguation_D_HPP
#define TEMPORAL_RS_OffsetDisambiguation_D_HPP

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
    enum OffsetDisambiguation {
      OffsetDisambiguation_Use = 0,
      OffsetDisambiguation_Prefer = 1,
      OffsetDisambiguation_Ignore = 2,
      OffsetDisambiguation_Reject = 3,
    };

    typedef struct OffsetDisambiguation_option {union { OffsetDisambiguation ok; }; bool is_ok; } OffsetDisambiguation_option;
} // namespace capi
} // namespace

namespace temporal_rs {
class OffsetDisambiguation {
public:
    enum Value {
        Use = 0,
        Prefer = 1,
        Ignore = 2,
        Reject = 3,
    };

    OffsetDisambiguation(): value(Value::Use) {}

    // Implicit conversions between enum and ::Value
    constexpr OffsetDisambiguation(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

    inline temporal_rs::capi::OffsetDisambiguation AsFFI() const;
    inline static temporal_rs::OffsetDisambiguation FromFFI(temporal_rs::capi::OffsetDisambiguation c_enum);
private:
    Value value;
};

} // namespace
#endif // TEMPORAL_RS_OffsetDisambiguation_D_HPP
