# FFI Fast API internals

This document describes the internal implementation of the `node:ffi` Fast API
path. It is intended for contributors working on the FFI implementation, not for
users of the public API.

The Fast API path is an optimization layer for FFI calls whose signatures can be
represented by V8 Fast API metadata and a generated native trampoline. It does
not replace the generic libffi path. Instead, Node.js creates the fastest
available callable for each signature and keeps the generic path available for
unsupported call shapes, deoptimized V8 calls, and validation behavior that must
match the public FFI API.

## Goals

The Fast API implementation is designed around these goals:

* Keep hot scalar FFI calls out of the generic `v8::FunctionCallbackInfo` path.
* Avoid per-call allocation for common numeric and pointer-like signatures.
* Preserve the public `node:ffi` behavior and error shape.
* Keep string lifetime management in JavaScript, where temporary buffers can be
  owned explicitly.
* Keep SharedBuffer and Fast API routing separate, with `lib/ffi.js` composing
  the two wrapper layers.
* Use generated per-signature native code instead of runtime loops inside the
  trampoline.
* Fall back cleanly when executable memory, platform trampoline support, or
  signature eligibility is unavailable.

## Main Files

The implementation is split across these files:

* `src/ffi/fast.h` declares the Fast API type model, metadata containers,
  detection query, and platform trampoline hooks.
* `src/ffi/fast.cc` maps public FFI type names to Fast API types, creates V8
  `CFunctionInfo` metadata, checks JIT-memory support and signature eligibility,
  and exposes buffer conversion helpers used by generated trampolines.
* `src/ffi/jit_memory.{h,cc}` performs the runtime executable-memory self-test
  used to decide whether generated trampolines are allowed in this process.
* `src/ffi/types.{h,cc}` parses public FFI signatures and implements
  `IsFastCallEligible()`, which rejects signatures that the current Fast API
  trampolines cannot represent.
* `src/ffi/platforms/*.cc` contain the platform trampoline generators. These
  files follow the contract exposed by `node_ffi_create_fast_trampoline()` and
  release code with `node_ffi_free_fast_trampoline()`.
* `src/node_ffi.cc` decides whether a function gets a Fast API callable,
  SharedBuffer callable, or generic callable, and attaches hidden metadata used
  by JavaScript wrappers.
* `src/node_ffi.h` stores `FastFFIMetadata` objects in `FFIFunctionInfo` so V8
  metadata and generated executable code stay alive with the JavaScript function.
* `lib/ffi.js` is the public module wrapper. It patches `DynamicLibrary` methods
  and composes SharedBuffer and Fast API wrappers.
* `lib/internal/ffi/fast-api.js` performs JavaScript-side pointer conversions
  for Fast API calls.
* `lib/internal/ffi-shared-buffer.js` contains only SharedBuffer-specific
  argument packing and result unpacking.

## Native Metadata Creation

`DynamicLibrary::CreateFunction()` creates one `FFIFunctionInfo` per generated
JavaScript function. That object owns the parsed `FFIFunction`, keeps the owning
`DynamicLibrary` object alive through an internal field, and owns any optimized
invocation metadata.

The creation flow is:

1. Call `CreateFastFFIMetadata(*fn)`.
2. If Fast API metadata is created, bind the JavaScript function with a V8
   `FunctionTemplate` that contains both the conventional callback and the Fast
   API `v8::CFunction`.
3. If Fast API metadata cannot be created, try the SharedBuffer path for
   eligible signatures.
4. If neither optimized path applies, bind the generic `InvokeFunction` path.

Returning `nullptr` from `CreateFastFFIMetadata()` is not a public signature
error. It means only that the current Fast API implementation cannot optimize
that signature or cannot safely allocate executable trampoline memory. The caller
then falls back to another invocation strategy.

## Runtime Support Detection

The Fast API path needs both a platform trampoline emitter and executable memory.
`IsFastCallSupported()` is the coarse process-level query for this. It returns
true only on supported architectures when `IsJitMemorySupported()` succeeds.

`IsJitMemorySupported()` runs a one-time self-test:

