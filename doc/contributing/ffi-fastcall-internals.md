# FFI Fast-Call Internals

This document is for contributors who maintain or extend the FFI
fast-call path (the V8 Fast API Calls implementation in `node:ffi`).
For end-user behavior, see [doc/api/ffi.md](../api/ffi.md).

## Overview

For each registered FFI function whose signature is fast-call eligible
(`src/ffi/types.cc:IsFastCallEligible`), Node generates a tiny native
trampoline that strips the `Local<Object>` receiver V8 fast calls
require and tail-calls the user's target function. The trampoline
address is handed to `v8::CFunction`. A JS wrapper
(`lib/internal/ffi-fastcall.js`) validates args, routes object-typed
pointer args to a libffi slow path, and checks a per-library "alive"
sentinel before each call.

The libffi path remains for callbacks (`ffi_prep_closure_loc`),
ineligible signatures (signatures containing the FFI `function` type),
and unsupported platforms.

## Eligibility (`src/ffi/types.cc:IsFastCallEligible`)

A signature is fast-call eligible iff all of:

1. The platform is supported (see Platform Support below).
2. Return type is one of: void, i8/u8/i16/u16, i32/u32, i64/u64,
   f32/f64, pointer.
3. Every arg type is in that set.
4. No arg or return is the FFI `function` type.
5. Per-ABI argument caps:
   - AArch64: ≤ 7 GP, ≤ 8 FP
   - x86_64 SysV: ≤ 6 GP, ≤ 8 FP
   - x86_64 Win64: GP + FP combined ≤ 3 (positional register slots — 4 minus the receiver)
   - AArch32 hardfp: ≤ 3 GP, ≤ 8 FP; i64/u64 args and return type rejected

`IsFastCallEligible(fn, &reason)` returns false with a static reason
string on miss.

## Platform support

| ABI | Emitter file | Status |
|---|---|---|
| AArch64 (Linux/macOS/FreeBSD/Windows) | `stub_emitter_aarch64.cc` | Implemented, runtime-verified |
| x86_64 SysV (Linux/macOS/FreeBSD) | `stub_emitter_x64_sysv.cc` | Implemented, CI-verified |
| x86_64 Win64 | `stub_emitter_x64_win.cc` | Implemented, CI-verified |
| AArch32 hardfp (Linux/FreeBSD) | `stub_emitter_arm.cc` | Implemented, CI-verified |

On platforms without an emitter, all registrations fall back to libffi.

Adding a new ABI: implement `EmitForwarder` for the new platform in a
new `stub_emitter_<abi>.cc`, gate it via `node.gyp` conditions on
`target_arch` and `OS`, and add the `(os, arch)` pair to
`fastcall_supported` in `configure.py`.

## Stub generation (`src/ffi/fastcall/stub_emitter_*.cc`)

Each stub does, at most, three things:

1. Shift GP regs down by one slot (drop the receiver).
2. (Win64 only) shift FP regs down by one slot — Win64's FP/GP register
   slots are positional, so stripping a GP arg also reindexes FP slots.
3. Tail-call the target via an indirect jump.

For SysV ≥ 6 GP args, the stub uses a call+ret pattern with stack
rewrite (because the 7th GP slot lives on the stack). Other ABIs cap
below their stack overflow point in v1 to keep emitters simple.

## JIT memory (`src/ffi/fastcall/jit_memory.cc`)

A process-global singleton on top of platform `mmap`/`VirtualAlloc`.
Allocates 64-byte slot-aligned chunks within page-aligned allocations.
After writing the stub, the page is transitioned to RX via `mprotect` /
`VirtualProtect`; once a page goes RX, no further allocation happens
in it (the bump cursor is locked).

The original spec called for `v8::PageAllocator`, but neither
`Isolate::GetArrayBufferAllocator()->GetPageAllocator()` nor
`Platform::GetPageAllocator()` returns a usable allocator in Node's
embedded configuration — both default to `nullptr`. The implementation
uses direct system calls (with `MAP_JIT` on Apple Silicon) instead.

`Free` decrements the live-byte counter but does not return memory.
Pages stay alive for the process lifetime.

Concurrent emit from multiple isolates is safe via
`JitMemory::EmitStub(code, size)`, which holds the singleton mutex across
allocate + memcpy + RX-transition. The lower-level `Allocate` /
`MakeExecutable` / `Free` methods remain public for the self-test only
(which writes platform-specific instruction bytes after Allocate but
before MakeExecutable, and needs that explicit step ordering).

## Self-test

