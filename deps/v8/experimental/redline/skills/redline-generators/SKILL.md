---
name: redline-generators
description: Guides changes to the generators, mutators, CodeGen helpers, presets and random d8 flags of Redline. The changes must help Redline find more bugs in general, without overfitting to known bugs. Covers the sources of ideas, the checks of each change, and the rules for defaults, weights and random d8 flags. Also covers the briefs and the reviews of this work. Use it when you add or change a generator, a mutator, a preset or a flag profile in experimental/redline.
---

# Generators, mutators, presets and d8 flags

CONTRIBUTE.md sections 3 to 8 show how to add each component, with real
commands. This skill adds the rules that keep the changes general.

## 1. Where ideas may come from

Good sources:

- the code generators and program templates of Fuzzilli, for parity
  (`Sources/Fuzzilli/CodeGen/` and `Sources/Fuzzilli/Profiles/` in the
  Fuzzilli repository),
- the JavaScript language and the builtins that the typed builtin model does
  not cover yet (the typed builtin model section of CONTRIBUTE.md),
- the V8 flags of an area (`src/flags/flag-definitions.h`) and the fuzz flag
  configurations of V8 (section 5 of this skill),
- coverage gaps: code that the sessions of Redline do not reach (section 1.1),
- the FAST node kinds that no generator produces (the list in
  `commands/selftest_selection.cc`).

Sources that you must not use:

- the regression test, the fix or the d8 flags of a known bug, also of a
  training bug,
- the crash files and the crash signature of a re-injected bug.

A generator must describe a general program shape. If you can name the bug
that a change targets, the change is overfit.

### 1.1 Find coverage gaps

1. Run Redline and Fuzzilli on the same d8 without seeds (the coverage setup of
   the [redline-ab-testing](../redline-ab-testing/SKILL.md) skill).
2. Replay the final corpus of each tool on one counters d8 with
   `redline replay --determinism=3 --dump-edges=FILE`. Remove the edges of an
   empty program.
3. Map the edges that only Fuzzilli reaches to files and functions with the
   edge map of `tools/edge_map.py`.
4. Work on the largest areas, with general program shapes or vocabulary.

`replay` adds no prelude, so use this method for runs with `--prelude=none`.
To compare at equal time, use only the corpus files of the first hours.

If a whole API family is missing, check the deny list of the introspection in
`core/environment.cc`. Also check that the d8 flags of the preset expose the
API.

## 2. Make the change

1. Follow CONTRIBUTE.md section 3 (generator), 4 (mutator), 6 (strategy) or 8
   (preset).
2. Make every number a `REDLINE_TUNABLE`. A replaced number keeps its old value
   as the default.
3. Use the typed builtin model for builtin calls. Do not write arguments by
   hand.
4. Before a generator calls a runtime function, make sure that
   `Runtime::IsEnabledForFuzzing` in `src/runtime/runtime.cc` allows it.
5. Do not iterate an absl hash container in code that makes programs. Its
   order changes from one process to the next, so a seed can give another
   program.
6. Write the comments in ASD-STE100 ([redline-ste](../redline-ste/SKILL.md)).

Read the preset file in `presets/` before you change a preset. The examples in
the documentation can be older than the files.

## 3. Check the change

| Check | Command | Pass criterion |
|---|---|---|
| Build | `autoninja -C out/redline redline` | no warning |
| Selftest | `redline selftest --offline` | passes after you examined the golden trace diff |
| Validity | `redline mutate --generate --generator=NAME --n=2000` | at least 99.8% V8-accepted |
| Run rate | the same command with `--run --d8=D8` | you can explain each type of failure and timeout |
| Audit | `redline audit --generator=NAME --n=1000` on each d8 with known bugs | no crash signature above 0.50% |
| Effect | the [redline-ab-testing](../redline-ab-testing/SKILL.md) skill | a measured gain against the build without the change |

Rules for the checks:

- Use `--n=2000` for the validity of a new generator. With 500 programs, one
  rejected program is already 0.2%.
- Generators that need a context (a class body, a method, a member slot) make
  nothing from an empty program. Also measure validity with seed programs that
  have the context (CONTRIBUTE.md section 3.5).
- Read about 50 programs with `--print`. A call in a `try` block can hide a
  `TypeError` that the `fail` column does not show.
- The audit applies no preset. Give it the `d8_extra_args` of the preset with
  `--d8-extra-args`. Also run it with the `d8_args` of each training bug
  build.
