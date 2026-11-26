# garage

A terminal UI for V8 engineers to view, navigate, search, and diff `d8`
trace output — Maglev, Turbofan and Turboshaft graphs,
optimized-code disassembly and bytecode listings — without
dumping megabytes into scrollback or switching to browser tools. The tool
`--print-maglev-graphs | less` should have been.

```
d8 --print-maglev-graphs --trace-deopt bench.js | garage
garage trace.log
```

Opens a 490 MB trace in ~0.5 s. Sections are indexed on load; compilations
parse lazily on first view; nothing is ever dropped — anything the parser does
not recognise stays visible as raw text with an `[unparsed]` badge.

For an exhaustive, feature-by-feature reference — what
each one reveals about the compiler, the `d8` flags and GN args it needs, and
every key that drives it — see **[docs/features.md](docs/features.md)**.

## Install

A prebuilt binary, checksum-verified against the release:

```sh
curl -fsSL https://raw.githubusercontent.com/victorgomes/garage/main/install.sh | sh
```

It refuses to install anything whose sha256 does not match the one published
alongside the release, and it is worth reading before you pipe it to a shell.

**To update, use `garage --update`** — it fetches the latest release for your
platform, checks it against the published sha256, and replaces itself in place.
It tells you when you are already current, and if the binary lives somewhere you
cannot write it says so rather than asking for privileges. (It shells out to
`curl` or `wget`, `tar`, and `shasum`; garage makes no network calls of its own
and never checks for updates unless you ask.)

Re-running the install command above does the same thing, and is the way to
update a `garage` old enough not to have `--update`.

A first install with no existing `garage` goes to `~/.local/bin`. Use
`--bin-dir` to choose somewhere else and `--version` to pin a release. If the
binary lives somewhere you cannot write, the script says so and stops rather
than asking for privileges.

