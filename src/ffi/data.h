#pragma once

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "base_object.h"
#include "ffi.h"

#include <cstdint>

namespace node::ffi {

bool GetValidatedSignedInt(Environment* env,
                           v8::Local<v8::Value> value,
                           int64_t min,
                           int64_t max,
                           const char* type_name,
                           int64_t* out);

bool GetValidatedUnsignedInt(Environment* env,
                             v8::Local<v8::Value> value,
                             uint64_t max,
                             const char* type_name,
                             uint64_t* out);

bool GetValidatedPointerAddress(Environment* env,
                                v8::Local<v8::Value> value,
                                const char* label,
                                uintptr_t* out);

size_t GetFFIReturnValueStorageSize(ffi_type* type);
}  // namespace node::ffi

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
