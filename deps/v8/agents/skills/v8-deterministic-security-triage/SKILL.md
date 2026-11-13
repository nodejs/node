---
name: v8-deterministic-security-triage
description: Self-contained deterministic V8 security issue triaging workflow. Guides POC reproduction, POC healing to eliminate extra flags, flag bisection, GDB crash primitive verification, and git archeology, then uses scripts/triage_report.py (--help for empirical symptom/scope guides and JSON schema) to empirically run d8 and deterministically compute the triage report and Buganizer MCP actions.
---

# Skill: V8 Deterministic Security Triage

Use this skill to perform deterministic initial analysis and impact assessment
of a V8 security issue report. This skill is **completely self-contained** for
POC reproduction, healing, flag minimization, and GDB investigation, and uses
[scripts/triage_report.py](./scripts/triage_report.py)
(`<skill-dir>/scripts/triage_report.py`, resolved relative to the directory
containing this `SKILL.md` — e.g.,
`agents/skills/v8-deterministic-security-triage/scripts/triage_report.py` in a
V8 checkout or
`~/.gemini/config/skills/v8-deterministic-security-triage/scripts/triage_report.py`
when installed locally) as the **single source of truth** for symptom
definitions, V8 component IDs, JSON input schema, and triage deductions.

## Core Mandates

- **Single Source of Truth (`triage_report.py --help`)**: Never guess or
  hardcode symptom names, component IDs, JSON fields, or triage deduction rules.
  Run `python3 <skill-dir>/scripts/triage_report.py --help` to inspect the
  supported `symptom` enum values, empirical identification criteria, dynamic
  `proposed_component` resolution rules, and JSON input schema. Do not load
  legacy classification skills that contain conflicting policy rules.
- **Direct Single-Call Intake (Zero Subagent Buganizer Calls)**: Perform
  Buganizer intake directly in the main agent via a single
  `render_issue_with_external` MCP call (plus attachment download if needed).
  **Subagents must never query Buganizer**: `render_issue_with_external` already
  returns all issue fields, and `triage_report.py` dynamically resolves
  components from `DIR_METADATA` in the V8 repository (or accepts a Buganizer
  component ID / `{"path": "...", "id": "..."}` from `list_components`) and
  derives `duplicate_of` and `proposed_owner` from git commits automatically.
- **Technical Skepticism & Empirical Reproduction**: Treat all reporter claims
  (e.g., "OOB write", "RCE", "Sandbox bypass", or claimed regression commits) as
  **hypotheses** requiring empirical verification in GDB or sanitizer output on
  the appropriate build configuration (`Release`, `Debug` for `DCHECK`
  violations, or `ASan`/`UBSan`).
- **Scope & Approval**: Restrict work strictly to **triaging and impact
  analysis** (do not write a fix or upload a CL unless explicitly asked after
  triage). Always obtain **explicit user approval** of the rendered report and
  Action Plan before posting comments or mutating issue fields on Buganizer.

## Workflow

### Phase 1: Intake & Workspace Setup

1. **Workspace Fast-Path**:
   - If already inside a dedicated Rift workspace (`security-NNN`) or checkout
     with build directories (`out/x64.*`), work directly in it.
   - Otherwise, use `manage-rift-workspaces` (if available) or `v8-workflow`
     (`cr-NNN` worktree, Isolated Strategy). Store temporary files in `tmp/`.
2. **Fetch Issue Metadata & POC in One Call** (skip if the user prompt already
   provides the POC and command line):
   - Call the Buganizer MCP tool `render_issue_with_external` directly in the
     main agent (do **not** use `issues readonly render`, which redacts external
     Chromium/V8 security reports, and do **not** run separate calls to inspect
     individual hotlist IDs or custom field definitions).
   - From `render_issue_with_external`, extract:
     - Inline POC or attachment filenames, `d8` flags, and reporter commit/V8
       version.
     - `current_issue` fields (`id`, `title`, `component_id`, `priority`,
       `severity`, `hotlists`, and `has_release_block` — `true` only if the
       `ReleaseBlock` custom field is present on the issue).
     - Any CLI flag constraints (e.g., `--enable-inspector` or d8-only flags
       like `--shell`/`--isolate`).
     - Whether the report targets an unsupported Chromium port (e.g., RISC-V,
       PPC, MIPS, LoongArch, or Drumbrake), in which case set
       `reproduction.unsupported_port: true`.
   - **Download Attachments / ClusterFuzz Testcase**:
     - If the POC is an attachment on the issue, use
       `list_attachments_with_external` or run
       `/google/bin/releases/issues-cli/issues readonly list-attachments <issue_id>`
       and
       `/google/bin/releases/issues-cli/issues readonly download-attachment <attachment_id> --output=tmp/poc.js`
       with `BypassSandbox: true` (required for `issues` CLI RPC
       authentication).
     - If a ClusterFuzz testcase link is present and the POC is not attached,
       use the `clusterfuzz` skill (`fetch_testcase.py`).
     - **Mandatory Stop Condition**: If the POC or command line remains redacted
       or inaccessible, **stop immediately** on turn 1 and ask the user to
       provide the POC and exact command line.

