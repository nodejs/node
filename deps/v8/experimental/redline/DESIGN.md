# Redline design

This document describes how Redline works and why. It does not describe the commands.
[README.md](README.md) gives an overview and the build steps. [TUTORIAL.md](TUTORIAL.md) shows the commands.
[CONTRIBUTE.md](CONTRIBUTE.md) shows how to change each part.
Source paths are relative to `experimental/redline/`. Paths such as `test/mjsunit` are relative to the root of the V8 checkout.
`OUT` is the output directory of a session.

## 1. Goals

Redline is a coverage-guided JavaScript fuzzer for V8.
It mutates JavaScript programs, runs them in an instrumented d8, and keeps the programs that reach new code in V8.
It is C++ code in the V8 tree, so that the V8 team can own and change it. It builds on Linux only, because it uses Linux process and memory APIs.

1. **Understandable.** A V8 engineer can follow and debug every step.
2. **Changeable.** An engineer can change one step without changes to the other steps. Each decision is a strategy behind one interface.
3. **Configurable.** One configuration sets every part of a session. A preset is a configuration for one area of V8, for example Maglev.
4. **Plain JavaScript.** Redline imports any script that V8 parses, for example the tests in `test/mjsunit`.
   Each corpus program and each crash is a plain JavaScript file.
5. **Observable.** Each stage and each component reports statistics. A session writes an event log.
6. **Useful crash reports.** Redline groups crashes, makes reproducible crashes smaller, and searches for the conditions that reproduce the other crashes.

## 2. Principles

- **KISS.** The fuzzing loop is a fixed sequence of stages in plain C++ code. There is no plugin framework and no dynamic graph.
  Strategies make the decisions that can change from one session to the next.
- **DRY.** One interface and one registry template serve the four selection points.
  One configuration holds every setting, and the command-line flags are shorthands for its keys.
- **In-tree C++.** Redline builds with the V8 build files and links V8. The V8 parser in the Redline process is the syntax oracle.
- **No magic numbers.** A new number in a generator, a mutator or a reducer must be a tunable:
  a named number with a default, a range and one line of documentation (`core/tunable.h`).
- **Each component runs alone.** The shell (`redline shell`) applies one mutator, generator or reducer to one program.
  `redline mutate`, `redline minimize`, `redline exec` and `redline amplify` run one part on files.
- **Missing data is normal.** A stage must work, possibly with less effect, when optional data is missing.
  For example, a generator must also work at the start of a program, where no variable is visible.
- **Pinned behavior.** Golden traces record the picks of the default strategies and the output of each generator and mutator for fixed seeds.
  A change of behavior shows as a diff.

## 3. Terms

| Term | Meaning |
|---|---|
| target | The instrumented d8 that Redline runs. It speaks the REPRL protocol and writes edge coverage to shared memory. |
| worker | A thread that owns one target and runs the fuzzing loop. |
| program | A JavaScript program, held as a FAST tree and its printed source. Its tree and its source do not change. |
| FAST | The typed JavaScript AST of Redline (`fast/`). |
| corpus | The programs that Accept admitted. All workers share one corpus. |
| candidate | A program that reached new edges. Accept decides if it enters the corpus. |
| component | A generator, a mutator or a reducer. A component has a name, a description and statistics. Generators and mutators also have a weight. |
| strategy | A pluggable policy for one decision, for example the choice of the next seed. |
| prelude | JavaScript that Redline puts in front of each program when it runs the program. The corpus does not store it. |

## 4. Architecture

### 4.1 Code layout

