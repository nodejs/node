# Redline

Redline is a coverage-guided JavaScript fuzzer for V8, written in C++ in the V8 tree.
It runs a fuzzing build of d8, the *target*, as a persistent child process.
Redline sends each program to the target over the REPRL protocol, and it reads the edge coverage from shared memory.

Redline parses each program into FAST, its typed syntax tree.
Generators write new code, mutators change the tree, and a printer writes the tree back as JavaScript.
Redline keeps the programs that reach new edges in V8, and it makes them smaller.
It groups the crashes by signature, and it minimizes the crashes that reproduce.
The corpus programs and the crash files are plain JavaScript files.

## Status

- Redline is experimental, and it builds only on Linux.
- The V8 bots compile Redline on Linux, because the root `BUILD.gn` adds `experimental/redline` to `gn_all`.
- The bots do not run the Redline tests yet. Run the [checks](#checks) before you upload a change.

## Quick start

You need a Linux x64 workstation, a V8 checkout, and depot_tools in your `PATH`.
Run all commands from the root of the V8 checkout.
[TUTORIAL.md](TUTORIAL.md) explains each step in more detail.

### Build the fuzzing d8

The target is a d8 with coverage instrumentation and the REPRL loop:

```shell
gn gen out/fuzzbuild --args='is_debug=false dcheck_always_on=true v8_enable_debug_code=true v8_static_library=true v8_enable_verify_heap=true v8_fuzzilli=true v8_enable_backtrace=true v8_enable_partition_alloc=false v8_enable_builtins_profiling=true sanitizer_coverage_flags="inline-bool-flag" target_cpu="x64" use_remoteexec=true'
autoninja -C out/fuzzbuild d8
```

### Build Redline

Redline itself needs no coverage instrumentation:

```shell
gn gen out/redline --args='is_debug=false target_cpu="x64" use_allocator_shim=false use_remoteexec=true'
autoninja -C out/redline redline
```

If you do not have remote build execution, remove `use_remoteexec=true` from the arguments of both builds.
Keep the binary in `out/redline`. Redline finds its presets and test data in `experimental/redline`, two directories above its binary.
`out/redline/redline help` lists the commands. `out/redline/redline help <command>` lists the flags of one command.

### Check the build

```shell
out/redline/redline selftest --offline
out/redline/redline selftest --d8=out/fuzzbuild/d8
```

The first command runs the checks that need no target.
The second command runs the same checks, and then it checks the execution layer against the target.
Both commands print `PASS` or `FAIL` for each check, and a summary at the end:

```text
...
PASS  presets: maglev.json loads
...
selftest: <n> passed, 0 failed
```

### Run a short session

Fuzz Maglev for one minute with the built-in preset `maglev`:

```shell
out/redline/redline fuzz --d8=out/fuzzbuild/d8 --preset=maglev --jobs=4 --duration=60 --out=/tmp/redline-maglev
```

```text
[<time> I] [fuzz] session: 4 worker(s), seed <n>, <n> edges in target, 0 file(s) to import, seeds rarity, partners rarity-partner, mutators adaptive, generators adaptive, output /tmp/redline-maglev
[<time> I] [fuzz] 00:00:30 | execs <n> (<n>/s) | cov <n>% (<n> edges) | corpus <n> | ok <n>% fail <n>% timeout <n>% | ...
...
[<time> I] [fuzz] final: 00:01:00 | execs <n> (<n>/s) | cov <n>% (<n> edges) | corpus <n> | ...
```

- `--preset=maglev` loads `experimental/redline/presets/maglev.json`.
  This preset adds the d8 flags `--maglev`, `--no-turbofan` and `--jit-fuzzing`, and it gives higher weights to some generators.
- `--jobs=4` starts four workers. Each worker runs its own d8.
- Without seed files, the session starts from generated programs. `--import=DIR` adds the `.js` files of a directory as seed files.
- Without `--duration`, the session runs until you press Ctrl-C.
- The output directory gets the corpus, the crashes and the event log `events.jsonl`.

## Documentation

- [DESIGN.md](DESIGN.md): the architecture, with the fuzzing loop, the stages, the strategies, the configuration and FAST.
- [TUTORIAL.md](TUTORIAL.md): build Redline, explore the components, and write and run your own configuration.
- [CONTRIBUTE.md](CONTRIBUTE.md): add or change components, run the checks before you upload, and measure a change.

## Directory layout

| Directory | Contents |
|---|---|
| `base/` | Flags, logging, JSON, files, time, subprocesses, and the in-process V8 parser that checks the syntax. |
| `commands/` | One file for each command, for example `fuzz`, `shell` and `selftest`. `main.cc` selects the command. |
| `core/` | Programs, the `Builder`, the `Environment`, components, random numbers, tunables, AST helpers, and the selection interface. |
| `corpus/` | The shared corpus, and the seed and partner strategies. |
| `engine/` | The configuration, the fuzzer and its workers, the stages, the shared coverage, the crash signatures, the amplify step, the importer and the prelude. |
| `exec/` | The REPRL client, the coverage map, the script runner, the target options, and the plain d8 runs of the amplify step. |
| `fast/` | FAST: the syntax tree, the arena, the lexer, the parser, the printer and the scope analysis. |
| `generators/` | The code generators, the expression synthesis that they share (`CodeGen`), and the Wasm module generator. |
| `minimizer/` | The minimizer and its reducers. |
| `mutators/` | The mutators. `Explore` and `Probing` run the program in the target before they choose a change. |
| `presets/` | The built-in presets: one JSON configuration for each area of V8. |
| `testdata/` | The golden traces of `redline selftest --offline`. [testdata/README.md](testdata/README.md) describes them. |

## Code conventions

Redline uses the V8 code style. Run `git cl format` before you upload.
These files add settings for `experimental/redline` only:

- `CPPLINT.cfg`: it turns off two cpplint checks that do not fit a standalone tool. Its comments give the reasons.
  `git cl lint` runs cpplint on the files of your change.
- `.clang-tidy`: the `bugprone`, `cert`, `clang-analyzer`, `cppcoreguidelines`, `modernize` and `performance` checks, and some `readability` checks.
  The findings of `bugprone`, `clang-analyzer` and `performance` are errors.
  The comments at the top of the file give the reasons for the checks that it turns off.
- `BUILD.gn`: Redline compiles with `-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wunused -Werror`.
  The C++ files also get `-Wnon-virtual-dtor -Wold-style-cast -Woverloaded-virtual`.
  The V8 and third-party headers count as system headers, so these flags apply only to the Redline code.

To run clang-tidy, write a compilation database for the `redline` target, and give it to clang-tidy:

```shell
gn gen out/redline --add-export-compile-commands=//experimental/redline:redline
third_party/llvm-build/Release+Asserts/bin/clang-tidy -p out/redline experimental/redline/corpus/corpus.cc
```

`gclient sync` downloads clang-tidy if your `.gclient` file sets `"checkout_clang_tidy": True` in `custom_vars`.

## Checks

The bots do not run these checks yet. Run them before you upload a change:

```shell
out/redline/redline selftest --offline
out/redline/redline selftest --d8=out/fuzzbuild/d8
out/redline/redline roundtrip --dir=test/mjsunit --no-run
```

- `selftest --offline` compares the picks and the outputs of the default components with the golden traces in `testdata/`.
  It also loads each preset and checks the configuration.
- `selftest --d8=PATH` runs the same checks, and then it checks the execution layer against the target.
- `roundtrip --no-run` checks the FAST parser and printer on each file below `test/mjsunit`.
  FAST must accept each file that V8 accepts.
  FAST and V8 must accept each printed program, and a second print must give the same text.

[CONTRIBUTE.md](CONTRIBUTE.md) has the full checklist for a change.
The [redline-presubmit](skills/redline-presubmit/SKILL.md) skill turns this
checklist into commands and a report.
