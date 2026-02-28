#ifndef TEMPORAL_RS_TemporalError_HPP
#define TEMPORAL_RS_TemporalError_HPP

#include "TemporalError.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "ErrorKind.hpp"
#include "diplomat_runtime.hpp"


namespace temporal_rs {
namespace capi {
    extern "C" {

    } // extern "C"
} // namespace capi
} // namespace


inline temporal_rs::capi::TemporalError temporal_rs::TemporalError::AsFFI() const {
    return temporal_rs::capi::TemporalError {
        /* .kind = */ kind.AsFFI(),
        /* .msg = */ msg.has_value() ? (temporal_rs::diplomat::capi::OptionStringView{ { {msg.value().data(), msg.value().size()} }, true }) : (temporal_rs::diplomat::capi::OptionStringView{ {}, false }),
    };
}

inline temporal_rs::TemporalError temporal_rs::TemporalError::FromFFI(temporal_rs::capi::TemporalError c_struct) {
    return temporal_rs::TemporalError {
        /* .kind = */ temporal_rs::ErrorKind::FromFFI(c_struct.kind),
        /* .msg = */ c_struct.msg.is_ok ? std::optional(std::string_view(c_struct.msg.ok.data, c_struct.msg.ok.len)) : std::nullopt,
    };
}


#endif // TEMPORAL_RS_TemporalError_HPP