| Directory | Contents |
|---|---|
| `main.cc`, `commands/` | The command-line tool, with one file per command. |
| `base/` | Flags, logging, JSON, files, subprocesses, and the V8 parser in the Redline process. |
| `exec/` | The REPRL client, the coverage map, the target options, and one plain d8 run for the amplify step. |
| `fast/` | FAST: the AST, the arena, the lexer, the parser, the printer and the scope analysis. |
| `core/` | `Program`, `Builder`, `Environment`, `Component`, the random generator, the selection interface and the tunables. |
| `generators/`, `mutators/` | The generators with their shared expression synthesis (`CodeGen`), and the mutators. |
| `corpus/` | The shared corpus and the seed and partner strategies. |
| `minimizer/` | The minimizer and its reducers. |
| `engine/` | The configuration, the stages, the fuzzer and its workers, the shared coverage, the crash signatures, the amplify step, the importer and the prelude. |
| `presets/`, `testdata/` | The presets, and the golden traces of the offline selftest. |

### 4.2 The fuzzing loop

```mermaid
flowchart LR
  init["Init (once)"] --> pick["Pick"]
  pick --> prefix["Prefix"]
  prefix --> mutate["Mutate"]
  mutate --> exec["Execute"]
  exec -->|"new edges"| accept["Accept"]
  accept -->|"admitted"| minimize["Minimize"]
  minimize --> publish["Publish"]
  publish --> more{"chain shorter than N?"}
  accept -->|"dropped"| more
  exec -->|"nothing new"| more
  exec -->|"crash"| triage["Crash triage"]
  more -->|"yes"| mutate
  more -->|"no"| pick
  triage --> pick
```

Init runs first (5.1). Then each worker runs this loop:

1. The worker picks a corpus program, the seed. While the corpus is small, it sometimes starts from the empty program (5.1).
2. It puts fresh value declarations at the top of the seed (the prefix, 5.3).
3. It applies a chain of up to N mutations (`stages.mutate.consecutive`) and runs each result in its target.
   A result that runs without an error becomes the parent of the next link. After a failure or a timeout, the next link uses the same parent.
4. A result with new edges goes to Accept. If Accept admits it, Minimize makes a smaller copy, and Publish adds the copy to the corpus.
   The chain continues from the result as it ran, because minimization would remove material that the next mutations can use.
5. A result that crashes goes to crash triage, and the chain ends.
6. After N links, the worker picks a new seed.

### 4.3 Observability

- Each component counts its invocations, results, executions, outcomes, new edges and time (`core/component.h`).
  Each stage counts its invocations, executions, time, timeouts and successes (`engine/stages.h`). A session prints these tables at an interval.
- The event log `OUT/events.jsonl` has one JSON object per event. The `start` event records the effective configuration and the strategies.
  The `stats` event records the counters of the session and of the stages.
- Each stage has three switches. `log` writes one event per occurrence. `capture` saves the programs of the stage when a trigger fires.
  `stats` adds a row to the stage table.
- Each corpus file starts with its lineage: its origin, its parent and its position in the chain.

## 5. Stages

The worker loop in `engine/fuzzer.cc` calls the stages in a fixed order. The stages delegate their decisions to strategies and components.
The stage statistics use the names `pick`, `prefix`, `mutate`, `check`, `minimize` and `publish`. `check` is the determinism check of Accept.

| Stage | What it does | Configuration |
|---|---|---|
| Init | fills the corpus with imported files, or lets the workers generate programs | `init` |
| Pick | picks a seed and, for `Splice` and `Combine`, a partner | `selection.seeds`, `selection.partners` |
| Mutate | adds a prefix, then applies a chain of mutations | `selection.mutators`, `selection.generators`, `stages.prefix`, `stages.mutate` |
| Execute | runs a program in the target of the worker | `d8`, `d8_args`, `timeout_ms` and other target keys |
| Accept | decides if a candidate enters the corpus | `accept`, `stages.check` |
| Minimize | makes a new corpus program or a crash smaller | `stages.minimize`, `crash_minimizer` |
| Publish | adds the program to the corpus and saves it | `stages.publish` |
| Crash triage | groups, reproduces, amplifies, minimizes and saves crashes | `amplify` and the crash keys |

### 5.1 Init

The `init` block selects one of two strategies:

