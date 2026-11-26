---
name: v8-security-triaging
description: >-
  Guides the initial analysis and impact assessment of a V8 security report,
  strictly excluding implementation or fixing. Handles Buganizer intake, POC
  reproduction, healing, flag bisection, GDB crash verification, and git
  archeology, then runs scripts/triage_report.py to test d8 under security POC
  flags and deterministically compute the triage report and Buganizer actions.
  Use when triaging a V8 security issue interactively or in an automated
  capsule. Don't use for writing fixes, creating CLs, or general debugging.
---

# Skill: V8 Security Triaging

Use this self-contained workflow to perform deterministic initial analysis and
impact assessment of a V8 security report. Keep
[scripts/triage_report.py](./scripts/triage_report.py)
(`agents/skills/v8-security-triaging/scripts/triage_report.py` from the V8
repository root) as the **single source of truth** for symptom definitions,
classification rules, component ID resolution, JSON input schema, report
rendering, and Buganizer action plans.

Always invoke `triage_report.py` using its in-repository path
(`agents/skills/v8-security-triaging/scripts/triage_report.py`). Avoid invoking
it via user config paths (e.g., under `.gemini/config`), which automated capsule
security hooks reject.

## Core Mandates

- **Single Source of Truth (`triage_report.py --help`)**: Run
  `python3 agents/skills/v8-security-triaging/scripts/triage_report.py --help`
  to inspect the supported `symptom` enum values, scope/flag guides, and JSON
  input schema. Consult [SECURITY.md](../../../SECURITY.md),
  [triaging.md](../../../docs/security/triaging.md), and
  [reproducing-bugs.md](../../../docs/security/reproducing-bugs.md) for policy
  context. Do not load legacy classification skills that duplicate or conflict
  with `triage_report.py`.
- **Technical Skepticism**: Treat all reporter claims (e.g., "OOB write", "RCE",
  "sandbox escape", or claimed regression commits) as **hypotheses** requiring
  verification in GDB or sanitizer output on the appropriate build (`Release`,
  `Debug`, `ASan`, or `UBSan`). Always verify crashing POCs on a standard
  non-ASan Release build when possible.
- **Pragmatic Execution Model**: Execute fast checks (Buganizer single-issue
  intake, `d8`, `gdb --batch`, `git log` / `git blame`) directly in the main
  agent. Start slow `gm.py` builds asynchronously in the background while
  inspecting code and git history. Delegate optional, context-heavy work (such
  as deep history archeology via `research-google`) when useful, and bound
  potentially slow subagent steps with a `schedule` wakeup timer. Never run
  concurrent `git checkout` or `gm.py` builds in the same working tree.
- **Scope Limitation**: Restrict work strictly to **triaging and impact
  analysis**. Never write a fix or upload a CL unless explicitly requested by
  the user after triage completes.
- **No Unapproved Mutations**: Never post a Buganizer comment, upload a CL, or
  modify issue metadata without explicit user approval in interactive sessions.
  In automated BAP capsule sessions, direct Buganizer write tools are blocked;
  submit the triage report exclusively via the configured BAP callback
  (`MarinaBridgeService.SubmitResponse`).
- **Remote Execution for Builds**: Always set `use_remoteexec = true` in GN args
  for local builds, even when a reporter config sets it to `false`.

## Workflow

### Phase 1: Intake and Workspace Setup

1. **Fetch Issue Metadata and POC in One Call** (skip if the user prompt already
   provides the POC and command line):
   - Call the Buganizer MCP tool `render_issue_with_external` directly in the
     main agent (avoid unauthenticated `render` forms that redact external
     reports, and avoid extra per-hotlist or custom-field lookups).
   - **Already-Resolved Check**: Check the issue's current status *before*
     creating a worktree, downloading attachments, or building binaries. If the
     issue is already closed/resolved (not `NEW`, `ASSIGNED`, or `ACCEPTED`),
     follow the environment's skip policy (in automated BAP runs, immediately
     submit the short skip message via callback; in interactive runs, ask the
     user whether to proceed).
   - Extract from `render_issue_with_external`:
     - Inline POC or attachment filenames (also check for mentioned files like
       `poc.html` or `crash.log` absent from the attachment list), `d8` flags,
       and reporter commit/V8 version.
     - `current_issue` fields: `id`, `title`, `component_id`, `priority`,
       `severity`, `hotlists`, and `has_release_block` (`true` only if the
       `ReleaseBlock` custom field is present).
     - Specialized execution modes or flags (e.g., REPL mode, `DebugEvaluate`,
       shell/extension flags, or removed `--enable-inspector`).
     - Unsupported Chromium ports/projects (e.g., RISC-V, PPC, S390, MIPS,
       LoongArch, or Drumbrake), mapping to
       `reproduction.unsupported_port: true`.
