---
name: v8-poc-classification
description: Checks if a POC provided by some JS and d8 flags is a vulnerability or just a regular bug.
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
  technical conclusion. If your analysis shows it is a `DCHECK` failure or
  relies on experimental flags, classify it as a **Bug**.

## Workflow

### 1. Initial Verification & Exhaustive Reproduction

Confirm the bug exists as reported.

- **Action**: Run `d8 <provided-flags> <poc.js>`.
- **Escalation**: If it doesn't reproduce, try multiple revisions (Reporter's
  commit and Main) and build configurations (Debug, ASan, Release).
- **Deep Dive**: Capture the initial backtrace and faulting instruction in GDB.
- **Check for Explicit Aborts:** Look for evidence that execution was
  intentionally terminated by V8 (e.g., via `SBXCHECK`, `FATAL`, or traps). If
  so, this is typically **Intended Behavior** (see guidelines below).

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
  "stale value" or "logical type confusion"), you MUST attempt to escalate it to
  a memory safety violation. Prove that the logical flaw can bypass V8's
  security boundaries.
  - **Techniques**: Type Confusion (misleading object layout), OOB Access (stale
    length), Pointer Overwrite (overwriting field/loop-carried pointer).
- **Verification**: Run the *minimized* POC on a **Standard Release build
  (non-ASan)**.
  - If it stops crashing or is caught by a hardened check (`SBXCHECK`, `FATAL`),
    classify it as **Intended Behavior** or a **Bug**.
- **Crashing POC for ClusterFuzz**: A crashing POC (segfault) is highly
  preferred for ClusterFuzz upload. Always try to provide a standalone `.js`
  file that crashes on a Release build.
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

- **Local Reproduction Findings**:
  - **Status**: Reproduced / Not Reproduced.
  - **Reproduction**: `d8 <flags> <poc.js>` (Exact command used locally).
  - **Result**: Summarize the result of running the command (output, crash,
    sandbox violation, harmless memory access).
  - **Build**: The build variant (e.g., x64.release, x64.debug, asan). Always
    include the V8 version from `src/utils/version.h` and the specific git hash
    using `git rev-parse HEAD` for technical accuracy.
  - **Verified Impact**: Summarize the **Verified Impact** (e.g., confirmed OOB
    write). If the bug is purely logical and caught by runtime protections (like
    `ref.cast` or bounds checks) without crashing, state this clearly.
  - **GDB Backtrace**: Include a snippet if it supports the classification.

Summary of rules from [triaging.md](../../../docs/security/triaging.md) based on
the threat model:

#### Regular Bugs (No initial in-sandbox write)

- **Type=Vulnerability**: Production code, enabled by default, survives the
  security POC flags (`--run-as-security-poc`).
- **Type=Bug**: Requires experimental flags (not in `--future`), developer
  flags, or only triggers a `DCHECK` or reliable `CHECK` (safe termination).
- **Intended Behavior**: Safe termination (e.g., via `SBXCHECK`, `FATAL`, or
  hardened libc++ checks).
  - **Indicators:** Look for traps like `int3` (x64), `brk #0` (arm64), or calls
    to the Abort builtin or the runtime function in the backtrace/disassembly.
    Also look for "Fatal error" or "Safely terminating process" in stderr, or
    calls to `v8::base::OS::Abort` in the backtrace.

#### Sandbox Bypasses (Starts with in-sandbox write/corruption)

- **Type=Vulnerability**: Demonstration of **write access** outside the sandbox
  (e.g., "V8 sandbox violation detected") using `--sandbox-testing`.
- **Intended Behavior**: Read-only bypass, or safe termination ("Caught harmless
  memory access violation"). Includes linear OOB writes (e.g., `memset`,
  `memcpy`) hitting guard regions, or corrupting trusted objects with
  well-formed data of the same type. Also includes breaking internal invariants
  using testing-only natives (restricted by `IsEnabledForFuzzing`) if the impact
  is safely contained.
