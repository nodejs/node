---
name: v8-review
description: Runs a parallel multi-subagent V8 code review covering security, V8 Sandbox safety, GC invariants, compiler/codegen logic, concurrency, performance, tests, and style, then verifies and merges the findings. Use when reviewing a V8 CL, commit, or local diff. Do not use for triaging bug reports or writing fixes from scratch.
---

# V8 Code Review Workflow

## Step 1: Context Setup (Main Agent)

1. Save the target commit (`git --no-pager show <commit>`), Gerrit CL
   (`gerrit_fetch`), or local diff (`git --no-pager diff HEAD`, plus any
   untracked files from `git status --short`) including the commit message (if
   any) to `v8-review-<id>.patch` in the conversation's `scratch/` directory.
   Never write temporary files into the V8 checkout.

## Step 2: Parallel Subagents

Call `invoke_subagent` ONCE with 8 entries (`TypeName: "self"`,
`Model: "inherit"`):

1. `sandbox-security`:

   - **In-sandbox TOCTOU / double-fetch**: In-sandbox memory (`HeapObject`
     fields, lengths, offsets, bitfields) can be concurrently mutated by an
     attacker; load once into a local variable before validating/reusing. Never
     read in-sandbox memory inside `SBXCHECK(...)` (trips
     `DisallowSandboxAccess`).
   - **Pointer tables, casts & swap attacks**: Never store raw pointers or
     direct `TrustedObject` refs in-sandbox; require `ExternalPointerTable`,
     `TrustedPointerTable`, `CodePointerTable`, `JSDispatchTable`, or
     `CppHeapPointerTable` with narrow tags (`ExternalPointerTag`,
     `IndirectPointerTag`), and `ProtectedPointerSlot` between `TrustedObject`s.
     Never use `TrustedCast<T>` on untrusted or 1:N TPT-tagged inputs (use
     `SbxCast<T>` / `TryCast<T>`). Guard against **pointer-swap attacks** across
     same-tag handles: never assume a `TrustedObject` (`BytecodeArray`, `Code`,
     `WasmTrustedInstanceData`, `RegExpData`) matches its holder's in-sandbox
     state (parameter count, context, internal fields). Read Wasm security state
     from `WasmTrustedInstanceData`, not `WasmInstanceObject`.
   - **Out-of-sandbox & trusted writes**: Bounds-check any `TrustedObject`
     (`TrustedFixedArray`, `WasmDispatchTable`), C++ heap, or stack access
     indexed by in-sandbox values (`Smi`, `BoundedSize`, buffer length/offset)
     via `SBXCHECK` / `SBXCHECK_BOUNDS` / `CSA_SBXCHECK` / `SbxCheck` /
     `sbxcheck` (never `DCHECK` / `UnsafeCast`), checking signedness and integer
     overflow before address math. Require `WritableJitAllocation` /
     `ThreadIsolation` for JIT code/jump-table writes.
   - **JS re-entrancy & language security**: Check C++/CSA/Torque builtins for
     user JS execution (`Object::ToNumber`, `GetProperty`, `Execution::Call`,
     Proxy traps, `valueOf`/`toString`, Wasm imports) that can detach/shrink
     `ArrayBuffer`s (including RAB/GSAB), transition `Map`s, change
     `ElementsKind`, or mutate lengths mid-operation. Ensure hole sentinels
     (`TheHole`, etc.) or uninitialized memory never leak to JS.

2. `correctness-gc`:

   - **Spec & functional correctness**: ECMAScript/Wasm spec compliance,
     exception propagation (`RETURN_ON_EXCEPTION`, `has_exception()`, never
     running JS with a pending exception), `MaybeDirectHandle`/`MaybeHandle`
     empty checks, RAII cleanup on early return.
   - **Integer overflow & truncation**: Signed/unsigned mismatches, 32-bit vs.
     64-bit truncation (`int32_t` vs. `size_t`/`uintptr_t`, Wasm `memory64`
     offsets), `Smi` range/tagging overflow, promoting to `size_t` or
     `base::CheckedNumeric` before arithmetic
     (`slots * kSystemPointerSize + offset`).
   - **GC safepoints, handles & Oilpan**: Raw `Tagged<T>` or interior pointers
     held across allocations/GC (must use `DirectHandle<T>` / `Handle<T>`),
     missing `DisallowGarbageCollection` scopes, stale pointers after
     resize/migration, Torque missing `transitioning` on macros/builtins that
     can allocate/call JS. In Oilpan (`CppHeap`), never place `cppgc::Member<T>`
     on the stack or raw `GarbageCollected<T>*` in heap objects (trace all
     `Member<T>` in `Trace(Visitor*)` and call `Base::Trace(visitor)`).
   - **Write barriers & heap invariants**: Missing `WriteBarrier`,
     `CONDITIONAL_WRITE_BARRIER`, `IndirectPointerWriteBarrier`, or
     `ProtectedPointerWriteBarrier`, unsafe `SKIP_WRITE_BARRIER`, `Smi` vs.
     `HeapObject` assumptions.

