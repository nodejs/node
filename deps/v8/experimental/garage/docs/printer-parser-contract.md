# Printer ↔ parser contract

`garage` parses text that V8 prints. When a printer changes, `garage` breaks.
The mitigation is that the audience maintains the printers — so this document
makes "update `garage` in the same breath" a mechanical step rather than an
archaeology exercise.

**Verified against V8 15.2.0 (`b80a7539b73`, 2026-07-29).** Line numbers drift;
the file paths and the symbol names are the durable part.

---

## 1. Who prints what

| Format | Flag | V8 source |
| :-- | :-- | :-- |
| Maglev compile banner (`Begin compiling method …`, `Compiling 0x… with Maglev`) | any maglev trace flag | `src/maglev/maglev-compiler.cc` (`MaglevCompiler::Compile`) |
| Maglev phase banner (`----- <phase> -----`) | `--print-maglev-graph[s]` | `src/maglev/maglev-compiler.cc`, `PrintGraph()` |
| **Maglev phase names** | — | `src/maglev/maglev-phase.h`, `PhaseName()` — the authoritative table, see §3 |
| Maglev graph body (blocks, nodes, deopt frames, registers) | `--print-maglev-graph[s]` | `src/maglev/maglev-graph-printer.cc` |
| Maglev deopt frame state (`↱ eager`, `↳ lazy`, `↳ throw`, `VOs`) | `--print-maglev-graph[s]`, verbosity via `--print-maglev-deopt-verbose` / `--trace-deopt-verbose` | `src/maglev/maglev-graph-printer.cc`, `PrintSingleDeoptFrame` / `PrintVirtualObjects` / `PrintExceptionHandlerPoint` |
| Maglev inlining trace (`[ML:id] ⚡ INLINE …`) | `--trace-maglev-inlining` | `src/maglev/maglev-inlining.cc`, `.h` |
| Maglev JSON (roadmap, Tier C) | `--trace-maglev` | `src/maglev/maglev-graph-serializer.cc` |
| Deopt events (`[bailout (kind: …)]`) | `--trace-deopt[-verbose]` | `src/deoptimizer/deoptimizer.cc` (~L873 header, ~L934 code invalidation) |
| Tiering decisions (`[marking … for optimization to …]`) | `--trace-opt` | `src/execution/tiering-manager.cc` |
| Compile lifecycle (`[compiling method …]`, `[completed compiling …]`) | `--trace-opt` | `src/codegen/compiler.cc` |
| OSR (`[OSR - …]`) | `--trace-osr` | `src/codegen/compiler.cc`, `src/runtime/runtime-compiler.cc` |
| TurboFan phase banner + graph | `--trace-turbo-graph` | `src/compiler/pipeline.cc` (~L585) |
| Turboshaft / Turbolev frontend | `--turbolev --print-turbolev-frontend` | `src/compiler/turboshaft/turbolev-frontend-pipeline.cc`, `turbolev-graph-builder.cc` |
| Optimized code disassembly | `--print-opt-code` | `src/compiler/pipeline.cc` → `src/diagnostics/disassembler.cc` |
| Bytecode disassembly | `--print-bytecode` | `src/interpreter/interpreter.cc` → `BytecodeArray::Disassemble` |
| GC events | `--trace-gc` | `src/heap/gc-tracer.cc` |
| Prototype user tracking | `--trace-prototype-users` | `src/objects/` (`JSObject`/`Map` prototype user registration) |

## 2. Flag inventory

Every flag the [plan](plan.md) names was checked against `src/flags/flag-definitions.h` and
against `d8 --help` on the tip-of-tree build. Two were wrong and have been
corrected in the plan:

| The plan (v1) said | Reality |
| :-- | :-- |
| `--trace-ic` | **Does not exist.** IC state transitions go to `v8.log` via `--log-ic` (for `tools/ic-processor`), not to stdout. IC visibility is therefore a `v8.log` ingestion feature — already far-term in PLAN §13 — not a day-one text format. |
| `--trace-prototypes` | **Misnamed.** The flag is `--trace-prototype-users` (`flag-definitions.h`, "Trace updates to prototype user tracking"). |

One flag the plan *omitted* turns out to matter:

- **`--print-maglev-deopt-verbose`** expands `↱ eager @2 (5 live vars)` into the
  full register→node→location frame state. This is the payload journey J3 asks
  for, and it was missing from the plan entirely
  ([spike-findings.md](spike-findings.md) §12).

**Flag implications are not inert.** `flag-definitions.h:854` declares
`DEFINE_WEAK_IMPLICATION(trace_deopt_verbose, print_maglev_deopt_verbose)`, so
`--trace-deopt-verbose` changes the *graph* grammar as a side effect, and adds
`VOs : { … }` lines on top. Any statement of the form "flag X selects format Y"
has to be checked against the implication graph in `flag-definitions.h`, not
just the flag's own definition. Others already relied on:
`--print-maglev-graphs → --print-maglev-graph`, and
`--print-maglev-graph`/`--trace-maglev-phi-untagging`/`--trace-maglev-regalloc`
→ `--maglev-print-bytecode` (all weak).

