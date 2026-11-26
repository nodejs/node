# Correlation keys: deopt events → compilation instances

A deopt line and a graph dump describe the same compilation but share no
identifier. This is the rule `garage` uses to join them anyway — and, just as
importantly, the rule for when it refuses to. It is what the deopt→graph jump
(`g`) and the timeline are built on.

Verified against **V8 15.2.0** (`b80a7539b73`) and **V8 14.9.0**, arm64 and x64.
Line formats below are quoted verbatim from the checked-in fixtures.

---

## 1. What each line actually gives us

### Deopt (`--trace-deopt`), `src/deoptimizer/deoptimizer.cc:873`

```
[bailout (kind: deopt-eager, reason: wrong map): begin. deoptimizing
 0x09b80101e435 <JSFunction read (sfi = 0x9b80101e32d)>,
 0x031a01000491 <Code TURBOFAN_JS>,
 opt id 1, bytecode offset 0, deopt exit 1,
 FP to SP delta 32, caller SP 0x00016f0117c0, pc 0x0001536402e4]
```

(one line in the fixture; wrapped here.) Fields:

| Field | Value | Useful as a key? |
| :-- | :-- | :-- |
| JSFunction address | `0x09b80101e435` | yes — stable under `--predictable` |
| SFI address | `0x9b80101e32d` | **yes — the primary key** |
| Function name | `read` | weak (empty for toplevel, ambiguous for duplicates) |
| Code object address | `0x031a01000491` | yes, but never printed on the compile side |
| Code kind | `TURBOFAN_JS` | yes — the tier |
| `opt id` | `1` | yes, but never printed on the compile side |
| `bytecode offset` | `0` | yes — the jump target within the compilation |
| `deopt exit` | `1` | index into the code's deopt exit table |

A closing line follows: `[bailout end. code_invalidation: invalidated, took N ms]`
(or `unaffected`), telling you whether the code object was thrown away — which
is what determines whether a *later* deopt at the same tier belongs to a new
compilation instance.

`--trace-deopt` also emits a code-invalidation line carrying an opt id
(`deoptimizer.cc:934`): `… (opt id %d) for deoptimization, reason: %s]`.

### Compile (`--trace-opt`), `src/execution/tiering-manager.cc`, `src/codegen/compiler.cc`

```
[marking 0x09b80101e435 <JSFunction read (sfi = 0x9b80101e32d)> for optimization to MAGLEV, ConcurrencyMode::kConcurrent, reason: hot and stable]
[compiling method 0x09b80101e435 <JSFunction read (sfi = 0x9b80101e32d)> (target MAGLEV), mode: ConcurrencyMode::kSynchronous]
[completed compiling 0x09b80101e435 <JSFunction read (sfi = 0x9b80101e32d)> (target MAGLEV) - took 0.000, 1.110, 0.025 ms]
```

OSR compilations add a bare ` OSR` token after the target:
`… (target MAGLEV) OSR, mode: …`.

### Graph dump (`--print-maglev-graphs`), `src/maglev/maglev-compiler.cc:150`

```
Compiling 0x09b80101e371 <JSFunction add (sfi = 0x9b80101e2cd)> with Maglev
```

## 2. The gap

**`opt id` and the Code object address are printed on the deopt side and on
neither the `--trace-opt` side nor the graph-dump side.** The two obvious exact
keys are therefore unusable as-is. What all three sides share is:

- the **SFI address** (`sfi = 0x…`) — the stable identity of the function,
- the **tier** (`MAGLEV` / `TURBOFAN_JS` vs. `with Maglev`),
- **chronological position** in the stream.

## 3. The rule

Correlate on `(sfi_address, tier, ordinal)`, where the ordinal counts
compilation instances of that (SFI, tier) pair in stream order:

1. **Index compilations.** Every `Compiling <addr> <JSFunction NAME (sfi =
   SFI)> with TIER` line opens instance *k* for the key `(SFI, TIER)`, where
   *k* increments per key. Where `--trace-opt` is also present, `[compiling
   method …]` / `[completed compiling …]` bracket the same instance and supply
   the OSR marker and the concurrency mode.
2. **Bind a deopt event.** For a bailout line with `(sfi = SFI)` and
   `<Code TIER>`, attach it to the most recent open instance of `(SFI, TIER)`.
   "Open" means: created earlier in the stream, and not yet followed by a
   `[bailout end. code_invalidation: invalidated]` for that same instance.
3. **Jump target.** Within that compilation, `bytecode offset N` selects the
   location. Maglev graph dumps interleave the source bytecode into blocks as
   `   N : <bytes>   <Bytecode>` lines, so offset → line is a direct lookup in
   the parsed phase. Prefer the earliest phase that contains the offset
   (`Maglev graph building`), since later phases drop dead bytecode.

### Confidence levels, and showing them

The rule is exact when the stream is clean and degrades predictably otherwise.
Record which case applied and show it in the UI rather than silently guessing:

| Situation | Result |
| :-- | :-- |
| `--predictable --no-concurrent-recompilation`, both flags present | exact |
| Only `--trace-deopt` (no graph dump) | event has no compilation to jump to; timeline entry only |
| Concurrent recompilation | ordinal may be wrong: compile lines from several threads interleave |
| Same SFI, same tier, code invalidated and recompiled | handled by rule 2's "open" test |
| Trace starts mid-stream (piped, truncated) | first instance of a key may be missing; mark the event unresolved |

Never invent a link. An unresolved deopt renders as a timeline event with a
disabled `g`, not as a jump to a plausible-looking compilation.

## 4. `--trace-deopt-verbose` gives an exact source position for free

```
[bailout (kind: deopt-eager, reason: Insufficient type feedback for object literal): begin. deoptimizing …]
            ;;; deoptimize at <workloads/deopt-eager.js:14:1>
  reading input frame  => bytecode_offset=70, args=1, height=6, retval=0(#0); inputs:
      0: 0x09b80101e3e1 ;  [fp -  16]  0x09b80101e3e1 <JSFunction (sfi = 0x9b80101e2c5)>
      …
  translating interpreted frame  => bytecode_offset=70, variable_frame_size=64, frame_size=136
```

The `;;; deoptimize at <script:line:col>` line is a direct source position, and
`bytecode_offset=` repeats the offset in a machine-friendlier form. When
`--trace-deopt-verbose` is present, use these instead of parsing the bracket
line's prose. The indented `reading input frame` / `translating interpreted
frame` blocks are the data behind the deopt frame panel.

## 5. Things deliberately not used as keys

- **Function name.** Empty for the toplevel script function, and nothing
  prevents two functions from sharing a name. Display only.
- **Code object address.** Correct in principle, but the compile side never
  prints it. Reconsider if V8's `--trace-opt` output ever gains it — a one-line
  printer change, and the cheapest possible upstream improvement for this tool.
- **`pc`, `caller SP`, `FP to SP delta`.** Native addresses; randomized per run
  (see [spike-findings.md](spike-findings.md) §10). Display only.
- **Wall-clock ordering across threads.** No timestamps in this output at all.

## 6. When this breaks

The formats above live in `src/deoptimizer/deoptimizer.cc` (line 873 for the
bailout header, 934 for code invalidation) and in the trace-opt printers listed
in [printer-parser-contract.md](printer-parser-contract.md). Re-verify this
document whenever those files change; the fixture corpus makes that a mechanical
check.