2. **Download Attachments or ClusterFuzz Testcase**:
   - First check `list_attachments_with_external` / `get_attachment` via MCP.
   - If MCP attachment retrieval fails, fall back to the `issues` CLI, passing
     the target issue ID explicitly (required by capsule security hooks):
     ```bash
     /google/bin/releases/issues-cli/issues readonly list-attachments <issue_id>
     /google/bin/releases/issues-cli/issues readonly download-attachment --issue_id=<issue_id> --attachment_id=<attachment_id> --output=tmp/<filename>
     ```
     Run `issues` commands outside the terminal sandbox (`BypassSandbox: true`)
     when RPC auth is required.
   - If a ClusterFuzz testcase link is present without an attached POC, use the
     `clusterfuzz` skill (`fetch_testcase.py`). Also note whether the crash is
     already uploaded to ClusterFuzz.
   - **Mandatory Stop Condition**: If the POC, attachments, or required command
     line remain redacted or inaccessible after MCP, CLI, and ClusterFuzz
     attempts, stop immediately and ask the user (or report the missing resource
     via BAP callback). Do not synthesize a substitute POC from commit diffs or
     stack traces.
3. **Duplicate Check (Environment-Aware)**:
   - **Interactive sessions** (cross-bug reads permitted): Search Buganizer
     across both open and closed issues (`lookup_similar_issues`,
     `lookup_recommendations`, or `issues readonly search` using the crash
     signature, assertion, and crashing function). Record matching issue IDs,
     status, and prior decisions:
     - If an open canonical issue already has a working reproducer and triage
       decision, ask the user whether to fast-path to deduplication.
     - If a matching ClusterFuzz issue was auto-closed (`VERIFIED` or
       `NOT_REPRODUCIBLE`) after an older testcase stopped reproducing, finish
       reproduction so the canonical issue can be reopened with the new POC.
   - **Autonomous / BAP capsule sessions** (single-issue security hook active):
     Skip all cross-bug Buganizer searches (`lookup_similar_issues`,
     `lookup_recommendations`, `get_bugs`, `get_bugs_with_external`,
     `list_hotlist_entries`). Rely only on git history (`Fixed:` trailers in
     `fixed_in_commit`).
4. **Prepare Workspace**:
   - If already inside a dedicated Rift workspace (`../.rift.json` or
     `../.rift_prime_directive.json` exists) or a checkout with build
     directories (`out/x64.*`), work directly in it.
   - Otherwise, use the `v8-workflow` skill (`agents/scripts/create_worktree.sh`
     `cr-NNN`, which creates `worktrees/task-cr-NNN` in a standard git checkout
     or forks a sibling Rift subvolume) or `manage-rift-workspaces` when
     available to avoid contaminating the current checkout. Store scratch files
     (`tmp/poc.js`, `tmp/triage_input.json`) in a relative `tmp/` subdirectory.

### Phase 2: Reproduction, Healing, and Investigation

1. **Smart Revision Check and Git Archeology**:
   - First test `tmp/poc.js` against any existing `out/x64.*/d8` binary at
     `HEAD` using the reporter's flags.
   - Use `git log` / `git blame` on the crashing subsystem/files to find
     `introduced_in_commit` and check whether a recent commit on `main` already
     fixed the issue (`fixed_in_commit`).
   - **If `HEAD` exits cleanly (`0`)**: Inspect `git log` on the subsystem or
     reporter's commit *before* building missing targets at `HEAD`. If
     `fixed_in_commit` already landed on `main`, confirm the fix at `HEAD`, copy
     `agents/skills/v8-security-triaging/scripts/triage_report.py` to
     `tmp/triage_report.py` before switching commits (so checking out a parent
     older than the script keeps it available; in a `git worktree`, the main
     checkout also keeps the script at `main`), then check out
     `<fixed_in_commit>~1` and build missing targets (`out/x64.release/d8`,
     `out/x64.debug/d8`, `out/x64.asan/d8`, `out/x64.ubsan/d8` with
     `use_remoteexec = true`) **once** at the reproducing parent commit. (In
     Rift or git worktrees whose shared `.git` metadata directory sits outside
     the workspace root, state-changing git commands such as `git checkout`
     require `BypassSandbox: true`.) Set `reproduction.build_commit` to that
     commit hash.
   - Preserve the exact original POC before modifying scratch copies.