3. `compiler-codegen`:

   - **IR reductions & effects**: Turboshaft, TurboFan, Maglev, Sparkplug, and
     Liftoff lowerings; 32-bit `I64` lowering (`Int64LoweringReducer`); 64-bit
     upper-32-bit sign/zero-extension; Maglev `kHoleyFloat64` signaling-NaN hole
     checks; accurate Turboshaft `OpEffects` and Maglev `OpProperties`
     (`can_allocate`, `can_eager_deopt`, `can_lazy_deopt`, `can_read`,
     `can_write`); CSA `TVARIABLE` binding on all incoming `BIND` paths.
   - **Deoptimization & protectors**: Missing deopt checks, lazy deopt frame
     state completeness, map/prototype/species/array protector checks and
     invalidation, Wasm subtyping and signature checks.
   - **MacroAssembler & backend**: Register aliasing (`dst` aliasing `src` in
     multi-instruction sequences, `UseUniqueRegister`), clobbered scratch
     registers (`TemporaryRegisterScope`, Liftoff cache spills) across
     helper/barrier calls, stack frame alignment and slot accounting,
     condition/branch inversions.
   - **Fail-fast assertions vs. defensive masking**: Prefer failing fast via
     `DCHECK`/`CHECK` on unexpected compiler states or invariant violations. Do
     NOT recommend adding defensive workarounds (e.g., silent early returns,
     fallback dummy values, defensive clamping, or non-exhaustive switch
     `default: break;`) that paper over upstream invariant violations.

4. `concurrency`:

   - **Object publication & TOCTOU**: Safe publication to background threads
     (`SharedObjectSafePublishGuard`, `SharedObjectConditionalSafePublishGuard`,
     release-store / acquire-load), concurrent map/descriptor/transition array
     reads. (Return `[]` for purely main-thread code; leave in-sandbox attacker
     races to `sandbox-security`.)
   - **Concurrent JIT & GC**: Background compilation (Maglev/Turboshaft/Wasm)
     racing with main-thread mutation or concurrent GC
     marking/sweeping/evacuation; shared space (`SharedStruct`, `SharedArray`,
     shared Wasm objects) synchronization.
   - **Atomics & locks**: `std::memory_order` correctness, lock
     ordering/deadlocks, accessing non-thread-safe `Isolate` state from
     background threads (`LocalIsolate`/`LocalHeap` boundaries).

5. `architecture-api`:

   - **Cross-arch, cross-tier & variant completeness**: Parity across the 4
     supported architectures (`x64`, `arm64`, `ia32`, `arm` — ignore community
     ports like `riscv`, `loong64`, `ppc64`, `s390x`, `mips64` unless targeted
     by the CL), execution tiers (Ignition, Sparkplug, Maglev, Turboshaft,
     Liftoff), and all affected opcodes/builtins. For bug fixes, perform variant
     analysis to check if the same bug pattern exists in sibling opcodes,
     builtins, reducers, or tiers.
   - **Optimization preservation & design**: Fix specific invariant flaws rather
     than disabling fast paths wholesale; reuse existing helpers/passes; avoid
     `#if V8_TARGET_ARCH_...` in shared code; flag edits to generated files
     (`out/`) or unrelated drive-by refactors.
   - **API surface, flags & presubmit**: Swallowed errors or defensive
     workarounds instead of `CHECK`/`DCHECK`/`UNREACHABLE()` (never suggest
     defensive fallbacks that mask unexpected states or invariant violations),
     error fallbacks bypassing subsequent checks, unused parameters/fields, dead
     code, missing flag implications (`DEFINE_IMPLICATION`) under `--fuzzing`,
     production `.cc` code calling `*ForTest*` helpers, new `V8_OBJECT`/`V8_IT_`
     headers missing from `src/objects/all-objects.h`.

6. `performance-containers`:

   - **Hot-path performance**: Heap allocations on hot paths (ICs, dispatch, GC,
     interpreter/JIT fast paths, frequent compiler phases), cold error/runtime
     paths inlined into hot assembly (prefer `OutOfLineCode` / deferred blocks),
     redundant runtime checks.
   - **Algorithmic & data layout**: `O(n^2)` scans over IR
     nodes/instructions/registers, Zone memory bloat, and (on hot/high-volume
     paths) missing `reserve()`, unnecessary copies, or poor struct field
     packing.
   - **Types & containers**: Strong types / `enum class` / `base::Flags` over
     boolean flags; V8 containers (`base::SmallVector`, `base::Vector`,
     `base::UniqueArray`, `absl::flat_hash_map`, `absl::flat_hash_set`,
     `base::FunctionRef`).

