#ifndef TEMPORAL_RS_PartialDateTime_D_HPP
#define TEMPORAL_RS_PartialDateTime_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "PartialDate.d.hpp"
#include "PartialTime.d.hpp"
#include "diplomat_runtime.hpp"
namespace temporal_rs {
struct PartialDate;
struct PartialTime;
} // namespace temporal_rs



namespace temporal_rs {
namespace capi {
    struct PartialDateTime {
      temporal_rs::capi::PartialDate date;
      temporal_rs::capi::PartialTime time;
    };

    typedef struct PartialDateTime_option {union { PartialDateTime ok; }; bool is_ok; } PartialDateTime_option;
} // namespace capi
} // namespace


namespace temporal_rs {
struct PartialDateTime {
    temporal_rs::PartialDate date;
    temporal_rs::PartialTime time;

    inline temporal_rs::capi::PartialDateTime AsFFI() const;
    inline static temporal_rs::PartialDateTime FromFFI(temporal_rs::capi::PartialDateTime c_struct);
};

} // namespace
#endif // TEMPORAL_RS_PartialDateTime_D_HPP
