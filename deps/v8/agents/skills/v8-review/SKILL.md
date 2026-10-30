---
name: v8-review
description: Reviews a V8 CL, commit, or local diff using targeted parallel subagents. Use when reviewing code changes in V8. Do not use for triaging bugs or authoring fixes.
---

# V8 Code Review Workflow

## Step 1: Context Setup & Triage (Main Agent)

1. Save the target commit (`git --no-pager show <commit>`), Gerrit CL
   (`gerrit_fetch`), or local diff (`git --no-pager diff HEAD`, plus any
   untracked files from `git status --short`) including the commit message (if
   any) to `v8-review-<id>.patch` in the conversation's `scratch/` directory.
   Never write temporary files into the V8 checkout.
2. **Select relevant subagents**: Inspect the patch (modified paths, subsystems,
   diff size, and whether the change is a bug fix, feature, or refactoring) and
   choose **only the applicable subagents** from Step 2 rather than spawning all
   8 unconditionally. If unsure whether a subagent applies (especially
   `sandbox-security`, `correctness-gc`, or `compiler-codegen`), include it
   rather than skipping it. If no subagents are needed (e.g., trivial comment,
   build, or 1–2 line edits), skip Step 2 and review the diff directly in Step
   3:
   - `sandbox-security`: Select when touching in-sandbox/out-of-sandbox
     boundaries, `HeapObject` layouts/fields, pointer tables (`*PointerTable`,
     `TrustedObject`, `ProtectedPointerSlot`, `SbxCast`/`TrustedCast`),
     `ArrayBuffer`/RAB/GSAB, Wasm instance/trusted data, JIT
     codegen/builtins/ICs/runtime functions reachable from untrusted JS/Wasm, or
     `SBXCHECK`s. Skip for pure internal compiler graph helpers with no sandbox
     boundary, test-only, build, or comment-only changes.
   - `correctness-gc`: Select when touching runtime/builtins, JS/Wasm semantics,
     heap/GC/Oilpan (`src/heap/`, `Tagged<T>`, `DirectHandle`/`Handle`,
     allocations, write barriers), Torque (`.tq`), object layouts
     (`src/objects/`), parser/interpreter, or arithmetic on sizes/offsets/Smis.
     Skip when the diff has no heap/GC, handle, exception, or integer-overflow
     surface.
   - `compiler-codegen`: Select **only** when touching JIT compilers
     (`src/compiler/`, `src/maglev/`, `src/baseline/`, `src/wasm/baseline/`),
     assemblers/codegen (`src/codegen/`, `src/execution/`, arch backends),
     deoptimizer (`src/deoptimizer/`), CSA/Torque builtins (`src/builtins/`,
     `.tq`), or ICs (`src/ic/`).
   - `concurrency`: Select **only** when touching multi-threaded or concurrent
     code: background compilation (`LocalIsolate`, `LocalHeap`, concurrent
     Maglev/Turboshaft/Wasm jobs), concurrent GC/marking/sweeping/evacuation,
     shared objects (`SharedStruct`, `SharedArray`, shared Wasm),
     atomics/locks/mutexes, concurrent map/descriptor/transition/feedback reads,
     or thread isolation/signal handlers. Skip for purely single-threaded
     main-thread code.
   - `architecture-api`: Select for all non-trivial code changes (especially bug
     fixes, new features, cross-tier/cross-arch changes, API/header edits, or
     flag additions) to evaluate minimal fix design, variant completeness, and
     architectural fit. Skip only for trivial comment, formatting, or test-only
     edits.
   - `performance-containers`: Select when touching hot paths (ICs,
     interpreter/JIT fast paths, frequent compiler passes, GC, Wasm execution,
     hot builtins) or adding/modifying data structures, containers, and loops.
     Skip for cold error paths, small localized fixes with no container/loop
     changes, or test-only diffs.
   - `tests`: Select when `test/` files are added/modified **or** when reviewing
     a bug fix (to verify the mandatory regression test is present and
     effective). Skip for pure refactorings, comments, or build changes that
     neither touch `test/` nor fix a bug.
   - `style-comments`: Select when adding/modifying C++ (`.cc`, `.h`, `-inl.h`)
     or Torque (`.tq`) files. Skip for test-only `.js`/`.mjs` diffs or tiny 1–2
     line changes where the main agent can check style directly in Step 3.

## Step 2: Parallel Subagents

