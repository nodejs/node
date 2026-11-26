# `garage` — Interactive TUI Tool for V8 Traces and Logs

## 1. Vision

`garage` is a terminal UI for V8 engineers to view, navigate, search, and diff `d8`
trace output (Maglev, Turboshaft/Turbolev, TurboFan, Ignition, deopts, ICs, GC)
without dumping megabytes of text into scrollback or switching to browser tools.

**Target user, concretely:** a V8 compiler engineer who today pipes
`--print-maglev-graphs` output into `less` twenty times a day. `garage` v0 must beat
`less` for that workflow. Everything else builds on that.

## 2. Non-Goals

Stating these explicitly to keep scope honest:

- **No 2D graph layout.** `garage` renders V8's textual output order, enriched with
  navigation and highlighting. Turbolizer remains the tool for visual graph layout.
- **Not a profiler.** No flamegraphs, no tick-sample aggregation UI (ingesting
  `v8.log` for cross-referencing is a roadmap item, not a goal of the core tool).
- **No Wasm support initially.** JS pipeline only until the JS experience is solid.
- **No log mutation.** `garage` is a viewer; it never rewrites trace files.

## 3. Design Principles

1. **Never lose data.** Anything unparsed lands in a raw, searchable, foldable
   fallback section. A parse failure degrades one compilation to raw text — never
   the whole session.
2. **The text output is the native input.** `--print-maglev-graphs` and friends
   are what V8 engineers actually have in front of them — on any build, with no
   extra flags — so first-class text parsing is the foundation of the tool.
   Format drift is managed with fixtures, golden tests, and graceful degradation
   (§6), and softened by the fact that the audience maintains the printers.
3. **Fast on huge logs.** Traces reach hundreds of MB to GB (fuzzer bisects,
   benchmark suites). Index sections on load; parse compilations lazily on first
   view. Opening a 1 GB trace must feel instant.
4. **Works over SSH.** The likely audience works on remote workstations. That
   means: OSC 52 clipboard (native clipboard via `arboard` fails over SSH), fully
   keyboard-driven (mouse optional), 256-color and 16-color fallback, tmux-safe.
5. **One coherent, remappable keymap.** Vim-flavored, no double-bound keys,
   user-overridable via config file.

## 4. Supported Inputs & Auto-Detection

Auto-detect and categorize output from `d8` flags, including:

- **Compiler graphs:** `--print-maglev-graphs`, `--print-maglev-graph`,
  `--print-maglev-deopt-verbose`, `--print-turbolev-frontend`,
  `--trace-turbo-graph`, `--print-opt-code`, `--print-code`, `--print-bytecode`
- **Tiering & lifecycle:** `--trace-opt`, `--trace-deopt`, `--trace-deopt-verbose`,
  `--trace-osr`, `--trace-prototype-users`, `--trace-gc`
- **Pass tracing:** `--trace-maglev-inlining`, `--trace-maglev-graph-building`,
  `--trace-maglev-regalloc`, `--trace-maglev-phi-untagging` (see §6.1)
- **Optional structured:** Turbolizer `.json` files from `--trace-turbo`
  (roadmap; supplemental source, nothing in the core depends on it)
- **Fallback:** any other flag from `flag-definitions.h` via the generic
  ANSI-preserving raw stream view

> **Verified** against `flag-definitions.h` at V8 15.2.0 — see
> [printer-parser-contract.md](printer-parser-contract.md) §2. Two v1
> entries were wrong: `--trace-ic` does not exist (IC transitions go to `v8.log`
> via `--log-ic`, which makes IC visibility a `v8.log`-ingestion feature, §13
> far-term), and `--trace-prototypes` is really `--trace-prototype-users`.
> One was **missing**: `--print-maglev-deopt-verbose`, which expands deopt
> frames from `(N live vars)` to the full frame state (§6.1,
> [spike-findings.md](spike-findings.md) §12).
> Note also that the Maglev graph printer flags require a build with
> `V8_ENABLE_MAGLEV_GRAPH_PRINTER`; without it they silently do nothing.
>
> Flags are not independent: `flag-definitions.h` weak-implications mean
> `--trace-deopt-verbose` also turns on `--print-maglev-deopt-verbose`, so
> "which flag selects which format" must be read off the implication graph, not
> the flag definition alone.