7. `tests`:

   - **Mandatory reproducer & fidelity**: Flag a `bug` if a bug fix omits a
     regression test. Verify tests (`test/mjsunit/`, `test/unittests/`,
     `test/cctest/`) exercise the modified path and fail without the fix (watch
     for early errors in helper closures before target code, or missing
     `%PrepareFunctionForOptimization` + warmup call before
     `%OptimizeFunctionOnNextCall`).
   - **Boundary coverage**: Edge cases (`0`, `-0`, `NaN`, `INT32_MIN`,
     `INT32_MAX`, `UINT32_MAX`, `Smi` range, empty/max-size buffers or Wasm
     modules).
   - **V8 test guidelines**: `assertEquals(expected, found)` order; never
     include `d8.file.execute('test/mjsunit/mjsunit.js')`; minimal leaf flags
     over `--fuzzing`/`--jit-fuzzing`; no hardcoded tier flags (`--no-liftoff`,
     `--turboshaft`, `--maglev`) already covered by test variants unless testing
     tier-up/OSR (`assertOptimized()` requires
     `--maglev`/`--turbolev`/`--turbofan`); `--allow-natives-syntax` with no
     space after `%` (`%Foo()`); `--stack-size=100` on stack-exhaustion tests;
     no CPU-burning warmup loops or fuzzer boilerplate (`__f_0`, `__v_0`,
     `__wrapTC`, swallowed `catch (e) {}`).

8. `style-comments`:

   - **V8 C++ style & presubmit**: Spell out types instead of `auto` unless
     obvious or overly long; `const`-correctness and `std::forward`;
     `base::bit_cast` (never `std::bit_cast`), `UNREACHABLE()`; V8 banned
     constructs (`<regex>`, `std::mutex`, `absl::FunctionRef`, `dynamic_cast`,
     exceptions, trailing whitespace); `-inl.h` rules (never
     `#include "*-inl.h"` in normal `.h`; `foo-inl.h` must `#include "foo.h"`
     first); `V8_NOEXCEPT` on copy/move constructors and assignment operators;
     `{}` braces on all multi-line `if` statements; copyright header on new
     files.
   - **Comments & readability**: Documenting class invariants and non-obvious
     rationale; updating stale adjacent comments contradicted by the diff;
     identifying pure code moves.

Instructions for every subagent:

- Read the patch file from Step 1, then inspect modified files, callers, and
  callees using `view_file` and `repo_git_grep` (or `git --no-pager grep` via
  `run_command`; avoid `code_search` as it searches stale/multiple google3 V8
  snapshots).
- NEVER modify any files, mutate git state, or run builds/tests (only read-only
  `git --no-pager grep`/`show`/`log` commands are allowed).
- Comment ONLY on modified or directly impacted lines (never flag unrelated
  pre-existing issues in untouched code; if the change newly exposes, triggers,
  or relies on a pre-existing bug, or misses a bug variant in adjacent code,
  flag it anchored on the relevant modified line; if an issue exists in moved
  code, note that it is pre-existing).
- Send ONLY a JSON array (`[]` if clean):
  `[{"file": "src/...", "line": 123, "type": "security|bug|design|question|nit", "comment": "Issue, impact, and suggested fix.", "suggestion": "..."}]`

## Step 3: Filter, Merge & Format (Main Agent)

1. **Verify**: Check every candidate comment against the source files; drop
   false positives, speculative issues without a concrete mechanism, and
   comments on unmodified lines (unless flagging a newly exposed bug or missed
   variant anchored to the diff).
2. **Commit Message** (if reviewing a commit/CL): Verify factual accuracy
   against the diff. Check `[subsystem] Imperative subject` (\<= 72 chars, no
   period), concise body (\<= 10 lines, wrapped at \<= 72 chars, explaining
   *why* and high-level *what*), CL references as `https://crrev.com/c/<number>`
   (not git hashes), and trailers (`Bug: <id>` or `Fixed: <id>` with plain
   numeric ID for Chromium or `b:<id>` for Buganizer, never `BUG=`; plus
   `TAG=agy` and `R=<reviewer>@chromium.org` for agent-authored CLs).
3. **Deduplicate**: Merge overlapping comments on the same `file:line` (within 2
   lines) or the same root cause repeated across architectures/files. Keep \<=
   25 comments ordered by `security` > `bug` > `design` > `question` > `nit`.
4. **Output**:
   - **Executive Summary**: Subsystems, Size, Risk, Key Findings (including
     verified pure code moves/refactorings), and Recommended File Review Order.
   - **Commit Message Review**: Current message, factual accuracy, and
     formatting/trailer check (omit if reviewing an uncommitted diff).
   - **Inline Comments**: Grouped by `Must Fix` (security & bugs),
     `Questions & Design`, and `Nits`, with clickable `file://` links
     (`[path:line](file:///abs/path#L123)`) and concrete code/diff suggestions.