Everything else in [plan.md](plan.md) §4 verified present: `--print-maglev-graph[s]`,
`--print-turbolev-frontend`, `--trace-turbo-graph`, `--print-opt-code`,
`--print-code`, `--print-bytecode`, `--trace-opt`, `--trace-deopt`,
`--trace-deopt-verbose`, `--trace-osr`, `--trace-gc`, `--trace-turbo`.

Two build-time gates matter:

- `--print-maglev-graph[s]`, `--trace-maglev-phi-untagging`,
  `--trace-maglev-regalloc` and `--print-maglev-deopt-verbose` exist only when
  `V8_ENABLE_MAGLEV_GRAPH_PRINTER` is defined (`BUILD.gn:1585`). Otherwise they
  are `DEFINE_BOOL_READONLY(..., false)` and silently do nothing. A user
  reporting "no output" most likely has a build without it.
- `--print-opt-code` needs `v8_enable_disassembler = true`.

## 3. Maglev phase names are generated, not guessed

`src/maglev/maglev-phase.h` holds a single `PhaseName()` switch that is the sole
source of the `----- <name> -----` banners. As of 15.2.0, in pipeline order:

```
Maglev graph building | Non-eager inlining | Non-eager loop peeling |
Truncation propagation | Truncation | Post optimizer |
Loop optimization (LICM) | Pre phi untagging | Phi untagging |
Escape analysis | Range analysis | AnyUseMarking | Dead nodes sweeping |
During register allocation | Register allocation | Code generation
```

The marker table ([`assets/markers.toml`](../assets/markers.toml)) is populated
from this file rather than from observed output — observed output only shows
the phases a given workload actually ran. **This list is version-specific**: every one of these names
changed between 14.9 and 15.2 (see [spike-findings.md](spike-findings.md) §3),
so the TOML needs a version axis.

Two banners share the grammar but are not phases:

- `----- Bytecode array -----` — the source bytecode dump.
- `----- Inlining 0x… <SharedFunctionInfo name> with bytecode -----` — one per
  inlining site, emitted before the first real phase.

## 4. Regenerating the corpus

```bash
tools/gen-fixtures.sh                      # the builds in DEFAULT_D8S
tools/gen-fixtures.sh /path/to/out/*/d8    # or explicit ones
tools/gen-large-trace.sh                   # 600 MB perf trace, not checked in
```

The script derives version, architecture and V8 git hash from the binary, writes
to `fixtures/<arch>-v<version>/`, and records the exact command line, byte
count, SHA-256 and reproducibility of every file in `manifest.json`. It runs
each command twice and compares, so `"reproducible": true` is a checked fact.

Adding a workload or a flag combination means editing the `FIXTURES` table at
the top of the script — one line per fixture — and re-running.

The corpus deliberately spans three builds so the two axes are separable:

| Directory | Purpose |
| :-- | :-- |
| `arm64-v15.2.0` | tip of tree, primary target |
| `arm64-v14.9.0` | **version** axis (same arch) |
| `x64-v14.9.0` | **architecture** axis (same version) |

## 5. Two generation profiles

| Profile | Flags | Property |
| :-- | :-- | :-- |
| `clean` | `--no-concurrent-recompilation --predictable --no-trace-with-compilation-id` | Byte-reproducible for most formats, including heap addresses. Golden tests should prefer these. |
| `raw` | none | Concurrent recompilation on, volatile `[ML:<id>]` prefixes, threads interleaved. What an engineer actually sees; not reproducible. |

`--predictable` pins the sandboxed V8 heap base (every clean fixture uses
`0x09b8…`, across both architectures and both V8 versions); without it the base
is randomized per run. Wall-clock timings, native pointers, PIDs and isolate
addresses stay volatile regardless — see [spike-findings.md](spike-findings.md)
§10 for the full list, which doubles as the canonicalizer's work list.

The sandbox only covers V8 heap objects: the `(addr:0x…)` host `DeoptFrame*` in
verbose deopt output escapes it and stays volatile even under `clean`, which is
why the `+deoptverbose` fixtures are `"reproducible": false` while their plain
counterparts are not (§12).

## 6. When a printer changes

1. Rebuild d8, re-run `tools/gen-fixtures.sh`.
2. `git diff fixtures/` shows exactly what changed in the output.
3. Golden tests fail; the marker TOML gets a new version entry.
4. If the change is structural (not just a renamed marker), update
   [spike-findings.md](spike-findings.md) and the parser.

If the printer change is *yours*: steps 1–3 are the whole job, and step 3 is
usually one TOML line.

## 7. Cheap upstream wins

Both are one-line printer changes in V8 that would materially simplify `garage`:

- **Print `opt id` in `--trace-opt`.** Maglev already assigns one
  (`compilation_info->set_optimization_id(local_isolate->NextOptimizationId())`
  in `maglev-compiler.cc`), and `--trace-deopt` prints it, but the compile-side
  trace does not. Printing it there turns deopt→compilation correlation from an
  ordinal-matching heuristic into an exact key
  ([correlation-keys.md](correlation-keys.md) §2).
- **Print the Code object address in `[completed compiling …]`.** Same benefit,
  independently.