- `import` loads JavaScript files (`paths`). With `keep: new-coverage`, a file stays only if it runs without an error and reaches new edges.
  With `keep: all`, each file that runs without an error stays. Use `keep: all` to continue a session from its corpus directory.
- `generate` imports nothing. Its parameter `programs` sets `initial_programs`.

While the corpus has fewer than `initial_programs` programs, a worker starts a chain from the empty program with a probability of one half.
It always does so while the corpus is empty. This rule also applies after an import.
A chain from the empty program uses only the `CodeGen` mutator until a result runs without an error.

The importer replaces top-level `load()` and `d8.file.execute()` calls with the statements of the loaded file, so that each program is self-contained.
It removes the loads of `mjsunit.js`, because the prelude supplies their functions. It also removes the loads that it cannot resolve.
The V8 parser in the Redline process checks each imported file before the file runs in a target.

### 5.2 Pick

Pick chooses the next seed. For `Splice` and `Combine`, it also chooses a partner: a second corpus program that supplies code.
The seed strategy and the partner strategy share one registry. The corpus calls them with its lock held, so they can keep plain state.

| Strategy | Picks |
|---|---|
| `uniform` | each program with the same probability. This is the default seed strategy. |
| `recency` | a part of the picks from the newest programs, and the other picks uniformly |
| `tournament` | the program with the fewest picks among a few random programs |
| `productive` | the program with the most children among a few random programs |
| `rarity` | the program with the highest energy (see below) |
| `uniform-partner` | a random program other than the seed. This is the default partner strategy. |
| `rarity-partner` | the program with the highest energy among a few random programs |

The `rarity` strategy prefers programs that reach rare edges, are small and had few picks (`corpus/corpus.cc`):

```text
energy(p) = rarity(p) / ((age(p) + 1)^age_exponent * (1 + ln(size(p))^size_exponent))
rarity(p) = sum over the tracked edges e of p with count(e) < rare_edge_limit of 1 / count(e)^rarity_exponent
```

- `count(e)` is the number of corpus programs that reach edge `e`. The age grows with each pick.
- A new corpus program records its rarest edges from its full edge set (5.7), or from its new edges if the set is missing.
- The edges of a reproducible crash count as one more program. This lowers the energy of the programs that lead to a known crash.

### 5.3 Mutate

A **generator** makes new statements for one insertion point. It declares what its code needs from the context, for example a loop for `continue`.
Redline uses a generator only where the context allows it. Generated code uses only the variables that are visible and initialized at that point.

A **mutator** changes a program. Mutators insert generated code (`CodeGen`, `JitStress`), change code in place (`Input`, `Operation`, `Expression`, `Call`),
copy code from a partner (`Splice`, `Combine`), change the structure (`Remove`, `Wrap`, `Duplicate`, `Reorder`), or ask the target (`Explore`, `Probing`).
`redline mutate --list` shows the mutators and generators with their weights. A weight of 0 disables a mutator or a generator.

One link of the chain works like this:

1. The mutator strategy picks a mutator. A mutator that needs a partner is eligible only when the corpus has at least two programs.
2. `ApplyMutation` copies the tree of the parent. The mutator edits the copy through a `Builder` (9.3).
   It makes from one change up to its own maximum, or exactly `stages.mutate.rounds` changes if that value is above 0.
3. If the mutator finds nothing to do, the link costs no execution. Otherwise, the worker prints the tree and runs the source.

**Prefix.** The prefix gives the mutators new values to work with. It uses only generators whose code cannot throw,
because an exception in the prefix would make each result of the chain fail. Minimize removes the values that a new corpus program does not use.
The `Prefix` mutator makes the prefix. Its weight is 0, so that the mutator strategy does not pick it.

**Runtime-assisted mutators.** A type guess from the source does not tell which properties a value has. `Explore` and `Probing` ask the target instead.
They insert probes, and the worker runs the instrumented program once with a helper script in front.
The helper writes its findings to the fuzzout channel. The mutator turns the findings into ordinary code and removes the probes.
`Explore` examines a variable and does an action that fits its value. `Probing` installs properties that the program asks an object for but the object does not have.