* Map one writable anonymous page.
* Write a minimal return instruction for the current architecture.
* Flush the instruction cache where required.
* Try to transition the page to read/execute with `mprotect(PROT_READ |
  PROT_EXEC)`.
* Unmap the page and cache the final result with `std::call_once`.

The probe deliberately does not execute the generated instruction. Executing a
freshly written capability probe could terminate the process on systems that
block generated code. The real trampoline emitter performs the same writable to
executable transition when creating a callable trampoline and falls back when it
is rejected. Windows uses `VirtualAlloc`, `VirtualProtect`, and
`FlushInstructionCache` for the same probe.

## Signature Eligibility

`IsFastCallEligible()` rejects signatures before native code generation. This
keeps unsupported cases out of the trampoline emitters and lets
`CreateFastFFIMetadata()` return `nullptr` cleanly.

Eligibility requires:

* A supported platform emitter: AArch64, x86\_64 SysV, Win64 x64, PPC64LE
  ELFv2, LoongArch64, RISC-V 64, or s390x.
* A return type that is numeric, pointer, or `void`.
* Argument types that are numeric or pointer. `void` cannot be an argument.
* No `function` typed argument or return value.
* At most eight public arguments, matching V8's fast-call cap.
* Matching `fn.args` and `fn.arg_type_names` lengths.
* Register pressure that fits the current trampoline generators.

AArch64 eligibility mirrors `src/ffi/platforms/arm64.cc`:

* `x0` is occupied by V8's receiver, so user GP arguments arrive in `x1..x7`.
* FP arguments use `v0..v7`.
* `buffer` and `arraybuffer` arguments use a `v8::Local<v8::Value>` plus
  `FastApiCallbackOptions`, so they consume one additional GP slot.
* Buffer-shaped arguments cannot coexist with FP arguments because the helper
  call would require preserving FP state.
* Effective GP count must be at most 7, and FP count must be at most 8.

x86\_64 SysV eligibility mirrors `src/ffi/platforms/x64.cc`:

* `rdi` is occupied by V8's receiver, leaving `rsi`, `rdx`, `rcx`, `r8`, and
  `r9` for incoming GP arguments.
* Scalar signatures may load one additional user GP argument from the caller
  stack into the target ABI's sixth GP register, for an effective cap of 6 GP
  arguments.
* FP arguments use `xmm0..xmm7`.
* Buffer-shaped arguments use a helper call and stay register-only, so the
  incoming GP count is capped at 5 and buffer-shaped arguments cannot coexist
  with FP arguments.

Win64 x64 eligibility mirrors the conservative Windows emitter in
`src/ffi/platforms/x64.cc`:

* The JavaScript receiver occupies the first positional register slot.
* Public arguments are shifted from positions 1..3 into positions 0..2.
* Integer and FP arguments are handled according to their positional Win64
  register slots.
* Only scalar register-only signatures with at most three public arguments are
  currently eligible.
* Buffer-shaped arguments and stack-passed arguments fall back.

PPC64LE eligibility mirrors `src/ffi/platforms/ppc64.cc`:

* `r3` is occupied by V8's receiver, so user GP arguments arrive in `r4..r10`.
* FP arguments use FPRs and are not shifted by the receiver slot.
* The generated trampoline shifts only GP registers and tail-branches to the
  target through `ctr`, with the target address in `r12` for ELFv2 global entry.
* Only scalar register-only signatures are currently eligible.
* Buffer-shaped arguments, stack-passed arguments, narrow returns, and PPC64BE
  platforms fall back. AIX/PPC64BE is intentionally a non-target for the current
  Fast FFI trampoline work because its ABI/linkage shape needs separate design.

LoongArch64 eligibility mirrors `src/ffi/platforms/loong64.cc`:

* `a0` is occupied by V8's receiver, so user GP arguments arrive in `a1..a7`.
* FP arguments use `fa0..fa7` and are not shifted by the receiver slot.
* The generated trampoline shifts only GP registers and tail-branches to the
  target through `jirl`.
* Only scalar register-only signatures are currently eligible.
* Buffer-shaped arguments, stack-passed arguments, and narrow returns fall back.

RISC-V 64 eligibility mirrors `src/ffi/platforms/riscv64.cc`:

* `a0` is occupied by V8's receiver, so user GP arguments arrive in `a1..a7`.
* FP arguments use `fa0..fa7` and are not shifted by the receiver slot.
* The generated trampoline shifts only GP registers and tail-branches to the
  target through `jalr`.
* Only scalar register-only signatures are currently eligible.
* Buffer-shaped arguments, stack-passed arguments, and narrow returns fall back.

s390x eligibility mirrors `src/ffi/platforms/s390x.cc`:

* `r2` is occupied by V8's receiver, so user GP arguments arrive in `r3..r6`.
* FP arguments use `f0`, `f2`, `f4`, and `f6` and are not shifted by the receiver
  slot.
* The generated trampoline shifts only GP registers and tail-branches to the
  target through `br`.
* Only scalar register-only signatures are currently eligible.
* Buffer-shaped arguments, stack-passed arguments, and narrow returns fall back.

The native trampoline generator still repeats its own register checks. The
eligibility function is the early, centralized rejection point; the generator
checks are a defense against direct or future callers.

## FastFFIType

The internal `FastFFIType` enum is intentionally smaller than the public FFI type
surface. It models the ABI categories that the generated trampoline knows how to
marshal directly:

* `kVoid`
* `kBool`
* signed and unsigned 8-bit, 16-bit, 32-bit, and 64-bit integers
* `kFloat32`
* `kFloat64`
* `kPointer`
* `kBuffer`

Public aliases are normalized in `FastScalarTypeFromName()` and
`FastArgTypeFromName()`.

`pointer`, `ptr`, `string`, `str`, `buffer`, and `arraybuffer` all represent
pointer-sized native values at the target ABI boundary. They differ in how
JavaScript values are accepted and converted before the target function is
called. `function` is pointer-sized for the generic FFI surface, but the current
Fast API eligibility check rejects it so it falls back.

## V8 CFunction Metadata

`CreateFastFFIMetadata()` creates a `FastFFIMetadata` object. That object owns:

* `FastFFITrampoline trampoline`, the executable bridge called by V8.
* `std::vector<v8::CTypeInfo> arg_info`, the V8 type list.
* `std::unique_ptr<v8::CFunctionInfo> c_function_info`, the V8 signature object.
* `v8::CFunction c_function`, the handle attached to the `FunctionTemplate`.

The metadata object must own `arg_info` and `c_function_info` because V8 keeps
pointers into that storage. Destroying or moving that storage while the function
is alive would leave V8 with dangling pointers.

The first V8 argument is always the JavaScript receiver. For that reason,
`CreateFastFFIMetadata()` prepends a `v8::CTypeInfo::Type::kV8Value` entry before
the public FFI arguments.

If any argument or return value needs a 64-bit integer or pointer, the V8
`CFunctionInfo` is configured with BigInt representation. This avoids precision
loss for pointer-sized values and 64-bit integers.

Buffer-shaped arguments require one additional
`CTypeInfo::kCallbackOptionsType` entry so `node_ffi_fast_buffer_data()` can
throw through `FastApiCallbackOptions` when conversion fails.

## Generated Trampolines

The generated trampoline bridges V8's Fast API calling convention to the native
ABI expected by the library symbol. Its responsibilities are:

* Move incoming V8 Fast API arguments into the registers expected by the target
  native function.
* Narrow 8-bit and 16-bit integer arguments after V8 has widened them.
* Convert `kBuffer` arguments from V8 values to backing-store pointers.
* Call or tail-jump to the target symbol.
* Normalize narrow integer return values before V8 observes them.
* Return control to V8 using the platform ABI.

The trampoline is generated per signature. It does not loop over arguments at
runtime using metadata tables. The code generator may loop while emitting
instructions, but the emitted code is a straight-line bridge specialized for the
signature.

Executable memory is allocated writable, populated with instructions, flushed
from the instruction cache as required by the platform, and then marked
executable. `FastFFIMetadata` releases that memory through
`node_ffi_free_fast_trampoline()` when the JavaScript function is collected.
Cleanup is idempotent and safe for partially initialized metadata.

## Buffer and ArrayBuffer Arguments

