#pragma once

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "base_object.h"
#include "ffi.h"

#include <cstdint>

namespace node::ffi {

v8::Maybe<int64_t> GetValidatedSignedInt(Environment* env,
                                         v8::Local<v8::Value> value,
                                         int64_t min,
                                         int64_t max,
                                         const char* type_name);

v8::Maybe<uint64_t> GetValidatedUnsignedInt(Environment* env,
                                            v8::Local<v8::Value> value,
                                            uint64_t max,
                                            const char* type_name);

v8::Maybe<uintptr_t> GetValidatedPointerAddress(Environment* env,
                                                v8::Local<v8::Value> value,
                                                const char* label);

size_t GetFFIReturnValueStorageSize(ffi_type* type);
}  // namespace node::ffi

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