Or take a tarball from the [releases
page](https://github.com/victorgomes/garage/releases) by hand.

The Linux builds are statically linked against musl and need no runtime
dependencies; the macOS build is a universal binary. Both keep line tables, so
a panic in a malformed trace still produces a usable backtrace.

Linux and macOS, in a terminal or over SSH, inside or outside tmux. The
terminal handling — including the case where a trace is on stdin and the
keyboard has to come from somewhere else — is exercised on both platforms by
an automated matrix, so `d8 … | garage` stays fully interactive.

## Compile

With V8's GN build (from the V8 checkout root):

```sh
autoninja -C out/x64.release garage          # binary -> out/x64.release/garage
autoninja -C out/x64.release experimental/garage:tests
```

Or standalone with Cargo (`Cargo.toml` resolves crates offline against
`//third_party/rust`):

```sh
../../third_party/rust-toolchain/bin/cargo build --release
```

## Getting a useful trace

```sh
# The one to start with:
d8 --print-maglev-graphs --trace-deopt bench.js | garage

# Cleaner, diffable traces (recommended for graph work):
d8 --no-concurrent-recompilation --predictable \
   --print-maglev-graphs bench.js | garage

# Richer deopt frames (register → node → location):
d8 --print-maglev-graphs --print-maglev-deopt-verbose bench.js | garage

# Turbolev-frontend graphs (Maglev IR in the TurboFan pipeline):
d8 --turbolev --print-turbolev-frontend bench.js | garage

# TurboFan / Turboshaft pipeline graphs, schedules and instruction sequences:
d8 --trace-turbo-graph bench.js | garage

# Optimized-code disassembly and interpreter bytecode:
d8 --print-opt-code bench.js | garage      # needs v8_enable_disassembler
d8 --print-bytecode bench.js | garage

# Graphs *and* the assembly they became — the code dump printed right after
# a pipeline shows up as that compilation's last phases:
d8 --print-maglev-graphs --print-opt-code bench.js | garage
d8 --print-maglev-graphs --print-maglev-code bench.js | garage

# Tiering + OSR story alongside the graphs:
d8 --print-maglev-graphs --trace-opt --trace-deopt --trace-osr app.js | garage
```

Notes:

- `--print-maglev-graphs` and friends exist only in builds with
  `V8_ENABLE_MAGLEV_GRAPH_PRINTER` (on by default in `debug`; check your GN
  args otherwise). Without it they silently print nothing.
- `--print-opt-code` additionally needs `v8_enable_disassembler = true`.
- `--trace-deopt-verbose` *implies* `--print-maglev-deopt-verbose` and changes
  the graph output as a side effect.
- Use `--no-trace-with-compilation-id` when you want two runs to be
  comparable; it strips the volatile `[ML:<id>]` prefixes.
- With concurrent recompilation on (the default), output from several threads
  interleaves; garage degrades the un-demultiplexable parts to raw sections.
  `--no-concurrent-recompilation --predictable` avoids that entirely.

## Keys

| Key | Action |
| :-- | :-- |
| `?` | help (generated from the live keymap) |
| `j`/`k`, `↑`/`↓` | move |
| `h`/`l`, `←`/`→` | focus sidebar ⇄ viewport |
| `Enter` | sidebar: expand/collapse · timeline event: jump to it · viewport: follow branch/jump targets (cycles) |
| `Ctrl+D`/`Ctrl+U`, `PgDn`/`PgUp`, `g`/`G` | paging, top/bottom |
| `I` | inlining decisions panel (Enter jumps to the decision line) |
| `S` | JS source alignment pane (Ctrl+W focuses it; Enter jumps to aligned rows) |
| `b` | show/hide the sidebar (a mid-height `▸` strip marks it when hidden; `h` or a click reopens) |
| `c` | sidebar: chronological ⇄ grouped by function |
| `f` | filter sidebar by regex |
| `/`, `n`, `N` | regex search (incremental), next/previous match |
| `Space` | fold/unfold the basic block under the cursor |
| `[` / `]` | previous / next block header |
| `z` | fold all blocks in view / unfold them all |
| `i` | jump to input definitions (cycles) · bytecode: jump target / `FBV[n]` slot / pool entry |
| `u` | cycle consumers of the node (or slot / pool entry / jump target) the cycle started from |
| `Ctrl+O` / `Ctrl+I` | jump history back / forward |
| `t` | show/fold interleaved trace annotations |
| `y` / `Y` | copy cursor line / whole section (OSC 52 over SSH/tmux) |
| `E` | export the current view as Markdown |
| `w` | wrap long lines · `<`/`>` scroll horizontally |
| `F` | follow the stream end (on for piped input) |
| `Tab` | timeline ⇄ compilation list |
| `:` | command palette (Tab completes) |
| `v` / `s` | vertical / horizontal split (same key closes) |
| `Ctrl+W` | focus the other pane |
| `d` | phase diff mode |
| `D` | dual-run diff: this function vs the other loaded trace |
| `}` | next source (multi-file runs) |
| `q`, `Ctrl+c` | quit |
| `Esc` | back / unfocus |

## Timeline & commands

`Tab` swaps the sidebar for the **timeline**: every `--trace-opt` /
`--trace-deopt` / `--trace-osr` event in stream order, colour-coded by
severity (deopts red, completions green, OSR yellow). Selecting a deopt shows
its whole bailout block — under `--trace-deopt-verbose` that is the full
frame-unwinding dump, styled — and `Enter` jumps to the correlated
compilation at the deopt's bytecode offset, per the `(SFI, tier, ordinal)`
rule in [docs/correlation-keys.md](docs/correlation-keys.md). Events that cannot be resolved honestly
(no graph in the trace, Turbofan-only deopts) say so instead of guessing.

`:` opens the command palette (Tab completes, unambiguous prefixes are accepted, the status line shows live
candidates):

| Command | Effect |
| :-- | :-- |
| `:deopts` | timeline, narrowed to deopt events, with a count |
| `:checks` | lens: only guard nodes (`Check*`/`Assert*`/`*Deopt*`), counted |
| `:phi` | lens: the control/phi backbone of the graph |
| `:spill` | lens: regalloc spill/reload traffic |
| `:megamorphic` | lens: megamorphic feedback and slow-path ICs |
| `:function <re>` | filter the sidebar (same as `f`) |
| `:copy`, `:export <file>` | clipboard / Markdown export |
| `:q`, `:quit` | quit |
| `:clear` | drop the lens and the timeline narrowing |

A lens filters the modeled view down to the banner/block skeleton plus
matching lines; any jump whose target a lens hides clears the lens rather
than failing.

## Splits & the phase diff

`v`/`s` split the viewport; each pane has its own section selection (the
sidebar drives the focused pane, `Ctrl+W` switches), and the same key closes
the split again.

`d` is the reason splits exist. On a phase row it diffs that phase against
the previous graph phase; on a compilation it diffs the first graph phase
against the last; with a split already open it diffs whatever graph phases
the two panes show — including phases of *different* compilations of the
same function. The diff is **node-identity based**, never a raw text diff:

- nodes match by their `nN` id within a compilation (V8 preserves it across
  phases), or by structural hash (opcode + canonically renumbered inputs)
  across compilations;
- registers, schedule ids, live ranges and `, N uses` decorations do not
  count as changes — a node is `~ changed` only when its opcode or its
  actual inputs changed;
- `nA: Identity [nB]` replacements render as `→ n5 replaced by n12`, and a
  consumer whose inputs merely followed such a replacement is reported as
  "rerouted via Identity", not as a real input change;
- a node that moved blocks is `≈ moved`, not deleted-and-added;
- non-node lines fall back to a canonicalized text diff (addresses, `[ML:…]`
  ids and timings masked).

The two columns are row-aligned (scrolling is synced by construction), the
gutter and row tint encode the status (`+` `−` `~` `→` `≈`), the status line
shows the summary counts and the story of the row under the cursor, and
`Y`/`E` export the aligned view with its gutters.

The mouse works too: the wheel scrolls the pane under the pointer, and a
left click focuses a pane and places the selection or cursor — clicking a
node line lights up its def-use highlighting. Mouse capture takes over the
terminal's own text selection; use Shift-drag (Option-drag in Terminal.app)
for native selection, or `y`/`Y`/`E` to copy through garage itself.

Every binding is remappable in `~/.config/garage/config.toml` (or
`--config <path>`):

```toml
[keys]
quit = ["x"]              # frees q
half-page-down = "Ctrl+f"
```

A key bound to two actions is a startup error, and a config typo is a normal
error message — the alternate screen only opens once the config is valid.

## Bytecode arrays & feedback vectors

Maglev prints the bytecode array and the feedback vector ahead of every
graph, and `--print-bytecode` emits the same rows for Ignition; garage
parses them rather than dimming them. Rows are styled like graph nodes
(mnemonic by shape, jumps blue, constants magenta), feedback-slot headers
colour by IC state — MEGAMORPHIC red, POLYMORPHIC yellow, MONOMORPHIC
green — and the `:megamorphic` lens counts them.

The def-use chain works here the way it does in a graph. On a bytecode
row, `i` cycles through what its operands reference: the jump target
(`(0x… @ N)` suffixes and switch `{ 0: @44, … }` tables), each `FBV[n]`
slot in the feedback vector below, each `[n:…]` constant-pool entry.
On a slot, pool entry, or jump-target row, `u` cycles the rows that
reference it. The cursor highlights the same links in place, and
`Ctrl+O` unwinds any jump. (Bare `[n]` operands are immediates and
`EmbeddedFeedback[n]` is not a vector slot — neither pretends to
navigate.)

## Assembly as part of the pipeline

A code dump printed right after the pipeline that produced it merges into
that compilation — both the `--print-opt-code` form (`--- Optimized code
---`) and the bare `0x…: [Code]` object print of `--print-maglev-code` /
`--print-code` — `Raw source`,
`Optimized code`, `Instructions`, … appear as its last phases, so one
sidebar entry tells the whole story from bytecode to machine code. The
merge follows the no-guessing rule: sections must be line-adjacent and
agree on tier and name; anything else stays separate.

The `Raw source` block renders as JavaScript — keywords, strings,
numbers and comments styled by a small JS tokenizer, identifiers left
plain.

Inside the listing, branch targets are resolved from what V8 printed —
arm64's `(addr 0x…)` and x64's `<+0x…>` both — and their destination rows
get their offset column labelled: those are the de-facto basic blocks.
`[`/`]` walk them, `i` on a branch jumps to its destination, `u` on a
destination cycles the branches that land on it, and the cursor lights up
the link in both directions — the same def-use navigation graphs and
bytecode listings have.

## Dual-run comparison

Load two traces (`garage a.log b.log`) and the telemetry bar shows both
runs' headline numbers side by side. `D` on a compilation diffs it
against the same function in the other run — matched by name and tier
(SFI addresses are not comparable across runs; nodes match by structural
hash, as in any cross-compilation diff). On a phase row the same-named
phase is preferred on the other side; otherwise both sides use their
last graph phase — which is also what makes cross-*version* comparisons
work when V8 renamed the banners. The active run is always the right
pane; `d` (or `D`) drops back out.

## Source alignment

`S` opens the **source pane**: the compilation's script, resolved from the
path V8 printed — as-is, relative to the trace file's directory, or one
directory up (where `d8` usually ran). When no file resolves, the
compilation's own `Raw source` block stands in, aligned through the code
dump's `source_position`; when neither exists the status line says so.

Alignment runs on two grains, honestly labelled by what the trace
contains: `NNN S>` bytecode markers give per-row character positions
(interleaved graph rows inherit them through their bytecode offset), and
SFI context lines give per-function anchors as the fallback. The trace
cursor keeps its source line highlighted and centred; `Ctrl+W` (or `l`)
focuses the pane, parks its cursor on that line, and `Enter` cycles
through the trace rows aligned with the line under the cursor —
`Ctrl+O` unwinds, as everywhere.

## What the UI shows

- **Telemetry bar** — compilations per tier, OSR count, deopt count, detected
  V8 version (inferred from the phase-banner vocabulary).
- **Sidebar** — compilations (`name · tier · #ordinal`, OSR and `[unparsed]`
  badges) interleaved with raw sections, chronological or grouped by function
  (grouped mode keys on the SFI address, so an empty function name is fine).
- **Viewport** — the trace itself, styled from the parse: guards red, control
  flow blue, constants magenta, phis yellow, interleaved source bytecode dim.
  The box-drawing gutter that draws block edges gets its own colour per
  column — an edge keeps one colour for its whole length, and neighbouring
  edges differ — independent of what the rows it crosses contain.
  The cursor node's definition, inputs, and consumers highlight in distinct
  colours; deopt frames show which registers hold which nodes.

## CLI

```
garage [FILE...]              files (mmapped; several files = } to switch)
garage a.log b.log            dual-run comparison (D diffs a function)
d8 ... | garage               piped stdin; keyboard input still works
garage -- d8 ... bench.js     wrapper mode: spawn d8, stream it live
garage --function '^process'  index-time prefilter on function names
garage --debug                write diagnostics to garage.log (--log-file)
garage --update               replace this binary with the latest release
```

Wrapper mode owns the child: stdout and stderr merge in arrival order
(the interleaving a terminal would show), a non-zero exit lands as a
visible note at the end of the trace, quitting garage terminates the
child, and SIGTERM/SIGINT/SIGHUP restore the terminal before dying.

## More

- **[docs/features.md](docs/features.md)** — every feature, one by one: what it
  shows you about the compiler, the flags it needs, and the keys that drive it.
- **[docs/correlation-keys.md](docs/correlation-keys.md)** — the rule that ties
  a deopt to the compilation it came from, and when `garage` refuses to guess.
- **[docs/development.md](docs/development.md)** — building, testing and
  hacking on `garage` itself.

Bug reports are much easier to act on with the trace attached — `garage --debug`
writes diagnostics to `garage.log`, and `E` exports the view you were looking at.