### 4.1. Input sources & pitfalls

- **Piped stdin** (`d8 ... | garage`): when stdin is the data pipe, keyboard input
  must come from the controlling terminal. This is a day-one requirement, not
  polish.

  > **Corrected in Phase 1.** "Read `/dev/tty`" is the wrong instruction on
  > macOS: a descriptor opened from `/dev/tty` cannot be registered with
  > `kqueue`, so crossterm's own fallback silently yields a UI that receives no
  > keys at all. The controlling terminal has to be reached by its real device
  > path — which `ttyname(1)`/`ttyname(2)` still report even when stdin is a
  > pipe — and `dup2`'d onto fd 0. Details and measurements in
  > [build-log.md](build-log.md) §1.5 and `src/tty.rs`.
- **Files** (`garage trace.log`, `garage a.log b.log` for dual-run mode).
- **Wrapper mode** (`garage -- d8 flags... script.js`): spawn `d8`, capture stdout
  and stderr with an explicit merge policy (tagged per-stream, ordered by arrival),
  stream into the UI live.
- **Interleaving is a day-one parsing problem, not a far-term feature.** With
  concurrent recompilation, graph dumps and trace lines from multiple threads can
  interleave. Strategy: (a) document that `--no-concurrent-recompilation
  --predictable` yields clean traces and recommend it for graph work; (b)
  best-effort demultiplexing by isolate/thread markers where present; (c) anything
  un-demultiplexable degrades to raw sections per principle 1.

## 5. Data Model & Navigation

### 5.1. Primary hierarchy: `Function × Tier` → `Phase` → `View`

Top level is a flat, chronologically sorted list of compilation instances (this is
the right call — it matches how engineers think about tier-up sequences):

```
[1] foo() @ script.js:12  [Ignition Bytecode]
[2] foo() @ script.js:12  [Maglev #1]
     ├── Phase: After graph building
     ├── Phase: After phi untagging
     ├── Phase: After register allocation
     └── Code generation (disassembly)
[3] foo() @ script.js:12  [Maglev #1, OSR @ offset 132]
[4] foo() @ script.js:12  [Deopt: eager]
[5] foo() @ script.js:12  [Turboshaft #2]
```

Additions over the original model:

- **OSR compilations are first-class.** They carry an OSR bytecode offset and must
  be distinguishable from regular tier-up compilations in the sidebar.
- **Grouping toggle.** Pure chronological order does not scale to thousands of
  compilations. Provide a sidebar toggle: chronological ⇄ grouped-by-function.
- **Correlation keys are specified, not assumed.** See
  [correlation-keys.md](correlation-keys.md). Short version: the deopt
  line carries `opt id` and the Code address, but neither is printed on the
  compile side, so the usable key is `(SFI address, tier, ordinal)` with
  `bytecode offset` selecting the location inside the compilation. Unresolvable
  events render as timeline entries with the jump disabled — never as a guess.
- **Source resolution strategy.** Traces contain source positions, not source text.
  Load `.js` from disk when the script path resolves; otherwise degrade gracefully
  (offsets still shown, source pane disabled) rather than failing.

### 5.2. Timeline view

A global chronological event log (`--trace-opt`, `--trace-deopt`, IC updates, GC),
with ordinals (`#001`, `#002`, …). Selecting an event jumps to the corresponding
compilation/phase via the correlation keys above.

## 6. Parser Strategy & Resilience

Parsing V8's text output is the highest-risk area of the project — and, by
design, it is also the point: `--print-maglev-graphs` output is what engineers
have on any build with no extra flags, so `garage` treats it as the primary,
first-class input rather than a stopgap.

