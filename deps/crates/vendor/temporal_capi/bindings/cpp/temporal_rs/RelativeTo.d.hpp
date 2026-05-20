#ifndef TEMPORAL_RS_RelativeTo_D_HPP
#define TEMPORAL_RS_RelativeTo_D_HPP

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
namespace capi { struct PlainDate; }
class PlainDate;
namespace capi { struct ZonedDateTime; }
class ZonedDateTime;
} // namespace temporal_rs



namespace temporal_rs {
namespace capi {
    struct RelativeTo {
      const temporal_rs::capi::PlainDate* date;
      const temporal_rs::capi::ZonedDateTime* zoned;
    };

    typedef struct RelativeTo_option {union { RelativeTo ok; }; bool is_ok; } RelativeTo_option;
} // namespace capi
} // namespace


namespace temporal_rs {
struct RelativeTo {
    const temporal_rs::PlainDate* date;
    const temporal_rs::ZonedDateTime* zoned;

    inline temporal_rs::capi::RelativeTo AsFFI() const;
    inline static temporal_rs::RelativeTo FromFFI(temporal_rs::capi::RelativeTo c_struct);
};

} // namespace
#endif // TEMPORAL_RS_RelativeTo_D_HPP
