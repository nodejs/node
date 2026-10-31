# Redline tutorial

This tutorial is for V8 developers who want to fuzz the part of V8 that they work on.

Redline is a coverage-guided JavaScript fuzzer for V8. It runs a fuzzing build of d8 as a child process.
It changes JavaScript programs, runs them in d8, and keeps the programs that reach new code in V8.

## Goal

At the end of this tutorial, you have these items:

- a fuzzing d8 in `out/fuzzbuild/d8`,
- a Redline binary in `out/redline/redline`,
- your own configuration file for one area of V8,
- a fuzzing session that runs your configuration against your d8,
- a procedure to reproduce and minimize a crash.

This tutorial uses typed arrays as the example area.
For your area, do the same steps with your own components and seed files.

You need a Linux x64 workstation, because Redline builds only on Linux.
You also need a V8 checkout, and depot_tools in your `PATH`.

Run all commands from the root of your V8 checkout.
The example files of this tutorial go into `/tmp/redline-tutorial`:

```shell
mkdir -p /tmp/redline-tutorial
```

The output samples come from real runs.
Values that change from run to run, for example times, edge counts and rates, show as `<n>`.

Contents:

1. [Build the fuzzing d8](#1-build-the-fuzzing-d8)
2. [Build Redline](#2-build-redline)
3. [Run one program](#3-run-one-program)
4. [Examine the syntax tree and the scopes](#4-examine-the-syntax-tree-and-the-scopes)
5. [Try the components](#5-try-the-components)
6. [Presets and configuration layers](#6-presets-and-configuration-layers)
7. [Fuzz your area](#7-fuzz-your-area)
8. [Next steps](#8-next-steps)

## 1. Build the fuzzing d8

Redline does not execute JavaScript in its own process.
A d8 with coverage instrumentation executes every program. This d8 is the *target*.
Build the target:

```shell
gn gen out/fuzzbuild --args='is_debug=false dcheck_always_on=true v8_enable_debug_code=true v8_static_library=true v8_enable_verify_heap=true v8_fuzzilli=true v8_enable_backtrace=true v8_enable_partition_alloc=false v8_enable_builtins_profiling=true sanitizer_coverage_flags="inline-bool-flag" target_cpu="x64" use_remoteexec=true'
autoninja -C out/fuzzbuild d8
```

If you do not have remote build execution, remove `use_remoteexec=true` from the arguments.

| Argument | What it does |
|---|---|
| `is_debug=false` | Makes a release build. d8 runs faster, so Redline runs more programs. |
| `dcheck_always_on=true` | Keeps the `DCHECK` assertions in the release build. A failed `DCHECK` is a crash for Redline. |
| `v8_enable_debug_code=true` | Makes the d8 flag `--debug-code` available. This flag adds assertions to the code that V8 generates. |
| `v8_static_library=true` | Builds the V8 libraries as static libraries instead of source sets. |
| `v8_enable_verify_heap=true` | Compiles the heap verifier (`VERIFY_HEAP`) into d8. |
| `v8_fuzzilli=true` | Adds the REPRL loop and the shared-memory coverage map to d8. Redline uses both. |
| `v8_enable_backtrace=true` | Makes d8 print a stack trace when it crashes. Redline uses the top frames for crash signatures. |
| `v8_enable_partition_alloc=false` | Makes d8 use the system allocator instead of PartitionAlloc. |
| `v8_enable_builtins_profiling=true` | Makes d8 report the basic blocks of the builtins as additional coverage edges. |
| `sanitizer_coverage_flags="inline-bool-flag"` | Gives each edge of the C++ code a flag byte. After each execution, d8 copies the flags into the coverage map. |
| `target_cpu="x64"` | Builds for x64. |
| `use_remoteexec=true` | Uses remote build execution. |

REPRL (read, eval, print, reset loop) keeps one d8 process alive.
Redline sends a program, d8 runs it and reports the result, and then d8 waits for the next program.

## 2. Build Redline

Redline is a normal V8 executable. It needs no coverage instrumentation.
Build it in its own output directory:

```shell
gn gen out/redline --args='is_debug=false target_cpu="x64" use_allocator_shim=false use_remoteexec=true'
autoninja -C out/redline redline
```

If you do not have remote build execution, remove `use_remoteexec=true` from the arguments.
`use_allocator_shim=false` keeps the system `malloc`.
With the allocator shim, all worker threads of Redline share one PartitionAlloc lock and lose CPU time to it.

Keep the binary in `out/redline`.
Redline looks for its presets and its test data in `experimental/redline`, two directories above its binary.
`out/redline/redline help` lists the commands. `out/redline/redline help <command>` lists the flags of one command.

### 2.1 Check the two binaries

Run the checks that need no target:

```shell
out/redline/redline selftest --offline
```

```text
...
PASS  presets: maglev.json loads
...
selftest: <n> passed, 0 failed
```

Then run the checks against the fuzzing d8:

```shell
out/redline/redline selftest --d8=out/fuzzbuild/d8
```

```text
...
PASS  REPRL handshake and coverage initialization
INFO  instrumented edges: <n>
PASS  empty script succeeds: succeeded (exit code 0, signal 0, <n> ms)
PASS  uncaught exception is reported as failed: failed (exit code 1, signal 0, <n> ms)
...
selftest: <n> passed, 0 failed
```

The second command checks the execution layer: the REPRL handshake, the outcomes, the output capture, the coverage and the restart of d8.
If a check fails, examine the build of the target first.

## 3. Run one program

Write this program to `/tmp/redline-tutorial/first.js`:

```javascript
function sum(ta) {
  let s = 0;
  for (let i = 0; i < ta.length; i++) s += ta[i];
  return s;
}
const ta = new Uint8Array([1, 2, 3]);
print(sum(ta));
```

Run it three times in the target:

```shell
out/redline/redline exec --d8=out/fuzzbuild/d8 --repeat=3 --print-output /tmp/redline-tutorial/first.js
```

```text
[<time> I] [exec] Target: out/fuzzbuild/d8 --expose-gc --expose-externalize-string --omit-quit --allow-natives-syntax --fuzzing --harmony --experimental-fuzzing --js-staging --wasm-staging --wasm-fast-api --expose-fast-api --wasm-test-streaming
[<time> I] [exec] <n> edges instrumented
/tmp/redline-tutorial/first.js [run 1]: succeeded in <n> ms, <n> edges hit, <n> new, <n> found total (<n>%)
---- stdout (2 bytes) ----
6
...
/tmp/redline-tutorial/first.js: <n> edges hit in every run, <n> in at least one run (<n> non-deterministic)
[<time> I] [exec] 3 executions: 3 succeeded, 0 failed, 0 crashed, 0 timed out; 1 spawns
```

The output shows what the fuzzer sees:

- The first line shows the d8 flags. `--d8-extra-args="..."` adds flags. `--d8-args="..."` replaces all of them.
- An *edge* is a transition between two basic blocks of V8 code. Coverage is the set of edges that successful executions reach.
- Some edges change from run to run, because of concurrent GC and compiler threads. Thus Redline runs a program again before it trusts its new edges.

| Outcome | Meaning |
|---|---|
| `succeeded` | The program ran to its end. |
| `failed` | The program threw an uncaught exception. Its edges do not count as coverage. |
| `crashed` | d8 stopped with a signal, for example after a failed `DCHECK`. |
| `timed_out` | The program did not stop before the timeout. |

## 4. Examine the syntax tree and the scopes

Redline does not edit program text. It parses each program into FAST, its typed syntax tree in the shape of ESTree.
The generators and mutators change the tree, and a printer writes the tree back as JavaScript.

Print the program as Redline prints it, with node ids:

```shell
out/redline/redline parse --print --ids /tmp/redline-tutorial/first.js
```

```text
/*n1*/ function sum(ta) {
  let s = 0;  // n5
  /*n13*/ for (let i = 0; i < ta.length; i++)
    s += ta[i];  // n21
  return s;  // n27
}
const ta = new Uint8Array([1, 2, 3]);  // n29
print(sum(ta));  // n38
```

Each node of the tree has an id. A node keeps its id until a mutation removes the node.
`--ids` shows the ids of the statements only. A statement on one line gets `// nN` at its end.
A statement that contains other statements, for example a function or a loop, gets `/*nN*/` in front.

Print the scope analysis:

```shell
out/redline/redline parse --scopes /tmp/redline-tutorial/first.js
```

```text
script scope n0 (Program)
  function sum (n2): 1 refs, pinned [n40]
  const ta (n31): 1 refs, pinned [n41]
  function scope n1 (FunctionDeclaration)
    param ta (n3): 2 refs [n15 n24]
    let s (n7): 2 refs, assigned [n22 n28]
    block scope n13 (ForStatement)
      let i (n11): 3 refs, assigned [n14 n19 n26]
globals: Uint8Array print
...
```

A scope line gives the kind of the scope and the id of the node that makes the scope.
A binding line, for example `let s (n7): 2 refs, assigned [n22 n28]`, has these parts:

- `let s`: the kind and the name of the binding.
- `(n7)`: the id of the identifier node that declares the binding.
- `2 refs [n22 n28]`: the number of references, and the ids of the identifier nodes that reference the binding. The list shows at most eight ids.
- `assigned`: the program assigns a new value to the binding.
- `pinned`: Redline does not rename the binding, because a rename can change the behavior. For example, code in `eval` can reach a script-level binding by its name.

The ids in the binding lines are ids of identifier nodes inside statements.
`--ids` prints only the statement ids, so you do not find most of these ids in the printed source.
No `parse` flag prints the id of every node. The `tree` command of the shell prints them:

```shell
printf 'tree 38\n' | out/redline/redline shell /tmp/redline-tutorial/first.js
```

```text
...
redline> tree 38
ExpressionStatement #38
  CallExpression #43  print(sum(ta))
    Identifier #39  print
    CallExpression #42  sum(ta)
      Identifier #40  sum
      Identifier #41  ta
(6 nodes in the subtree)
```

`n40` and `n41` are the references of `sum` and `ta` in the binding lines above.
`tree` without an id prints the whole program.

The scope analysis keeps the mutations plausible.
Generated code uses only the variables that are visible and initialized at its position.

## 5. Try the components

Redline has three kinds of components:

- A *generator* writes new code, for example a typed array declaration.
- A *mutator* changes a program, for example it replaces a variable or an operator. The `CodeGen` mutator inserts the code of the generators.
- A *reducer* makes a program smaller while a condition, the *oracle*, stays true.

*Strategies* make the decisions of a session: the next corpus program, its partner program, the mutator and the generator.

### 5.1 List the components

```shell
out/redline/redline mutate --list          # mutators and generators, with their default weights
out/redline/redline minimize --list        # reducers
out/redline/redline fuzz --help-config     # the configuration format, and the strategies with their parameters
out/redline/redline fuzz --help-tunables   # the numbers inside the components
```

A weight is a relative probability.
The selection strategies do not pick a component with weight 0, but the commands of this section can apply it.

Find the components for your area:

```shell
out/redline/redline mutate --list | grep -i typed
```

```text
  TypedArrayDecl     w=1    let ta = new XArray(...)
  TypedArrayOps      w=1.5  typed array element access, detach, views
```

### 5.2 Generators

Show the code that a generator writes:

```shell
out/redline/redline mutate --generate --generator=TypedArrayOps --n=3 --print
```

```text
----- TypedArrayOps #0: 2 statements -----
let ta0 = new Uint8Array(["0", "constructor", "true", "a"]);
ta0.set(ta0);
----- TypedArrayOps #1: 2 statements -----
const ta0 = new Uint8ClampedArray(1174);
const v0 = ta0.byteLength;
...
```

`--generate` starts from an empty program.
Then run the results in the target:

```shell
out/redline/redline mutate --generate --generator=TypedArrayDecl,TypedArrayOps --n=20 --run --d8=out/fuzzbuild/d8 --quiet
```

The table has one line for each generator.
`executed` counts the programs that ran, and `ok%` is the share of programs that succeeded.
`fail`, `tmo` and `crash` count the failures, the timeouts and the crashes.
`new` counts the programs that reached new edges.

A program that fails stops at the exception, so the code after it does not run.
Give a high weight only to generators whose programs usually succeed.

### 5.3 Mutators

Apply two mutators to your program:

```shell
out/redline/redline mutate --mutator=Input,Operation --n=2 --seed=10 --print /tmp/redline-tutorial/first.js
```

```text
----- Input #0: input i -> ta -----
function sum(ta) {
  let s = 0;
  for (let i = 0; ta < ta.length; i++)
...
----- Operation #0: number 0 -> -1 -----
function sum(ta) {
  let s = (-1);
...
```

The header of each result names the mutator and the change.
With `--seed=S`, sample `i` uses the seed `S+i`. The same seed gives the same result on the same program.
`Splice` and `Combine` need a second program. Give it with `--partner=FILE`.

### 5.4 The shell

The shell is a workbench for one program.
Apply generators, mutators and reducers one at a time, run the result, and undo a step.

```shell
out/redline/redline shell --d8=out/fuzzbuild/d8 /tmp/redline-tutorial/first.js
```

```text
redline> generate TypedArrayOps 1
=> TypedArrayOps (seed 1): inserted 1 statement(s): 50 nodes, V8 accepts
+   ta.set(ta);
redline> run
succeeded in <n> ms, exit 0; <n> edges
redline> generate JitOptimize 2
=> JitOptimize (seed 2): inserted 6 statement(s): 71 nodes, V8 accepts
+ %PrepareFunctionForOptimization(sum);
+ sum();
...
redline> run
failed (uncaught exception) in <n> ms, exit 1; <n> edges (+<n> new, -<n> gone since the previous run)
  stdout: fuzzcode.js:71: TypeError: Cannot read properties of undefined (reading 'set')
...
redline> undo
undid generate JitOptimize seed=2; 50 nodes
```

The second argument of `generate` and `mutate` is the seed.
The first generator used the parameter `ta`, because `ta` is visible at the insertion point.
The second generator added a call `sum()` without an argument, so `ta.set(ta)` threw a `TypeError`.
Type `help` in the shell for all commands.
The shell reads its commands from stdin, so you can also give them with `printf`, as in section 4.

### 5.5 Reducers

Minimize your program, and keep its output:

```shell
out/redline/redline minimize --d8=out/fuzzbuild/d8 --oracle=output /tmp/redline-tutorial/first.js
```

```text
function sum() {
  let s = 0;
  for (let i = 0; i < ta.length; i++)
    s += ta[i];
  return s;
}
const ta = [1, 2, 3];
print(sum());
```

The program still prints `6`.
The reducers removed the parameter and the argument, and they replaced the typed array by an array.
A reducer keeps only what its oracle measures.

| Oracle | The reduced program must |
|---|---|
| `edges` (default) | reach the same stable edges. With `--baseline=FILE`, only the edges that `FILE` does not reach count. |
| `crash` | crash. Any crash is sufficient. |
| `fail` | throw the same uncaught exception, with the same type and message. |
| `output` | have the same outcome and print the same output. |

`--reducer=NAME,...` runs only these reducers, in this order.
Without it, `minimize` runs all reducers.
A session runs only `statements`, `unwrap`, `lists` and `expressions`, unless the configuration key `stages.minimize.reducers` names other reducers.

### 5.6 Selection strategies

`fuzz --help-config` lists the strategies for the four decisions of a session:

| Decision | Configuration key | Strategies |
|---|---|---|
| the next corpus program (the seed) | `selection.seeds` | `uniform` (default), `recency`, `tournament`, `productive`, `rarity` |
| the partner for `Splice` and `Combine` | `selection.partners` | `uniform-partner` (default), `rarity-partner` |
| the mutator | `selection.mutators` | `weighted` (default), `adaptive` |
| the generator | `selection.generators` | `weighted` (default), `adaptive` |

- `rarity` prefers small programs that reach rare edges and that Redline used less often. Use `rarity-partner` with it.
- `weighted` uses the weights as they are.
- `adaptive` moves the weight of each component toward its recent yield of new edges per execution. A weight stays between `min_factor` and `max_factor` times its base weight.

All built-in presets use `rarity`, `rarity-partner` and `adaptive`.
The seed and partner strategies work only in a session.
The flag `--scheduler=NAME` of `fuzz` sets the seed strategy for one session.

## 6. Presets and configuration layers

A *preset* is a JSON configuration for one area of V8, for example `maglev`, `runtime` or `wasm`.
The built-in presets are in `experimental/redline/presets/`.

```shell
cat experimental/redline/presets/maglev.json
```

```json
{
  "d8_extra_args": ["--maglev", "--no-turbofan", "--jit-fuzzing"],
  "selection": {"seeds": {"strategy": "rarity"}, "partners": {"strategy": "rarity-partner"}, "mutators": {"strategy": "adaptive"},
    "generators": {"strategy": "adaptive", "params": {"weights": {
      "JitOptimize": 8, "Deopt": 3, "OsrLoop": 3, "ElementsKind": 4, "ShapeChange": 3}}}}
}
```

- `d8_extra_args` adds flags to the d8 flags, like `--d8-extra-args`.
- `selection` sets the strategies. `weights` sets the base weight of the named generators. The other generators keep their default weights.

`--preset=maglev` loads `presets/maglev.json`. `--preset=FILE.json` loads any file.

Redline makes the effective configuration from four layers. A later layer wins:

1. the built-in defaults,
2. the preset (`--preset=NAME` or `--preset=FILE`),
3. the configuration (`--config=FILE`, or inline JSON such as `--config='{"jobs": 4}'`),
4. the command-line flags.

These rules apply when the layers merge:

- Each layer adds its `d8_extra_args` to the d8 flags. The flag `--d8-extra-args` adds more flags.
- A `params` object in `selection` replaces the `params` object of an earlier layer. For example, the `weights` of your file replace all the weights of the preset.
- The flags `--generator-weights`, `--mutator-weights`, `--disable-generator` and `--disable-mutator` change single weights. The other weights stay.
- The flag `--import` adds files to the seed files of the configuration.

Print the effective configuration:

```shell
out/redline/redline fuzz --preset=maglev --dump-config
```

The output lists every key, every weight and every tunable with its value.
The command needs no target.
You can edit the output and give it back with `--config`.

## 7. Fuzz your area

### 7.1 Select the components and the seed files

Use sections 5.1 to 5.3 for your area. For typed arrays, this tutorial uses:

- the generators `TypedArrayDecl` and `TypedArrayOps`, and also `JitOptimize` and `Deopt` for the optimizing compilers,
- some typed array tests of `test/mjsunit` as seed files,
- the d8 flag `--jit-fuzzing`, which lowers the JIT tiering thresholds,
- the strategies of the built-in presets.

Redline imports `.js` files and directories. It replaces each `load()` call of a test by the loaded file.
At execution time, Redline puts a small prelude before each program.
The prelude defines the mjsunit assertion functions, which never throw, and empty `load` and `quit` functions.

### 7.2 Write the configuration file

Write this file to `/tmp/redline-tutorial/typedarray.json`:

```json
{
  "d8_extra_args": ["--jit-fuzzing"],
  "init": {"strategy": "import", "params": {"paths": [
    "test/mjsunit/es6/typedarray.js",
    "test/mjsunit/es6/typedarray-detached.js",
    "test/mjsunit/es6/typedarray-set-length.js",
    "test/mjsunit/compiler/typedarray-keyed.js",
    "test/mjsunit/compiler/typed-array-constructor.js"
  ]}},
  "selection": {
    "seeds": {"strategy": "rarity"},
    "partners": {"strategy": "rarity-partner"},
    "mutators": {"strategy": "adaptive"},
    "generators": {"strategy": "adaptive", "params": {"weights": {
      "TypedArrayDecl": 6, "TypedArrayOps": 8, "JitOptimize": 6, "Deopt": 2}}}
  },
  "tunables": {"generators.TypedArrayOps.max_args": 3}
}
```

| Key | Meaning |
|---|---|
| `d8_extra_args` | The d8 flags for your area. Redline adds them to its default d8 flags. |
| `init` | The seed files and directories. Redline keeps a seed file only if it reaches new edges. |
| `selection` | The strategies and the generator weights. The other generators keep their default weights. |
| `tunables` | The numbers inside the components. Here, a typed array method call gets up to three arguments. |

The paths of the seed files are relative to the directory where you start Redline.
Keep the session settings, for example `--jobs` and `--out`, on the command line.
Then the same file can also be a preset (section 7.9).

### 7.3 Check the configuration

Print the effective configuration, and look at your values:

```shell
out/redline/redline fuzz --config=/tmp/redline-tutorial/typedarray.json --dump-config > /tmp/redline-tutorial/effective.json
grep -E '"(TypedArrayDecl|TypedArrayOps|JitOptimize|Deopt)"|TypedArrayOps.max_args' /tmp/redline-tutorial/effective.json
```

```text
          "TypedArrayDecl": 6,
          "TypedArrayOps": 8,
          "JitOptimize": 6,
          "Deopt": 2,
    "generators.TypedArrayOps.max_args": 3,
```

Also make sure that `d8_args` ends with `--jit-fuzzing`, and that `init.params.paths` lists your seed files.

Redline checks the names in the `init`, `selection`, `stages` and `tunables` blocks before it starts d8.
A wrong name stops the command with an error, for example:

```text
[<time> E] [fuzz] selection.generators: unknown generator in weights: TypedArrayOp
```

Redline does not check the top-level keys.
It ignores a top-level key that it does not know, for example `"jobz"`.
It also ignores `d8_extra_args` if the value is not a list.
Thus, always compare the output of `--dump-config` with your file.

### 7.4 Run the session

```shell
out/redline/redline fuzz --d8=out/fuzzbuild/d8 --config=/tmp/redline-tutorial/typedarray.json --jobs=4 --duration=120 --out=/tmp/redline-typedarray
```

- `--jobs=N`: the number of worker threads. Each worker runs its own d8 process.
- `--duration=SEC`: stop after `SEC` seconds. Without this flag, the session runs until you stop it.
- `--out=DIR`: the output directory. Use a new directory for each session. Redline adds to an existing `events.jsonl` and writes over old corpus files.

Start with a short session, as above.
When the session works, start a long session without `--duration`.
For an unknown flag, `fuzz` shows a warning, for example `unknown flag --jobz (ignored)`.

### 7.5 Read the progress output

```text
[<time> I] [fuzz] environment: <n> globals/namespaces with <n> members discovered in the target
[<time> I] [fuzz] session: 4 worker(s), seed <n>, <n> edges in target, 5 file(s) to import, seeds rarity, partners rarity-partner, mutators adaptive, generators adaptive, output /tmp/redline-typedarray
[<time> I] [fuzz] import done: 5 files, 5 added, 0 redundant, 0 failed; coverage <n>%, corpus 5
[<time> I] [fuzz] 00:00:30 | execs <n> (<n>/s) | cov <n>% (<n> edges) | corpus <n> | ok <n>% fail <n>% timeout <n>% | invalid <n>% noop <n> | crashes 0 (0 sigs, 0 repro) | cands <n> flaky <n> | min <n> tests (<n>% of execs, <n>% of time) | exec <n>% busy | avg exec <n> ms | avg nodes <n> (corpus <n>) (+<n>)
...
[<time> I] [fuzz] final: 00:02:00 | execs <n> (<n>/s) | cov <n>% (<n> edges) | corpus <n> | ...
```

The first lines:

- `environment`: d8 lists its globals at the start. The `DiscoveredApi` generator uses this list.
- `session`: the workers, the seed of the session and the strategies.
- `import done`: the result of the seed files. An `added` file reached new edges. A `redundant` file reached no new edges. A `failed` file threw an exception, timed out or crashed.

Examine the `import done` line first. A seed file that fails does not go into the corpus.

After each interval of `--stats-interval` seconds, Redline prints a status line:

| Field | Meaning |
|---|---|
| `00:00:30` | The time since the start of the session. |
| `execs` | All executions in d8, and the rate per second. |
| `cov` | The edges that successful executions reached, as a share of all edges. |
| `corpus` | The number of programs in the corpus. |
| `ok`, `fail`, `timeout` | The outcomes of the mutated programs. |
| `invalid`, `noop` | The programs with a syntax error, and the mutations that changed nothing. |
| `crashes A (B sigs, C repro)` | The crashed executions, the different crash signatures, and the crashes that reproduced. |
| `cands`, `flaky` | The programs with new edges, and the candidates whose new edges did not stay stable. |
| `min` | The executions of the minimizer, as a share of all executions and of the worker time. |
| `exec ... busy` | The share of the worker time that d8 used. |
| `avg exec`, `avg nodes` | The mean execution time, and the mean size of the mutated programs and of the corpus programs. |
| `(+N)` | The new corpus programs since the last status line. |

After each status line, Redline prints tables for the mutators, the reducers, the stages, the seed strategy and the generators.
`--no-component-stats` removes the tables. `--quiet` removes all periodic output.

### 7.6 Stop the session

Press Ctrl-C one time.
Redline stops the workers, prints the `final:` status line, and writes the `end` event:

```text
[redline] stopping (press Ctrl-C again to abort)
[<time> I] [fuzz] final: 00:00:46 | execs <n> (<n>/s) | cov <n>% (<n> edges) | corpus <n> | ...
```

If you press Ctrl-C a second time, Redline stops immediately. Then the last line of `events.jsonl` can be incomplete.
The signal `SIGTERM` has the same effect as Ctrl-C.

### 7.7 Examine the output directory

```text
/tmp/redline-typedarray/
  corpus/000001.js    one file for each corpus program
  crashes/            the crashes and their reports
  events.jsonl        one JSON object for each event
```

Each corpus file starts with a lineage line, for example:

```text
// redline corpus #120 | origin Input | parent #118 +1 | generation 10 | <n> new edges | 12 nodes | worker 3 | input f0 -> a0; input f0 -> obj2
```

- `origin`: the mutator that made the program, or `import` for a seed file.
- `parent #118 +1`: the mutation chain started from corpus program 118, and this program is the first mutation of the chain.
- `generation`: the number of corpus ancestors of the program.
- The last part tells what the mutator changed.

`events.jsonl` has these event kinds: `start`, `import`, `import_done`, `corpus_add`, `flaky`, `crash`, `stats` and `end`.
The `start` event records the effective configuration, so you can repeat a session.

To continue a session, import its corpus into a new session:

```shell
out/redline/redline fuzz --d8=out/fuzzbuild/d8 --config=/tmp/redline-tutorial/typedarray.json --import=/tmp/redline-typedarray/corpus --import-all --jobs=4 --out=/tmp/redline-typedarray-2
```

`--import-all` keeps all programs that run successfully, also the programs without new edges.

### 7.8 Reproduce and minimize a crash

When a worker finds a crash, Redline does these steps:

1. It runs the program again in a new d8, to find out if the crash reproduces.
2. If the crash does not reproduce alone, Redline runs the earlier programs of the same d8 before it. A crash that needs them is *state-dependent*.
3. If the crash still does not reproduce, Redline searches for d8 flags that make it reproduce. This is the amplify step.
4. Redline minimizes a reproducible crash. The minimized program must give the same crash signature.
5. Redline saves the files, and adds the signature to `crashes/signatures.txt`.

The log shows one line for each saved crash:

```text
[<time> W] [fuzz] CRASH [<signature>] (new crash edges, minimized) saved to /tmp/redline-typedarray/crashes/crash_sig6_<hash>.js
```

The file names contain the signal number and a hash:

| File | Content |
|---|---|
| `crash_sig6_<hash>.js` | The minimized program, or the original program if Redline did not minimize it. |
| `crash_sig6_<hash>.orig.js` | The original program, if Redline minimized it. |
| `crash_sig6_<hash>.txt` | The report: the signature, the reproduction runs, the minimization, and the output of d8. |
| `crash_sig6_<hash>.history/` | For a state-dependent crash only: the programs to run first, in their order. |
| `signatures.txt` | One line for each crash signature. |

The `.js` file starts with a header:

```text
// redline crash: signal 6, minimized
// signature: <signature>
// reproducible (2/2 fresh-target runs crashed), worker 2 (target had run <n> programs), origin <origin>, parent #<n>, generation <n>
// d8 out/fuzzbuild/d8 --expose-gc --expose-externalize-string --omit-quit --allow-natives-syntax --fuzzing --harmony --experimental-fuzzing --js-staging --wasm-staging --wasm-fast-api --expose-fast-api --wasm-test-streaming --jit-fuzzing
```

The last header line is the d8 command line of the session.

Reproduce the crash in the target, as the fuzzer does:

```shell
CRASH=/tmp/redline-typedarray/crashes/crash_sig6_<hash>.js
out/redline/redline exec --d8=out/fuzzbuild/d8 --d8-extra-args=--jit-fuzzing --print-output "$CRASH"
```

```text
/tmp/redline-typedarray/crashes/crash_sig6_<hash>.js: crashed (signal 6) in <n> ms, <n> edges hit, 0 new, 0 found total (<n>%)
---- stderr (<n> bytes) ----
...
```

`--d8-extra-args` adds the d8 flags of your configuration. Use the same flags as the session.

Reproduce the crash without Redline, with the d8 command line of the header:

```shell
$(sed -n 's|^// d8 ||p' "$CRASH") "$CRASH"
```

The crash files do not contain the prelude, and `exec`, `replay` and d8 do not add it.
If the program calls an mjsunit assertion function, it fails in these commands before it gets to the crash.
`redline minimize` adds the prelude, so use it for these programs.

Minimize the crash again, for example with all reducers and a larger budget:

```shell
out/redline/redline minimize --d8=out/fuzzbuild/d8 --d8-extra-args=--jit-fuzzing --oracle=crash --max-tests=1000 --out=/tmp/redline-tutorial/crash.min.js /tmp/redline-typedarray/crashes/crash_sig6_<hash>.orig.js
```

```text
[<time> I] [minimize] <n> -> <n> nodes (<n>%), <n> tests (<n> accepted, 0 syntax-rejected) in <n> s; property HOLDS on the result
```

The `crash` oracle of `minimize` accepts any crash, not only the signature of the session.
Run the result with `exec --print-output`, and compare its stderr with the report.

After a change in V8, run all crash files of a session again:

```shell
out/redline/redline replay --d8=out/fuzzbuild/d8 --d8-extra-args=--jit-fuzzing --corpus=/tmp/redline-typedarray/crashes
```

```text
[<time> W] [replay] /tmp/redline-typedarray/crashes/crash_sig6_<hash>.js crashed the target (signal 6)
[<time> W] [replay] /tmp/redline-typedarray/crashes/crash_sig6_<hash>.orig.js crashed the target (signal 6)
replay: 2 files (0 unreadable): 0 succeeded, 0 failed, 2 crashed, 0 timed out
```

`replay` runs every `.js` file below the directory in one d8, as a corpus import does.

If the report says `NOT reproduced`, the crash needs a special timing or state of d8.
Run the amplify step again on this crash:

```shell
out/redline/redline amplify --d8=out/fuzzbuild/d8 --d8-extra-args=--jit-fuzzing --crash=/tmp/redline-typedarray/crashes/crash_sig6_<hash>.txt "$CRASH"
```

`amplify` runs the program in new d8 processes with additional flags, for example `--stress-runs`.
It prints the flags that reproduce the crash as `amplified_args`.
For a state-dependent crash, run the files of the `.history/` directory in their order, and then the crash program.

### 7.9 Make a named preset

`--preset=NAME` loads `experimental/redline/presets/NAME.json`.
Copy your file into this directory, and check it:

```shell
cp /tmp/redline-tutorial/typedarray.json experimental/redline/presets/typedarray.json
out/redline/redline selftest --offline
```

```text
...
PASS  presets: typedarray.json loads
...
```

`selftest --offline` loads every file of the presets directory, as `fuzz --preset` does.
Then use the name of the preset:

```shell
out/redline/redline fuzz --d8=out/fuzzbuild/d8 --preset=typedarray --jobs=4 --out=/tmp/redline-typedarray-3
```

A preset is the second layer. Thus, a `--config` file and the flags can still change it.
If you want to share your preset with other V8 developers, read [CONTRIBUTE.md](CONTRIBUTE.md) before you upload it.

## 8. Next steps

- [CONTRIBUTE.md](CONTRIBUTE.md): add your own generator, mutator or reducer, if your area needs code that no component writes.
- [DESIGN.md](DESIGN.md): the architecture of Redline, the stages of a session and the strategies.