**Tier A — known text formats:** Maglev graphs first; then deopt/opt traces,
Turboshaft, disassembly. Parsed by table-driven matchers (section markers in an
embedded TOML). Honest caveat the original plan glossed over: a TOML table
handles *renamed markers*, but new node syntax still requires code changes. The
TOML is a maintenance aid, not magic. Two things keep this tractable: the
fixture corpus below, and the fact that the audience maintains the printers —
when Maglev output changes, the same people can update `garage` in the same
breath (or keep the printer stable because a tool now depends on it).

> **Phase 0 raised the stakes on this.** Between V8 14.9 and 15.2 — one quarter
> apart — *every* Maglev phase banner was renamed and the compilation banner
> changed shape. So: the marker table needs a **version axis**, the compilation
> anchor is the `Compiling 0x… with <Tier>` line (present in both versions), not
> the `Begin compiling method` banner, and node-line syntax varies *per phase*
> within one compilation. Architecture, by contrast, affects only register
> names. Full findings — and the ANSI-in-piped-output surprise — in
> [spike-findings.md](spike-findings.md).

**Tier B — raw fallback:** everything unrecognized. Searchable, foldable, never
dropped.

**Tier C — optional structured sources:** Turbolizer `--trace-turbo` JSON can be
imported later behind the same parser trait (handy for cross-phase node identity
in the diff engine), but nothing in the core design depends on it.

**Resilience plan:**

1. A checked-in **fixture corpus**: real `d8` outputs from tip-of-tree and at least
   one stable branch, on x64 and arm64, for every supported flag. This exists
   *before* the parsers do (see TODO Phase 0).
2. Golden-file tests over the corpus; fuzz the parsers with garbage input (a
   malformed trace must never panic the TUI).
3. Per-section graceful degradation with a visible "unparsed" badge in the sidebar.
4. CI job in V8 (or a cron against tip-of-tree) to catch format drift early.

### 6.1. Interleaved pass tracing (`--trace-maglev-inlining` & friends)

Pass-specific debug flags print free-form diagnostic lines *between and inside*
graph dumps. Treating them as parse errors — or exiling them to an orphan raw
section — would destroy exactly the context that makes them useful. Instead:

- **The section model is a context tree, not a flat partition.** The indexer
  assigns every line to the innermost open context: run → compilation → phase →
  block → node. A line matching no structural grammar becomes a **positioned
  annotation** attached to that context — inlining-trace lines land on the
  compilation they were printed in, regalloc lines on the node they follow.
- **Annotation channels.** Annotations are classified by line prefix into
  channels (`inlining`, `regalloc`, …) via an optional TOML prefix map; unknown
  prefixes fall into a generic channel. No per-flag parser is required — new
  `--trace-maglev-*` flags work on day one.
- **Rendering.** Annotations render dimmed/italic like code comments, folded by
  default (`[+] 12 trace lines (inlining)`); `t` toggles inline visibility,
  `:trace <channel>` filters by channel.
- **Node linking.** Any annotation mentioning a node ID (`n42`) is cross-linked:
  cursor highlighting includes it, and it feeds the node biography view (§7.11).

> **Corrected in Phase 0.** v1 of this plan used `--trace-maglev-truncation` as
> the exemplar. It isn't one: its output is byte-identical to plain
> `--print-maglev-graphs`, because the truncation information is a node-line
> suffix the graph printer emits unconditionally. The flags that genuinely
> interleave are `--trace-maglev-inlining` (free-form lines attached to a
> compilation *before any phase opens* — the attachment rule must handle that
> case), `--trace-maglev-graph-building`, `--trace-maglev-regalloc` and
> `--trace-maglev-phi-untagging`.
>
> Channel prefixes are real (`[ML:<compilation-id>]`, `[TLV:<id>]`) but must not
> be load-bearing: `--no-trace-with-compilation-id`, the flag engineers already
> use to get diffable traces, removes the prefix entirely. Classify on it when
> present; fall back to the enclosing context otherwise.
>
> One structural correction: deopt-frame lines (`↱ eager @2 (5 live vars)`, with
> `│` depth for inlining) are the *most common* non-node line in the corpus, and
> they are IR structure, not annotations. They attach to the preceding node and
> need their own type — folding them away by default would hide real content.
>
> And `(5 live vars)` is only a summary. `--print-maglev-deopt-verbose` prints
> the whole frame state (`{<closure>:n3:[constant:v-1], a0:n2:[stack:-7|t], …}`),
> which is the frame state J2 ultimately wants; `--trace-deopt-verbose` switches it on
> implicitly and adds `VOs : { … }` lines. A third arrow, `↳ throw`, is printed
> unconditionally with a *fourth* payload syntax. Frames are shared and carry an
> address that interns them (5.5:1 in the corpus). See
> [spike-findings.md](spike-findings.md) §12.

