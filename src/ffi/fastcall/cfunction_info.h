#pragma once

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS && \
    defined(HAVE_FFI_FASTCALL)

#include <vector>

#include "ffi/fastcall/stub_emitter.h"
#include "node_ffi.h"
#include "v8-fast-api-calls.h"

namespace node::ffi::fastcall {

// Owns dynamically-allocated CFunctionInfo + CTypeInfo[] arrays for a
// single FFI function. Lifetime is tied to the FFIFunctionInfo. Free
// on weak-callback / dynamic library teardown.
struct CFunctionInfoBundle {
  v8::CFunctionInfo* info = nullptr;
  v8::CTypeInfo* arg_types = nullptr;
  std::vector<ArgClass> arg_classes;
  ResultClass result_class = ResultClass::kVoid;

  CFunctionInfoBundle() = default;
  ~CFunctionInfoBundle();
  CFunctionInfoBundle(const CFunctionInfoBundle&) = delete;
  CFunctionInfoBundle& operator=(const CFunctionInfoBundle&) = delete;
  CFunctionInfoBundle(CFunctionInfoBundle&&) noexcept;
  CFunctionInfoBundle& operator=(CFunctionInfoBundle&&) noexcept;
};

// Build a CFunctionInfo + CTypeInfo[] for `fn`. The caller must already have
// verified `fn` via IsFastCallEligible before calling this. With
// -fno-exceptions, `new` aborts on OOM rather than throwing, so this function
// never fails — it returns directly instead of using optional.
CFunctionInfoBundle BuildCFunctionInfo(const FFIFunction& fn);

}  // namespace node::ffi::fastcall

#endif  // NODE_WANT_INTERNALS && HAVE_FFI_FASTCALL
