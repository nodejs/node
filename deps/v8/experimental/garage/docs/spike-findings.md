# Measured V8 output behaviour

What a throwaway indexer learned from running over the whole fixture corpus
before any of the real parsers existed. Several assumptions in the original
[plan](plan.md) turned out to be wrong, and the findings below are why the
parsers are shaped the way they are — most of them are cited directly from the
code that encodes them.

**This document is load-bearing.** Read it before changing a parser.

Every claim is reproducible from the checked-in fixtures. The throwaway indexer
itself is gone (it lived in `spike/`; recoverable from git history if you ever
want it), because the job it did for drift — showing which lines nothing
classifies — is now done properly by
[`tools/check-format-drift.sh`](../tools/check-format-drift.sh).

---

## 1. `--print-maglev-graphs` output is ANSI-colored even when piped

d8 does not check `isatty`. A trace redirected to a file contains SGR sequences
mid-line, including *inside* node lines:

```
\x1b[0m   \x1b[0m5: HeapConstant(0x3a1001004 <NativeContext[310]>), 2 uses
```

**Consequence.** ANSI stripping is step zero of every line-shape decision, not a
rendering detail. The raw fallback view must still render the original escapes
(PLAN §4 promises an ANSI-preserving raw stream), so the indexer needs both the
original byte range and a stripped view of each line. Budget for this in the
`RawSection` type (`src/model.rs`) rather than retrofitting it.

## 2. The compilation anchor is `Compiling …with <Tier>`, not the banner

PLAN §6 assumed section markers are stable-ish. The two V8 versions in the
corpus disagree about the compilation banner:

| | V8 14.9 | V8 15.2 |
| :-- | :-- | :-- |
| `---------------------------------------------------` rule | absent | present |
| `Begin compiling method <name> using Maglev` | **absent** | present |
| `Finished compiling method <name> using Maglev` | **absent** | present |
| `Compiling 0x… <JSFunction add (sfi = 0x…)> with Maglev` | present | present |

An indexer anchored on `Begin compiling method` finds **0 compilations** in
every 14.9 fixture and drops the entire file into the raw fallback. Anchored on
`Compiling … with <Tier>` it finds all 4 in both versions, with 0 orphan lines.

**Consequence.** Anchor compilations on the `Compiling <addr> <JSFunction …>
with <Tier>` line. It is also the only boundary line carrying the JSFunction and
SFI addresses, which is what the deopt correlation needs
([correlation-keys.md](correlation-keys.md)). Treat `Begin compiling method` as
a supplementary source for the display name only — it is the one place the
toplevel script function's name is rendered as empty rather than as
`<JSFunction (sfi = 0x…)>`.

## 3. Every Maglev phase banner was renamed between 14.9 and 15.2

Not one survived:

| V8 14.9 | V8 15.2 |
| :-- | :-- |
| `----- After graph building -----` | `----- Maglev graph building -----` |
| `----- After propagating truncation -----` | `----- Truncation propagation -----` |
| `----- After truncation -----` | `----- Truncation -----` |
| `----- After use marking -----` | `----- AnyUseMarking -----` |
| `----- After register allocation pre-processing -----` | `----- Dead nodes sweeping -----` |
| `----- After register allocation -----` | `----- Register allocation -----` |
| *(no equivalent)* | `----- Phi untagging -----` |

This is the single strongest argument for the table-driven marker TOML (PLAN §6
Tier A) — and it says the table needs a **version axis**. It also validates
journey J5: a tool that hard-codes phase names is broken by a routine V8 release.

The banner *grammar* (`----- <name> -----`) is stable, and it is shared with
TurboFan (`----- Graph after V8.TFTyper ----- `, note the trailing space) and
with the instruction-sequence dumps. So: one grammar, a version-keyed name table.

## 4. Architecture affects only register names, not structure

At the same V8 version, arm64 and x64 fixtures have an identical phase
vocabulary and identical node counts per phase. The differences are confined to
register tokens inside node lines:

```
arm64:   14/15: Return [v13/n14:v-1(=x0)]      16: GapMove([stack:-7|t] → [x0|R|t])
x64:     14/15: Return [v13/n14:v-1(=rax)]     16: GapMove([stack:-7|t] → [rax|R|t])
```

**Consequence.** The marker table is version-keyed, not arch-keyed. The *lexer*
(`src/markers.rs`) needs the arch axis, for register-name highlighting only. Detect
arch from the register vocabulary in the trace, not from the host.

