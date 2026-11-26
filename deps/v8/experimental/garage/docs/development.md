# Developing `garage`

For working on `garage` itself. If you just want to *use* it, the
[README](../README.md) and [features.md](features.md) are the pages you want.

## Build and test

With V8's GN build (from the V8 checkout root):

```sh
autoninja -C out/x64.optdebug garage experimental/garage:tests
./out/x64.optdebug/garage_unittests
./out/x64.optdebug/garage_golden_tests
```

Or via Cargo in `experimental/garage/` (uses V8's vendored crates offline):

```sh
../../third_party/rust-toolchain/bin/cargo build --release
../../third_party/rust-toolchain/bin/cargo test
```

The MSRV is **1.88**, and it is not a guess: edition 2024 alone would only need
1.85, but ratatui 0.30 and `time` (via tracing-subscriber) require 1.88, and the
parsers use let-chains. CI builds with exactly the `rust-version` in
`Cargo.toml`, so if that number is wrong the build says so.

## The fixture corpus is generated on demand

`garage_golden_tests` ([`tests/golden.rs`](../tests/golden.rs)) generates the
trace corpus on the fly in memory from the sibling `d8` binary (or `$GARAGE_D8`)
against [`fixtures/workloads/`](../fixtures/workloads), so multi-megabyte `.log`
files are never checked into the V8 repository.

```sh
./out/x64.optdebug/garage_golden_tests       # generates traces from out/x64.optdebug/d8
GARAGE_D8=../../out/x64.release/d8 cargo test --test golden
```

Three invariants hold for every generated trace, enforced on every run:

- the indexer's sections **tile the file** — no gaps, no overlaps, nothing
  orphaned;
- every graph phase banner is recognised in `assets/markers.toml` (no format
  drift);
- indexing and parsing **never panic**, including on 200 seeded rounds of byte
  flips, truncations, boundary splices and slice duplications.

To save a local corpus to disk for manual inspection:

```sh
tools/gen-fixtures.sh /path/to/out/x64.release/d8
```

## What CI checks

`.github/workflows/ci.yml`, on every push and PR:

| Job | What it gates |
| :-- | :-- |
| `lint` | `cargo fmt --check`, `cargo clippy -D warnings` |
| `test` | the suite on Ubuntu and macOS, debug **and** release |
| `msrv` | builds with exactly the `rust-version` in `Cargo.toml` |
| `terminal` | the terminal matrix, on Linux, including over SSH |

Release runs matter because `lto = "fat"` + `codegen-units = 1` is a different
codegen path from debug. Binaries are deliberately **not stripped**: "a
malformed trace must never panic the TUI" is a promise that will occasionally
break, and a usable backtrace in the bug report is worth a few hundred KB.

## Tools you run by hand

These need things CI cannot have — a V8 checkout, or a terminal.

```sh
tools/check-format-drift.sh [/path/to/d8]   # after syncing V8
tools/check-terminal.py                     # 9-case local terminal matrix
tools/check-terminal-ssh.sh                 # 5 cases over an sshd pty
```

**[`check-format-drift.sh`](../tools/check-format-drift.sh)** is the answer to
"did a new V8 break the parser?". It regenerates the whole fixture matrix from a
fresh d8 into a scratch directory (never touching the corpus) and reports drift.
It needs a d8 built with `V8_ENABLE_MAGLEV_GRAPH_PRINTER` and
`v8_enable_disassembler` — official prebuilt V8 binaries have both off and will
produce a corpus that looks like drift everywhere, which is exactly why this is
not a hosted CI job.

It is a *report*, not a golden. New V8 output is allowed to be new; what fails
is the parser losing ground:

| Finding | Meaning | Usual fix |
| :-- | :-- | :-- |
| `BANNER` | a phase banner is in no version of `markers.toml` | add the new version's names to [`assets/markers.toml`](../assets/markers.toml) |
| `PARTITION` | sections stopped tiling a file | a parser change — something the indexer anchors on moved |
| `EMPTY` | a graph flag produced no compilation at all | the compilation anchor itself stopped matching |

Unknown banners still open phases (they render with an `[unparsed]` badge), so
`garage` keeps working on a new V8 while you catch up. That degradation is
designed, not accidental.

**The terminal matrix** covers the awkward part of this program: when a trace is
on stdin, the keyboard has to come from somewhere else. `src/tty.rs` walks
`ttyname(stdout)` → `ttyname(stderr)` → `/dev/tty`, and the matrix exercises
each rung, plus viewport sizes, `TERM`, resize, and having no terminal at all.

The `/dev/tty`-only case is asserted **per platform on purpose**: on macOS a
descriptor from opening `/dev/tty` cannot be registered with kqueue, so the
fallback is documented to fail (cleanly, with a named error); on Linux epoll
takes it and it must work. Asserting one behaviour on both would either hide the
macOS limitation or invent a Linux bug.

## Releasing

Tag-driven. `release.yml` builds static musl
binaries for linux-x86_64/aarch64 and a `lipo` universal macOS binary,
smoke-tests each one, and publishes them with sha256 sums.

```sh
# bump `version` in Cargo.toml first — the tag must match it
git tag v0.1.0 && git push --tags
```

The version check is not ceremony: it means `garage --version` in a bug report
always names the release it came from.

Bumping the version means bumping **`Cargo.lock` too** — every CI job builds
`--locked`, so the manifest and the lockfile have to move in the same commit.

[`install.sh`](../install.sh) consumes what this workflow publishes:
it resolves the latest tag, downloads the tarball for the running platform, and
verifies it against the `.sha256` beside it before writing anything. Two
consequences for anyone changing `release.yml`:

- **The asset names are an interface.** `garage-<tag>-<linux-x86_64 |
  linux-aarch64 | macos-universal>.tar.gz`, each with a `.sha256`, each
  containing a `<same-name>/garage`. Renaming any of it breaks installs for
  everyone already using the script.
- **Every release must keep publishing checksums.** Dropping them does not
  degrade the installer; it stops it dead, by design.

The script keeps no state of its own. It writes exactly one file — the binary —
and everything it needs it reads from the release and from `garage --version`.
An update replaces the `garage` already on `PATH` **in place**; installing to a
fixed directory instead would leave a second copy behind and let `PATH` order
decide which one runs, which is an update that silently does nothing.

### `garage --update` delegates to the script

[`src/update.rs`](../src/update.rs) decides **whether** to update; `install.sh`
decides **how**. It resolves the latest tag, exits if the running binary already
is it, then downloads the installer *at that tag* and runs it with `--bin-dir`
pointed at its own directory.

This split is deliberate, and it is the reason not to reimplement installing in
Rust. The asset names, checksum format and tarball layout would otherwise be
frozen into every released binary: rename a release asset once and `--update`
404s forever on every copy already in the field — the worst possible failure for
something whose whole job is to run on old binaries. Delegating means an old
binary picks up today's installer.

The installer is fetched at the **release tag**, never from `main`, so the
script always matches the release it installs and an unfinished edit on `main`
cannot break updates in the field. That means **every tag must contain
`install.sh`** — `v0.1.0` does not, because the script came after it, which is
harmless only because a `v0.1.0` binary stops at the version check before ever
fetching.

Executing a downloaded script is the same trust decision the documented
`curl … | sh` install already makes. What garage does *not* do is check for
updates on its own: there is no network call anywhere except inside `--update`.

## The design records

Read these before changing a parser — most of the non-obvious code cites them
directly.

- **[spike-findings.md](spike-findings.md)** — measured V8 output behaviour, and
  why the parsers are shaped the way they are. The load-bearing one.
- **[printer-parser-contract.md](printer-parser-contract.md)** — which V8 source
  file prints what, so "update `garage` in the same breath" is mechanical rather
  than archaeology.
- **[correlation-keys.md](correlation-keys.md)** — how deopt events map to
  compilations, with explicit confidence levels and a no-guessing rule.
- **[plan.md](plan.md)** — the original design.
- **[build-log.md](build-log.md)** — how it actually got built, and every place
  reality disagreed with the plan. Usually where "why is this like this?" is
  answered.

## House rules

Two that the code will not let you forget:

- **Never guess.** An unresolved deopt renders as an event with a disabled jump,
  not as a jump to a plausible-looking compilation. Unrecognised tiers are kept
  verbatim. Sections merge only when they are line-adjacent *and* agree on tier
  and name.
- **Never drop a line.** Anything the parser does not recognise stays visible as
  raw text with an `[unparsed]` badge. The partition invariant is the machine
  check for this.
