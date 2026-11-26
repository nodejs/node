# Evaluating `doc-invalidation-checker` and `docs/` Auto-Sync

Test cases for `doc-invalidation-checker` are checked in as declarative
Promptfoo configs (`*.promptfoo.yaml`) and `.patch` fixtures under
[`agents/prompts/eval/doc-invalidation-checker/`](../../../prompts/eval/doc-invalidation-checker/prompt.md),
graded with
[`agents/testing/asserts/check_changes.py`](../../../testing/asserts/check_changes.py)
and validated on presubmit by
[`agents/testing/lint_promptfoo_testcases.py`](../../../testing/lint_promptfoo_testcases.py).

Use [`eval_doc_checker.py`](../scripts/eval_doc_checker.py) to run end-to-end
evaluations across isolated git worktrees.

## 1. Benchmark Cases (`agents/prompts/eval/doc-invalidation-checker/`)

| Case ID                         | Config File                                    | Category                      | What It Tests                                                                                                                                                                                                                                              |
| :------------------------------ | :--------------------------------------------- | :---------------------------- | :--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `maglev-max-stack-slots`        | `maglev_max_stack_slots.promptfoo.yaml`        | `positive-constant-invariant` | Forward-looking `src/maglev/maglev-compiler.cc` change (`kMaxStackSlots` raised from `4 * KB` to `8 * KB` / `1024` slots). Verifies `docs/compiler/maglev/compiler-maglev.md` is updated.                                                                  |
| `tdz-hole-rename`               | `tdz_hole_rename.promptfoo.yaml`               | `positive-cross-tier-rename`  | Cross-file symbol rename (`ValueNode::IsTdzHole()` -> `ValueNode::CanElideTdzHoleCheck()`). Verifies both `docs/parsing/hole-checks-optimized.md` and `docs/parsing/hole-check-elision.md` are updated.                                                    |
| `string-instance-type-bitmasks` | `string_instance_type_bitmasks.promptfoo.yaml` | `positive-bitfield-layout`    | Bitmask swap (`kSharedStringMask = 1 << 4`, `kUncachedExternalStringMask = 1 << 5` in `src/objects/instance-type.h`). Verifies `docs/objects/strings.md` bit descriptions match `src/`.                                                                    |
| `security-triaging-guardrail`   | `security_triaging_guardrail.promptfoo.yaml`   | `guardrail-protected-doc`     | `JSDispatchEntry` x64-LA57 layout update (`padding_` -> `call_count_`) in `src/sandbox/js-dispatch-table.h` referencing `docs/security/triaging.md`. Verifies `docs/sandbox/architecture.md` is updated while `docs/security/triaging.md` stays untouched. |
| `internal-noop-refactor`        | `internal_noop_refactor.promptfoo.yaml`        | `negative-no-doc-churn`       | Local variable rename (`truncate` -> `truncation_processor`) in `src/maglev/maglev-compiler.cc`. Verifies `0` files in `docs/` are modified (no false-positive doc churn).                                                                                 |

## 2. Evaluation Modes

`eval_doc_checker.py setup` supports two prompt modes (`--mode`) and two diff
modes (`--diff-kind`):

- **`--mode explicit`**: Prompts the subagent with `prompt.md` to check whether
  the changes in the checkout invalidate or expand `docs/` and update affected
  `.md` files in place.
- **`--mode implicit`**: Prompts the subagent to run V8 pre-upload checks on a
  finished code change (without mentioning `docs/` in the prompt), testing
  whether `agents/rules/v8-best-practices.md` and
  `agents/skills/v8-workflow/SKILL.md` cause the agent to spontaneously check
  and update `docs/`.
- **`--diff-kind committed|uncommitted`**: Applies the `.patch` fixture either
  as a branch commit on top of `@{upstream}` (`committed`) or as an unstaged
  working-tree modification (`uncommitted`, matching Promptfoo's
  `gemini_provider.py`).

## 3. Running an End-to-End Evaluation

1. **Verify fixtures apply cleanly against `HEAD`** (optional pre-flight):

   ```bash
   agents/skills/doc-invalidation-checker/scripts/eval_doc_checker.py check --case all
   ```

2. **Set up isolated worktrees and print prompts**:

   ```bash
   agents/skills/doc-invalidation-checker/scripts/eval_doc_checker.py setup --case all --mode explicit
   ```

3. **Spawn one isolated subagent per case**: Call `invoke_subagent`
   (`TypeName: "self"`, `Workspace: "inherit"`) for each case using the `prompt`
   printed by `setup` (or from `--json` output).

4. **Grade the worktrees**:

   ```bash
   agents/skills/doc-invalidation-checker/scripts/eval_doc_checker.py grade --case all
   ```

   `grade` executes the Promptfoo assertions in `check_changes.py` plus markdown
   hygiene checks:

   - **Recall (`check_files_changed`)**: Every expected `docs/` file was
     modified.
   - **Guardrail (`check_files_unchanged`)**: Protected files (including
     `docs/security/triaging.md`, or `docs/` on negative cases) were not
     modified.
   - **Precision (`no_extra_docs`)**: No unrelated `docs/` files were churned.
   - **Accuracy (`check_file_content`)**: Updated `.md` files contain all
     `present` substrings and no `absent` substrings.
   - **Hygiene**: Zero `U+2014` em-dashes, clean `git diff --check`, and valid
     relative markdown links.

5. **Clean up temporary worktrees and branches**:

   ```bash
   agents/skills/doc-invalidation-checker/scripts/eval_doc_checker.py cleanup --case all
   ```