**Environment.** Generators use a hand-written model of the globals, methods and runtime functions of d8 (`core/environment.h`).
At the start of a session, one target runs an introspection script that lists its globals and their members.
With this list, the `DiscoveredApi` generator and `Explore` also reach builtins that the model does not describe.

**Strategies.** `weighted` is the default: one weighted random draw among the eligible components.
`adaptive` moves the weights toward the components with a higher recent yield (`core/adaptive_selector.h`).
With `reward: edges`, the yield is new edges per target execution spent. These executions include the reruns, the minimizer tests and the crash triage of the results.
The weight stays between `min_factor` and `max_factor` times the base weight, because the yield of a component changes as the corpus grows.

### 5.4 Execute

The target is the fuzzing build of d8, which has sanitizer coverage and the REPRL interface. [TUTORIAL.md](TUTORIAL.md) shows how to build it.
Redline starts the target once and sends it many programs over the REPRL protocol (`exec/reprl.h`).
The target gets two control pipes and two data channels.
One data channel carries the script. The other carries the text that the script sends back through a builtin of the fuzzing build (fuzzout).
For each execution, Redline writes the script, sends a command and waits for a status word, the death of the target or the timeout.

| Outcome | Meaning |
|---|---|
| `succeeded` | The script ended with status 0. |
| `failed` | The script ended with an uncaught exception or a nonzero exit status. |
| `crashed` | The target died from a signal. |
| `timed_out` | The script ran longer than the timeout. Redline kills the target. |

- Redline replaces the target after a timeout, after a crash and after `max_execs_before_respawn` executions. This limits the state in one process.
- After an infrastructure failure, Redline tries the execution again once with a fresh target. Ten such failures in a row stop the session.
- A session uses a short timeout (`FuzzConfig::Load` in `engine/config.cc`), because a mutant that runs longer is almost always an endless loop.
- By default, the target decides if a program is valid: an uncaught SyntaxError without a stack frame marks it invalid.
  With `syntax_check`, the V8 parser in the Redline process checks each mutant first.
- The default prelude defines the mjsunit assertion functions so that they do not throw.

**Coverage.** Each target has its own coverage map: POSIX shared memory with the edge count and one bit per edge.
Redline clears the bitmap before each execution and reads it after the execution.
Only successful executions count for coverage, so the corpus holds only programs that run to the end. Crashes have a separate set of crash bits.
All workers share one map of the edges that the session found (`engine/shared_coverage.h`).
A worker claims new edges with atomic operations, so two workers never claim the same edge. If no corpus program results, the worker gives the edges back.

### 5.5 Accept

Accept decides if a candidate enters the corpus. It has two parts:

1. **Criterion.** The `accept` block names the criterion. The only criterion is `new-edges`:
   a result is a candidate if it claims at least one edge that the session did not find before.
2. **Determinism check** (`stages.check`). The target is not deterministic, because concurrent compilation and garbage collection change the edges of a run.
   Thus Redline runs the candidate again:
   - It runs the candidate at least `min_reruns` times, and until the set of new edges stays the same for `min_stable_reruns` runs.
   - An edge that a rerun does not reach leaves the set. Redline gives the edge back, so that a later program can claim it.
   - An edge that fails the check `flaky_threshold` times stays claimed. It costs no more reruns, and it does not count as coverage.
   - If a rerun fails, or if the set does not settle within `max_reruns` runs, Redline drops the candidate.

**The oracle.** When Accept admits a program, it also sets the property that Minimize must keep.
For `new-edges`, the property is "the program runs without an error and reaches all its new edges that survived the check".
Accept computes this property once and freezes it. The property does not depend on the corpus at a later time.

This rule keeps Minimize generic. Minimize sees only an oracle, not the criterion.
Thus a reducer must work with any oracle, and a new criterion must supply its own frozen oracle.