- The audit only catches the worst overfitting. Passing it does not prove that
  a change is general.
- If a generator fails the audit, make it more general. Do not lower its weight
  or its probabilities to pass.
- A generator that crashes a bug build in the audit, with the signature of
  that bug, is a template of the bug. Remove the path.
- Only the builder of a held-out bundle runs the audit on it.

## 4. Defaults and weights

- A new generator starts at weight 1, as in CONTRIBUTE.md section 3.3. Its
  effect needs an A/B result.
- A new mutator starts at weight 0. An A/B result decides its weight.
- A new path in an existing generator or mutator changes the default behavior.
  Give it the probability 0 until an A/B result decides.
- A change of an existing default, weight or probability needs an A/B result
  against the old default. Put it in the CL description.
- Do not change a default because of a difference below 0.5 points.
- Do not raise the weight of a generator because it finds one known bug
  (CONTRIBUTE.md section 8.3).
- Keep a change of behavior out of a commit that only updates golden traces or
  counts. Describe each change of behavior in the commit message.

## 5. Random d8 flags

The random flags are in `engine/d8_flags.cc`. Each flag has one `flags.*`
tunable for its probability. A `maglev` and a `turbolev` profile select the
flags of each preset family.

The fuzz flag configurations of V8 are in these files:

- `tools/clusterfuzz/trials/clusterfuzz_trials_config.json`
- `tools/clusterfuzz/foozzie/v8_fuzz_flags.json`
- `tools/testrunner/testproc/fuzzer.py`
- `tools/testrunner/local/variants.py` (the test variants)

`FlagList::ResolveContradictionsWhenFuzzing` in `src/flags/flags.cc` also names
flags that V8 prepares for fuzzing.

Rules:

- Take flags from these configurations or from the V8 profile of Fuzzilli.
  You can also take a family of flags, for example all flags that turn off one
  optimization.
- Do not take a flag from the d8 flags of a known bug. Ask the builder of the
  held-out bundles if a new flag is in their `d8_args`. The builder answers
  only yes or no.
- V8 marks `DEFINE_EXPERIMENTAL_FEATURE` flags as not ready for fuzz testing.
  Do not add such a flag, unless the V8 configurations or Fuzzilli already draw
  it.
- Keep the probability of a flag that stops background threads at 0, for
  example `--single-threaded`. Such flags hide races. Check the
  `NEG_IMPLICATION` lines of a new flag for the same effect.
- Read the uses of a new flag in `src/`. A flag that changes only Wasm code
  does nothing in a JavaScript preset.
- Check each new flag on every d8 of the bug sets: d8 must accept it without a
  warning, and `--print-flag-values` must show it. Some flags exist only in
  some build types.
- Check the implications and contradictions of the flag in
  `flag-definitions.h`. A contradiction aborts d8.
- Keep the profiles apart. The selftest checks that the Maglev profile draws no
  Turbolev flag and that the Turbolev profile draws no Maglev-only flag.
- The golden traces do not record the random flags. A new rule for the draw
  needs its own selftest check.
- Measure the slowdown of expensive flags, for example verifiers.
- Check what an agent says about a flag against `flag-definitions.h`.

## 6. Brief another agent

- Give the general capability and its source: a Fuzzilli template, the
  specification or the V8 code.
- Name the files to change and the checks of section 3.
- Keep the material of known bugs out of a brief. This includes bug IDs,
  fixes, crash signatures, the d8 flags of a bug and the shape of a
  regression test.
- Do not give a goal such as "crash each bug build" or "stay below the audit
  threshold".
- Check each command, path and flag of the brief against the tree before you
  send it.

## 7. Review the work

These defects can pass the checks of section 3:

- Compare each port with its Fuzzilli source, line by line, and name each
  difference.
- Find the origin of each list of constants. A list from Fuzzilli is good. A
  list from a regression test is a copy of a bug.
- Look for a rare branch that writes a fixed sequence of statements. Such a
  branch can hide a template from the audit.
- Predict the golden trace diff. Explain each changed line that you did not
  predict.
- Compare the claims of the commit message with the code.
- A test of a fix must fail without the fix.
- A reviewer can read training tests to find copies, but never the material of
  held-out bugs.

## 8. Show the effect

A change is useful only if it improves one of the measurements of
[AGENTS.md](../../AGENTS.md) section 2 without making another worse. Measure
it with the [redline-ab-testing](../redline-ab-testing/SKILL.md) skill against
the build without the change, and against origin/main.
