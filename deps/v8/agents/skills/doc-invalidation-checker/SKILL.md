---
name: doc-invalidation-checker
description: Checks whether uncommitted changes, current branch diffs (git diff @{upstream}...HEAD), or recent commits invalidate or expand documentation in docs/, and updates affected markdown files. Use before uploading a CL, during pre-commit verification, or when auditing recent commits for documentation drift. Do not use for general C++ compilation or running test suites.
---

# Documentation Invalidation Checker

Use this skill to verify whether code changes on the current branch, in the
working tree, or in recent commits invalidate or expand any design documentation
in the `docs/` directory, and to update the affected `.md` files so `docs/`
stays synchronized with `src/`.

## Workflow

### 1. Retrieve the Target Diff

Select the command matching your workflow stage:

- **Current Branch / Pre-Upload Check (Default for CL Authoring)**: Inspect all
  committed changes on the current branch relative to upstream
  (`@{upstream}...HEAD`) together with any uncommitted `src/` and `include/`
  changes in the working tree:

  ```bash
  agents/scripts/get_recent_commits.py --branch
  ```

- **Uncommitted Working-Tree Changes**: Inspect staged and unstaged edits
  against `HEAD`:

  ```bash
  agents/scripts/get_recent_commits.py --uncommitted
  ```

- **Recent Commits Audit**: Inspect the last `N` commits (e.g., `5`) or a
  specific revision range:

  ```bash
  agents/scripts/get_recent_commits.py 5
  agents/scripts/get_recent_commits.py --range origin/main..HEAD
  ```

### 2. Locate Relevant Documentation in `docs/`

1. Review the `CANDIDATE DOCS DIRECTORIES/FILES TO CHECK` output from
   `get_recent_commits.py`, which maps modified `src/` directories to their
   corresponding `docs/` folders and files.

2. Search across `docs/` for every C++ class, method, enum, bitmask, register,
   pipeline pass, bytecode, IR node, or flag modified, renamed, or removed in
   the diff:

   ```bash
   git --no-pager grep -n "<SymbolOrFlagName>" docs/
   ```

3. Check both subsystem subdirectories (`docs/compiler/`, `docs/heap/`,
   `docs/interpreter/`, `docs/objects/`, `docs/parsing/`, `docs/regexp/`,
   `docs/runtime/`, `docs/sandbox/`, `docs/snapshot/`, `docs/torque/`,
   `docs/wasm/`, `docs/builtins/`, `docs/codegen/`) and top-level files in
   `docs/`.

### 3. Verify Documentation Against `src/`

For each candidate `.md` file in `docs/`:

- **Direct Verification (Single CL / Pre-Upload)**: Read the relevant sections
  of the `.md` file and compare every claim against the diff and `src/`.

- **Parallel Subagent Verification (Multi-Commit / Broad Audit)**: When auditing
  multiple commits or many documentation files, call `invoke_subagent` with
  `TypeName: "self"`, passing the diff and target `docs/` file path(s) with the
  following prompt:

  > Compare this diff and the current implementation in `src/` with
  > `[doc_path]`. Identify any statement, class/method/enum name, bitmask,
  > register, pipeline pass, bytecode/IR node, flag, or invariant in
  > `[doc_path]` that is now obsolete, inaccurate, or missing a key invariant
  > introduced by the change. Provide exact file:line citations and concrete
  > replacement text verified against `src/`.

Check specifically for:

1. **Renamed or Removed Symbols**: Class names, method names, enum constants,
   bitmasks, bytecodes, IR operations, or CLI flags
   (`src/flags/flag-definitions.h`).
2. **Pipeline & Ordering Changes**: Added, reordered, or removed compiler
   phases/reducers (`src/maglev/`, `src/compiler/turboshaft/`, `src/baseline/`),
   register allocation steps, or GC/write-barrier paths.
3. **Cross-Tier & Security Invariants**: Changes to object layouts (`V8_OBJECT`,
   `TaggedMember`, `TrustedPointerMember`), pointer table tags, hole sentinels
   (`TheHole` vs. `TdzHole`), deoptimization frame state, or background
   compilation serialization contracts.

### 4. Update Affected Documentation

1. **Apply Updates Directly**: When authoring a CL or fixing a bug, edit the
   affected `.md` file(s) in `docs/` directly so the documentation stays in sync
   within the same CL.
2. **Preserve Manual Security Triaging Rules**: Keep `docs/security/triaging.md`
   unchanged unless the user explicitly asks to modify it, as it is maintained
   manually.
3. **Formatting & Link Hygiene**:
   - Verify every C++ symbol, constant, and invariant written into `docs/`
     against `src/`.
   - Ensure all relative markdown links resolve to existing files.
   - Use standard ASCII punctuation (commas, colons, parentheses) rather than
     the Unicode em-dash (`U+2014`).

## Tools & References

- **[get_recent_commits.py](../../scripts/get_recent_commits.py)**: Retrieves
  branch diffs (`--branch`), uncommitted diffs (`--uncommitted`), revision
  ranges (`--range`), or recent commits (`[N]`), and maps modified `src/` paths
  to candidate `docs/` directories.