**Scope now vs later:** only the attachment rule and folded rendering ship
early — that part is structural and expensive to retrofit. Channels, `:trace`
filtering, and node linking are icebox items layered on top later.

## 7. Key TUI Features

### 7.1. Graph view & semantic node highlighting

Cursor-based (not "hover" — this is a terminal; mouse is optional, never required):

- Placing the cursor on a node (`n55`/`v12`) highlights its **definition**, its
  **inputs** (def-use), and its **consumers** (use-def) in distinct styles.
- `i` jumps to input definition; `u` cycles through consumers; `Ctrl+O`/`Ctrl+I`
  navigate the jump history.
- Syntax highlighting for opcodes, block labels, registers, node IDs, type
  annotations.

### 7.2. Basic block folding & node filtering

- `Space` folds/unfolds basic blocks (`[+] b1 (loop) — 14 instructions hidden`).
- Command-palette filters: `:phi` (control/phi backbone only), `:check` (highlight
  and count guards: `CheckMaps`, `CheckSmi`, `CheckBounds`), `:spill` (regalloc
  spills/reloads), `:megamorphic` (slow-path ICs).

### 7.3. Split panes

- `v` vertical split, `s` horizontal split, `Ctrl+W`+direction to move focus.
- Typical use: Phase 1 vs Phase 4 of the same compilation side by side.

### 7.4. Diff engine (intra-run and dual-run)

**Design correction from v1:** raw Myers line diff will mark nearly everything as
changed, because node IDs renumber between phases and runs. The diff pipeline is:

1. **Canonicalize** both sides: strip/normalize memory addresses, timestamps,
   compilation ids; renumber node IDs into a canonical space. The exact list of
   what varies between two runs of the same binary — timings, native pointers,
   PID/isolate, `[ML:<id>]` prefixes — is measured in
   [spike-findings.md](spike-findings.md) §10. Useful discovery: under
   `--predictable` the sandboxed V8 heap base is pinned, so `<JSFunction … (sfi
   = 0x…)>` addresses are *stable* and comparable across runs. Recommending that
   flag pair turns most of the matching problem into exact-address matching.
2. **Match nodes by identity** where the format preserves IDs across phases
   (Turbolizer JSON does; Maglev text partially does), else by structural hash
   (opcode + canonicalized inputs).
3. **Fall back to line diff** (`similar` crate) only for unmatched regions.

Applies to both phase-vs-phase diff (`d`) and dual-run mode
(`garage baseline.log patched.log`), which additionally shows a telemetry
comparison header (compilation counts, deopt counts, guard counts per function).

### 7.5. JS source & bytecode alignment

- `S` opens JS source / Ignition bytecode panes aligned with the IR view.
- Cursor on an IR node cross-highlights the bytecode instruction and JS line via
  bytecode offsets; selecting a JS line highlights all IR nodes derived from it.
- Subject to the source resolution strategy in §5.1.

### 7.6. Disassembly view

- For `--print-opt-code` / codegen phases: jump-target highlighting for branches,
  and a register trace (cursor on `rax`/`x0` highlights all reads/writes in view).
