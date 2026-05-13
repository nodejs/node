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
  source of truth. Use `render_issue_with_external` if content is redacted.
- **Sandbox Bypasses vs. Regular Bugs**:
  - **Sandbox Bypass**: Reports that start with in-sandbox memory corruption
    (using `--sandbox-testing` or the Sandbox API). These are strictly governed
    by the V8 Sandbox threat model.
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
  user with manual upload instructions in Step 5.
- **Exhaustive Verification**: Never classify a bug based solely on the report.
  Exhaustive technical verification via `v8-poc-classification` is mandatory.
- **Official Documentation**: Always refer to
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

### 1. Phase: Intake (Delegation)

Task the **Researcher** subagent with gathering all necessary data from
Buganizer.

- **Orchestrator Instruction**: "Invoke the Researcher to render Buganizer issue
  `<id>`, extract the POC and d8 flags, find top experts for affected files, and
  identify the correct component. Instruct the subagent to return only a concise
  technical summary."
- **Extraction**: The summary must include the POC script, required `d8` flags,
  the reporter's environment (commit hash/version), and the identified
  **introduction commit (regression range)**.
- **Version and Commit Identification**: Always retrieve the current V8 version
  number from `src/utils/version.h` and the specific git hash using
  `git rev-parse HEAD`. Prioritize referencing specific git hashes over generic
  terms like "HEAD" in triage reports.
- **Attachment Check**: Ensure the subagent checks for mentioned files (e.g.,
  "poc.html", "crash.log") that are NOT in the attachments list.
- **Stop Condition**: If the subagent reports that critical attachments (POC,
  flags, etc.) are inaccessible or redacted, the Orchestrator **MUST** stop
  immediately and ask the user to provide them manually before proceeding to
  Phase 2.
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

- **Boundary Check**: Task **Tester** to run with
  `reporter_flags + --run-as-[sandbox]-security-poc`.
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

Draft a concise synthesis based on verified subagent findings.

- **Mandatory First Sentence**: "This analysis is AI-generated using the
  `v8-security-triaging` skill (Conversation ID: `<id>`)." You **MUST** retrieve
  the `<id>` from the `INVOKER_INFO_SESSION_ID` environment variable.
- **Formatting Requirement**: Use a **bulleted list** format for the main points
  (Status, Classification, Rationale, etc.) and ensure there are **double line
  breaks** between each list item for optimal rendering in Buganizer.
- **Content**:
  - **Classification**: Vulnerability / Bug / Not a Bug (Intended Behavior) /
    Failed to Reproduce. **MANDATORY**: Only classify as "Vulnerability" if
    local reproduction was successful. If reproduction fails, classify as
    "Failed to Reproduce".

  - **Security Impact**: Provide the label (e.g., `Security_Impact-Head`) and a
    short explanation. Skip or simplify the CVSS vector unless requested.

  - **Proposed Severity**: Provide the proposed severity (e.g., `S1`) based on
    [triaging.md](../../../docs/security/triaging.md) and Chromium guidelines.

  - **Introduced In / Regression Range**: Provide the commit or version where
    the vulnerability was introduced, if identifiable.

  - **Rationale**: Explain the technical conclusion. For sandbox bypasses,
    explicitly state if it violates the threat model.

  - **Local Reproduction Findings**: Follow the structure and mandatory fields
    defined in the **Classification Guidelines** of `v8-poc-classification`.
    Ensure all technical data (Status, Reproduction command, Result, Build
    (including version from `src/utils/version.h` and git hash), Verified
    Impact, and optional GDB Backtrace) is included here.

  - **Proposed Owner**: Based on expert discovery. Include a very short (half
    sentence) explanation for the choice (e.g., "author of affected code",
    "primary maintainer of subsystem").

  - **Proposed Component**: Propose the most specific Buganizer component
    possible (e.g., `Parser`, `Maglev`, `Turbofan`) if the current component is
    the top-level V8 engine component or is otherwise incorrect. Include the
    component path and ID.

  - **Proposed Title**: If the current title is generic, propose a more
    descriptive title.

  - **ClusterFuzz Upload Info (User Only)**: If a real crash or memory
    corruption is confirmed and it has NOT yet been uploaded to ClusterFuzz,
    provide all necessary details for a manual upload (repro file, job name,
    issue ID, and flags) to the user. Explicitly advise the user to perform the
    upload.

### 6. Phase: Verification & Self-Correction (Audit)

Task a **Generalist** subagent acting as a "Security Triage Auditor" to review
the draft.

- **Orchestrator Instruction**: "Audit the attached triage draft against
  `docs/security/triaging.md` and the Technical Quality Checklist. Ensure the
  classification is technically sound and the formatting is correct. If errors
  are found, distinguish between text-only corrections and missing technical
  work."
- **Loop-back Mandate**: If the Auditor identifies missing technical evidence
  (e.g., skipped boundary checks, missing GDB analysis, or unverified impact on
  Release builds), the Orchestrator **MUST** return to the relevant previous
  phase (Phase 2, 3, or 4) and re-delegate the work to the appropriate subagent
  before proceeding.
- **Review**: The auditor must verify that:
  - The classification (Vulnerability vs. Bug) is consistent with the
    reproduction results (e.g., if it needs experimental flags, it's a Bug).
  - The "Local Reproduction Findings" section contains the exact d8 command, V8
    version, git hash, and the observed result.
  - The formatting (double line breaks between list items) is strictly followed.
  - The conversation ID is valid or "N/A".
- **Action**: Present the *audited and verified* analysis to the user for
  approval ONLY after all technical gaps identified by the auditor have been
  addressed.

## Technical Quality Checklist

- [ ] **Classification Accuracy**: Does the result match the criteria in
  `docs/security/triaging.md`? (e.g., `nullptr` is a Bug, safe termination is
  Intended Behavior).
- [ ] **Boundary Verification**: Was the POC tested with the security filter
  flag (`--run-as-[sandbox]-security-poc`)?
- [ ] **Mandatory Data**: Are the V8 version and git hash included in the Build
  description?
- [ ] **Formatting**: Are there double line breaks between all bulleted list
  items?
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