Fast API buffer arguments are represented internally as `FastFFIType::kBuffer`.
In V8 metadata they are described as `v8::Local<v8::Value>`, not as `uint64_t`.
This lets the generated trampoline receive the original JavaScript object and
call `node_ffi_fast_buffer_data()` immediately before the native target call.

`node_ffi_fast_buffer_data()` accepts:

* `Buffer` and other ArrayBuffer views
* `ArrayBuffer`
* `SharedArrayBuffer`

For views, the pointer is the backing store plus `byteOffset`. For ArrayBuffer
and SharedArrayBuffer, the pointer is the start of the backing store.

Invalid values cause the helper to throw through `FastApiCallbackOptions` and
return a sentinel value. The generated trampoline checks for that sentinel and
returns zero without calling the native target. This prevents native code from
observing an invalid pointer after JavaScript validation has failed.

## Pointer-Like Arguments

Pointer-like public types include `pointer`, `ptr`, `string`, and `str`. They are
represented as raw unsigned pointer values in the scalar Fast API signature.

JavaScript wrappers in `lib/internal/ffi/fast-api.js` convert accepted non-BigInt
values before entering the scalar Fast API function:

* `null` and `undefined` become `0n`.
* Strings become temporary NUL-terminated UTF-8 buffers.
* Buffer and ArrayBuffer-backed values become backing-store pointers.

Keeping these conversions in JavaScript preserves public FFI semantics and keeps
temporary object lifetime explicit.

## String Arguments

String conversion intentionally stays in JavaScript. A string accepted by a
`string`, `str`, or pointer-like parameter is encoded into a temporary Buffer
with a trailing NUL byte. The pointer to that Buffer is passed to the scalar Fast
API function.

Each owning `DynamicLibrary` keeps a hidden array of string conversion entries.
Each argument index gets one reusable entry. If the same string is passed again
at the same argument index, the wrapper reuses the existing encoded buffer and
pointer. This is a single-entry reuse strategy, not an unbounded cache. Buffers
grow when needed and are kept alive by the `DynamicLibrary` object.

This design avoids native lifetime ambiguity. The generated trampoline never
allocates temporary string storage and never has to guess how long a pointer to a
converted string must stay alive.

## Secondary Buffer Invoke for Pointer Signatures

A single pointer-like function can be called efficiently in two different ways:

* With a scalar pointer value, such as `BigInt`, `null`, or a string-converted
  pointer.
* With a memory object, such as `Buffer`, a typed array, `DataView`,
  `ArrayBuffer`, or `SharedArrayBuffer`.

These two cases require different V8 Fast API representations. The scalar case
uses a `uint64_t`/BigInt-shaped argument. The memory-object case uses a
`v8::Local<v8::Value>` argument so the generated trampoline can extract the
backing-store pointer.

For monomorphic single-argument pointer-like signatures, native code may attach
a secondary function under the hidden `kFastBufferInvoke` symbol. This secondary
function uses a cloned signature where the pointer-like argument is described as
`buffer` for Fast API metadata purposes, while still calling the same native
target symbol.

The JavaScript wrapper dispatches to this secondary function only when the
runtime argument is Buffer or ArrayBuffer-backed memory. Other pointer inputs use
the primary scalar Fast API function.

This keeps both important call shapes fast. Replacing the primary scalar Fast
API function with the buffer-shaped function would simplify the machinery, but
it would force BigInt, null, and string-converted pointer calls onto a slower
fallback path. Keeping both entrypoints preserves performance for both pointer
representations.

## Hidden Symbols

Native code attaches internal metadata to raw generated functions using
per-isolate Symbols. These symbols are exported through `internalBinding('ffi')`
and are not part of the public API.

SharedBuffer metadata uses:

* `kSbSharedBuffer`
* `kSbInvokeSlow`
* `kSbArguments`
* `kSbReturn`

Fast API metadata uses:

* `kFastArguments`
* `kFastBufferInvoke`

The two groups are intentionally separate. SharedBuffer wrappers should not need
Fast API metadata, and Fast API wrappers should not need SharedBuffer metadata.
`lib/ffi.js` is the composition layer that reads both groups and decides which
wrapper to apply.

## JavaScript Wrapper Routing

