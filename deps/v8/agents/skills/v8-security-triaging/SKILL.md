---
name: v8-security-triaging
description: Guides the initial analysis and impact assessment of a V8 security report, strictly excluding implementation or fixing.
---

# Skill: V8 Security Triaging

Use this skill to orchestrate the initial analysis and impact assessment of a V8
security vulnerability report.

## Core Mandates

- **Strategic Orchestration**: You are the **Orchestrator**. Your goal is to
  manage specialized subagents to verify reporter claims empirically while
  keeping your own context lean.
- **Technical Skepticism**: Treat all reporter claims (e.g., "OOB write", "RCE",
  "Silent Write") as **hypotheses**, not facts. Your primary job is empirical
  verification.
- **Mandatory Local Reproduction**: A report MUST NOT be classified as a
  "Vulnerability" unless it is empirically reproduced locally (e.g.,
  demonstrating a crash, memory corruption, or a clear security boundary
  violation). If the provided POC does not reproduce the claimed behavior, do
  NOT classify it as a vulnerability. Instead, report that it did not reproduce
  in the final report.
- **Scope Limitation**: This skill is strictly for **triaging and impact
  analysis**. It does NOT include implementing a fix or creating a CL. Fixing is
  a separate task that must be explicitly requested by the user after triage is
  complete.
- **No External Actions without Approval**: NEVER upload a CL, post a comment,
  or modify issue metadata on Buganizer or Gerrit without explicit user approval
  of the exact content and action.
- **Buganizer First**: Use the Buganizer MCP (`render_issue`) as the primary
  source of truth. Use `render_issue_with_external` if content is redacted. If
  attachment retrieval via `get_attachment_enrichment` fails, **MUST**
  immediately attempt retrieval using the `buganizer-cli` skill before asking
  the user.
- **Sandbox Bypasses vs. Regular Bugs**:
  - **Sandbox Bypass**: Reports that start with in-sandbox memory corruption
    (using `--sandbox-testing` or `--expose-memory-corruption-api` or the
    Sandbox API). These are strictly governed by the V8 Sandbox threat model.
  - **Regular Bug**: Vulnerabilities that do not require an initial in-sandbox
    write primitive.
- **Title Accuracy**: If the reporter provided a generic title, identify a more
  descriptive and accurate title based on the crash state or root cause.
- **Component Verification**: Propose only actual Buganizer components. Use
  `mcp_Buganizer_list_components` to verify existence and
  `mcp_Buganizer_get_component` to verify the component path and ID before
  posting.
- **Remote Execution Priority**: Always use `use_remoteexec = true` in GN
  arguments for all local builds to speed up the process, even if a reporter
  provides a configuration where it is set to `false`. Remote execution is
  strictly an environmental optimization and does not affect reproduction logic.
- **Environmental Awareness**: During information gathering, identify if the
  crash involves **specialized execution modes** (e.g., REPL mode,
  `DebugEvaluate`, or experimental features) that might have different security
  properties.
- **Attachment Access**: Only attempt to retrieve attachment content (e.g.,
  `poc.js`) using the Buganizer MCP tools. If the tools fail, **MUST** ask the
  user to provide the content manually. Do not speculatively search for
  restricted attachments.
- **ClusterFuzz Check**: Check the issue's comments for indications that the
  crash has already been uploaded to ClusterFuzz. If not uploaded, provide the
  user with manual upload instructions in Step 5 using the `clusterfuzz` skill.
- **Exhaustive Verification**: Never classify a bug based solely on the report.
  Exhaustive technical verification via `v8-poc-classification` is mandatory.
- **Artifact Preservation**: Before removing a worktree or cleaning up a
  session, you MUST identify all generated artifacts (e.g., minimized POCs,
  crashing `crash.js` scripts, or logs) and ensure they are either uploaded to
  Buganizer or moved to a permanent location outside the worktree if requested
  by the user.
- **Official Documentation**: Consult [SECURITY.md](../../../SECURITY.md) for
  high-level threat models and entry points. Always refer to
  [triaging.md](../../../docs/security/triaging.md) for the definitive rules on
  labeling and classification.