### Phase 2: Technical Investigation (Main Agent + Background Builds)

Prefer executing fast checks (`d8`, `gdb --batch`, `git log` / `git blame`)
**directly in the main agent** when `out/x64.*/d8` binaries already exist. If a
slow `gm.py` build is needed, launch the build asynchronously in the background
while performing read-only code and git archeology in the main agent (or use a
single read-only `research-google` subagent for deep git history archeology;
never run concurrent `git checkout` or `gm.py` builds in the same working tree).

1. **Smart Revision Check & Git Archeology**:
   - First test `tmp/poc.js` against any existing `out/x64.*/d8` binary at
     `HEAD`.
   - Use `git log` / `git blame` on the crashing subsystem/files to identify
     `introduced_in_commit` and check whether a recent commit on `main` already
     fixed the issue (`fixed_in_commit`).
   - **If `HEAD` exits cleanly (`0`)**: check `git log` on the relevant
     subsystem/file (or the reporter's commit hash) **before** building missing
     targets at `HEAD`. If a `fixed_in_commit` already landed on `main`, verify
     the fix at `HEAD`, then `git checkout <fixed_in_commit>~1` and build
     missing targets (`out/x64.release/d8`, `out/x64.debug/d8`,
     `out/x64.asan/d8`, `out/x64.ubsan/d8` with `use_remoteexec = true`)
     **once** at the reproducing parent commit. Set `reproduction.build_commit`
     to that reproducing commit hash.
2. **Heal `tmp/poc.js` & Eliminate Extra Flags for `EmpiricalPocRunner`**:
   - `triage_report.py` runs `d8` in staged checks:
     - **Run 1a (`run1_flagless`)**: `d8 --run-as-[sandbox]-security-poc poc.js`
       with **no extra flags** (and **Run 1b (`run1_with_poc_helpers`)** if
       standard POC trigger/verification helpers such as
       `--allow-natives-syntax`, `--expose-gc`, or `--verify-heap` are still
       needed).
     - **Run 2 (`run2_with_flags`)**:
       `d8 --run-as-[sandbox]-security-poc <d8_flags> poc.js` (when non-default
       feature flags are required).
   - Heal `tmp/poc.js` on disk and strip every unnecessary flag so **Run 1
     reproduces with `reproduction.d8_flags: []`** whenever the issue is
     triggerable in default V8:
     - **Replace `--allow-natives-syntax`**: Replace
       `%PrepareFunctionForOptimization` / `%OptimizeFunctionOnNextCall` with a
       JS hot loop (`for (let i = 0; i < 10000; i++) f();`),
       `%ArrayBufferDetach(b)` with `b.transfer()`, and `%DeoptimizeFunction` /
       `%DeoptimizeNow` with a natural shape/type change.
     - **Replace `--expose-gc`**: Replace `gc()` with JS allocation pressure.
     - **Self-load preload scripts**: Load helper files like
       `wasm-module-builder.js` inside `tmp/poc.js` via
       `d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js')` or inline
       them.
     - **Strip redundant/default/tracing flags & bisect catch-all flags**:
       Remove `--run-as-[sandbox]-security-poc`,
       `--expose-memory-corruption-api`, `--sandbox-testing`, diagnostic tracing
       flags (`--trace-*`, `--print-*`), and flags already enabled by default in
       `src/flags/flag-definitions.h`. Bisect `--future` or `--wasm-staging` to
       the single required feature flag.
     - **Minimize heavy loops/parameters**: Reduce huge parameter counts or deep
       recursion so `d8` crashes quickly without timing out.
3. **GDB Crash Verification & `DCHECK`-Only Handling**:
   - Inspect the crash in GDB on `out/x64.release/d8` (and Debug/ASan where
     applicable):
     - **Faulting instruction (`x/i $pc`)**: Verify whether the fault is a
       memory load, memory store, or indirect call/jump.
     - **Register & address control (`info registers`, `si_addr`)**: Check
       whether the faulting address or written value is attacker-controlled vs.
       a fixed null/near-null offset (`0x0`).
     - **Safe termination vs. memory violation (`bt`)**: Check for deliberate
       traps (`int3`, `brk #0`, `ud2`, `v8::base::OS::Abort`, `Abort` builtin,
       `__libcpp_verbose_abort`) or messages like `"Fatal error"`,
       `"Safely terminating process"`, or
       `"Caught harmless memory access violation"`.
     - **Sandbox boundary**: When `sandbox_mode` is `true`, check whether the
       crash reports `"V8 sandbox violation detected"` or corrupts
       out-of-sandbox state vs. faulting safely inside the 4GB sandbox or
       hitting an `SBXCHECK`.
   - **`DCHECK`-only failures**: When a POC hits a `DCHECK` in
     `out/x64.debug/d8` and exits cleanly in `out/x64.release/d8`, inspect the
     `DCHECK` site and downstream C++/compiler code in GDB/source on Release to
     determine whether continuing past the compiled-out `DCHECK` corrupts state
     or is benign, and pass `"out/x64.debug/d8"` as `reproduction.d8_binary`
     (per `triage_report.py --help` and
     [reproducing-bugs.md](../../../docs/security/reproducing-bugs.md)).
   - **Silent corruption escalation**: If a POC only causes silent corruption
     that is not already fixed, check Debug/ASan/UBSan or craft a separate
     ClusterFuzz escalation reproducer per the `clusterfuzz` skill.

### Phase 3: Deterministic Triage Report & Buganizer Actions

1. **Consult Schema & Run `triage_report.py`**:
   - Run `python3 <skill-dir>/scripts/triage_report.py --help` if you have not
     already inspected the empirical symptom/scope guides and JSON schema.
   - Write `tmp/triage_input.json`:
     - Pass the crashing V8 source directory/file (e.g. `"src/maglev"` or
       `"src/wasm"`, resolved dynamically via `DIR_METADATA`), a numeric
       Buganizer component ID, or `{"path": "...", "id": "..."}` (from
       `list_components` with `query: "Blink>JavaScript"` when no `DIR_METADATA`
       exists) as `proposed_component`.
     - Provide `introduced_in_commit` and `fixed_in_commit` (when set,
       `triage_report.py` automatically resolves the commit author for
       `proposed_owner` if omitted and parses `Fixed:` commit trailers from
       `fixed_in_commit` to detect `duplicate_of`).
     - Propose a subsystem-prefixed `proposed_title` (e.g. `[maglev] ...`) when
       `current_issue.title` is generic.
   - Run `triage_report.py` from the V8 repository root:
     ```bash
     python3 <skill-dir>/scripts/triage_report.py tmp/triage_input.json
     ```
     Fix any validation errors reported on `stderr` and re-run until exit code
     is `0`.
2. **Present for User Approval & Execute Action Plan**:
   - Present the exact rendered Markdown output (both the Buganizer comment and
     the **Buganizer MCP Action Plan**) to the user and wait for explicit
     approval.
   - Once approved, execute the steps in the order printed by
     `triage_report.py`:
     - **Always post the Buganizer Analysis comment first**
       (`add_buganizer_comment`, omitting everything from
       `--- (User Only: Do Not Post to Buganizer) ---` downward).
     - **If `duplicate_of` is set**: only run the streamlined closure plan
       (`add_buganizer_comment` -> `safe_move_issue` -> `edit_title` ->
       `mark_issue_as_duplicate`).
     - **Otherwise**: apply the remaining steps in the Action Plan
       (`update_issue_type`, `update_issue_severity`, `update_issue_priority`,
       hotlist updates, `set_issue_custom_field` if clearing release block,
       `safe_move_issue`, `edit_title`, and `safe_reassign_issue` after
       `safe_add_collaborator` on restricted issues).
     - **Component Move Fallback**: If `safe_move_issue` fails with
       `FAILED_PRECONDITION` due to an unresolvable principal on the source
       component, do not retry in a loop — complete the remaining steps and note
       that the component move requires the Buganizer web UI.
   - Before removing any temporary workspace/worktree, ask the user whether to
     preserve `tmp/poc.js` and `tmp/triage_input.json`.
