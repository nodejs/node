#ifndef TEMPORAL_RS_RoundingOptions_D_HPP
#define TEMPORAL_RS_RoundingOptions_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "RoundingMode.d.hpp"
#include "Unit.d.hpp"
#include "diplomat_runtime.hpp"
namespace temporal_rs {
class RoundingMode;
class Unit;
} // namespace temporal_rs



namespace temporal_rs {
namespace capi {
    struct RoundingOptions {
      temporal_rs::capi::Unit_option largest_unit;
      temporal_rs::capi::Unit_option smallest_unit;
      temporal_rs::capi::RoundingMode_option rounding_mode;
      temporal_rs::diplomat::capi::OptionU32 increment;
    };

    typedef struct RoundingOptions_option {union { RoundingOptions ok; }; bool is_ok; } RoundingOptions_option;
} // namespace capi
} // namespace


namespace temporal_rs {
struct RoundingOptions {
    std::optional<temporal_rs::Unit> largest_unit;
    std::optional<temporal_rs::Unit> smallest_unit;
    std::optional<temporal_rs::RoundingMode> rounding_mode;
    std::optional<uint32_t> increment;

    inline temporal_rs::capi::RoundingOptions AsFFI() const;
    inline static temporal_rs::RoundingOptions FromFFI(temporal_rs::capi::RoundingOptions c_struct);
};

} // namespace
#endif // TEMPORAL_RS_RoundingOptions_D_HPP