- **Strict Subagent Delegation**: To maintain a lean context, the Orchestrator
  **MUST NOT** call Buganizer, Gerrit, or local execution tools directly during
  Phases 1-4. All technical tasks must be delegated to subagents via
  `invoke_agent`. The Orchestrator's role is strictly limited to reviewing
  subagent summaries and coordinating the next step.

## Strategic Orchestration Guidelines

When executing a triage task, delegate tactical steps to subagents:

- **Researcher**: Fetching report details, identifying experts
  (`find_experts_for_file`), and locating Buganizer components.
- **Builder**: Setting up the environment (`git checkout`) and building specific
  variants (Release, ASan, non-ASan).
- **Tester**: Baseline reproduction across multiple configurations and security
  boundary checks.
- **Generalist**: POC minimization, flag bisection, and "healing" POCs
  (replacing natives with standard JS).
- **Debugger**: Interactive crash analysis in GDB to verify primitives and
  attacker control.

## Workflow

### 0. Phase: Prepare isolated worktree

- **Orchestrator Instruction**: "Use v8-workflow skill to create a fresh
  worktree (cr-NNN, where NNN is the bug number) and respective branch for
  investigation. Choose "Isolated Strategy", don't ask user. Switch to the new
  worktree and work there to avoid contamenating current workspace. Use "tmp"
  subdirectory for temporary files."

### 1. Phase: Intake (Delegation)

Task the **Researcher** subagent with gathering all necessary data from
Buganizer.

- **Orchestrator Instruction**: "Retrieve the `INVOKER_INFO_SESSION_ID` from the
  environment. Invoke the Researcher to render Buganizer issue `<id>`, extract
  the POC and d8 flags, find top experts for affected files, and identify the
  correct component. Instruct the subagent to return only a concise technical
  summary."
- **Conversation ID Persistence**: The Orchestrator MUST store the
  `INVOKER_INFO_SESSION_ID` in its current context immediately to ensure the
  mandatory first sentence in Phase 5 uses the correct, verified ID.
- **Extraction**: The summary must include the POC script, required `d8` flags,
  the reporter's environment (commit hash/version), the issue's current
  `Priority`, `Severity`, `ReleaseBlock` custom field (`1223086`), and attached
  hotlists (specifically any `Security_Impact-*` hotlists), as well as the
  identified **introduction commit (regression range)**.
