# Redline: instructions for AI agents

Redline is the coverage-guided JavaScript fuzzer of V8 in `experimental/redline/`.
This file tells an agent how to work on Redline. It applies to all agents, for
example Jetski and Claude Code.

## 1. Read first

1. [README.md](README.md): what Redline is, the quick start and the directory
   layout.
2. [DESIGN.md](DESIGN.md): the architecture, the stages and the design rules.
3. [TUTORIAL.md](TUTORIAL.md): the commands, from the build to change-based
   fuzzing.
4. [CONTRIBUTE.md](CONTRIBUTE.md): how to add a component, measure a change and
   prepare a CL.

The skills are not in the skill list of your agent. Read the `SKILL.md` file
of the skill for your task:

| Task | Skill |
|---|---|
| Check a change before you upload it | [redline-presubmit](skills/redline-presubmit/SKILL.md) |
| Add or change a generator, a mutator, a preset or a random d8 flag | [redline-generators](skills/redline-generators/SKILL.md) |
| Measure Redline against Fuzzilli, origin/main or another build | [redline-ab-testing](skills/redline-ab-testing/SKILL.md) |
| Make a d8 with a known bug, to measure bug finding | [redline-bug-reinjection](skills/redline-bug-reinjection/SKILL.md) |
| Run, debug or measure change-based fuzzing | [redline-change-fuzzing](skills/redline-change-fuzzing/SKILL.md) |
| Write a comment, a Markdown file, a commit message or a CL description | [redline-ste](skills/redline-ste/SKILL.md) |

## 2. What to improve

Three measurements tell if Redline improves (CONTRIBUTE.md section 10.3).
Measure bug finding and coverage against Fuzzilli and against origin/main.
Measure change-based fuzzing against normal mode and against origin/main.

| Measurement | Question | How to measure it |
|---|---|---|
| Bug finding | Does Redline find known bugs that Fuzzilli does not find, or find them sooner? | Re-injected bugs, one run for each arm and bug, at least 3 repetitions |
| Change-based fuzzing | Does change-based fuzzing find the bug of a change sooner than normal mode? | The same bugs, change-based fuzzing against normal mode, everything else equal except the d8 build |
| Coverage | Does Redline reach more coverage than Fuzzilli without seeds? | The same d8, the same d8 flags, the same workers and the same duration |

Measure each area preset in `presets/` in the same way, for example `maglev`,
`turbolev`, `runtime` and `builtins`. Skip change-based fuzzing for an area
that has no C++ code in d8, for example the CSA builtins.

## 3. Rules

### 3.1 Online actions

- Do an online action only when the user asks for that action.
- The online actions include an upload, a change of a CL description, a start
  of the commit queue, a vote, a comment and the abandon or delete of a CL.
- "Make a plan for a CL" means "write a local plan". It does not mean "create
  the CL".
- Do not push to a remote branch.
- If you are not sure that the user wants an online action, ask.

### 3.2 What goes into the repository

- Code, tests, golden traces, presets, tools and documentation of methods go
  into the repository.
- Benchmark results, run logs, bug lists, reports and plans do not go into the
  repository. Keep them in a local directory outside the V8 checkout.
- Do not put benchmark results or bug lists in code comments.
- Do not write numbers that change with the code into the documentation or the
  code, for example the number of tunables. They become wrong at the next
  change.
- A CL description can give the measured result of that CL. It records the
  result at one date. Give the setup and the number of runs with each number.

### 3.3 Honest results

- Give only numbers that a tool printed from raw data. Do not interpolate,
  estimate or copy a number from memory.
- Name each build by its commit. Do not mix builds in one statistic.
- Report every failed, crashed or killed run. Do not drop a run because its
  result is bad. Give the real duration of each run, not the planned duration.
- Give the number of runs. One run is a hint, not a result.
- Compare only runs with the same setup. The
  [redline-ab-testing](skills/redline-ab-testing/SKILL.md) skill lists the
  rules.
- Count a bug as found only if its crash matches the signature of the bug and
  reproduces with the same signature.
- Measure validity and the audit again on the final version of a CL before
  you quote them.
- Check each claim about the origin of a flag or a number against its source,
  for example a fuzz configuration of V8.

### 3.4 Overfitting

- Do not design a generator, a mutator, a generator weight or a d8 flag from a
  known bug. This rule includes its regression test, its fix and its d8 flags.
- You can tune the weights of change-based fuzzing on training bugs
  (redline-change-fuzzing section 5).
- The overfitting audit (CONTRIBUTE.md section 3.7) is a check, not a target.
  Do not tune a generator to stay just below 0.50%.
- Keep a held-out set of bugs. Only the agent or person that builds a held-out
  bug reads its test, its fix and its d8 flags. Others never read them.
- Do not tie a generator change to a bug ID in a brief, a comment or a commit
  message.
- A change that you tuned on bug builds needs its own CL and an A/B result on
  held-out bugs.
- When you delete a copy of a bug from the code, discard the results that you
  measured with it.

### 3.5 Other agents and models

- If you switch your model or the model of a worker, tell the user in your next
  message, with the reason.
- Review all work of other agents before it goes into a CL, a report or a
  message to the user.
- Review and test code before it runs in a long campaign. A short test run
  does not replace a review.
- If an agent stops answering, check its state and its last step before you
  start new work.

### 3.6 Shared machines

- Do not stop processes that you did not start. Put the output directory of
  your run in the pattern, and one letter in brackets:
  `pkill -f '[r]edline fuzz.*--out=DIR( |$)'`. Without the brackets,
  `pkill -f` also stops the shell that runs it. Check the list with
  `pgrep -af` first.
- Stop a session with one `SIGINT` or `SIGTERM`, and wait for the `end` event.
  A second signal stops Redline at once. Then look for d8 processes that stay
  alive.
- Cap the memory of a large build or replay, for example with
  `systemd-run --user --scope -p MemoryMax=40G`.
- Before a new run reuses an output directory, archive the old one with its
  corpus. Many scripts that start a run delete the output directory first.
- Stop the d8 and fuzzer processes of your runs when the runs end.
- Before you delete a worktree or a clone, run `git status` in it. Make sure
  that its commits are on a branch that you keep.

## 4. Workflow for a change

1. Read the code and the documentation of the area.
2. Make the change. Follow CONTRIBUTE.md for the component type.
3. Build and run `redline selftest --offline`.
4. Check validity, the run rate and the audit (CONTRIBUTE.md sections 3.4 to
   3.7).
5. Measure the effect with the
   [redline-ab-testing](skills/redline-ab-testing/SKILL.md) skill.
6. Write the comments and the commit message in ASD-STE100
   ([redline-ste](skills/redline-ste/SKILL.md)).
7. Run the [redline-presubmit](skills/redline-presubmit/SKILL.md) checks.
8. Tell the user the result. Upload only if the user asks.
