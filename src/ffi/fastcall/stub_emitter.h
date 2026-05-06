#pragma once

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS && \
    defined(HAVE_FFI_FASTCALL)

#include <optional>
#include <vector>

#include "ffi/fastcall/jit_memory.h"

namespace v8 { class Isolate; }

namespace node::ffi::fastcall {

// One slot per native arg of the target. Does not include kVoid — that is
// only meaningful as a return type (see ResultClass).
enum class ArgClass : uint8_t {
  kGP,        // any 32/64-bit integer or pointer (single GP slot)
  kFP,        // float or double (single FP slot)
  kGPPair,    // 64-bit integer on AArch32 (two GP slots; not used on
              // AArch64/x86_64 — unsupported in v1)
};

// Classification of the return type. Separate from ArgClass to make it
// impossible to pass kVoid as an argument or kGPPair as a result.
enum class ResultClass : uint8_t {
  kGP,    // integer or pointer return
  kFP,    // float or double return
  kVoid,  // void return
};

// Build a stub for the target. The receiver is implicit and consumes
// one GP slot from V8 ahead of the listed args. Returns std::nullopt on
// failure (out of JIT memory, signature exceeds ABI cap, unsupported
// platform, kGPPair on non-AArch32).
//
// `target` is the native function pointer to tail-call.
// `args` lists the target's args in order. The receiver is implicit.
// `result` is informational on most ABIs; the stub does not synthesize
// a return value, only forwards.
std::optional<EmittedStub> EmitForwarder(
    v8::Isolate* isolate,
    void* target,
    const std::vector<ArgClass>& args,
    ResultClass result);

}  // namespace node::ffi::fastcall

#endif  // NODE_WANT_INTERNALS && HAVE_FFI_FASTCALL