## 5. Node-line syntax changes from phase to phase

This is the assumption most likely to sink a naive parser. The same node prints
four different ways within one compilation:

```
Maglev graph building     11: Int32AddWithOverflow [n9, n10], 1 uses, can truncate to int32 [-2147483648, 2147483647]
AnyUseMarking             11: Int32AddWithOverflow [v0/n9:(x), v0/n10:(x)] → (x)
Dead nodes sweeping     11/6: InitialValue(a0) → v-1(=-7S), live range: [11-44]
Register allocation      9/9: CheckedSmiUntag [v5/n2:[x0|R|t]] → [x0|R|w32], live range: [9-11]
```

Three separate variations: the id becomes `<schedule-pos>/<node-id>`; inputs
gain `v<virtual-reg>/n<id>:<location>` decoration; the trailing `, N uses`
becomes `→ <location>` and then `live range: [a-b]`.

**Consequence.** `IRNode` (`src/model.rs`) must model *node identity* separately from
*per-phase rendering*. Parse `nN` as the stable identity across phases (this is
what makes the phase diff in PLAN §7.4 step 2 feasible without Turbolizer JSON),
and treat register/virtual-register/live-range decoration as phase-local
attributes. Do not assume `, N uses` exists.

## 6. Deopt frames are a structural sub-grammar, not annotations

The largest group of "unclassified" lines across the corpus is deopt frame
state hanging off node lines, drawn with box characters:

```
   9: CheckedSmiUntag [n2], 1 uses
      ↱ eager @2 (5 live vars)
      ↳ lazy @-1 (4 live vars)
   │ │ @2 (5 live vars)
   │││ ↱ eager @14 (7 live vars)
```

`↱`/`↳` mark eager/lazy deopt frames; the `│` depth indicates the inlining depth
of the frame. Roughly 6 000 such lines across the corpus — the single most
common non-node line.

**Consequence.** These belong to the preceding node, not to the enclosing phase.
If the annotation-attachment rule swallows them as generic
annotations, the folded-by-default rendering will hide real IR structure. Give
them a node-attached type of their own before the generic annotation fallback.
The `│` depth is also free inlining-tree data for `I` (PLAN §7.7).

**`(N live vars)` is the collapsed form, not the only form** — see §12. The same
sub-grammar has a third arrow, `↳ throw`, which the workloads above never
produced; the corpus now covers both via `throw.js`.

## 7. `--trace-maglev-truncation` produces no interleaved lines

PLAN §6.1 and journey J8 use `--trace-maglev-truncation` as the exemplar for
free-form pass-tracing interleaved with graph dumps. Measured: the output of
`--print-maglev-graphs --trace-maglev-truncation` is **byte-identical** to
`--print-maglev-graphs` alone. The truncation information (`can truncate to
int32 [-2147483648, 2147483647]`) is printed unconditionally by the graph
printer as a node-line suffix; the flag adds nothing.

The same holds for `--trace-maglev-kna`, `--trace-maglev-escape-analysis`, and
`--trace-maglev-graph-optimizer` on these workloads: they emit only the
begin/finished compile banners.

The real exemplars are:

| Flag | Extra lines on `truncation.js` |
| :-- | --: |
| `--trace-maglev-regalloc` | ~2 000 |
| `--trace-maglev-graph-building` | ~1 400 |
| `--trace-maglev-phi-untagging` | ~370 |
| `--trace-maglev-inlining` | ~20 |
| `--trace-maglev-truncation` | 0 |

**Consequence.** PLAN §6.1 and J8 have been rewritten around
`--trace-maglev-inlining`, which prints genuinely interleaved free-form lines
inside a compilation:

```
Begin compiling method outer using Maglev
[ML:38564] ⚡ INLINE small                          Small function, skipping max-depth
[ML:38564] ❌ SKIP   big                            big function, size (174) >= max-size (100)
----- Maglev graph building -----
```

Note these lines land between the compilation banner and the first phase banner
— i.e. attached to the compilation, before any phase exists. The attachment rule
must handle "inside a compilation, no phase open yet".

## 8. Channel prefixes exist, but the ids are volatile

Pass-trace lines carry a `[ML:<compilation-id>]` prefix (`[TLV:<id>]` for
Turbolev). The id changes on every run even under `--predictable`, and
`--no-trace-with-compilation-id` removes the prefix **entirely** — not just the
number.