2. **Translate and Heal `tmp/poc.js` (Eliminate Unnecessary Flags)**:
   - `triage_report.py` validates `d8` in staged checks under
     `--run-as-security-poc` (or `--run-as-sandbox-security-poc` when
     `sandbox_mode` is `true`):
     - `run1_flagless`: no extra flags (`enabled_by_default = true`).
     - `run1_with_poc_helpers`: only standard trigger/verification helpers such
       as `--allow-natives-syntax`, `--expose-gc`, `--verify-heap`
       (`enabled_by_default = true`).
     - `run2_with_flags`: non-default feature/config flags
       (`enabled_by_default = false`).
   - Convert browser/HTML reproducers (`poc.html`) to standalone `d8` JS:
     - **Web Workers in `d8`**: Use `new Worker(fn, {type: 'function'})`.
       Messages are delivered unwrapped to `onmessage = function(msg) { ... }`
       (not `msg.data` like DOM `MessageEvent`), and the main thread receives
       messages via `worker.getMessage()`.
   - Heal `tmp/poc.js` so `run1_flagless` reproduces with `d8_flags: []`
     whenever possible:
     - Replace `--allow-natives-syntax` calls: swap
       `%PrepareFunctionForOptimization` / `%OptimizeFunctionOnNextCall` with a
       warm-up loop (`for (let i = 0; i < 10000; i++) f();`),
       `%ArrayBufferDetach(b)` with `b.transfer()`, and `%DeoptimizeFunction` /
       `%DeoptimizeNow` with a shape or type change.
     - Replace `--expose-gc` / `gc()` with JS allocation pressure.
     - Self-load preload scripts (`wasm-module-builder.js`) inside `tmp/poc.js`
       via `d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js')` or
       inline them.
     - Convert raw `.wasm` binaries via `wami --mjsunit` when needed.
     - Strip redundant, default-enabled, and diagnostic tracing flags
       (`--trace-*`, `--print-*`, `--run-as-[sandbox]-security-poc`,
       `--expose-memory-corruption-api`, `--sandbox-testing`). Verify defaults
       and implications in `src/flags/flag-definitions.h`, and bisect umbrella
       flags (`--future`, `--wasm-staging`) to the single required feature flag.
     - Reduce oversized loops/recursions so `d8` crashes quickly without timing
       out. For bloated reproducers, run `minimize_dd.py` from the
       `minimize-reproducer` skill on a scratch copy.
