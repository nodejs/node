---
name: redline-ste
description: Explains how to write the text of experimental/redline in ASD-STE100 Simplified Technical English. The text includes the Markdown files, the code comments, the commit messages and the CL descriptions. Shows how to check the text with the linter of the redline-presubmit skill. Lists the main rules, the common fixes and the pass criteria. Use it when you write or review any text for Redline.
---

# ASD-STE100 for Redline

All text of Redline uses ASD-STE100 Simplified Technical English: the Markdown
files, the code comments, the commit messages and the CL descriptions.

## 1. Main rules

- Write one instruction in each sentence. Use the imperative:
  `Run the selftest.`, not `You should run the selftest.`
- Keep an instruction at 20 words or less, and a description at 25 words or
  less.
- Use the active voice. Name the actor: `The minimizer removes the statement.`,
  not `The statement is removed.`
- Do not use semicolons. Make two sentences.
- Use one word for one meaning, and the approved meaning of a word.
- Put names of code, flags, files and commands in backticks. The linter does not
  read them as words.
- Write numbers as digits. Give the unit.

## 2. Common fixes

The bad examples are in backticks, so that the linter does not read them.

| Finding | Bad example | Fix |
|---|---|---|
| Long sentence | A sentence with 31 words | Split it into two sentences. |
| Passive voice | `The bug is found by change-based fuzzing.` | `Change-based fuzzing finds the bug.` |
| Semicolon | `It is fast; it is small.` | `It is fast. It is small.` |
| Two words for one action | `deletes` in one sentence and `removes` in the next | Use one word every time, for example `removes`. |
| Flag read as words | a flag name without backticks | Put the flag name in backticks, for example `--verify-turboshaft`. |
| Hedge on an instruction | `You may want to run it.` | `Run it.` |

## 3. Check the text

Run the linter as section 4 of the
[redline-presubmit](../redline-presubmit/SKILL.md) skill tells. It checks the
text of your change against the base revision:

```shell
experimental/redline/skills/redline-presubmit/scripts/ste_check.py --base=REV
```

The same section tells how to lint one file, for example a draft of a commit
message.

| Text | Pass criterion |
|---|---|
| A Markdown file that the change adds or edits | No more hard findings than at the base revision |
| The commit message | No hard finding |
| A comment that the change adds | No hard finding |

- `check.sh` runs the same check. A FAIL line names the file, the line and the
  rule.
- Advisory findings, for example passive voice, do not fail the check. Fix them
  if the text stays clear.
- If you cannot use the linter, apply section 1 yourself, and tell the reviewer.

## 4. CL descriptions

- Use the same rules as for commit messages. Keep lines at 72 characters or
  less.
- Keep the footers: `Bug:`, `Test:` and the `Change-Id:` line. A tool that
  rewrites the description must keep the `Change-Id:` line.
- Give only measured numbers, with the setup and the number of runs.
