#pragma once

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "ffi.h"
#include "v8-fast-api-calls.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace node::ffi {

struct FFIFunction;

enum class FastFFIType : uint8_t {
  kVoid,
  kInt8,
  kUint8,
  kInt16,
  kUint16,
  kInt32,
  kUint32,
  kInt64,
  kUint64,
  kFloat32,
  kFloat64,
  kPointer,
  kBuffer,
};

struct FastFFITrampoline {
  void* code = nullptr;
  size_t size = 0;
};

struct FastFFIMetadata {
  FastFFIMetadata() = default;
  ~FastFFIMetadata();

  FastFFIMetadata(const FastFFIMetadata&) = delete;
  FastFFIMetadata& operator=(const FastFFIMetadata&) = delete;

  FastFFITrampoline trampoline;
  std::vector<v8::CTypeInfo> arg_info;
  std::unique_ptr<v8::CFunctionInfo> c_function_info;
  v8::CFunction c_function;
};

// Public detection queries.

// Returns true if the fast-call path is available at all on this process
// (platform stub emitter exists + JIT memory self-test passed). Independent
// of any particular signature — if this returns false, no signature can
// use the fast-call path.
bool IsFastCallSupported();

bool SignatureNeedsRawPointerConversions(const FFIFunction& fn);
bool IsPointerTypeName(const std::string& name);
bool SignatureNeedsFastBufferInvoke(const FFIFunction& fn);
std::shared_ptr<FFIFunction> CloneWithFastBufferArgNames(
    const std::shared_ptr<FFIFunction>& fn);
std::unique_ptr<FastFFIMetadata> CreateFastFFIMetadata(const FFIFunction& fn);

}  // namespace node::ffi

extern "C" {
uintptr_t node_ffi_fast_buffer_data(v8::Local<v8::Value> value,
                                    v8::FastApiCallbackOptions* options,
                                    uint32_t index);
bool node_ffi_create_fast_trampoline(void* target,
                                     const node::ffi::FastFFIType* args,
                                     size_t argc,
                                     node::ffi::FastFFIType result,
                                     node::ffi::FastFFITrampoline* out);
void node_ffi_free_fast_trampoline(node::ffi::FastFFITrampoline* trampoline);
}

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
