#pragma once

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

namespace node::ffi {

// Returns true if executable memory allocation (mmap + mprotect to RX) works
// on this process. Runs a one-time self-test that allocates a tiny stub,
// writes a ret-style instruction, and transitions it to RX. The page is not
// executed: a successful RX transition is the support signal, and executing a
// freshly generated probe could crash the process on systems that block it.
//
// Catches:
//   - macOS MAP_JIT entitlement missing
//   - Hardened-runtime restrictions
//   - SELinux execmem denial
//   - Other OS-level restrictions on executable memory
//
// The self-test runs exactly once (std::call_once) and the result is cached
// process-wide. Subsequent calls return the cached value without re-running
// the test.
bool IsJitMemorySupported();

}  // namespace node::ffi

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
