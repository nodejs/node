#ifndef TEMPORAL_RS_Precision_D_HPP
#define TEMPORAL_RS_Precision_D_HPP

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
    struct Precision {
      bool is_minute;
      temporal_rs::diplomat::capi::OptionU8 precision;
    };

    typedef struct Precision_option {union { Precision ok; }; bool is_ok; } Precision_option;
} // namespace capi
} // namespace


namespace temporal_rs {
struct Precision {
    bool is_minute;
    std::optional<uint8_t> precision;

    inline temporal_rs::capi::Precision AsFFI() const;
    inline static temporal_rs::Precision FromFFI(temporal_rs::capi::Precision c_struct);
};

} // namespace
#endif // TEMPORAL_RS_Precision_D_HPP