3. **Audit Non-Default Flags and Shell / Embedder Helpers**:
   - `--run-as-[sandbox]-security-poc` does **not** reject every `d8`-only or
     extension `DEFINE_BOOL` flag (for example, `--use-external-strings`).
   - Whenever reproduction requires an extra flag or external resource, inspect
     `src/` and re-test without that flag. If the crash or sandbox violation
     depends on `src/d8/` or `src/extensions/` shell/test-only flags (e.g.,
     `--use-external-strings`, `--isolate`, `--shell`) or mock embedder
     resources unreachable in production Chromium/Blink, set
     `reproduction.d8_only_or_embedder_mock: true`.
   - Note that `--enable-inspector` was removed from `d8`
     (https://crbug.com/573042262); keep it listed in `d8_flags` when the report
     relies on it so `triage_report.py` routes it to DevTools
     (`inspector-test`).
4. **GDB Crash Verification and Special Cases**:
   - Inspect the crash in GDB on `out/x64.release/d8` (plus Debug/ASan/UBSan as
     applicable):
     - **Faulting instruction (`x/i $pc`)**: Confirm load (read) vs store
       (write) vs indirect call/jump.
     - **Register and address control (`info registers`, `si_addr`)**: Check
       whether the faulting address or written value reflects
       attacker-controlled data vs a fixed null or near-null offset (`0x0`).
     - **Explicit safe terminations (`bt`)**: Watch for `int3`, `brk #0`,
       `ud1`/`ud2`, `v8::base::OS::Abort`, builtin `Abort`,
       `__libcpp_verbose_abort`, `"Fatal error"`,
       `"Safely terminating process"`, or `"Caught harmless ..."`.
     - **Exact exit code and sandbox filter output**: Record the exit code or
       signal in `reproduction.result`. When `--sandbox-testing` /
       `--run-as-sandbox-security-poc` is active, check stderr for
       `"V8 sandbox violation detected"`,
       `"The sandbox violation was a *read* access"`, or
       `"Caught harmless ..."`. Note whether out-of-sandbox memory is actually
       corrupted vs staying contained inside the sandbox or bounded same-tag
       pointer-table entries.
   - **`DCHECK`-only failures**: For regular POCs that hit a `DCHECK` in
     `out/x64.debug/d8` and exit `0` in `out/x64.release/d8`, inspect the site
     and downstream code in Release to determine whether continuing past the
     compiled-out `DCHECK` corrupts state, and pass `"out/x64.debug/d8"` as
     `reproduction.d8_binary`. In `sandbox_mode`, `src/sandbox/testing.cc`
     catches `SIGABRT` and `SIGTRAP` as harmless `_exit(0)`, so check a
     Release/ASan build to verify any real out-of-sandbox write.
   - **Silent Corruption Escalation (Bounded)**: If the POC exhibits silent
     corruption without crashing, test `Debug`, `ASan`, and `UBSan` builds. If
     still clean, make **at most 1 to 2 focused attempts** to escalate it into a
     detectable crash (for ClusterFuzz). If escalation hits internal safety
     filters or still exits `0` under `--run-as-[sandbox]-security-poc`, stop
     immediately, record the blocked step(s) in `blocked_steps`, and set
     `reproduction.unconfirmed_vulnerability: true` when empirical evidence
     supports a potential security flaw without a confirmed crash.

### Phase 3: Deterministic Report Generation and Delivery

1. **Inspect Schema and Write `tmp/triage_input.json`**:
   - Run
     `python3 agents/skills/v8-security-triaging/scripts/triage_report.py --help`
     if you have not already verified the symptom guide and JSON schema.
   - Populate `tmp/triage_input.json`:
     - Set `proposed_component` to the crashing V8 source path (e.g.,
       `"src/maglev"` or `"src/wasm"`, resolved via `DIR_METADATA`), a numeric
       Buganizer component ID, or `{"path": "...", "id": "..."}` verified with
       `list_components` / `get_component` (avoid broad root components when a
       specific subcomponent applies).
     - Set `introduced_in_commit` and `fixed_in_commit` when known.
       `triage_report.py` formats commit titles, Gerrit links, V8 versions, and
       `Cr-Commit-Position` revisions automatically, resolves `proposed_owner`
       from the commit author if omitted, and derives `duplicate_of` only from
       `Fixed:` commit trailers (non-`Fixed:` tracking-bug trailers are ignored;
       set `duplicate_of` explicitly when verified).
     - Record exact local paths only in `reproduction.poc_path` /
       `reproduction.d8_binary`. Name `reproduction.poc_path` (`tmp/<filename>`)
       to match the existing Buganizer attachment (or the `BAP_ATTACHMENT`
       filename you attach), because `triage_report.py` prints its basename on
       the `Reproduction:` line. Omit or trim local workstation prefixes (e.g.,
       `/usr/local/google/btrfs_mount/.../v8/` or `/home/<user>/v8/`) from all
       text fields (`rationale`, `result`, `verified_impact`, `gdb_backtrace`,
       `blocked_steps`).
     - Propose a subsystem-prefixed `proposed_title` (e.g., `[maglev] ...`) when
       `current_issue.title` is generic.
     - Use optional `blocked_steps`, `reproduction.d8_only_or_embedder_mock`,
       and `reproduction.unconfirmed_vulnerability` per
       `triage_report.py --help`.
2. **Semantic Self-Check Before Running**:
   - Confirm `symptom` matches verified GDB/sanitizer behavior (`read` vs
     `write` vs `call`, not a reporter claim).
   - Confirm any extra `d8` flag was checked against `src/d8/` and
     `src/extensions/`.
   - Confirm `reproduction.result` records the exact exit code/signal and any
     sandbox crash-filter message.
   - Confirm every blocked verification step and any unconfirmed crash is
     explicitly recorded so `triage_report.py` surfaces it clearly.
3. **Run `triage_report.py` Against the Active Checkout or Worktree**: When
   using a git worktree (`worktrees/task-cr-NNN`), either `cd` into the worktree
   or pass `--repo-root worktrees/task-cr-NNN` so `d8_binary`, `poc_path`,
   `d8.file.execute`, and git version lookups target the worktree rather than
   the main checkout.
   - **Interactive session** (emits comment + empirical summary + Buganizer MCP
     Action Plan):
     ```bash
     python3 agents/skills/v8-security-triaging/scripts/triage_report.py tmp/triage_input.json
     ```
   - **Automated BAP capsule session** (emits only the clean Buganizer Markdown
     comment for `MarinaBridgeService.SubmitResponse`):
     ```bash
     python3 agents/skills/v8-security-triaging/scripts/triage_report.py \
       --output-mode=bap tmp/triage_input.json
     ```
   - **If checked out at an older commit** where
     `agents/skills/v8-security-triaging/scripts/triage_report.py` does not
     exist in the working tree, run your `tmp/triage_report.py` copy (or the
     main checkout's script when inside a git worktree). If neither copy is
     present, extract it from `origin/main` without modifying git state:
     ```bash
     git show origin/main:agents/skills/v8-security-triaging/scripts/triage_report.py > tmp/triage_report.py
     python3 tmp/triage_report.py tmp/triage_input.json  # add --output-mode=bap in capsule runs
     ```
   - Fix any validation errors printed to `stderr` and re-run until exit code
     `0`.
4. **Deliver Results**:
   - **Interactive session**:
     - Present the rendered output to the user and wait for **explicit
       approval**. If a confirmed crash is not yet on ClusterFuzz, assemble the
       manual upload details (reproducer path, job name, issue ID, revision, and
       flags) using the `clusterfuzz` skill in the user-only section; omit it
       when posting to Buganizer.
     - Once approved, execute the steps in the order printed by
       `triage_report.py`, **always posting the Buganizer Analysis comment
       first** (`add_buganizer_comment`, stripping everything from
       `--- (User Only: Do Not Post to Buganizer) ---` downward).
     - For duplicates, follow the streamlined closure plan
       (`add_buganizer_comment` -> `safe_move_issue` -> `edit_title` ->
       `mark_issue_as_duplicate`). If the canonical issue was prematurely
       auto-closed by ClusterFuzz, reopen it to `ASSIGNED` and attach the
       minimized POC.
     - On `safe_move_issue` `FAILED_PRECONDITION` errors (unresolvable principal
       on the source component), skip retrying and inform the user that the move
       needs the Buganizer web UI.
     - On restricted issues, add the current assignee as a collaborator via
       `safe_add_collaborator` before calling `safe_reassign_issue`.
   - **Automated BAP capsule session**:
     - Submit the `--output-mode=bap` comment verbatim via
       `MarinaBridgeService.SubmitResponse`.
     - If you generated a substantially cleaner, standalone, healed, or
       minimized POC than what is already attached, save it under
       `tmp/<filename>` matching the `BAP_ATTACHMENT` name before running
       `triage_report.py` (so `Reproduction:` prints the matching filename), and
       append a `<!-- BAP_ATTACHMENT: <filename> -->` block after the comment
       per the capsule's `bap-callback` instructions. Never reference an
       unattached capsule-only filename in the `Reproduction:` line.

### Phase 4: Cleanup and Preservation

- Before deleting a worktree or ending an interactive session, ask the user
  whether to preserve `tmp/poc.js`, minimized reproducers, logs, or
  `tmp/triage_input.json` outside the worktree.
- Once confirmed or preserved, remove only the isolated worktree or forked
  subvolume created for this session (`v8-workflow`). Do **not** close a
  pre-existing top-level Rift workspace opened by the user.