### 5.6 Minimize

Minimize makes a program smaller while its oracle stays true.
For a crash that reproduces alone, the oracle is "the program crashes with the same signature", so the minimizer cannot move to a different bug.

| Reducer | What it removes or replaces |
|---|---|
| `statements` | chunks of statements of halving size from the end of each statement list, down to single statements |
| `unwrap` | a block, an `if`, a loop, a `try` or an immediately invoked function, replaced by its body |
| `lists` | call arguments, array elements, object properties, class members, declarators and parameters |
| `expressions` | an expression, replaced by one of its operands or by a constant |
| `truncate` | all top-level statements after the shortest prefix that keeps the oracle true |
| `dataflow` | the top-level statements that the rest of the program does not use, from a def-use graph of the scope analysis |

The default order is `statements`, `unwrap`, `lists`, `expressions` (`Minimizer::DefaultOrder`). `stages.minimize.reducers` sets another list.
The def-use graph only proposes a cut. The oracle decides, because globals, prototypes or getters can connect statements that share no binding.

- The minimizer runs its reducers in rounds until a round changes nothing or the budget ends.
  A reducer edits the tree and asks the oracle. If the oracle says no, the reducer undoes the edit.
- The budget limits the tests, the time and the rounds. It has fewer tests for slow programs (`target_time_ms`).
  Crashes have a larger budget (`crash_minimizer`), because they are rare.
- Each test has a short timeout from the run time of the program (`probe_timeout_*`), because a reduction can make an endless loop.
  For the same reason, `expressions` replaces a loop condition only with `0`.
- The result must pass the oracle again in one of `verify_attempts` runs. If it does not, Redline keeps the original program.

### 5.7 Publish

Publish sets the new edges and the run time of the program.
If a strategy asks for the full edge set, Publish runs the program two more times and keeps the edges that both runs reach.
Then it adds the program to the shared corpus, which gives the program its id.
It writes `OUT/corpus/NNNNNN.js` and a `corpus_add` event.

### 5.8 Crash triage

A crash can come from a result, from an imported file or from an instrumented program.

1. **Signature.** For a V8 fatal error, the signature is the file, the line and the check message.
   For a sanitizer report, it is the kind of error and the first function. For a signal, it is the signal and the first three named frames (`engine/crash.h`).
   For a stack overflow, the functions that repeat in the trace name the crash, because the first frames depend on where the stack ran out.
2. **Budget.** Redline saves at most `max_crashes_per_signature` crashes of one signature, and minimizes at most `minimize_crashes_per_signature` of them.
3. **Reproduction.** Redline runs the program again in a fresh target, up to `crash_reruns` times.
4. **History replay.** If the crash does not reproduce alone, Redline replays the earlier programs of the same target, and then the crash.
   If the crash then occurs, a binary search finds the smallest number of programs from the end of this history that cause it.
5. **Amplify.** If the crash does not reproduce alone, the amplify step searches for d8 flags that make it reproduce.
   This also applies after a successful replay, because an earlier program can turn on a feature that a d8 flag can also turn on.
6. **Minimize.** Redline minimizes a crash that reproduces alone, against its signature.
7. **Save.** Redline writes the program, the report and, if they exist, the original program and the history to `OUT/crashes/`.

History replay and the amplify step run only for crashes that Redline saves.

**Amplify.** Some crashes depend on timing, for example on a profiler signal or a GC at one moment.
One run in a fresh target does not show them, and ClusterFuzz cannot reproduce them either.
The amplify step searches for a plain d8 command line that reproduces the crash reliably, because ClusterFuzz accepts a testcase together with its d8 flags.
The `flags` strategy (`engine/amplifier.cc`) works like this:

- The candidates are the flags of the stack rules that match the crash output, and then no extra flags.
  For example, a rule adds the CPU profiler flags for profiler frames.