- **Feasibility note (new):** inline decoding of tagged addresses into field names
  is only possible to the extent V8's disassembly comments already provide it; do
  not promise decoding that requires heap metadata the log does not contain. The
  same applies to IC timing values and exact register-pressure levels elsewhere in
  this plan — features are tagged *[needs data not in text logs]* and gated on
  structured input or V8-side changes.

### 7.7. Inlining tree

- `I` shows the inlining decision tree (inlined/rejected, reasons, costs) parsed
  from trace output; selecting an inlined callee filters the graph view to its
  blocks.

### 7.8. Telemetry header & command palette

- Top bar: compilation counts per tier, deopt count, GC events, source status
  (file/stream/live).
- `:` palette with completion: `:deopts`, `:checks`, `:spill`, `:phi`,
  `:megamorphic`, `:copy`, `:export <file>`, `:function <name>`.

### 7.9. Export & clipboard (moved up from "medium-term" — cheap and high value)

- `:copy` copies the current selection/phase — via OSC 52 when running over
  SSH/tmux, `arboard` locally.
- `:export report.md` writes the current annotated view as Markdown for bug
  tickets and Gerrit CL comments.

### 7.10. Deopt frame unwinding panel

- With `--trace-deopt-verbose`: render the reconstructed interpreter frame
  (registers, stack slots, materialized objects) for the selected deopt event.
- The *compile-side* half of this is already in the graph: the same flag implies
  `--print-maglev-deopt-verbose`, so each node's frame lists every live register
  with its producing node and location, and `VOs : { … }` gives the virtual
  objects that would be materialized. Showing the two side by side — what Maglev
  said it would restore, next to what the deoptimizer actually restored — is the
  real feature here, and it needs no new input format.

> **§7.11–§7.15 are unscheduled roadmap ideas** — documented so the core design
> (annotation model, canonicalizer, parser reuse) keeps them cheap to add later,
> but deliberately absent from the TODO's build phases (see its icebox section).

### 7.11. Node biography — "explain this node" (`e`)

Press `e` on any node to see its chronological story across the compilation,
assembled from its creation site, phase diffs, and every annotation (§6.1) that
mentions it:

```
n42 [Phi]
  Phase 1   created in b3 (loop header)
  Phase 3   cannot truncate to int32 [-∞, +∞]
  Phase 4   untagged → Int32
  regalloc  spilled at gap 118                                [trace:regalloc]
```

This is the payoff of the annotation model: arbitrary `--trace-maglev-*` flags
become per-node explanations of *why* a phase diff looks the way it does,
without `garage` understanding each flag. Node IDs inside the panel are
themselves navigable (`Enter` jumps to `n17`).

### 7.12. Loop lens (`:loops`)

Optimization work is loop work. `:loops` lists every loop across compilations
with per-loop stats — node count, checks, spills, phis, deopt points inside the
body — and jumps straight into loop bodies. Composes with `:check`/`:spill`.

### 7.13. Deopt flip-flop detector

From `--trace-opt`/`--trace-deopt` alone, flag functions caught in
optimize → deopt → reoptimize cycles (N opts / M deopts) with a sidebar badge
and a telemetry-bar count. One of the most common perf pathologies, essentially
free to compute.

### 7.14. Watch mode (`garage --watch -- d8 ...`)

The daily compiler-dev loop: edit C++/JS, rebuild, rerun, eyeball the graph.
`--watch` re-runs the wrapped command when inputs change and — the important
part — **restores your position** (function → phase → nearest matching node) in
the new trace, with a per-compilation `changed`/`unchanged` badge versus the
previous run (reusing the dual-run canonicalizer, §7.4).

### 7.15. Non-interactive mode (`garage stats|grep|diff`)

The same parsers without the TUI, for scripts and CI:

- `garage stats trace.log` — compilations per tier, deopts, flip-flops (JSON).
- `garage grep --node CheckMaps --in-loops trace.log` — structural queries.
- `garage diff --summary base.log patched.log` — non-zero exit on regressions
  (guard counts, deopt counts), usable in presubmits and benchmark CI.