`JitMemory::SelfTest` allocates a tiny stub, writes a `ret`-style
native sequence, transitions to RX, and calls it. Cached in a
process-wide atomic via `std::call_once`. Run once per process at
first FFI registration. On failure, every subsequent registration
falls back to libffi-only and a process warning is emitted via
`ProcessEmitWarning`.

This catches:
- macOS `MAP_JIT` entitlement missing (e.g., signed binary without
  `com.apple.security.cs.allow-jit`).
- Hardened-runtime restrictions.
- SELinux execmem denial.

## JS wrapper (`lib/internal/ffi-fastcall.js`)

For each fast-call-eligible inner v8::Function returned from C++,
`buildWrapper` creates a JS wrapper that:

1. Reads the per-library "alive" `Uint8Array` and throws
   `ERR_FFI_LIBRARY_CLOSED` if `[0] !== 0`.
2. Per-arg validation, mirroring `ToFFIArgument` in
   `src/ffi/types.cc:ToFFIArgument`. Same `ERR_INVALID_ARG_VALUE`
   codes, same messages, same range bounds.
3. Pointer args:
   - BigInt or null/undefined: pass through as primitive.
   - String / Buffer / ArrayBuffer / ArrayBufferView: `ReflectApply`
     the `kFastcallInvokeSlow` libffi-backed v8::Function with the
     original args.
4. Calls the inner v8::Function with positional primitives. V8's fast
   call engages when TurboFan inlines the wrapper.

The wrapper body is **arity-specialized**: arities 0..6 are unrolled into
distinct closures with named parameters (`function(a0, a1, ...)`), so V8
inlines them and the per-arg type info / pointer flag are read from
closure locals instead of arrays. Arities 7+ use a rest-args fallback. This
matters: an earlier draft used a single generic `function(...args)` plus
`ReflectApply`, which dropped FFI throughput by 30–50% vs. the libffi+SB
path. The arity specialization gets the throughput back to 5–13× the
libffi+SB baseline (see commit `81d908e48da` for the fix and benchmarks).

The wrapper is patched onto `DynamicLibrary.prototype.getFunction`,
`getFunctions`, and the `functions` accessor.

## Internal symbols

The JS wrapper looks for these per-isolate Symbols on the inner
`v8::Function`. They are defined in `src/env_properties.h` and
attached by `DynamicLibrary::CreateFunction` for fast-call-eligible
signatures only:

| Symbol | Value | Purpose |
|---|---|---|
| `kFastcallAlive` | `Uint8Array(1)` shared with `DynamicLibrary` | close sentinel |
| `kFastcallInvokeSlow` | `v8::Function` over `InvokeFunction` | object-arg fallback |
| `kFastcallParams` | `string[]` of parameter type names | wrapper introspection |
| `kFastcallResult` | result type name string | wrapper introspection |

## Lifecycle

**Registration:** `CreateFunction` in `src/node_ffi.cc` builds a
`fastcall::CFunctionInfoBundle` (which owns the heap-allocated
`v8::CFunctionInfo` + `v8::CTypeInfo[]`), allocates and emits the stub via
`JitMemory::EmitStub`, then constructs the inner `v8::Function` via a
`FunctionTemplate` with the `CFunction` attached. Per-function fast-call
state is stored on `FFIFunctionInfo::fast` (a `unique_ptr<FastCallState>`,
null when fast-call is unavailable for that signature).

**Per-call:** wrapper validates → calls inner. V8 picks fast or slow
callback. Slow = `InvokeFunction` (libffi); fast = our generated stub →
target.

**`lib.close()`:** flips the alive sentinel (`alive[0] = 1`). The wrapper
throws `ERR_FFI_LIBRARY_CLOSED` on subsequent calls. Slow-path
`InvokeFunction` independently checks `fn->closed` for the same effect on
ineligible signatures. Stubs are NOT freed at close.

**Weak callback (function GC'd):** `CleanupFunctionInfo` resets
`info->fast`, whose `~FastCallState` destructor calls `JitMemory::Free`
on the stub.

## Testing

- `test/cctest/test_ffi_fastcall_*.cc`: unit tests for emitters, JIT
  memory, eligibility, CFunctionInfo builder.
- `test/ffi/test-ffi-*.js`: JS-level integration tests covering
  types, arity, callbacks, permissions, etc. (existing FFI suite —
  reused as the integration baseline).

When debugging unexpected fast-call behavior, log the eligibility miss
reason via the second arg to `IsFastCallEligible`. Set the
`--without-ffi-fastcall` configure flag to A/B test against the
libffi-only path.