Call `invoke_subagent` for each selected subagent in parallel in a single turn
(`TypeName: "self"`, `Model: "inherit"`), passing each subagent only its own
checklist and the shared subagent instructions below:

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
     `TrustedPointerTag`), and `ProtectedPointerSlot` between `TrustedObject`s.
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
     `CONDITIONAL_WRITE_BARRIER`, `TrustedPointerWriteBarrier`, or
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

   - **Minimal design fix & maintainability**: Check whether the change is the
     smallest, simplest design fix for the underlying root cause rather than
     patching symptoms across multiple call sites. Question fixes that plumb new
     state, parameters, flags, or special-case branches across layers, or
     introduce unnecessary abstractions/helpers, when a smaller, more local fix
     at a single choke point or an existing invariant/helper would be cleaner
     and more maintainable long-term.
   - **Cross-arch, cross-tier & variant completeness**: Parity across the 4
     supported architectures (`x64`, `arm64`, `ia32`, `arm` — ignore community
     ports like `riscv`, `loong64`, `ppc64`, `s390x`, `mips64` unless targeted
     by the CL), execution tiers (Ignition, Sparkplug, Maglev, Turboshaft,
     Liftoff), and all affected opcodes/builtins. For bug fixes, perform variant
     analysis to check if the same bug pattern exists in sibling opcodes,
     builtins, reducers, or tiers.
   - **Optimization preservation & encapsulation**: Fix specific invariant flaws
     rather than disabling fast paths wholesale; reuse existing helpers/passes
     and encapsulate state mutations on the owning class; keep headers lean
     (`.cc` definitions for non-hot methods, `namespace {}` in `.cc` for helpers
     not using `this`); avoid `#if V8_TARGET_ARCH_...` in shared code; flag
     edits to generated files (`out/`) or unrelated drive-by refactors.
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
   - **V8 test guidelines**: Component-specific subdirectory over catch-all
     `test/mjsunit/regress/`; `assertEquals(expected, found)` order; never
     include `d8.file.execute('test/mjsunit/mjsunit.js')`; minimal leaf flags
     over `--fuzzing`/`--jit-fuzzing`/`--experimental-fuzzing`; no hardcoded
     tier flags (`--no-liftoff`, `--turboshaft`, `--maglev`) already covered by
     test variants unless testing tier-up/OSR (`assertOptimized()` requires
     `--maglev`/`--turbolev`/`--turbofan`); `--allow-natives-syntax` with no
     space after `%` (`%Foo()`); `--stack-size=100` on stack-exhaustion tests;
     no CPU-burning warmup loops or fuzzer boilerplate (`__f_0`, `__v_0`,
     `__wrapTC`, swallowed `catch (e) {}`).

8. `style-comments`:

   - **V8 C++ style & presubmit**: Spell out types instead of `auto` unless
     obvious or overly long; tight local variable scope (declared at first use);
     `const`-correctness and `std::forward`; `base::bit_cast` (never
     `std::bit_cast`), `UNREACHABLE()`; V8 banned constructs (`<regex>`,
     `std::mutex`, `absl::FunctionRef`, `dynamic_cast`, exceptions, trailing
     whitespace); `-inl.h` rules (never `#include "*-inl.h"` in normal `.h`;
     `foo-inl.h` must `#include "foo.h"` first); `V8_NOEXCEPT` on copy/move
     constructors and assignment operators; `{}` braces on all multi-line `if`
     statements; copyright header on new files.
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
- Treat each test case (`TEST`, `UNINITIALIZED_TEST`, `TEST_F`, `TEST_P`) in
  `test/cctest/`, `test/unittests/`, and `test/mjsunit/` as running in its own
  isolated OS process (`tools/run-tests.py` runs one test per process/fork);
  only flag static, global, or `v8_flags` state issues in those suites if they
  cause a bug within a single test execution.
- Send ONLY a JSON array (`[]` if clean):
  `[{"file": "src/...", "line": 123, "type": "security|bug|design|question|nit", "comment": "Issue, impact, and suggested fix.", "suggestion": "..."}]`

## Step 3: Filter, Merge & Format (Main Agent)

1. **Verify**: Check every candidate comment against the source files; drop
   false positives, speculative issues without a concrete mechanism, and
   comments on unmodified lines (unless flagging a newly exposed bug or missed
   variant anchored to the diff).
2. **Commit Message** (if reviewing a commit/CL): Verify factual accuracy
   against the diff. Check `[subsystem] Imperative subject` (\<= 64 chars,
   aiming for ~50, no period), concise body (\<= 10 lines, wrapped at \<= 72
   chars, explaining *why* and high-level *what*), CL references as
   `https://crrev.com/c/<number>` (not git hashes), and trailers (`Bug: <id>` or
   `Fixed: <id>` with plain numeric ID for Chromium or `b:<id>` for Buganizer,
   never `BUG=`; plus `TAG=agy` and `R=<reviewer>@chromium.org` for
   agent-authored CLs).
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
