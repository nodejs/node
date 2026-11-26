---
name: v8-poc-classification
description: Reproduces, heals (e.g., converting browser/HTML POCs to d8), minimizes, and classifies whether a V8 POC is a security vulnerability or a regular bug.
---

# Skill: V8 POC Classification

Use this skill to determine if a reported Proof-of-Concept (POC) crash is a
security vulnerability or a regular bug through empirical technical analysis.

## Core Mandates

- **Scope Limitation:** This skill is strictly for **technical analysis and
  classification**. It does NOT include implementing a fix.
- **Empirical Verification of ALL Claims:** Do not take ANY part of the
  reporter's classification or impact claims at face value. Every claim (e.g.,
  "arbitrary memory write", "sandbox escape", "silent write") MUST be
  empirically verified.
- **Verify with Debuggers:** A claim of memory corruption must be verified by
  observing the crash or memory state using debugging tools (GDB).
  - **Read vs. Write**: You MUST verify the faulting instruction type.
  - **Pointer Control**: Check if faulting addresses or register values are
    actually controlled by the attacker (e.g., contain values from the POC).
- **Mandatory Non-ASan Verification:** For any potential security bug, you MUST
  verify reproduction on a standard **Release build without ASan/UBSan**.
- **Autonomous Classification:** You are responsible for forming your own
  technical conclusion based on the rules in
  [triaging.md](../../../docs/security/triaging.md) and
  [reproducing-bugs.md](../../../docs/security/reproducing-bugs.md).
- **Unconfirmed Crashes & Filter Blocks:** NEVER classify a POC as an
  unqualified `Vulnerability` if `d8` with `--run-as-[sandbox]-security-poc`
  exits `0` (even if it prints `"SUCCESS"` or GDB shows a redirected C++ flag
  write); without a crash or write `"V8 sandbox violation detected"` (under
  `--sandbox-testing`), any vulnerability classification **MUST** be
  `Tentative Vulnerability`. Explicitly state **each** applicable limitation
  whenever any steps/subagents fail due to internal filters and/or
  `--run-as-[sandbox]-security-poc` does *not* confirm a crash/violation.

## Workflow

### 1. Initial Verification & Exhaustive Reproduction

Confirm the bug exists as reported.

- **Action**: Run `d8 <provided-flags> <poc.js>`.
- **Escalation**: If it doesn't reproduce, try multiple revisions (Reporter's
  commit and Main) and build configurations (Debug, ASan, Release).
- **Browser Web Worker Translation**: When converting a browser `Worker` POC
  (e.g., from `poc.html`) to `d8`, note that `d8` uses
  `new Worker(fn, {type: 'function'})`, passes messages directly to
  `onmessage = function(msg) { ... }` (not wrapped in `{data: msg}` like DOM
  `MessageEvent`), and receives messages on the main thread via
  `worker.getMessage()`.
- **Deep Dive**: Capture the initial backtrace and faulting instruction in GDB.
- **Check for Explicit Aborts:** Look for evidence that execution was
  intentionally terminated by V8 (e.g., via `SBXCHECK`, `CHECK`, `FATAL`, or
  traps), which maps to **`Not a Bug (Intended Behavior)`** for sandbox POCs or
  **`Bug`** (`Security_Impact-None`) for regular bugs (see guidelines below).

### 2. Security Boundary Investigation (The Filter)

Determine if the bug remains a vulnerability under production restrictions.

- **Action**: Run with `provided_flags + --run-as-[sandbox]-security-poc`.
- **Failure Loop (If reproduction stops)**:
  1. **Analyze**: Identify which flag or syntax is restricted. Is it
     `--allow-natives-syntax`? Is it an experimental compiler flag?
  2. **Heal**: Attempt to rewrite the POC to avoid the restriction.
     - *Example*: Replace `%OptimizeFunctionOnNextCall(f)` with a loop that
       triggers Tier-up: `for(let i=0; i<10000; i++) f();`.
     - *Example*: Replace `%ArrayBufferDetach(b)` with a standard way to
       detach/transfer if possible.
  3. **Conclude**: If the logic *requires* a restricted feature (like
     `--fuzzing` or a non-shipping experimental flag) to trigger the memory
     corruption, the report is likely a **Bug**, not a vulnerability.
- **Audit Extra Flags & Shell Helpers (Even When a Violation Fires)**:
  `--run-as-[sandbox]-security-poc` does **not** block every `d8`-only or
  test-helper `DEFINE_BOOL` flag (e.g., `--use-external-strings`). Whenever a
  crash or sandbox violation reproduces with extra flags or external/embedder
  resources, inspect `src/` and test without the extra flag: if that crash or
  violation depends on `src/d8/` or `src/extensions/` shell/test-only flags or
  mock embedder resources (not reachable via production Chromium/Blink
  embedders), conclude **`Bug`** (`Security_Impact-None`) per
  [triaging.md](../../../docs/security/triaging.md) immediately, even if
  `"V8 sandbox violation detected"` fires in `d8` (whereas non-experimental V8
  engine features merely disabled by default remain **`Vulnerability`** with
  `Security_Impact-None`, and sandbox POCs with no memory-safety violation at
  all remain **`Not a Bug (Intended Behavior)`**).

### 3. Impact Assessment & Minimization

Technical proof of the vulnerability's severity.

