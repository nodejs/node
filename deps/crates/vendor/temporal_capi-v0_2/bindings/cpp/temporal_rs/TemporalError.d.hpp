#ifndef TEMPORAL_RS_TemporalError_D_HPP
#define TEMPORAL_RS_TemporalError_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "ErrorKind.d.hpp"
#include "diplomat_runtime.hpp"
namespace temporal_rs {
class ErrorKind;
} // namespace temporal_rs



namespace temporal_rs {
namespace capi {
    struct TemporalError {
      temporal_rs::capi::ErrorKind kind;
      temporal_rs::diplomat::capi::OptionStringView msg;
    };

    typedef struct TemporalError_option {union { TemporalError ok; }; bool is_ok; } TemporalError_option;
} // namespace capi
} // namespace


namespace temporal_rs {
struct TemporalError {
    temporal_rs::ErrorKind kind;
    std::optional<std::string_view> msg;

    inline temporal_rs::capi::TemporalError AsFFI() const;
    inline static temporal_rs::TemporalError FromFFI(temporal_rs::capi::TemporalError c_struct);
};

} // namespace
#endif // TEMPORAL_RS_TemporalError_D_HPP
