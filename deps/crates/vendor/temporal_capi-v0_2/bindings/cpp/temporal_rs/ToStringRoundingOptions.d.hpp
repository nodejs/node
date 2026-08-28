#ifndef TEMPORAL_RS_ToStringRoundingOptions_D_HPP
#define TEMPORAL_RS_ToStringRoundingOptions_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "Precision.d.hpp"
#include "RoundingMode.d.hpp"
#include "Unit.d.hpp"
#include "diplomat_runtime.hpp"
namespace temporal_rs {
struct Precision;
class RoundingMode;
class Unit;
} // namespace temporal_rs



namespace temporal_rs {
namespace capi {
    struct ToStringRoundingOptions {
      temporal_rs::capi::Precision precision;
      temporal_rs::capi::Unit_option smallest_unit;
      temporal_rs::capi::RoundingMode_option rounding_mode;
    };

    typedef struct ToStringRoundingOptions_option {union { ToStringRoundingOptions ok; }; bool is_ok; } ToStringRoundingOptions_option;
} // namespace capi
} // namespace


namespace temporal_rs {
struct ToStringRoundingOptions {
    temporal_rs::Precision precision;
    std::optional<temporal_rs::Unit> smallest_unit;
    std::optional<temporal_rs::RoundingMode> rounding_mode;

    inline temporal_rs::capi::ToStringRoundingOptions AsFFI() const;
    inline static temporal_rs::ToStringRoundingOptions FromFFI(temporal_rs::capi::ToStringRoundingOptions c_struct);
};

} // namespace
#endif // TEMPORAL_RS_ToStringRoundingOptions_D_HPP