## 8. Keyboard Map (fixed)

Conflicts in v1 resolved: help is `?` only (`h` was double-bound); `s` added.

| Key | Action |
| :-- | :-- |
| `?` | Help modal (keys + commands) |
| `j`/`k`, `↑`/`↓` | Move cursor / scroll |
| `h`/`l`, `←`/`→` | Focus sidebar ⇄ viewport |
| `Enter` | Select function / phase / event |
| `Tab` | Toggle compilation list ⇄ timeline view |
| `v` / `s` | Vertical / horizontal split |
| `d` | Phase diff mode |
| `S` | JS source / bytecode alignment pane |
| `A` | Disassembly view |
| `I` | Inlining tree |
| `Space` | Fold / unfold basic block |
| `i` / `u` | Jump to inputs / cycle consumers |
| `e` | Node biography ("explain this node") |
| `t` | Toggle inline trace annotations |
| `Ctrl+O` / `Ctrl+I` | Back / forward in jump history |
| `/`, `n`, `N` | Regex search, next, previous |
| `f` | Filter sidebar entries |
| `g` | (on deopt event) jump to guard/IR location |
| `:` | Command palette |
| `q` / `Ctrl+c` | Quit |
| `Esc` | Back / unfocus |

All bindings remappable via config file (`~/.config/garage/config.toml`).

## 9. MVP Definition (explicit)

The MVP is deliberately narrow. It ships when:

`d8 --print-maglev-graphs --trace-deopt bench.js | garage` gives you:

1. Sidebar of `Function × Tier` compilations (incl. OSR), chronological + grouped
   toggle, quick filter.
2. Phase view with syntax highlighting, block folding, def-use/use-def cursor
   highlighting, `i`/`u`/history jumps.
3. Regex search with match navigation.
4. Raw fallback sections for everything unparsed, searchable.
5. `:copy` (OSC 52 + local) and `:export`.
6. Instant open on multi-hundred-MB files (indexed, lazy parse).
7. Interleaved pass-trace lines (`--trace-maglev-*`) kept in place as folded,
   dimmed annotations — never dropped or orphaned (§6.1, minimal form only:
   attachment + folding, no channels or linking).

**Definition of done:** the author stops using `less` for daily Maglev work.

Everything else — timeline, diffs, splits, source alignment, dual-run, wrapper
mode, Turboshaft/disassembly parsers — is post-MVP and sequenced in the [build log](build-log.md).

## 10. User Journeys

### J1: Investigating Maglev optimization passes
1. `d8 --print-maglev-graphs bench.js | garage`
2. Select `compute() [Maglev #1]` → `After phi untagging`.
3. `v` split against `After graph building`, `d` to diff.
4. Canonicalized diff shows exactly which `Phi` nodes became `Int32`; cursor on
   `n32` highlights its loop-header inputs.

### J2: Deopt root-cause
1. `d8 --trace-deopt --trace-opt --print-maglev-graphs app.js | garage`
2. `Tab` → timeline; red `[DEOPT eager] processArray() @ app.js:88`.
3. `Enter` shows reason (`wrong map @ bytecode offset 14`); `g` jumps to the
   corresponding compilation at that offset (via §5.1 correlation keys).
4. The node at that offset carries its deopt frame. With
   `--print-maglev-deopt-verbose` (or `--trace-deopt-verbose`, which implies it)
   that frame lists every live register and where its value lives — the state
   the interpreter resumes with. `garage` renders the shared frame once and
   references it from each node.

### J3: Live iteration
1. `garage -- d8 --print-maglev-graphs --trace-deopt test.js`
2. Sidebar populates as compilations finish; `:deopts` confirms whether the run
   deopted; `r` (roadmap) edits flags and re-runs without leaving the TUI.

### J4: Dual-run CL verification
1. `garage baseline.trace patched.trace`
2. Telemetry header compares counts; `:checks` on both sides shows `CheckMaps`
   dropping 14 → 3 in the patched run, with per-function drill-down.