- **Version and Commit Identification**: Always retrieve the current V8 version
  number from `include/v8-version.h` and the revision number from
  `Cr-Commit-Position`
  (`git log -n 1 --format="%(trailers:key=Cr-Commit-Position)" HEAD`).
  Distinguish between commits referenced for their **change** vs. their
  **version**:
  - When the **change** is important (e.g., an introduction/regression commit or
    a fixing commit), include the commit title and link to its Gerrit changelist
    (from `Reviewed-on:` in `git log -n 1 --format=%b <hash>`).
  - When only the **version** is important (e.g., the version used for a local
    build or the reporter's tested version), include both the V8 version and the
    `Cr-Commit-Position` revision number, skipping the git hash and with **no
    link to Gerrit**.
- **Attachment Check**: Ensure the subagent checks for mentioned files (e.g.,
  "poc.html", "crash.log") that are NOT in the attachments list. If retrieval of
  an attachment via MCP tools fails, the subagent **MUST** use the
  `buganizer-cli` (`issues download-attachment`) as a fallback.
- **Stop Condition**: If both MCP and CLI retrieval attempts for critical
  attachments (POC, flags, etc.) fail or if they are redacted/inaccessible, the
  Orchestrator **MUST** attempt retrieval via the `clusterfuzz` skill (using
  `fetch_testcase.py`) for ClusterFuzz links before stopping and asking the user
  to provide them manually. This includes asking for the **exact command line**
  if a ClusterFuzz report link remains inaccessible.
- **Mapping**: Include identified experts and the specific Buganizer component
  (e.g., `Blink > JavaScript > Maglev`).

### 2. Phase: Exhaustive Reproduction (Delegation)

Task the **Tester**, **Builder**, and **Generalist** subagents with confirming
the issue. You MUST confirm the bug exists before proceeding.

- **Baseline**: Task **Tester** to reproduce with the reporter's exact flags and
  revision.
- **Escalation**: If initial attempt fails, task **Builder** and **Tester** to
  verify across Release, Debug, and ASan variants at HEAD.
- **Healing (Initial)**: If still failing, task **Generalist** to analyze and
  "heal" the POC for environmental dependencies.
- **Summary**: Review the subagent's reproduction command and result. Stop if
  reproduction fails after exhaustive attempts.

### 3. Phase: Security Boundary Verification (Delegation)

Task the **Tester** and **Generalist** to determine if the bug violates a
security boundary.

- **Boundary Check**: Task **Tester** to replace
  `--expose-memory-corruption-api` in `reporter_flags` with `--sandbox-testing`
  and run with updated `reporter_flags` and `--run-as-[sandbox]-security-poc`.
- **Investigation Loop**: If the crash stops reproducing with the security flag,
  task **Generalist** to identify why and attempt to "heal" the POC by replacing
  forbidden syntax with standard JS while maintaining the bug trigger.
- **Conclude**: Review the subagent's conclusion on whether the bug is a
  security vulnerability or a regular bug.

### 4. Phase: Impact Determination & Minimization (Delegation)

Task the **Debugger**, **Tester**, and **Generalist** to provide technical proof
of impact.

- **Crash/Corruption Priority**: Task **Generalist** to make the POC either
  crash or demonstrate clear memory corruption.
- **ClusterFuzz Compatibility (Crash Escalation)**: Because ClusterFuzz requires
  a detectable crash (DCHECK, segfault, SIGILL, or int3), you **MUST**
  exhaustively pursue a crashing reproducer if the initial POC only proves
  silent memory corruption, following the guidelines in the `clusterfuzz` skill.
  Task **Builder** and **Tester** to run the POC against **Debug**, **ASan**,
  and **UBSan** builds. If no crash occurs naturally, task **Generalist** to
  modify the POC to force a crash (e.g., by overwriting a Map with a garbage
  pointer) while maintaining the original bug trigger.
- **Verification**: Task **Tester** to verify on Standard Release and ASan
  builds.
- **Minimization**: Task **Generalist** to reduce the POC and flags to the
  smallest possible set using the methodology in `v8-poc-classification`.
  - **Security Impact Check**: If minimization/bisection shows that a
    vulnerability only reproduces with flags disabled in production (e.g., a
    developer flag), it should be classified as `Security_Impact-None`.
- **Deep Dive**: Task **Debugger** to capture the crash in GDB and verify the
  primitive (Read/Write) and attacker control. Review the provided backtrace.

### 5. Phase: Drafting Findings

Draft a short, precise synthesis based on verified subagent findings.

- **Strict Brevity Mandate**: Keep the entire report compact and to the point
  (~20–30 lines total). Avoid filler prose, redundant explanations across
  bullets, full `args.gn` dumps, or untrimmed stack traces. Never include local
  workstation paths (e.g., `/usr/local/google/home/...` or `file:///`) in the
  Buganizer comment text; use only the POC filename (e.g., `poc.js`) in the
  `Reproduction` line and keep local paths strictly in the `(User Only)`
  section.
- **Mandatory First Sentence**: "This analysis is AI-generated using the
  `v8-security-triaging` skill (Conversation ID: `<id>`)." You **MUST** retrieve
  the `<id>` from the Orchestrator's `INVOKER_INFO_SESSION_ID` environment
  variable.
- **Formatting Requirement**: Use a **bulleted list** format for the main points
  (Status, Classification, Rationale, etc.) and ensure there are **double line
  breaks** between each list item for optimal rendering in Buganizer. If a list
  item has sub-bullets (specifically in 'Local Reproduction Findings'), they
  **MUST** be indented by **at least four spaces** and **MUST NOT** be separated
  by double line breaks within the same nested list.
- **Content**:
  - **Classification** (1 line): `Vulnerability` / `Bug` /
    `Not a Bug (Intended Behavior)` / `Failed to Reproduce`. **MANDATORY**: Only
    classify as "Vulnerability" if local reproduction was successful.

  - **Security Impact** (1 line): Classify as "**None**" (experimental,
    disabled, or V8 Sandbox bypass with `Security_Impact-None` and `v8-sandbox`
    hotlist) or "**Yes**" (shipping, web-exploitable) with a brief parenthetical
    rationale; omit specific channel labels (e.g., `Security_Impact-Stable`).

  - **Proposed Severity** (1 line): Provide the severity (e.g., `S1` or `S2`)
    and proposed priority (if it needs adjustment) based on
    [triaging.md](../../../docs/security/triaging.md), where priority is at
    least the severity for active vulnerabilities, can be lower than severity
    for `Security_Impact-None` (e.g., `S1` -> `P2`), and should be lowered
    (e.g., `P2` or `P3`) when downgrading to a non-security `Bug`.

  - **Introduced In** (1 line): Commit and version where the bug was introduced
    (and fixed, if already fixed), including the commit title and a clickable
    Gerrit link (e.g.,
    `[commit <hash>](https://chromium-review.googlesource.com/c/v8/v8/+/<cl>) ("<title>", V8 <version>)`).

  - **Rationale** (2–4 concise sentences max): State the root cause and why it
    violates (or does not violate) the security threat model. Do not repeat
    reproduction outputs or register values covered in
    `Local Reproduction Findings`.

  - **Local Reproduction Findings** (1 concise line per sub-bullet, except
    `GDB Backtrace` if multiple lines help): Follow the mandatory fields in
    `v8-poc-classification`:

    - `Status`: `Reproduced` / `Not Reproduced`.
    - `Reproduction`: `d8 <flags> <poc_filename.js>` (filename only, no local
      paths).
    - `Result`: 1–2 short sentences summarizing the observed crash/output.
    - `Build`: Build variant(s), V8 version (from `include/v8-version.h`), and
      revision number (from `Cr-Commit-Position`), skipping the git hash and
      Gerrit link (omit boilerplate GN args).
    - `Verified Impact`: 1 sentence stating the verified primitive and attacker
      control.
    - `GDB Backtrace`: A concise snippet of the faulting instruction/registers
      and relevant stack frames (can span multiple lines if helpful; trim
      unrelated frames).

  - **Proposed Owner** (1 line): `<email>` with a short phrase explaining why
    (e.g., "author of introducing commit and JSPI maintainer").

  - **Proposed Component** (1 line): Short component path and ID (e.g.,
    `Blink > JavaScript > WebAssembly` (ID: `1456332`, already set)).

  - **Proposed Title** (1 line): New descriptive title, or
    `Keep current (<title>)` if already accurate.

  - **ClusterFuzz Upload Info (User Only)**: If a real crash or memory
    corruption is confirmed and not yet on ClusterFuzz, assemble the upload
    package (local repro file path, job name, issue ID, target revision, and
    flags) using the `clusterfuzz` skill for manual upload. Omit this section
    when posting the comment to Buganizer.

### 6. Phase: Verification & Self-Correction (Audit)

Task a **Generalist** subagent acting as a "Security Triage Auditor" to review
the draft.

- **Orchestrator Instruction**: "Audit the attached triage draft against
  `docs/security/triaging.md` and the Technical Quality Checklist. Ensure the
  classification is technically sound, the report is concise, and the formatting
  is correct. If errors are found, distinguish between text-only corrections and
  missing technical work."
- **Loop-back Mandate**: If the Auditor identifies missing technical evidence
  (e.g., skipped boundary checks, missing GDB analysis, or unverified impact on
  Release builds), the Orchestrator **MUST** return to the relevant previous
  phase (Phase 2, 3, or 4) and re-delegate the work to the appropriate subagent
  before proceeding.
- **Review**: The auditor must verify that:
  - The classification (Vulnerability vs. Bug) is consistent with the
    reproduction results (e.g., if it needs experimental flags, it's a Bug).
  - The "Local Reproduction Findings" section contains the exact d8 command, V8
    version and revision number (skipping the git hash and Gerrit link), and the
    observed result.
  - Commits where the *change* is important include both the commit title and a
    Gerrit link, whereas references where only the *version* is important
    include both the V8 version and revision number, skipping the git hash and
    Gerrit link.
  - **Brevity & Cleanliness**: The report follows the line/sentence budgets
    (1-line metadata fields, 2–4 sentence Rationale, concise 1-line sub-bullets
    in Local Reproduction Findings except GDB Backtrace when multi-line helps,
    and no local `/usr/local/...` paths outside the `User Only` section).
  - The formatting (double line breaks between top-level list items AND
    four-space indented sub-bullets for findings without internal double line
    breaks) is strictly followed.
  - The conversation ID matches the Orchestrator's `INVOKER_INFO_SESSION_ID`
    (passed by the Orchestrator to the Auditor, as subagents have their own
    session IDs).
- **Action**: Present the *audited and verified* analysis to the user for
  approval ONLY after all technical or formatting gaps identified by the auditor
  have been addressed.
- **Applying Approved Buganizer Updates**: Once the user approves updating
  Buganizer metadata:
  - **Hotlist Cleanup**: When adding `Security_Impact-None` (`5433277`) via
    `add_issue_to_hotlist`, you **MUST** also remove any existing
    release-channel `Security_Impact-*` hotlists from the issue via
    `remove_issue_from_hotlist`: `Security_Impact-Head` (`5432216`),
    `Security_Impact-Beta` (`5433097`), `Security_Impact-Stable` (`5432902`),
    and `Security_Impact-Extended` (`5432548`). When applicable, also add
    `v8-sandbox` (`4802478`) or `v8-unsupported-chrome` (`8384111`).
  - **Priority & ReleaseBlock Adjustment**: When lowering `Severity` or
    downgrading an issue (e.g., to `Type=Bug` or `Security_Impact-None`), you
    **MUST** check that `Priority` still makes sense and lower it accordingly
    via `update_issue_priority` (e.g., `P2` or `P3` for downgraded bugs or
    `Security_Impact-None` issues), and clear any `ReleaseBlock` values on
    custom field `1223086` (`set_issue_custom_field` with
    `customFieldId: "1223086"` and `customFieldValue: ""`) so release automation
    does not re-promote the issue.

### 7. Phase: Cleanup & Preservation

Finalize the session by securing artifacts and cleaning up the environment.

- **Artifact Check**: Identify all important generated files (e.g., `crash.js`,
  `minimized_poc.js`, or specialized logs).
- **Preservation**: If any important artifacts were generated in the worktree,
  you **MUST** ask the user if they would like to preserve them in the main
  repository directory before deleting the worktree.
- **Worktree Removal**: Once artifacts are preserved or the user confirms they
  are no longer needed, use the `v8-workflow` skill to remove the isolated
  worktree and branch.

## Technical Quality Checklist

- [ ] **Classification Accuracy**: Does the result match the criteria in
  `docs/security/triaging.md`? (e.g., `nullptr` is a Bug, safe termination is
  Intended Behavior).
- [ ] **Boundary Verification**: Was the POC tested with the security filter
  flag (`--run-as-[sandbox]-security-poc`)?
- [ ] **Mandatory Data**: Are both the V8 version and revision number (without
  git hash or Gerrit link) included in the Build description?
- [ ] **Commit vs. Version Linking**: Do commits where the *change* is important
  (e.g., introduction or fix CLs) include their commit title and Gerrit link,
  while references where only the *version* is important include both the V8
  version and revision number, skipping the git hash and Gerrit link?
- [ ] **Brevity & No Local Paths**: Is the Rationale \<= 4 sentences, are all
  other bullets/sub-bullets 1 line each (except `GDB Backtrace` if multiple
  lines help), and are local `/usr/local/...` paths excluded from the Buganizer
  comment text?
- [ ] **Formatting**: Are there double line breaks between all top-level
  bulleted list items? Are sub-bullets indented by at least four spaces without
  internal double line breaks?
- [ ] **Impact Evidence**: Is the Verified Impact supported by GDB analysis or
  ASan/UBSan logs?
- [ ] **Component Mapping**: Does the proposed component exist with the stated
  ID and is it the most specific one available (not just `V8`)?

## Common Misclassification Pitfalls

- **Experimental Flags**: If a bug requires `--experimental-*` flags and is not
  part of `--future` or `--wasm-staging`, it is a **Bug**, not a Vulnerability.
- **DCHECK vs. CHECK**: `DCHECK` failures are **Bugs**. `CHECK` failures are
  **Intended Behavior** (safe termination) unless they are in-sandbox and part
  of a sandbox bypass claim.
- **Sandbox Read-only**: Sandbox bypasses that only provide **Read** access are
  currently **Bugs**.
- **d8-only Flags**: Bugs requiring flags like `--shell` or `--isolate` are
  **Bugs**.
- **nullptr Dereference**: Always a **Bug**, never a Vulnerability.
