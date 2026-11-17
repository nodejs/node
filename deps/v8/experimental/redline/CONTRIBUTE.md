# Contributing to Redline

This guide is for V8 engineers who want to change Redline.
Each section is a walkthrough with real commands and real outputs.
We ran every command on a workstation in October 2026, with a fuzzing d8 of 2,038,918 edges.
Your numbers will be different. The shape of the output will be the same.

Read these documents first:

- [DESIGN.md](DESIGN.md): the pipeline, the stages and the rules that keep Redline simple.
- [TUTORIAL.md](TUTORIAL.md): the commands, from one file to an overnight session.

The walkthroughs add three example components: the generator `AtomicsOps`, the mutator `NegateCondition` and the seed strategy `smallest`.
They are examples only. They are not part of Redline.

Contents:

1. [Prepare your workstation](#1-prepare-your-workstation)
2. [Concepts](#2-concepts)
3. [Add a generator](#3-add-a-generator)
4. [Add a mutator](#4-add-a-mutator)
5. [Add a reducer](#5-add-a-reducer)
6. [Add a selection strategy](#6-add-a-selection-strategy)
7. [Change the other stages](#7-change-the-other-stages)
8. [Tune a preset](#8-tune-a-preset)
9. [Debug a stage](#9-debug-a-stage)
10. [Measure a change](#10-measure-a-change)
11. [Checklist for a CL](#11-checklist-for-a-cl)

---

## 1. Prepare your workstation

```sh
V8=<your V8 checkout>
cd $V8
gn gen out/redline --args='is_debug=false target_cpu="x64" use_allocator_shim=false use_remoteexec=true'
autoninja -C out/redline redline        # TUTORIAL.md section 2 explains the args
alias redline=$V8/out/redline/redline
export REDLINE_D8=$V8/out/fuzzbuild/d8  # TUTORIAL.md section 1 builds it
```

You need two builds of d8:

| d8 build | Use |
|---|---|
| a fuzzing d8 at your revision (`$REDLINE_D8`) | everyday checks, the shell, sessions |
| a fuzzing d8 with known bugs (`$D8_BUGS`) | the overfitting audit (section 3.7) |

To make a d8 with known bugs, build the fuzzing d8 of [TUTORIAL.md](TUTORIAL.md) section 1 at a V8 revision that is a few months old. Each `test/mjsunit/regress/` test that is newer than that revision shows a bug that this d8 has.

`experimental/redline/BUILD.gn` compiles Redline with `-Werror` and strict warnings, for example `-Wconversion`, `-Wshadow` and `-Wold-style-cast`.
The build fails on every warning. Format and lint your files with these commands:

```sh
git cl format                           # uses the V8 style (the root .clang-format)
gn gen out/redline --add-export-compile-commands=//experimental/redline:redline
clang-tidy -p out/redline experimental/redline/generators/generators.cc
```

---

## 2. Concepts

### 2.1 Where the code is

| Stage | You add | File | Registration | Configuration key |
|---|---|---|---|---|
| Init | an init strategy | `engine/config.cc` | `ReadInitConfig` | `init` |
| Pick | a seed or partner strategy (`Selector<const Program>`) | `corpus/corpus.cc` | `RegisterBuiltinSelectors<const Program>` | `selection.seeds`, `selection.partners` |
| Mutate | a mutator (`Mutator`) | `mutators/mutators.cc` | `MutatorSet::CreateDefault` | `selection.mutators` |
| Mutate | a generator | `generators/generators.cc` | `GeneratorSet::CreateDefault` | `selection.generators` |
| Mutate | a mutator or generator strategy | `mutators/mutators.cc`, `generators/generators.cc` | `RegisterBuiltinSelectors<Mutator>`, `RegisterBuiltinSelectors<Generator>` | `selection.mutators`, `selection.generators` |
| Accept | a criterion | `engine/config.cc`, `engine/fuzzer.cc` | `ReadAcceptConfig`, `Worker::Commit` | `accept`, `stages.check` |
| Minimize | a reducer (`Reducer`) | `minimizer/minimizer.cc` | `Minimizer::CreateDefaultReducers` | `stages.minimize.reducers` |
| Crash triage | an amplify strategy (`AmplifyStrategy`) | `engine/amplifier.cc` | `CreateAmplifyStrategy`, `AmplifyStrategies` | `amplify` |
| any | a number (tunable) | next to its use | `REDLINE_TUNABLE` | `tunables` |

Execute and Publish are fixed code in `engine/fuzzer.cc` and `exec/`. Crash triage is in `engine/crash.cc`, `engine/fuzzer.cc` and `engine/amplifier.cc`.
File names in the text and the tables are relative to `experimental/redline/`. The commands run in the root of the V8 checkout.

### 2.2 Programs, FAST and the Builder

- A `Program` (`core/program.h`) is immutable. It holds a FAST tree, its printed source, its lineage and its corpus data.
- FAST (`fast/ast.h`) is the typed JavaScript AST of Redline. One X-macro list defines the node kinds. Every node has a stable `id`.
- A mutator never changes a corpus program. `ApplyMutation` copies the tree, and the mutator edits the copy through a `Builder` (`core/builder.h`).
- The `Builder` knows the scopes, the parent of each node and the visible variables at each position. It also knows the syntactic context, for example "strict" or "in a loop".
- The `Builder` has edit primitives: `Insert`, `InsertAll`, `Remove`, `Replace` and `Blockify`. If you edit the tree directly, call `Builder::Invalidate()`. The Builder then computes its analyses again.
- New nodes come from the `mk::` helpers (`core/ast_builder.h`), for example `mk::CallMethod(t, object, "name", {arguments})`.
- The in-process V8 parser is the syntax oracle. Redline never executes JavaScript in its own process. The target d8 executes every program.

### 2.3 Components

A component is a generator, a mutator or a reducer. Every component derives from `Component` (`core/component.h`). A component has:

- a name, which the configuration, the shell and the statistics use,
- a description of one line, which `redline mutate --list` and the shell show,
- a `ComponentStats` record: invocations, results, invalid results, executions, outcomes, new coverage and time.

The engine fills the statistics. Your component does not count anything.

Generators and mutators also have a default weight. The selection strategy uses it. A weight of 0 disables a component. The configuration can change every weight without a new build.

### 2.4 Strategies

A strategy makes one decision: which seed, which partner, which mutator or which generator.
A strategy is a `Selector<T>` (`core/selection.h`). A `SelectorRegistry<T>` keeps the strategies by name, with the parameters (`ParamSpec`) of each one.
The registry checks the configuration before any target starts. It also writes the strategy part of `redline fuzz --help-config`.

### 2.5 Tunables

A tunable is a named number with a default, a range and one line of documentation (`core/tunable.h`):

```cpp
REDLINE_TUNABLE(kAtomicsOpsMaxCalls, "generators.AtomicsOps.max_calls", 3, 1,
                16, "most Atomics calls");
// ...
const int calls = 1 + r.SmallCount(kAtomicsOpsMaxCalls.AsInt() - 1);
```

Rules:

- Every probability, count and limit in a component is a tunable. Do not write a number in the code.
- The name starts with the owner: `generators.<Generator>.`, `mutators.<Mutator>.`, `codegen.<helper>.` or `minimizer.<reducer>.`.
- When you replace a number in existing code, the default is the old number. Then the behavior does not change.
- `redline fuzz --help-tunables` lists all tunables. The flag `--tunable=NAME=VALUE` and the `tunables` block set them. The flag also works with `mutate` and `audit`.

Redline has 267 tunables today: 169 in generators, 61 in the CodeGen helpers, 36 in mutators and 1 in the minimizer.

### 2.6 Configuration

One JSON configuration sets everything. Four layers apply, and a later layer wins:

1. the built-in defaults,
2. the preset (`--preset=NAME` or `--preset=FILE`),
3. the configuration (`--config=FILE` or inline JSON),
4. the command-line flags.

`redline fuzz --dump-config` prints the complete result, with every default. It needs no target. Section 8 shows how to tune a preset.

### 2.7 Tests that protect you

| Test | Command | It finds |
|---|---|---|
| golden traces | `redline selftest --offline` | a change in the picks or the output of a component, for fixed seeds |
| node kinds | `redline selftest --offline` | a FAST node kind that no generator produces |
| configuration | `redline selftest --offline` | a preset that does not load, an error that is not reported, a dump that does not load back |
| validity | `redline mutate --generate --generator=NAME --n=500` | output that the V8 parser rejects |
| runtime | the same command with `--run` | output that throws or times out too often |
| overfitting | `redline audit --generator=NAME` | a generator that is a template of one known bug |
| parser | `redline roundtrip --dir=test/mjsunit` | JavaScript that FAST cannot parse or print |

The offline selftest takes less than one second. Run it after every change.

---

## 3. Add a generator

We add `AtomicsOps`. It calls `Atomics` functions on an integer typed array.

### 3.1 How a generator works

The `CodeGen` mutator picks an insertion point in a program. Then it asks the generator strategy for a generator that can run at that point.
The generator appends statements to `out`, and `CodeGen` inserts them at the insertion point.

A generator gets a `GenContext` (`generators/generator.h`):

| Member | Use |
|---|---|
| `ctx.syntax` | the syntactic context at the insertion point: `strict`, `in_function`, `in_loop`, `in_async` and others |
| `ctx.visible` | the variables that are visible and initialized at the insertion point, innermost first, each with a type guess (`JsType`) |
| `ctx.rng()` | the random generator of the worker. Use only this one: the same seed must give the same program. |
| `ctx.tree()` | the tree that owns the new nodes (`T(ctx)` in `generators.cc`) |
| `ctx.depth`, `ctx.max_depth` | the nesting depth of generated code |

The helper `CodeGen cg` (`generators/codegen.h`) makes values and statements.
Examples: `cg.Integer()`, `cg.Value(type)`, `cg.Variable(type)`, `cg.Arguments(min, max)`, `cg.Declare(init, type, prefix)`, `cg.DeclareGuarded(init, type)` and `cg.Body(nested, count)`.

Requirements tell the strategy where a generator can run.
For example, `kNeedsFunction` means "this generator emits `return`". The strategy then never picks it outside a function.
`kRecursive` means "this generator emits nested code". The strategy stops it at `max_depth`.

### 3.2 Write the generator

Add this code to `generators/generators.cc`, before `#undef GEN`:

```cpp
REDLINE_TUNABLE(kAtomicsOpsMaxCalls, "generators.AtomicsOps.max_calls", 3, 1,
                16, "most Atomics calls");
REDLINE_TUNABLE(kAtomicsOpsLength, "generators.AtomicsOps.length", 16, 1, 1024,
                "elements of a new Int32Array");
REDLINE_TUNABLE(
    kAtomicsOpsReuseChance, "generators.AtomicsOps.reuse_chance", 0.3, 0, 1,
    "probability to use a visible typed array instead of a new one");

GEN(AtomicsOps) {
  Random& r = ctx.rng();
  Tree* t = T(ctx);

  // A visible typed array, or a new Int32Array on a SharedArrayBuffer.
  Identifier* array = r.Chance(kAtomicsOpsReuseChance())
                          ? cg.Variable(JsType::kTypedArray)
                          : nullptr;
  const bool reused = array != nullptr;
  const int length = kAtomicsOpsLength.AsInt();
  if (!reused) {
    Expression* buffer =
        mk::New(t, mk::Id(t, "SharedArrayBuffer"), {mk::Num(t, 4.0 * length)});
    Push(out, cg.Declare(mk::New(t, mk::Id(t, "Int32Array"), {buffer}),
                         JsType::kTypedArray, "ta"));
    array = mk::Id(
        t, ctx.visible.front().name);  // Declare() put the new name first
  }

  const int calls = 1 + r.SmallCount(kAtomicsOpsMaxCalls.AsInt() - 1);
  for (int i = 0; i < calls; i++) {
    std::string_view op = r.Pick({"add", "sub", "and", "or", "xor", "exchange",
                                  "compareExchange", "load", "store"});
    // An index inside the new array; any integer for a visible array.
    Expression* index =
        reused ? cg.Integer()
               : mk::Num(t, static_cast<double>(r.Range(0, length - 1)));
    std::vector<Expression*> args = {mk::Id(t, array->name), index};
    if (op != "load") {
      args.push_back(cg.Integer());
    }
    if (op == "compareExchange") {
      args.push_back(cg.Integer());
    }
    Expression* call =
        mk::CallMethod(t, mk::Id(t, "Atomics"), op, std::move(args));
    // A visible typed array can have an element type that Atomics rejects.
    if (reused) {
      for (Statement* s : cg.DeclareGuarded(call, JsType::kNumber)) {
        Push(out, s);
      }
    } else {
      Push(out, cg.Declare(call, JsType::kNumber));
    }
  }
  return true;
}
```

What the code does:

- `GEN(AtomicsOps)` declares the function `GenAtomicsOps(GenContext& ctx, CodeGen& cg, Statements* out)`.
- `cg.Variable(JsType::kTypedArray)` returns a visible typed array, or `nullptr`. Code at the start of a program sees no variables. Thus a generator must also work without them. DESIGN.md calls this rule "missing data is normal".
- `cg.Declare(...)` makes a `let`, `const` or `var` declaration with a fresh name. It also adds the name to `ctx.visible`, so `ctx.visible.front()` is the new name. Generators that run later in the same insertion can use the new variable.
- A visible typed array can be a float array, and `Atomics` throws a `TypeError` for it. `cg.DeclareGuarded(...)` puts the call in a `try` block, so the exception does not end the program.
- A generator returns `false` when it cannot generate anything at this point. `CodeGen` then discards its output.
- Each number in the generator is a tunable.

### 3.3 Register the generator

Add one line at the end of the list in `GeneratorSet::CreateDefault()`:

```cpp
add("AtomicsOps", "Atomics operations on a shared Int32Array", 1,
    kNoRequirement, GenAtomicsOps);
```

The arguments are the name, the description, the default weight, the requirements and the function.

### 3.4 Build it and check the syntax

```
$ autoninja -C out/redline redline
$ redline mutate --list | grep AtomicsOps
  AtomicsOps         w=1    Atomics operations on a shared Int32Array
$ redline mutate --generate --generator=AtomicsOps --n=500 --quiet
generators
  component                    invoked  produced invalid  executed    ok%   fail    tmo  crash    new   edges     spent       ms
  AtomicsOps                       500       500       0         0   0.0%      0      0      0      0       0         0        0
summary: 500 produced, 0 no-op, 500 V8-accepted (100.0%); mutation time 0.7 ms total (0.00 ms/sample), wall 0.0 s
```

`--generate` starts from an empty program. The in-process V8 parser checks each result. The target is at least 99.8% accepted.
`--print` shows every program. `--print-invalid` shows only the rejected programs, with the error of the parser.

### 3.5 Run it in d8

```
$ redline mutate --generate --generator=AtomicsOps --n=500 --run --quiet
generators
  component                    invoked  produced invalid  executed    ok%   fail    tmo  crash    new   edges     spent       ms
  AtomicsOps                       500       500       0       500 100.0%      0      0      0     70       0         0     1863
summary: 500 produced, 0 no-op, 500 V8-accepted (100.0%), 500 executed, 500 succeeded (100.0%), 0 crashed; mutation time 4.9 ms total (0.01 ms/sample), wall 1.9 s
```

The columns:

- `ok%`: the share of programs that ran to the end without an uncaught exception.
- `fail`: uncaught exceptions. `tmo`: timeouts. `crash`: crashes.
- `new`: programs that reached edges that the earlier programs of the same command did not reach.

An empty program has no visible typed array, so this run tests only one branch.
To test the other branch, make a program with a typed array, and force the branch with a tunable:

```
$ redline mutate --generate --generator=TypedArrayDecl --n=1 --seed=7 --out-dir=/tmp/contrib/seeds --quiet
$ redline mutate --generator=AtomicsOps --n=500 --run --quiet \
      --tunable=generators.AtomicsOps.reuse_chance=1 /tmp/contrib/seeds/TypedArrayDecl-0-0.js
generators
  component                    invoked  produced invalid  executed    ok%   fail    tmo  crash    new   edges     spent       ms
  AtomicsOps                       500       500       0       500 100.0%      0      0      0     71       0         0     1842
```

The first command writes a program that declares an `Int16Array`.
With a file argument, `mutate` inserts the generated code at a random position of that program.
Before the declaration, the typed array is not visible. Then the generator makes a new array.

### 3.6 Examine it in the shell

```
$ redline shell /tmp/contrib/seeds/TypedArrayDecl-0-0.js
target: .../d8 (2038918 edges)
loaded /tmp/contrib/seeds/TypedArrayDecl-0-0.js: 10 nodes, 50 bytes
redline> generate AtomicsOps 3
=> AtomicsOps (seed 3): inserted 2 statement(s): 28 nodes, V8 accepts
redline> run
succeeded in 4.64 ms, exit 0; 14680 edges
```

The shell also prints the new program and a diff. We do not show them here.
The last argument of `generate` is the seed. Try other seeds to see other outputs. `undo` goes back one step. `tree` shows the FAST nodes, and `scopes` shows the visible variables.

### 3.7 Run the overfitting audit

```
$ EXTRA="--random-seed=42 --single-threaded --future --jit-fuzzing --no-baseline-batch-compilation \
         --no-lazy-feedback-allocation --turbolev --turbolev-future --wasm-inlining-ignore-call-counts"
$ redline audit --d8=$D8_BUGS --d8-extra-args="$EXTRA" --generator=AtomicsOps --n=1000
...
  AtomicsOps                      1000      1000       0      1000 100.0%      0      0      0     88     4425
audit: largest share of one crash signature per generator (threshold 0.50%)
audit: 0 of 1 generators crashed, 0 above the threshold
```

`$EXTRA` holds the d8 flags of the A/B experiment.
A generator passes if no crash signature takes more than 0.5% of its programs.
If your generator fails, make it more general. A lower weight does not fix the problem. DESIGN.md section 10 tells why.

### 3.8 Update the golden traces

```
$ redline selftest --offline
...
FAIL  selection: generator picks match the golden trace: first difference at line 1: golden '0	PropertyStore', now '0	ElementLoad'
FAIL  selection: CodeGen insertions match the golden trace: first difference at line 1: ...
FAIL  components: every generator's output matches the golden trace: first difference at line 77: ...
FAIL  components: every mutator's output matches the golden trace: first difference at line 2: ...
selftest: 61 passed, 4 failed
```

These failures are correct. A new generator with a weight above 0 changes the generator draws.
Write the new traces and examine the diff:

```
$ redline selftest --offline --write-golden
$ git diff --stat experimental/redline/testdata
 experimental/redline/testdata/selection/codegen.trace       | 1994 ++++++++++----------
 .../testdata/selection/generator_outputs.trace     |  131 +-
 experimental/redline/testdata/selection/generators.trace    | 1364 ++++++-------
 .../testdata/selection/mutator_outputs.trace       |   17 +-
 4 files changed, 1758 insertions(+), 1748 deletions(-)
$ redline selftest --offline
...
selftest: 65 passed, 0 failed
```

(This run also contains the mutator of section 4.) How to examine the diff:

- `generators.trace` and `codegen.trace` record the generator of each draw. They change for each new generator with a weight above 0.
- `generator_outputs.trace` records five outputs of each generator. Your generator adds five lines. Generators that draw nested code, for example `FunctionDecl`, also change.
- `mutator_outputs.trace` changes for the mutators that insert generated code, for example `CodeGen`.
- Other changes are not expected. Find their cause before you continue.

Commit the new traces in the same CL as the generator.

### 3.9 Measure the effect

A generator is useful only if it finds coverage or bugs that Redline did not find before.
Section 10 shows how to compare two sessions.
Put the result in the CL description.

---

## 4. Add a mutator

We add `NegateCondition`. It negates the test of an `if` statement, a loop or a conditional expression.

### 4.1 How a mutator works

The worker picks a mutator with the mutator strategy. `ApplyMutation` copies the tree of the parent, makes a `Builder` on the copy and calls `Mutate` (`mutators/mutator.h`):

```cpp
virtual bool Mutate(Builder& builder, MutationContext& ctx) = 0;
```

The contract:

1. Edit the tree through the `Builder`. After a direct edit, call `builder.Invalidate()`.
2. Return `false` when there is nothing to do. The tree must then be unchanged.
3. Write one line about the change in `ctx.detail`. The corpus lineage and the shell show it.
4. Keep the program valid. For example, do not move a declaration after its uses. `VisibleAt`, `EscapingBindings` and `ContextAt` of the Builder help.
5. Use only `builder.rng()`.

The arguments of the `Mutator` constructor are the name, the description, the default weight, `needs_partner` and `max_simultaneous`.
`needs_partner` is true for mutators that take code from a second program (`ctx.partner`), for example `Splice`.
`max_simultaneous` is the largest number of changes in one application. The engine draws the number from 1 to this value.

### 4.2 Write the mutator

Add this class to `mutators/mutators.cc`, in the unnamed namespace:

```cpp
// Negates the test of an if, a loop or a conditional expression.
class NegateConditionMutator final : public Mutator {
 public:
  NegateConditionMutator()
      : Mutator("NegateCondition",
                "negates the test of an if, a loop or a conditional", 0,
                false) {}

  bool Mutate(Builder& b, MutationContext& ctx) override {
    std::vector<Node*> nodes = b.Collect(HasTest);
    if (nodes.empty()) {
      return false;  // nothing to do: the tree is unchanged
    }
    Node* node = nodes[b.rng().Index(nodes.size())];
    Expression** test = TestOf(node);
    *test = mk::Unary(b.tree(), UnaryOp::kNot, *test);
    b.Invalidate();  // a structural edit outside the Builder's own primitives
    ctx.detail = std::string("negated the test of ") + node->kind_name();
    return true;
  }

 private:
  // True if `n` has a test that this mutator can negate.
  static bool HasTest(Node* n) { return TestOf(n) != nullptr; }

  // The test slot of `n`, or nullptr if `n` has no test.
  static Expression** TestOf(Node* n) {
    switch (n->kind) {
      case NodeKind::kIfStatement:
        return &n->Cast<IfStatement>()->test;
      case NodeKind::kWhileStatement:
        return &n->Cast<WhileStatement>()->test;
      case NodeKind::kDoWhileStatement:
        return &n->Cast<DoWhileStatement>()->test;
      case NodeKind::kConditionalExpression:
        return &n->Cast<ConditionalExpression>()->test;
      case NodeKind::kForStatement: {
        auto* loop = n->Cast<ForStatement>();
        return loop->test != nullptr ? &loop->test : nullptr;
      }
      default:
        return nullptr;
    }
  }
};
```

### 4.3 Register the mutator

Add one line at the end of `MutatorSet::CreateDefault()`:

```cpp
add(std::make_unique<NegateConditionMutator>());
```

The default weight is 0. A session does not use the mutator until you enable it.
A new mutator starts at weight 0. An A/B experiment decides its weight.

### 4.4 Test the mutator

`$FILES` holds eight `test/mjsunit` files:

```sh
FILES="test/mjsunit/array-push3.js test/mjsunit/set-prototype-of-Object_prototype.js
       test/mjsunit/new-function.js test/mjsunit/regress-1146106.js
       test/mjsunit/unbox-smi-field-indexed.js test/mjsunit/allocation-folding.js
       test/mjsunit/optimized-map.js test/mjsunit/regress-507036687.js"
```

```
$ redline mutate --mutator=NegateCondition --n=50 --run --quiet $FILES
mutators
  component                    invoked  produced invalid  executed    ok%   fail    tmo  crash    new   edges     spent       ms
  NegateCondition                  400       100       0       100  97.0%      0      3      0     60       0         0    11134
summary: 100 produced, 300 no-op, 100 V8-accepted (100.0%), 100 executed, 97 succeeded (97.0%), 0 crashed; mutation time 6.4 ms total (0.02 ms/sample), wall 11.5 s
```

How to read the result:

- 300 no-ops: six of the eight files have no `if`, no loop and no conditional expression. A no-op costs no execution.
- 100% accepted: a negated test is always valid syntax.
- 3 timeouts: a negated test can make a loop run until the timeout. A timeout costs the full timeout (1,000 ms by default). The other programs take about 20 ms. Thus these 3 programs took more time than the other 97.

### 4.5 Use the mutator in a session

Enable the mutator with a weight, and save the programs that time out:

```
$ redline fuzz --preset=default --config='{"selection": {"seeds": {"strategy": "smallest"}}}' \
      --mutator-weights=NegateCondition=1 --capture=mutate:timeout:NegateCondition:20 \
      --jobs=4 --seed=1 --duration=120 --stats-interval=60 --out=/tmp/contrib/session
[fuzz] session: 4 worker(s), seed 1, 2038918 edges in target, 0 file(s) to import, seeds smallest, partners rarity-partner, mutators adaptive, generators adaptive, output /tmp/contrib/session
[fuzz] 00:02:00 | execs 58249 (485/s) | cov 5.484% (112149 edges) | corpus 503 | ok 68.3% fail 31.4% timeout 0.4% | ...
  component                    invoked  produced invalid  executed    ok%   fail    tmo  crash    new   edges     spent       ms
  NegateCondition                  157        70       0        70  75.7%     16      1      0     19       0         0      922
  ...
  AtomicsOps                        45        45       0        13  69.2%      4      0      0      5      230
$ ls /tmp/contrib/session/captures/mutate/
0000-timeout-NegateCondition.child.js  0000-timeout-NegateCondition.json  0000-timeout-NegateCondition.parent.js
```

This session also uses the seed strategy `smallest` of section 6.
The capture saved the parent, the child and a JSON file with the worker, the time and the lineage of the parent.
Compare the parent with the child. In this capture, the mutator negated the test of a `for` loop, and the program timed out.
A possible change: remove the loop cases from `TestOf`. Measure the change with section 10 before you keep it.

---

## 5. Add a reducer

A reducer makes a program smaller while the oracle stays true. The oracle is a function: "does this candidate still have the property?".
For a corpus program, the property is "these new edges are still set". For a crash, it is "the same crash signature".

### 5.1 How a reducer works

```cpp
class Reducer : public Component {
 public:
  Reducer(std::string name, std::string_view description);
  // Applies all reductions this reducer can find, keeping the ones the
  // oracle accepts. Returns the number of accepted reductions.
  virtual int Reduce(MinimizationHelper& helper) = 0;
};
```

The contract (`minimizer/minimizer.h`):

1. Edit the tree of `helper.tree()`.
2. Call `helper.Test()`. It prints the tree and asks the oracle.
3. If `Test()` returns false, undo your edit.
4. Stop when `helper.BudgetExhausted()` is true. `Test()` also returns false then.
5. Return the number of accepted edits.

A reducer must work with any oracle. It does not know why the program entered the corpus. DESIGN.md section 5.5 explains this rule.

### 5.2 The smallest reducer: `truncate`

`truncate` is the simplest reducer in `minimizer/minimizer.cc`. Copy it to start your reducer:

```cpp
class Truncator : public Reducer {
 public:
  Truncator()
      : Reducer("truncate",
                "keep the shortest prefix of the top-level statements") {}

  int Reduce(MinimizationHelper& helper) override {
    StatementList* body = StatementListOf(helper.tree()->root());
    if (body == nullptr || body->size() < 2) {
      return 0;
    }
    const std::vector<Statement*> all = *body;
    // Invariant: the prefix of length `good` passes (the whole program
    // does), prefixes shorter than `lo` were tested and failed.
    size_t lo = 0, good = all.size();
    while (lo < good && !helper.BudgetExhausted()) {
      size_t mid = lo + (good - lo) / 2;
      body->assign(all.begin(), all.begin() + static_cast<ptrdiff_t>(mid));
      if (helper.Test()) {
        good = mid;
      } else {
        lo = mid + 1;
      }
    }
    body->assign(all.begin(), all.begin() + static_cast<ptrdiff_t>(good));
    return good < all.size() ? 1 : 0;
  }
};
```

### 5.3 Register the reducer

1. Add `reducers.push_back(std::make_unique<YourReducer>());` to `Minimizer::CreateDefaultReducers()`.
2. Do not add it to `Minimizer::DefaultOrder()` yet. The default order applies to every session. Change it only with A/B evidence.
3. Select your reducer in the configuration:

```json
{"stages": {"minimize": {"reducers": ["truncate", "dataflow", "statements", "unwrap", "lists", "expressions"]}}}
```

An unknown name in this list is an error at startup.

### 5.4 Try the reducer on captured pairs

A session can save the parent and the child of each mutation that found new edges: `--capture=mutate:interesting:400`.
Then `redline minimize --baseline=PARENT CHILD` keeps the edges that the child has and the parent does not have. That is the oracle of the fuzzer.
Here, one captured pair goes through three reducer lists:

```
$ for list in statements,unwrap,lists,expressions truncate,dataflow \
              truncate,dataflow,statements,unwrap,lists,expressions; do
    redline minimize --d8=$D8_BUGS --d8-extra-args="$EXTRA" --prelude=none --oracle=edges \
        --baseline=$P.parent.js --reducer=$list --out=/dev/null $P.child.js
  done
[minimize] 302 -> 73 nodes (24.2%), 236 tests (23 accepted, 0 syntax-rejected) in 1.3 s; property DOES NOT HOLD on the result
[minimize] 302 -> 283 nodes (93.7%), 19 tests (3 accepted, 0 syntax-rejected) in 0.1 s; property HOLDS on the result
[minimize] 302 -> 61 nodes (20.2%), 252 tests (31 accepted, 0 syntax-rejected) in 1.3 s; property HOLDS on the result
```

`$P` is the path of the pair without the suffix, for example `OUT/captures/mutate/0016-interesting-Splice`.
Examine the first line: the final check of the result failed once. The target is not deterministic, so one run of one pair proves nothing.
Measure on many pairs. On 198 pairs, `truncate` and `dataflow` alone used about 10 times fewer tests than the default order. Their programs stayed larger: 91% to 95% of the original size, against 73% to 75%.

The shell runs reducers too: `reduce truncate`, `reduce truncate,statements`, `reduce default` or `reduce all`.

---

## 6. Add a selection strategy

We add the seed strategy `smallest`. Of `size` random corpus programs, it picks the one with the fewest AST nodes.

### 6.1 How a strategy works

A strategy is a `Selector<T>` (`core/selection.h`). `T` is `const Program` for seeds and partners, `Mutator` for mutators and `Generator` for generators.

| Method | Called when |
|---|---|
| `Initialize(candidates, error)` | once, before the first pick. Check the parameters that name candidates here. |
| `Pick(candidates, rng, eligible)` | for every pick. Return an index into `candidates`, or `kNoChoice`. |
| `PickRelated(candidates, anchor, rng, eligible)` | for a partner. The default picks like `Pick`, but never the anchor. |
| `OnAdded(candidates, index)` | after a program enters the corpus |
| `OnOutcome(picked, outcome)` | after each execution of a mutation result |
| `OnCrashEdges(edges)`, `WantsHitEdges()` | corpus strategies only, see `rarity` in `corpus/corpus.cc` |
| `Params()` | for the `start` event: the effective parameters |

Threads: the corpus calls its strategies with the corpus lock held, so a seed strategy can keep plain state.
All workers share the mutator and generator strategies. These strategies must have no state between calls, or they must synchronize their state.

### 6.2 Write the strategy

Add this class to `corpus/corpus.cc`, next to `TournamentSeeds`:

```cpp
// Of `size` random programs, the smallest one (fewest AST nodes) wins.
class SmallestSeeds final : public ProgramSelector {
 public:
  static std::vector<ParamSpec> Specs() {
    return {
        {"size", ParamSpec::Type::kNumber, "3", "candidates per tournament"}};
  }
  explicit SmallestSeeds(const JsonValue& params)
      : size_(static_cast<int>(params.GetInt("size", 3))) {}
  std::string_view name() const override { return "smallest"; }
  size_t Pick(const Candidates& candidates, Random& rng,
              const Filter& /*eligible*/) override {
    size_t best = rng.Index(candidates.size());
    for (int i = 1; i < size_; i++) {
      size_t candidate = rng.Index(candidates.size());
      if (candidates[candidate]->node_count() <
          candidates[best]->node_count()) {
        best = candidate;
      }
    }
    return best;
  }
  JsonObject Params() const override {
    JsonObject o;
    o.Add("size", size_);
    return o;
  }

 private:
  int size_;
};
```

### 6.3 Register the strategy

Add one entry to `RegisterBuiltinSelectors<const Program>()`:

```cpp
registry.Register(Entry{"smallest", "smallest of a few random candidates",
                        SmallestSeeds::Specs(),
                        [](const JsonValue& params,
                           std::string*) -> std::unique_ptr<ProgramSelector> {
                          return std::make_unique<SmallestSeeds>(params);
                        }});
```

The `ParamSpec` list declares the parameters. The registry uses it for three things:

```
$ redline fuzz --help-config | grep -A1 "^    smallest"
    smallest: smallest of a few random candidates
      size = 3  candidates per tournament
$ redline fuzz --dump-config --config='{"selection": {"seeds": {"strategy": "smallest", "params": {"sizes": 4}}}}'
[fuzz] selection.seeds: selection strategy 'smallest': unknown parameter 'sizes' (known: size)
$ redline fuzz --dump-config --config='{"selection": {"seeds": {"strategy": "smallest", "params": {"size": 4}}}}' \
      | python3 -c 'import json, sys; print(json.load(sys.stdin)["selection"]["seeds"])'
{'strategy': 'smallest', 'params': {'size': 4}}
```

### 6.4 Use the strategy

The session of section 4.5 used `smallest`. Its statistics have one row for the seed strategy:

```
  component                    invoked  produced invalid  executed    ok%   fail    tmo  crash    new   edges     spent       ms
  smallest                         472       472       0         0   0.0%      0      0      0    495       0         0        0
```

`invoked` counts the picks. `new` counts the new corpus programs that came from these picks.
A seed strategy changes the whole session. Compare it with `rarity`, the seed strategy of the presets, as section 10 shows.
New strategies for mutators and generators follow the same steps in `mutators/mutators.cc` and `generators/generators.cc`.

---

## 7. Change the other stages

### 7.1 Init

Init fills the corpus before the loop starts. `ReadInitConfig` in `engine/config.cc` reads the `init` block.
Each strategy sets fields of `FuzzConfig` that the engine already uses:

| Strategy | Parameters | `FuzzConfig` fields |
|---|---|---|
| `generate` | `programs` | `initial_programs` (the imports are cleared) |
| `import` | `paths`, `root`, `keep` (`new-coverage` or `all`), `minimize` | `imports`, `import_root`, `import_all`, `minimize_imports` |

To add a strategy:

1. Add its name and its parameter names to `ReadInitConfig`. Unknown names stay errors.
2. Set existing `FuzzConfig` fields if possible. Add a field only if no field fits.
3. Write the `init` block back in `FuzzConfig::ToJson`. Then `--dump-config` shows it and the `start` event records it.
4. Add a case to `CheckConfigErrors` in `commands/selftest_selection.cc`. Then the offline selftest checks the new error messages.

### 7.2 Accept

Accept decides if a program enters the corpus. Today it has one criterion: `new-edges`.
The code is in `Worker::Commit` (`engine/fuzzer.cc`):

1. The mutate stage claims the new edges of a result (`ClaimNewEdges`). If there are none, the result does not go to Commit.
2. The determinism check runs the program again. It keeps only the edges that every run sets. Its parameters are in `stages.check`.
3. The minimizer runs with an oracle: "the program succeeds and all kept edges are set".
4. Publish adds the program to the corpus.

A second criterion must follow the rule of DESIGN.md section 5.5: it returns a frozen reason, and the reason becomes the oracle of step 3.
The reason must not depend on the corpus state at a later time. To add a criterion:

1. Add its name to `ReadAcceptConfig` in `engine/config.cc`.
2. In the mutate stage, decide if a result is a candidate for your criterion.
3. In `Worker::Commit`, compute your reason once and build the minimizer oracle from it.
4. Add a selftest case and an A/B result.

When Accept has a second criterion, it gets a registry like the selection strategies.

### 7.3 A new stage parameter

1. Add a field to `FuzzConfig` (`engine/config.h`). Its default is the old behavior.
2. Read it in the stage block of `engine/config.cc` (search for `"stages"`), and write it in `FuzzConfig::ToJson`.
3. Document it in `FuzzConfig::JsonHelp`, which prints `--help-config`.
4. Use it in `engine/fuzzer.cc`.
5. Add a case to `CheckStageConfig` in `commands/selftest_selection.cc`. Then run `redline selftest --offline`. It also loads the full dump back and compares the result.

### 7.4 A new JavaScript feature in FAST

When V8 adds syntax, FAST must parse it, and a generator must produce it. Two tests enforce this:

- `redline roundtrip --dir=test/mjsunit` fails when FAST cannot parse or print the tests of the feature.
- `redline selftest --offline` fails when a FAST node kind comes out of no generator (40 samples per generator).

The steps for a new node kind:

1. Add the kind to the X-macro list in `fast/ast.h`, and define its struct.
2. Build. `ForEachChildSlot` in `fast/ast_visit.h` has no `default` case, so the compiler reports the missing kind. Add its children there.
3. Parse it in `fast/parser.cc`, and print it in `fast/printer.cc`.
4. If it declares or uses names, add it to the scope analysis in `fast/scope.cc`.
5. Add an `mk::` helper in `core/ast_builder.h`.
6. Generate it: extend a generator, or add one (section 3).
7. Run `redline roundtrip --dir=test/mjsunit` and `redline selftest --offline`.

If no generator can produce a kind, add it to `kUngeneratedKinds` in `commands/selftest_selection.cc`, with a reason.

---

## 8. Tune a preset

A preset is a JSON configuration for one area of V8, in `experimental/redline/presets/`.
`--preset=maglev` loads `presets/maglev.json`. `--preset=FILE.json` loads any file.

### 8.1 Start from a preset

```
$ cat experimental/redline/presets/maglev.json
{
  "d8_extra_args": ["--maglev", "--no-turbofan", "--jit-fuzzing"],
  "selection": {"seeds": {"strategy": "rarity"}, "partners": {"strategy": "rarity-partner"}, "mutators": {"strategy": "adaptive"},
    "generators": {"strategy": "adaptive", "params": {"weights": {
      "JitOptimize": 8, "Deopt": 3, "OsrLoop": 3, "ElementsKind": 4, "ShapeChange": 3}}}}
}
```

- `d8_extra_args` is appended to the d8 flags, like `--d8-extra-args`.
- `selection` sets the strategies. All presets use the strategies of the latest A/B experiment: `rarity`, `rarity-partner` and `adaptive`.
- `weights` replaces the base weight of the named generators. The others keep their built-in weight.

### 8.2 Make your own preset

Copy a preset and change it. This example gives more weight to object-model generators, and changes two tunables:

```json
{
  "d8_extra_args": ["--maglev", "--no-turbofan", "--jit-fuzzing"],
  "selection": {
    "seeds": {"strategy": "rarity"}, "partners": {"strategy": "rarity-partner"},
    "mutators": {"strategy": "adaptive"},
    "generators": {"strategy": "adaptive",
                   "params": {"weights": {"JitOptimize": 8, "ShapeChange": 4, "DefineProperty": 3, "WasmModule": 0}}}
  },
  "tunables": {"generators.ClassDecl.max_members": 6, "generators.ShapeChange.max_changes": 6},
  "stages": {"minimize": {"max_tests": 200}}
}
```

Check the result before a long run:

```
$ redline fuzz --preset=/tmp/my-preset.json --dump-config > /tmp/effective.json   # needs no target
$ sed 's/max_changes/max_change/' /tmp/my-preset.json > /tmp/bad-preset.json
$ redline fuzz --preset=/tmp/bad-preset.json --dump-config
[fuzz] --preset=/tmp/bad-preset.json: config: unknown tunable 'generators.ShapeChange.max_change' (redline fuzz --help-tunables lists them)
$ redline fuzz --preset=/tmp/my-preset.json --jobs=32 --duration=36000 --out=/tmp/night
```

The full dump lists every weight, every tunable and every strategy parameter, with the values of your preset.
An unknown generator, tunable, strategy or key stops `redline fuzz` before any d8 starts.
`redline fuzz --help-tunables` lists the tunable names. `redline mutate --list` lists the generator and mutator names.

### 8.3 Rules for presets

- Do not raise the weight of a generator that resembles one known bug. The A/B results must stay valid for every preset.
- Keep `WasmModule` at weight 0, except in the `wasm` preset. The A/B experiment compares JavaScript-only runs.
- Measure a preset with section 10 before you commit it. A new preset also needs `redline selftest --offline`: it loads every preset.

---

## 9. Debug a stage

### 9.1 The shell

The shell runs each stage on one program. [TUTORIAL.md](TUTORIAL.md) section 5.4 explains it. Useful commands:

| Command | Stage |
|---|---|
| `mutate NAME SEED`, `generate NAME SEED`, `random N` | Mutate |
| `run`, `run verbose` | Execute |
| `accept N` | Accept: N runs, then the deterministic and the flaky edges |
| `reduce NAMES ORACLE` | Minimize |
| `tree`, `scopes`, `info`, `diff`, `check` | FAST and the syntax oracle |

The shell reads commands from stdin, so you can script a reproduction:

```sh
printf 'generate AtomicsOps 3\nrun\naccept 10\n' | redline shell /tmp/contrib/seeds/TypedArrayDecl-0-0.js
```

### 9.2 Captures and stage logs

A capture saves the programs of a stage when a trigger fires:

| Stage | Triggers |
|---|---|
| mutate | `timeout`, `crash`, `failed`, `invalid`, `noop`, `interesting` |
| check | `flaky`, `timeout` |
| minimize | `rejected`, `timeout` |
| prefix | `noop` |

```sh
redline fuzz ... --capture='mutate:timeout:NegateCondition:20;check:flaky:20'
redline fuzz ... --stage-log=check,minimize
```

The first command saves up to 20 timeouts of `NegateCondition` and up to 20 flaky candidates in `OUT/captures/<stage>/`.
The second command writes one event per check and per minimization in `OUT/events.jsonl`.
[TUTORIAL.md](TUTORIAL.md) section 7.7 shows how to read the event log.

### 9.3 A crash

A crash goes to `OUT/crashes/` with the minimized program (`.js`), the original program (`.orig.js`) and a report (`.txt`).
The report has the crash signature and the result of the reproduction runs.
`redline minimize --oracle=crash FILE` runs the program with the same prelude as a session, and minimizes it again.
If the crash needs earlier programs, the `.history/` directory of the crash has them.

---

## 10. Measure a change

A change to a component changes the whole session. Only a comparison of two sessions shows its effect.

### 10.1 A local A/B run

Run two sessions at the same time, with the same d8, flags, seed and number of workers. Change only one thing:

```sh
COMMON=(--d8=$REDLINE_D8 --d8-extra-args="--single-threaded --random-seed=42" --preset=default
        --jobs=8 --seed=42 --duration=600 --stats-interval=60 --no-component-stats)
redline fuzz "${COMMON[@]}" --out=/tmp/ab/base > /tmp/ab/base.log 2>&1 &
redline fuzz "${COMMON[@]}" --mutator-weights=NegateCondition=1 --out=/tmp/ab/negate > /tmp/ab/negate.log 2>&1 &
wait
```

Compare the coverage at the same elapsed time. The `stats` events of `events.jsonl` have it:

```sh
python3 - /tmp/ab/base/events.jsonl /tmp/ab/negate/events.jsonl <<'EOF'
import json, sys
runs = [[json.loads(line) for line in open(path, errors="replace")] for path in sys.argv[1:]]
series = [{round(e["t"] / 60): e for e in run if e["event"] == "stats"} for run in runs]
for minute in sorted(set(series[0]) & set(series[1])):
    a, b = series[0][minute], series[1][minute]
    print(f"{minute:4d} min  base {100 * a['coverage']:.3f}%  change {100 * b['coverage']:.3f}%  "
          f"diff {100 * (b['coverage'] - a['coverage']):+.3f} pp  execs {a['executions']} / {b['executions']}")
EOF
```

The result of this run:

```
   1 min  base 4.309%  change 4.623%  diff +0.314 pp  execs 41350 / 41916
   2 min  base 5.867%  change 5.455%  diff -0.413 pp  execs 79525 / 71356
   ...
   9 min  base 8.175%  change 7.902%  diff -0.273 pp  execs 228442 / 219440
  10 min  base 8.335%  change 8.232%  diff -0.103 pp  execs 238887 / 233144
```

The sign of the difference changed after one minute, and the difference stayed below 0.5 points.
This short run shows no effect of `NegateCondition`. It does not show that the mutator is useless.

### 10.2 Read the result correctly

- Ten minutes show only the start of a session. Use at least one hour on a workstation, and repeat the run.
- Two identical sessions differ by 0.25 to 0.35 percentage points. A difference below 0.5 points is probably noise.
- Compare at the same elapsed time first: a slower component must pay for its time. The same number of executions gives a second view.
- For bugs, use the d8 with known bugs and the time to the first crash of each bug.

---

## 11. Checklist for a CL

The [redline-presubmit](skills/redline-presubmit/SKILL.md) skill turns this
checklist into commands and a report.

- [ ] `autoninja -C out/redline redline` builds without a warning.
- [ ] `git cl format` reports nothing for your files, and `clang-tidy` reports no new findings in them.
- [ ] Every new number is a tunable with a name, a range and a description.
- [ ] `redline selftest --offline` passes. You examined the diff of the golden traces.
- [ ] Validity: at least 99.8% of the programs of a new generator or mutator are accepted.
- [ ] `--run`: the share of failures and timeouts is known and explained in the CL description.
- [ ] Audit: no crash signature above 0.5% for a new or changed generator.
- [ ] A/B: a change of a weight, a strategy or a default has a measured result in the CL description.
- [ ] [TUTORIAL.md](TUTORIAL.md), [DESIGN.md](DESIGN.md) and this guide are still correct.