### J5 (new): Parser breaks on a new V8 version
1. Tip-of-tree renames a Maglev phase banner.
2. `garage` still opens the trace; the affected compilation shows an `[unparsed]`
   badge and its content is available as a raw, searchable section.
3. User keeps working, then either edits the marker TOML or files an issue with
   the offending fixture attached. Nothing is ever silently dropped.

### J6 (new): 1.5 GB fuzzer trace
1. `garage huge.trace` opens in seconds: sections are indexed, not parsed.
2. `:function processArray` (or `garage --function processArray huge.trace`)
   narrows the sidebar to one function's compilations; only viewed phases are
   parsed.

### J7 (new): Remote workstation over SSH
1. Engineer works on a remote Linux box via SSH + tmux.
2. Colors degrade correctly, no mouse needed, `:copy` uses OSC 52 so the snippet
   lands in the *local* clipboard, `:export` writes Markdown pasted into a Gerrit
   comment.

### J8 (new): Why wasn't this callee inlined?
1. `d8 --print-maglev-graphs --trace-maglev-inlining bench.js | garage`
2. The graph for `outer()` has a call to `big()` that was expected to disappear.
3. `t` unfolds the compilation's trace annotations: `❌ SKIP big — big function,
   size (174) >= max-size (100)`, attached to exactly that compilation. `e` on
   the call node continues into its biography.

## 11. Technology & Implementation Notes

- **Rust + `ratatui` + `crossterm`.** Right choice: single static binary,
  ecosystem standard, fast.
- **Concurrency:** a reader/parser thread feeding the UI via channels is
  sufficient; adopt `tokio` only if wrapper-mode process management genuinely
  needs it. Don't cargo-cult the async runtime.
- **Rendering:** event-driven redraw (on input / new data / resize), not a fixed
  60 fps tick loop burning CPU in an idle terminal.
- **Memory:** index + lazy parse (§3.3); intern opcode/register strings; drop the
  v1 plan's unsubstantiated "gigabytes per second" claim and instead set a
  measurable target: open 500 MB in < 2 s, cursor latency < 16 ms.
- **Dependencies (MVP):** `ratatui`, `crossterm`, `clap`, `regex`, `memmap2`,
  `anyhow`/`thiserror`, `similar` (later), `arboard` (later, with OSC 52 first).
  Add the rest when a phase actually needs them.

## 12. Success Criteria

1. Author and ≥2 colleagues use it daily instead of `less` within a month of MVP.
2. Opens a 500 MB trace in under 2 seconds; UI stays responsive while streaming.
3. A V8 format change never crashes or blanks a session — worst case is a raw
   section with an `[unparsed]` badge.

## 13. Roadmap

### Near-term (post-MVP)
- Timeline view + deopt→graph jump (needs correlation-key design, §5.1).
- Split panes + canonicalizing phase diff (§7.4).
- Turboshaft/Turbolev text parser; disassembly parser.
- Inlining tree (`I`).
- Node lineage/ancestry across phases (`L`) — scope after the diff canonicalizer
  exists; needs origin info in the printed output (or Turbolizer JSON).

### Medium-term
- Dual-run diffing; wrapper/live mode with flag editing + re-run (`r`).
- Icebox ideas as appetite allows: annotation channels + node biography (`e`),
  loop lens, flip-flop detector, watch mode, non-interactive `stats`/`grep`/
  `diff` (§7.11–§7.15).
- JS source / bytecode alignment (`S`).
- Deopt frame unwinding panel; representation-transition ("boxing ping-pong")
  visualizer *[needs data audit]*.
- Turbolizer JSON import as an optional supplemental source.
- Bookmarks, notes, Markdown session export.

### Far-term
- IC state visualizer, register pressure heatmap *[both need extra data in the
  trace output — small printer-side additions in V8]*.
- Escape-analysis panel; isolate/thread selector UI; scriptable checks
  (`garage --check '...'`); `v8.log` ingestion; graph projection modes; minimap.
