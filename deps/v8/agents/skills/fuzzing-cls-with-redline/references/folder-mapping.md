# V8 Diff-to-Fuzzing Customization Reference

`scripts/analyze_diff.py` owns default folder-to-preset, flag, generator, and
seed mappings. Read this file only when diff hunks or user answers require
custom overrides beyond `plan.json`.

## 1. Change-Based Target Eligibility (`change_targets.py`)

`experimental/redline/tools/change_targets.py` maps C++ and header lines
(`*.cc`, `*.h`, `*.inl`) under `src/` and `include/` to runtime coverage edges
in `d8`.

| Diff Content                                                                 | Eligible      | Action                                                                                                          |
| ---------------------------------------------------------------------------- | ------------- | --------------------------------------------------------------------------------------------------------------- |
| C++ or header compiled into x64 `d8`                                         | Yes           | Keep `change_based: true` on `counters_slow_dcheck` (`inline-8bit-counters,pc-table`, `symbol_level=1`).        |
| Only `.tq` (Torque) or `src/builtins/*-gen.cc` (CSA)                         | No            | Set `change_based: false` and `builtins_coverage: true` on all arms. These files run in `mksnapshot`, not `d8`. |
| Only `src/**/arm64/`                                                         | On `arm64sim` | Set `change_based: true` on `arm64sim` (`v8_target_cpu="arm64"`).                                               |
| Only other non-x64 arches (`ia32`, `arm`, `riscv`, `ppc`, `s390`, `loong64`) | No            | Warn the user that these files do not compile into x64 `d8`.                                                    |
| Only `test/`, `docs/`, `tools/`, or `agents/`                                | No            | Tell the user that no runtime code changed before you build.                                                    |

## 2. Semantic Hunk Overrides

When diff hunks point to a specific V8 feature inside a broad folder, add these
targeted `generator_weights` or `d8_extra_args` overrides in `plan.json`:

| Semantic Clue in Diff Hunks             | Generator Weights (`generator_weights`)                 | Extra `d8` Flags (`d8_extra_args`)                                               |
| --------------------------------------- | ------------------------------------------------------- | -------------------------------------------------------------------------------- |
| TypedArrays, ArrayBuffers, or DataViews | `TypedArrayOps: 5`, `TypedArrayDecl: 3`                 | `--stress-wasm-memory-moving` (for Wasm memory)                                  |
| Strings or template literals            | `StringOps: 6`, `TemplateDecl: 4`                       | `--shared-strings` (for shared string changes)                                   |
| RegExp fast paths or irregexp           | `RegExpPaths: 6`, `RegExpDecl: 5`, `StringOps: 5`       | `--force-slow-path` (on one arm)                                                 |
| Collections (`Map`, `Set`, `WeakRef`)   | `MapSetOps: 5`, `Gc: 3`                                 | `--verify-heap`                                                                  |
| Promises, async, or microtasks          | `PromiseOps: 5`, `AwaitUsingDecl: 2`                    | `--future`                                                                       |
| Frozen, sealed, or element-kind arrays  | `FrozenArrayOps: 4`, `ElementsKind: 5`, `FreezeSeal: 3` | `--stress-ic`                                                                    |
| JSON parsing or serialization           | `JsonRoundTrip: 6`, `ObjectDecl: 4`, `ArrayDecl: 4`     | `--verify-heap`                                                                  |
| `Intl` or `Temporal` builtins           | `IntlOps: 5`, `TemporalOps: 5`                          | `--future`                                                                       |
| Liftoff (`src/wasm/baseline/`)          | `WasmModule: 6`, `TypedArrayOps: 3`                     | `--liftoff-only` (on one arm)                                                    |
| Sparkplug (`src/baseline/`)             | `JitBaseline: 5`, `Deopt: 3`, `Closure: 3`              | `--always-sparkplug`                                                             |
| Deoptimizer or OSR paths                | `Deopt: 5`, `LazyDeopt: 5`, `OsrLoop: 4`                | `--deopt-every-n-times=100`, `--always-osr` (on one arm)                         |
| New flag defaulting to `false`          | Match subsystem                                         | Pass `--<flag-name>` (or `--wasm-<name>` for `WASM_FEATURE`) in `d8_extra_args`. |

## 3. Valid `d8` Flag Combinations

Obey these V8 flag rules when you edit `d8_extra_args` and `d8_flag_profile`:

- Pair `--assert-types` and `--turboshaft-assert-types` with `--single-threaded`
  only. Both flags disable concurrent recompilation and clash with
  `--stress-concurrent-inlining`.
- Do not pair `--maglev-as-top-tier` with `--turbolev` or `--turbolev-future`.
  `--maglev-as-top-tier` implies `--no-turbofan`.
- Do not pass `--verify-heap` on `--sandbox-fuzzing` arms. In-sandbox memory
  corruption triggers false-positive heap verification crashes.
- For `--sandbox-fuzzing`, add `v8_enable_memory_corruption_api=true` to
  `extra_gn_args` so that `sandbox_fuzzing` is writable.
- Choose `d8_flag_profile` to match the preset:
  - Use `"maglev"` only for standalone `maglev` preset arms.
  - Use `"turbolev"` for `turbolev` preset arms.
  - Use `"fuzzilli"` for `default`, `wasm`, `builtins`, `parser`, and sandbox
    arms.
  - Set `randomize_d8_flags: false` on `runtime` preset arms (`--no-turbofan`,
    `--no-maglev`, `--no-sparkplug`) so that random flags do not re-enable
    Maglev.
- Always append custom flags in `d8_extra_args` so that Redline keeps its
  default fuzzing flags and preset flags.