`lib/ffi.js` patches the native `DynamicLibrary` methods that expose generated
functions:

* `DynamicLibrary.prototype.getFunction`
* `DynamicLibrary.prototype.getFunctions`
* The `DynamicLibrary.prototype.functions` accessor

All three routes call `wrapFFIFunction()` before returning functions to user
code.

`wrapFFIFunction()` applies wrappers in this order:

1. Recover hidden SharedBuffer or Fast API argument metadata when the caller did
   not pass an explicit signature, such as through the `functions` accessor.
2. Initialize Fast API buffer metadata for raw Fast API functions.
3. Try `wrapWithSharedBuffer()`.
4. If SharedBuffer did not apply, try `wrapWithRawPointerConversions()`.
5. Return the original raw function unchanged if no wrapper is needed.

This ordering keeps SharedBuffer-specific behavior inside
`lib/internal/ffi-shared-buffer.js`, Fast API pointer conversion behavior inside
`lib/internal/ffi/fast-api.js`, and public wrapper orchestration inside
`lib/ffi.js`.

Fast API raw pointer conversion wrappers specialize arity 1, 2, and 3 to avoid
rest-argument allocation on common call shapes. Larger signatures use a generic
`ReflectApply()` path after converting only the required argument indexes.

## SharedBuffer Fallback

SharedBuffer remains a separate optimized path for signatures that are not using
Fast API but can still avoid per-argument native conversion overhead. The
SharedBuffer wrapper writes arguments into a fixed 8-byte slot layout, calls a
raw native function with no JavaScript arguments, and reads the return value from
slot zero.

Pointer signatures using SharedBuffer have a slow invoker attached under
`kSbInvokeSlow`. The wrapper uses the SharedBuffer path for BigInt and nullish
pointer values, and falls back to the slow invoker for values that require the
generic native conversion path.

Fast API and SharedBuffer are independent. A function uses either the Fast API
path, the SharedBuffer path, or the generic path as its primary native callable.
`lib/ffi.js` only composes the JavaScript wrappers needed to preserve public
argument behavior.

## Generic Fallback Behavior

Every Fast API function is also bound with the conventional native callback.
V8 can call that callback when a JavaScript call site is not eligible for the
Fast API path. Node.js also uses the generic path directly when metadata creation
rejects a signature.

The generic path remains responsible for complete validation and public error
compatibility. Fast wrappers should match those errors for conversions they
perform in JavaScript.

## Argument Count and Signature Limits

The Fast API path intentionally supports only a bounded set of signatures. This
keeps V8 metadata, wrapper specialization, and generated trampolines simple and
predictable. Signatures outside that bound fall back to SharedBuffer or the
generic path.

Important limits are:

* At most eight public arguments before ABI-specific register checks.
* No stack arguments in the current AArch64 trampoline.
* At most one stack-loaded scalar GP argument in the current x86\_64 SysV
  trampoline.
* No stack arguments or buffer-shaped arguments in the current Win64 x64
  trampoline.
* No stack arguments, buffer-shaped arguments, or narrow returns in the current
  PPC64LE trampoline.
* No stack arguments, buffer-shaped arguments, or narrow returns in the current
  LoongArch64, RISC-V 64, and s390x trampolines.
* No mixed buffer-shaped and FP arguments.
* No `function` argument or return type in the Fast API path.

Linux x86 and armv7 are experimental Node.js platforms, but the current Fast FFI
trampoline model remains 64-bit only. They continue to use SharedBuffer or
generic libffi fallback paths. Linux s390x is a Tier 2 Node.js platform, but
bundled FFI is not currently enabled for that target; if built with
`--shared-ffi`, scalar register-only Fast API FFI can use the s390x emitter. AIX
PPC64BE is intentionally not covered by this implementation.

These are optimization boundaries, not public FFI signature boundaries. User
code can still call supported public FFI signatures through fallback paths.

## Wrapper Metadata Preservation

JavaScript wrappers preserve selected public function metadata:

* `name`
* `length`
* `pointer`

The `pointer` property mirrors the raw function's pointer descriptor so user
code that reads or reassigns it continues to work through wrappers. Internal
Symbol-keyed metadata is not forwarded to wrappers.