- **Minimization**: Strip the POC and flags to the absolute minimum required to
  trigger the crash under the security POC flags.
  - **Flag Bisection**: If reproduction requires catch-all flags like
    `--future`, you MUST identify the minimal set of flags responsible for the
    behavior.
    - **Identify Implications:** Search `src/flags/flag-definitions.h` for flags
      implied by the reported flags.
    - **Check Production Defaults:** Verify if the identified flags are enabled
      or disabled by default in production.
    - **Systematic Testing:** Test each implied flag individually (or in groups)
      with the POC to pinpoint the specific optimization or feature at fault.
- **Impact Escalation**: If the bug exists but does NOT cause a crash (e.g., a
  "stale value" or "logical type confusion"), attempt to escalate it to a memory
  safety violation to prove whether the flaw can bypass V8's security
  boundaries.
  - **Techniques**: Type Confusion (misleading object layout), OOB Access (stale
    length), Pointer Overwrite (overwriting field/loop-carried pointer).
  - **Escalation Bound**: Cap escalation at 1–2 focused attempts; if attempts
    hit internal safety filters or still exit `0` / trap harmlessly under
    `--run-as-[sandbox]-security-poc`, stop escalating and report the empirical
    outcome.
- **Verification**: Run the *minimized* POC on a **Standard Release build
  (non-ASan)**.
  - **Sandbox Bypass POCs** (`--sandbox-testing`,
    `--expose-memory-corruption-api`, or `Sandbox.MemoryView`): Because
    in-sandbox corruption is artificially injected, if completed verification
    shows the POC only hits a safe check/trap (`SBXCHECK`, `CHECK`, `FATAL`,
    hardened libc++, or `"Caught harmless ..."`) or only perturbs in-sandbox
    state / bounded same-tag pointer-table entries (e.g., racing handles so an
    `ExternalPointerTable`/`CppHeapPointerTable` slot receives a valid same-tag
    wrapper/pointer, yielding `NaN` or a table `CHECK`, or resizing within
    allocated `max_byte_length_`) **without** corrupting or reading raw
    out-of-sandbox memory, classify as **`Not a Bug (Intended Behavior)`** —
    only downgrade a sandbox report to **`Bug`** (`Security_Impact-None`) for
    out-of-sandbox *read-only* access or `d8`/extension-only /
    non-production-flag crashes per
    [triaging.md](../../../docs/security/triaging.md).
  - **Regular Bug POCs** (no injected corruption): If caught by an unconditional
    production `CHECK`, `SBXCHECK`, `UNREACHABLE()`, or `nullptr` dereference,
    classify as **`Bug`** (`Security_Impact-None`).
- **Crashing POC for ClusterFuzz**: A crashing POC (segfault) is highly
  preferred for ClusterFuzz upload (see the `clusterfuzz` skill). Always try to
  provide a standalone `.js` file that crashes on a Release build.
- **Deep Dive (GDB)**:
  - Verify **Attacker Control**: Do registers or memory at the crash site
    reflect values set in the POC (e.g., `0x41414141`)?
  - **Read vs. Write**: Explicitly identify the primitive.
  - **Scope Analysis**: For logical bugs, use tracing flags (e.g.,
    `--trace-maglev-graph-building`) to confirm the discrepancy.
  - **Component Mapping**: Identify the specific V8 subsystem affected (e.g.,
    Maglev, Turbofan, Ignition, WebAssembly) to support component
    classification.

### 4. Classification Guidelines

The classification MUST be supported by empirical evidence from the local
reproduction:

- **Local Reproduction Findings** (keep sub-bullets to 1 concise line, except
  `GDB Backtrace` if multiple lines help):
  - **Status**: `Reproduced` / `Not Reproduced`.
  - **Reproduction**: `d8 <flags> <poc.js>` (filename only, omit local paths).
  - **Result**: 1–2 short sentences stating the exact exit code/signal, whether
    `"V8 sandbox violation detected"` or `"Caught harmless ..."` appeared (when
    `--sandbox-testing` is used), and the observed crash/output.
  - **Build**: Build variant(s) (e.g., `x64.release`, `x64.asan`), V8 version
    (from `include/v8-version.h`), and revision number (from
    `Cr-Commit-Position`), skipping the git hash and with no link to Gerrit
    (only link to Gerrit and include the commit title when the specific *change*
    in a commit is important; omit boilerplate GN args).
  - **Verified Impact**: 1 sentence stating the verified primitive and attacker
    control.
  - **GDB Backtrace**: A concise snippet of the faulting instruction/registers
    and relevant stack frames (can span multiple lines if helpful; trim
    unrelated frames).

Read and apply [triaging.md](../../../docs/security/triaging.md) and
[reproducing-bugs.md](../../../docs/security/reproducing-bugs.md) for the
authoritative classification rules (`Type=Vulnerability` vs. `Type=Bug` vs.
intended behavior, regular bugs vs. sandbox bypasses, flag restrictions, and
special cases).

- **Debugging Indicators for Safe Termination & Sandbox Filter Output**:
  - For regular bugs: Traps like `int3` (x64), `brk #0` (arm64), calls to the
    Abort builtin / `v8::base::OS::Abort`, or `"Fatal error"` indicate safe
    check termination (**`Bug`** / `Security_Impact-None`, or
    **`Not a Bug (Intended Behavior)`** for safe OOM
    `"Safely terminating process"` or unsupported `%`-syntax).
  - For sandbox testing (`--sandbox-testing`): Look for
    `"V8 sandbox violation detected"` on a write/execute fault
    (**`Vulnerability`**), `"V8 sandbox violation detected"` with
    `"The sandbox violation was a *read* access"` (**`Bug`** /
    `Security_Impact-None`), vs. `"Caught harmless ..."`
    (**`Not a Bug (Intended Behavior)`** for safe traps, in-sandbox/guard-region
    faults, or RO safe-region hits).