- Each candidate runs fresh d8 processes with `--stress-runs` and a new `--random-seed`. It succeeds when `min_hits` of `processes` runs crash with the same signature.
- d8 stops the stress loop at the first uncaught exception. Thus, if the program ends with an uncaught exception,
  the candidates run on a copy with all statements in one try statement (`wrap`).
- The amplify step runs at most `per_signature` times for one signature in a session.
- The report gets the flags in `amplified_args`. A tool that uploads the crash must send these flags with the testcase.

The `none` strategy turns the amplify step off.

## 6. Data flow

- A **program** (`core/program.h`) holds the FAST tree, the printed source, a hash of the source and the lineage.
  Seed strategies keep their state in the program under the corpus lock.
- An **execution** (`exec/execution.h`) holds the outcome, the exit code or the signal, the run time, and the text from fuzzout, stdout and stderr.
  The coverage bitmap stays in the coverage map of the worker until the next execution.
- The **edges** of a candidate are the new edges that its worker claimed. After Accept, the edges that survived become the edges of the program.

A stage reads only the data that it needs. A strategy that needs optional data declares it, and the engine computes the data only in that case.
For example, `WantsHitEdges` asks for the full edge set. A strategy has a fallback when the data is missing.

## 7. Strategies and registries

The four selection points (seeds, partners, mutators and generators) use one interface, `Selector<T>`, and one registry template, `SelectorRegistry<T>` (`core/selection.h`).

- A strategy is one C++ class and one registration line. The registry keeps its name, its documentation and its parameters (`ParamSpec`).
- At startup, the configuration loader builds each configured strategy once.
  An unknown strategy, parameter or weight name, or a wrong type, stops the session before any target starts.
- The registry writes the strategy part of `redline fuzz --help-config`.
- Strategies get feedback through hooks: `OnAdded` (a new corpus program), `OnOutcome` (the result of a pick) and `OnCrashEdges` (the edges of a reproducible crash).
- All workers share the mutator and generator strategies. Such a strategy must keep no state between calls, or it must synchronize its state.

```json
{"selection": {"seeds": {"strategy": "rarity"}, "partners": {"strategy": "rarity-partner"},
               "mutators": {"strategy": "adaptive", "params": {"weights": {"Splice": 4}}}}}
```

Some parts are pluggable without this registry. The amplify strategies come from `CreateAmplifyStrategy` (`engine/amplifier.cc`), with the same `ParamSpec` declarations.
The loader checks the names of the Init strategies and Accept criteria (`ReadInitConfig`, `ReadAcceptConfig`).
The minimizer finds reducers by name (`stages.minimize.reducers`).

## 8. Configuration and presets

One JSON configuration describes a session. `FuzzConfig` (`engine/config.h`) defines the keys and their defaults.
`redline fuzz --dump-config` prints the complete effective configuration. It needs no target, and the loader reads its output back to the same configuration.

The configuration comes from four layers. A later layer wins:

1. the built-in defaults,
2. the preset (`--preset`),
3. the configuration of `--config`: a file, or inline JSON that starts with `{`,
4. the command-line flags.

Inline JSON can set any key, so a key does not need its own flag. In one JSON object, the `stages` block wins over the older flat keys.
The loader checks the names inside the blocks: stages, capture triggers, Init, Accept, amplify, strategies, parameters, weights, tunables and reducers.
A wrong name stops the session before any target starts.
The loader ignores unknown top-level keys, and unknown keys in some blocks, for example in `selection` and in `crash_minimizer`.

**Presets.** `--preset=NAME` loads `presets/NAME.json`, and `--preset=FILE.json` loads any file.
A preset can add d8 flags for its area (`d8_extra_args`) and change the weights of generators and mutators.
Each preset selects `rarity`, `rarity-partner` and `adaptive`. Without a preset, the defaults are `uniform`, `uniform-partner` and `weighted`.