**Consequence.** The channel-classification idea in PLAN §6.1 is sound but must
not depend on the prefix being present: the flag every V8 engineer already uses
for diffable traces strips it. Classify on the prefix when present, fall back to
the enclosing phase otherwise. The corpus carries both variants
(`maglev-graphs+inlining.inlining.log` without ids,
`maglev-graphs+inlining-ids.inlining.log` with).

## 9. Inlined callees appear as sibling phases

`----- Inlining 0x… <SharedFunctionInfo tiny> with bytecode -----` uses the same
banner grammar as a phase, appears *before* `Maglev graph building`, and repeats
once per inlining site (recursion into the inlining tree). A flat phase list
therefore mixes real phases with inlining events.

**Consequence.** Match this banner specially and hang it off the compilation as
inlining-tree data, so the phase list stays a real phase list.

## 10. What varies between two identical runs

Measured by generating the corpus twice and comparing (the generator records the
result per fixture as `"reproducible"` in `manifest.json`; roughly half of the
27 per build are byte-stable). Directly relevant to the canonicalizer in
PLAN §7.4 step 1:

| Varies | Example | Canonicalize by |
| :-- | :-- | :-- |
| Compile timings | `took 0.000, 0.144, 0.021 ms` | drop the numbers |
| Native pointers | `pc 0x0001536402e4`, `caller SP 0x…`, code addresses, `Node*` in graph-building traces | normalize to a symbol |
| PID + isolate | `[24260:0x12c00f00000]` in `--trace-gc` | normalize |
| Compilation ids | `[ML:38564]` | strip prefix |
| Thread interleaving | concurrent recompilation | not fixable; document |

**Not** varying, and this is the useful part: under `--predictable` the
sandboxed V8 heap base is pinned — every fixture in the corpus uses `0x09b8…`,
across both architectures and both V8 versions. So `<JSFunction add (sfi =
0x9b80101e2cd)>` style addresses are stable identifiers within a run *and*
comparable across runs of the same build. Without `--predictable` the base is
randomized per run (observed `0x2ef4…`, `0x0259…`).

This is why the dual-run diff (PLAN §7.4) should push
`--no-concurrent-recompilation --predictable` in its documentation: it converts
a fuzzy structural-hash matching problem into an exact-address matching problem
for most nodes.

**`"reproducible": true` is weaker evidence than it looks.** Two runs can prove
volatility but not stability. `maglev-graphs+graphbuilding.truncation.log`
produced **4 distinct outputs in 6 runs**, so a double-run probe catches it
easily — but a fixture that varies one run in fifty would be recorded as stable.
Golden tests (2.7) should therefore run through the canonicalizer rather than
trusting the flag as a raw-byte guarantee.

## 11. The toplevel script function has an empty name

`Begin compiling method  using Maglev` (two spaces) and `<JSFunction (sfi =
0x…)>` with nothing between `JSFunction` and `(`. It is a normal, frequently
compiled function — every fixture has one.

**Consequence.** An empty function name is not a parse failure. The sidebar
needs a display fallback (`<toplevel>`) and the grouped-by-function mode must
key on the SFI address, not the name.

## 12. `(N live vars)` is a *summary*; the deopt frame has a verbose form

`--print-maglev-deopt-verbose` replaces the count in §6 with the actual frame
state — every live register, the node producing it, and its location:

```
      ↱ eager @2 (5 live vars)                                        # default
      ↱ eager @0 : {<closure>:n3:, <this>:n1:, a0:n2:, <context>:n4:} (addr:0x124011b0fb8)
```

This is the frame state journey J2 (deopt root-cause) ultimately wants — what
the interpreter is actually resumed with — and PLAN.md did not know it existed. It
roughly doubles the trace
(1 498 → 2 801 lines on `deopt-eager.js`).

**The trap: `--trace-deopt-verbose` turns this on implicitly.**
`flag-definitions.h:854` is `DEFINE_WEAK_IMPLICATION(trace_deopt_verbose,
print_maglev_deopt_verbose)`. So the flag [correlation-keys.md](correlation-keys.md)
recommends for the `;;; deoptimize at` shortcut *silently changes the graph
grammar too*, and adds a further line shape of its own — `VOs : { … }`, virtual
objects, gated directly on `trace_deopt_verbose` (`PrintVirtualObjects`):

