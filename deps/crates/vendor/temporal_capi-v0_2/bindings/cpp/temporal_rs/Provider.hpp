#ifndef TEMPORAL_RS_Provider_HPP
#define TEMPORAL_RS_Provider_HPP

#include "Provider.d.hpp"

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
    extern "C" {

    typedef struct temporal_rs_Provider_new_zoneinfo64_result {union {temporal_rs::capi::Provider* ok; }; bool is_ok;} temporal_rs_Provider_new_zoneinfo64_result;
    temporal_rs_Provider_new_zoneinfo64_result temporal_rs_Provider_new_zoneinfo64(temporal_rs::diplomat::capi::DiplomatU32View data);

    temporal_rs::capi::Provider* temporal_rs_Provider_new_compiled(void);

    temporal_rs::capi::Provider* temporal_rs_Provider_empty(void);

    void temporal_rs_Provider_destroy(Provider* self);

    } // extern "C"
} // namespace capi
} // namespace

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Provider>, std::monostate> temporal_rs::Provider::new_zoneinfo64(temporal_rs::diplomat::span<const uint32_t> data) {
    auto result = temporal_rs::capi::temporal_rs_Provider_new_zoneinfo64({data.data(), data.size()});
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Provider>, std::monostate>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::Provider>>(std::unique_ptr<temporal_rs::Provider>(temporal_rs::Provider::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Provider>, std::monostate>(temporal_rs::diplomat::Err<std::monostate>());
}

inline std::unique_ptr<temporal_rs::Provider> temporal_rs::Provider::new_compiled() {
    auto result = temporal_rs::capi::temporal_rs_Provider_new_compiled();
    return std::unique_ptr<temporal_rs::Provider>(temporal_rs::Provider::FromFFI(result));
}

inline std::unique_ptr<temporal_rs::Provider> temporal_rs::Provider::empty() {
    auto result = temporal_rs::capi::temporal_rs_Provider_empty();
    return std::unique_ptr<temporal_rs::Provider>(temporal_rs::Provider::FromFFI(result));
}

inline const temporal_rs::capi::Provider* temporal_rs::Provider::AsFFI() const {
    return reinterpret_cast<const temporal_rs::capi::Provider*>(this);
}

inline temporal_rs::capi::Provider* temporal_rs::Provider::AsFFI() {
    return reinterpret_cast<temporal_rs::capi::Provider*>(this);
}

inline const temporal_rs::Provider* temporal_rs::Provider::FromFFI(const temporal_rs::capi::Provider* ptr) {
    return reinterpret_cast<const temporal_rs::Provider*>(ptr);
}

inline temporal_rs::Provider* temporal_rs::Provider::FromFFI(temporal_rs::capi::Provider* ptr) {
    return reinterpret_cast<temporal_rs::Provider*>(ptr);
}

inline void temporal_rs::Provider::operator delete(void* ptr) {
    temporal_rs::capi::temporal_rs_Provider_destroy(reinterpret_cast<temporal_rs::capi::Provider*>(ptr));
}


#endif // TEMPORAL_RS_Provider_HPP