| Preset | d8 flags added | Higher weights |
|---|---|---|
| `default` | none | none |
| `maglev` | `--maglev --no-turbofan --jit-fuzzing` | JIT, deoptimization, OSR, elements-kind and shape generators |
| `turbolev` | `--turbolev --turbolev-future --jit-fuzzing` | the same generators as `maglev` |
| `runtime` | `--no-turbofan --no-maglev --no-sparkplug` | property, prototype, proxy, freeze and shape generators. The JIT generators are off. |
| `parser` | none | class, destructuring, arrow, template, label, `using`, `eval` and `with` generators, and `Splice` and `Combine` |
| `builtins` | none | discovered-API, builtin-call, method-call, string, typed-array, collection and worker generators |
| `wasm` | none | `WasmModule`, and JIT, OSR and typed-array generators |

The `WasmModule` generator builds WebAssembly modules that are valid by construction.
Its default weight is 0, so a session without the `wasm` preset generates no WebAssembly modules.
A preset must not raise the weight of a generator that resembles one known bug (section 10 tells why).

## 9. FAST

FAST is the typed JavaScript AST of Redline (`fast/`). Redline does not use the AST of V8 for two reasons.
The V8 parser desugars some constructs before its AST is visible, for example the `let` bindings of `for` loops.
Also, the AST of V8 lives in a Zone that belongs to an Isolate.

### 9.1 The tree and node ids

- The tree has the shape of ESTree, the AST format of tools such as acorn and esprima.
  Each literal type has its own node kind, and so do `%Name(...)` runtime calls. The printer derives the parentheses from precedence.
- One X-macro list in `fast/ast.h` defines all node kinds. `ForEachChildSlot` (`fast/ast_visit.h`) has no default case, so the compiler reports a new kind that it does not handle.
- Nodes are plain structs in an arena that belongs to one program. The arena owns all nodes, so a mutator can detach a subtree and forget it.
- Each node has an id that is unique in its tree. `Tree::Copy` keeps the ids, so an id that the shell shows stays valid in the next mutation step.
  The ids are dense, so analyses keep facts per node in vectors indexed by id, not in hash maps.

### 9.2 Parser, printer and syntax oracle

- The parser (`fast/parser.cc`) is a recursive-descent parser for scripts: ES2024, plus the V8 extensions that tests use. It does not parse modules.
- The parser does not report early errors that keep the tree shape, for example duplicate declarations. V8 is the validity oracle instead.
  The parser must reject what FAST cannot represent, and it must never make a tree that prints to a program with a different meaning.
- The printer (`fast/printer.cc`) always writes semicolons. Its output parses back to a tree that prints to the same text.
- The syntax oracle (`SyntaxChecker` in `base/v8_runtime.h`) compiles a script but never runs it. The target executes each program.

### 9.3 Scope analysis and the Builder

- The scope analysis (`fast/scope.h`) resolves each identifier to its binding. Each mutation that moves, removes or adds code uses it.
  It is a static approximation of the specification. It marks scopes with direct `eval` or `with` as dynamic, and it marks bindings in cases that it does not model, for example Annex B.
- `redline roundtrip --rename` tests the analysis. It renames each binding that it can rename safely, and checks that the program behaves the same.
- Mutators and generators edit a program through a `Builder` (`core/builder.h`): a copy of the tree, its analyses and edit operations.
  The Builder knows the syntactic context of each position and the visible variables, each with a coarse type guess. The guess is not a type system.
- `EscapingBindings` finds the declarations that a removal or a move would separate from their uses. `Remove`, `Wrap` and `Reorder` use it.

### 9.4 New JavaScript features

The goal: when V8 gets a new JavaScript feature, FAST must represent it and Redline must generate it.

A compile-time check cannot enforce this goal. A new feature often adds no V8 AST node type, and V8 AST nodes do not map one to one to syntax.
Two tests enforce the goal instead, at the point where V8 engineers already add tests:

1. **FAST must represent the feature.** A new feature comes with `test/mjsunit` tests.
   `redline roundtrip --dir=test/mjsunit` parses, prints and runs each file. It fails until FAST parses and prints the new syntax.