```
      │       VOs : { }
```

Three renderings of one sub-grammar, then, selected by flags the user is likely
to combine without realising:

| Flags | `↱ eager` payload | `VOs` lines | Lines (`deopt-eager.js`) |
| :-- | :-- | :--: | --: |
| `--print-maglev-graphs` | `(5 live vars)` | no | 1 498 |
| `+ --print-maglev-deopt-verbose` | `{reg:node:loc, …} (addr:…)` | no | 2 801 |
| `+ --trace-deopt-verbose` | `{reg:node:loc, …} (addr:…)` | yes | 4 154 |

**The location field is phase-dependent**, exactly as in §5 — empty during graph
building, `(x)` after AnyUseMarking, a real slot after register allocation:

```
Maglev graph building   ↳ lazy @-1 : {<closure>:n3:, <this>:n1:, a0:n2:, …}
AnyUseMarking           ↳ lazy @-1 : {<closure>:n3:(x), <this>:n1:(x), a0:n2:(x), …}
Register allocation     ↳ lazy @-1 : {<closure>:n3:[constant:v-1], <this>:n1:[stack:-6|t], a0:n2:[stack:-7|t], …}
```

So the frame-state parser cannot assume a fixed field syntax any more than the
node parser can. Note the trailing `:` with nothing after it in the first form —
an *empty* location, not a missing field.

### `(addr:…)` is a frame identity key, and the only source of volatility

The trailing `(addr:0x124011b0fb8)` is a raw host `DeoptFrame*` — outside the V8
sandbox, so unlike the heap addresses in §10 it is **not** stabilised by
`--predictable`. Masking it makes two runs byte-identical (measured: 430
differing lines → 0), which is why the `+deoptverbose` fixtures are the only
`clean`-profile graph fixtures marked `"reproducible": false`.

It is also useful: frames are heavily shared. In
`maglev-graphs+deoptverbose.deopt.log`, 215 rendered frame lines resolve to
**39 distinct frames** (5.5:1). Keying on the address lets `garage` render a
shared frame once and reference it, instead of repeating a 200-character line
per node — and it recovers the frame-sharing structure, which no other output
exposes.

**Consequence.** Canonicalize `(addr:0x…)` per §10 *and* keep it as an
intern key. `DeoptFrame` (`src/model.rs`) is a first-class object with its own
identity, referenced by nodes — not a string attached to a node.

### `↳ throw`: the third arrow, printed unconditionally

`PrintExceptionHandlerPoint` is gated on no flag at all, and has two forms
depending on whether the handler block has phis:

```
      ↳ throw @26 (b2) : {<this>:n1, a0:n2, <context>:n4, r0:n10}   # phis
      ↳ throw (b2)                                                   # no phis
```

Note the payload uses `reg:node` — **no location field**, unlike eager/lazy
above. Same arrow glyph, third syntax.

Getting one printed is fussy, which is why the original corpus had **zero**
`↳ throw` lines across all 69 files. The handler must survive: the callee must
not be inlined (or the throwing call node disappears) *and* the throw path must
actually be taken (or the handler is swept). `fixtures/workloads/throw.js` now
does both.

And the form is *per phase*, a further instance of §5 — of the 8 throw sites in
`maglev-graphs.throw.log`, all 8 print the `@26 (b2) : {…}` form through Phi
untagging, then 5 drop to the bare `(b2)` form once their catch-block phis are
swept:

| Phase | `@N (bN) : {…}` | `(bN)` |
| :-- | --: | --: |
| Maglev graph building / AnyUseMarking / Phi untagging | 8 | 0 |
| Dead nodes sweeping / Register allocation | 3 | 5 |

**Consequence.** `↳ throw` needs its own parse arm, and the catch-block id `bN`
is a real graph edge — worth drawing in the block navigation (PLAN §7.7), since
it is the only place the exception edge appears.

### The sub-grammar is stable across versions, unlike the banners

All three arrow forms and the verbose frame payload are **byte-identical**
between 14.9 and 15.2, and across arm64/x64 — the same comparison that found
§3's total phase-banner rename finds no change here at all. So the marker TOML's
version axis (§3) covers banners; the deopt frame parser can be a fixed grammar
until evidence says otherwise. Worth re-checking on each corpus regeneration
rather than assuming, which is what the golden tests are for.