2. **Redline must generate the feature.** `redline selftest --offline` fails if a FAST node kind comes out of no generator.
   The exceptions are on a list with a reason each (`kUngeneratedKinds` in `commands/selftest_selection.cc`).
   The check also fails when a listed kind starts to appear, so that the list stays current.

## 10. Checks

These checks protect the design. [CONTRIBUTE.md](CONTRIBUTE.md) tells when to run them.

- `redline selftest --offline` needs no target. It checks the golden traces, the configuration, the crash signatures, the FAST node kinds and the presets.
- `redline selftest --d8=PATH` also checks the execution layer against a real target.
- `redline roundtrip --dir=test/mjsunit` checks that V8 and FAST agree on validity, and that each printed program behaves like the original.
- `redline audit` checks generators for overfitting. It generates programs with each generator and runs them in a d8 with known bugs.
  It fails if one crash signature takes more than `--threshold` percent of the programs of one generator.

Such a generator is a template of one bug. It finds that bug again, and little else.
A lower weight does not solve the problem: the weight changes how often the generator runs, not what its programs do. Make the generator more general instead.

The V8 bots build Redline, because the root `BUILD.gn` adds it to `gn_all` on Linux. The bots do not run these checks.

## 11. Measuring a change

[CONTRIBUTE.md](CONTRIBUTE.md) shows how to compare two sessions.

## 12. Threading and memory

A session is one process. The main thread loads the configuration, starts the targets and starts one thread per worker (`jobs`).
Then it only checks the stop conditions and writes the statistics.
A worker owns its target, its coverage map, its random generator, its isolate for the syntax oracle and its crash history.

| Shared state | Protection |
|---|---|
| corpus, and the seed and partner strategies | a mutex. Programs do not change, so a worker uses a program without the lock. |
| seen edges and crash edges | atomic operations on 64-bit words |
| mutator and generator strategies | no state between calls, or a mutex of their own (`adaptive`) |
| flaky-edge counts, crash bookkeeping, event log | a mutex each |
| counters | atomic counters |

- Each worker seeds its random generator from the session seed and its worker number.
  With one worker and a deterministic target, a session with the same seed repeats the same steps.
  Time limits can change the steps, for example the time budget of the minimizer and the time window of `adaptive`.
- Each tree has its own arena. The arena takes memory in segments from a pool of its thread, and gives them back when the tree dies.
  Thus a tree costs only bump allocation.
- Programs are immutable and shared (`std::shared_ptr<const Program>`). A mutation copies the tree.
- The corpus keeps each program for the whole session, so memory grows with the corpus.
- All isolates of the syntax oracle share one pointer-compression cage. Thus each isolate has small heap limits and compiles lazily.

## 13. Design decisions

| Decision | Reason |
|---|---|
| Mutate a typed AST of plain JavaScript. | Redline imports any script that V8 parses, without loss. The scope analysis keeps edits valid. |
| Keep the parser lenient, with V8 as the validity oracle. | FAST and V8 cannot disagree about validity. |
| Keep the target alive over REPRL, and replace it from time to time. | A new process for each program is slow. A process that lives for a long time collects state. |
| Check candidates for determinism, and suppress flaky edges. | Without the check, noise fills the corpus. Without the suppression, an edge that never settles costs reruns forever. |
| Freeze the oracle at acceptance. | Minimize and the reducers stay independent of the criterion. |
| Group crashes by signature, with a budget per signature. | The work per bug stays bounded. |
| Replay the history of a crash, and search for amplifying flags. | Some crashes depend on earlier programs or on timing. |
| Put the prelude in front of each execution, and never store it. | Corpus files stay clean, and imported tests run without `mjsunit.js`. |
| Run all workers in one process, with one corpus and one coverage map. | The workers need no protocol to exchange programs. |
| Keep the pipeline fixed, and make the decisions pluggable. | The loop stays easy to read, and a decision can change without a change to the loop. |
