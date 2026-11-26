// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Integration tests over the workload trace corpus.
//!
//! Rather than checking megabytes of generated `.log` traces into the V8
//! repository, this suite generates the trace corpus on the fly in memory from
//! the sibling `d8` binary (or `$GARAGE_D8`), caching it once per test-binary
//! invocation.
//!
//! Every generated trace must satisfy:
//! - sections partition the file: no line is dropped or double-counted
//!   (PLAN principle 1),
//! - indexing and parsing never panic,
//! - every graph phase banner is recognised in `assets/markers.toml` (no
//!   format drift).

use std::path::{Path, PathBuf};
use std::process::Command;
use std::sync::OnceLock;
use std::sync::atomic::{AtomicUsize, Ordering};

use garage::index::TraceIndex;
use garage::model::{LineInfo, PhaseKind};
use garage::parse::maglev;
use garage::source::LogBuffer;

fn manifest_dir() -> PathBuf {
    let dir = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    if dir.join("fixtures").exists() {
        return dir;
    }
    if let Ok(exe) = std::env::current_exe()
        && let Some(bin_dir) = exe.parent()
    {
        let candidate = bin_dir.join(&dir);
        if candidate.join("fixtures").exists() {
            return candidate;
        }
    }
    dir
}

fn fixtures_root() -> PathBuf {
    manifest_dir().join("fixtures")
}

#[derive(Clone, Copy)]
enum Profile {
    Clean,
    RawPredictable,
    Raw,
}

struct FixtureSpec {
    file: &'static str,
    profile: Profile,
    workload: &'static str,
    flags: &'static [&'static str],
}

const CLEAN_FLAGS: &[&str] = &[
    "--no-concurrent-recompilation",
    "--predictable",
    "--no-trace-with-compilation-id",
];

const RAW_PREDICTABLE_FLAGS: &[&str] = &["--no-concurrent-recompilation", "--predictable"];

/// Mirrors `tools/gen-fixtures.sh`: every flag combination and workload
/// exercised against live `d8` output.
const FIXTURE_SPECS: &[FixtureSpec] = &[
    FixtureSpec {
        file: "maglev-graphs.simple.log",
        profile: Profile::Clean,
        workload: "workloads/simple.js",
        flags: &["--print-maglev-graphs"],
    },
    FixtureSpec {
        file: "maglev-graphs.truncation.log",
        profile: Profile::Clean,
        workload: "workloads/truncation.js",
        flags: &["--print-maglev-graphs"],
    },
    FixtureSpec {
        file: "maglev-graphs.inlining.log",
        profile: Profile::Clean,
        workload: "workloads/inlining.js",
        flags: &["--print-maglev-graphs"],
    },
    FixtureSpec {
        file: "maglev-graphs.poly.log",
        profile: Profile::Clean,
        workload: "workloads/poly.js",
        flags: &["--print-maglev-graphs"],
    },
    FixtureSpec {
        file: "maglev-graphs.mixed.log",
        profile: Profile::Clean,
        workload: "workloads/mixed.js",
        flags: &["--print-maglev-graphs"],
    },
    FixtureSpec {
        file: "maglev-graphs.throw.log",
        profile: Profile::Clean,
        workload: "workloads/throw.js",
        flags: &["--print-maglev-graphs"],
    },
    FixtureSpec {
        file: "maglev-graph.simple.log",
        profile: Profile::Clean,
        workload: "workloads/simple.js",
        flags: &["--print-maglev-graph"],
    },
    FixtureSpec {
        file: "maglev-graphs+deoptverbose.deopt.log",
        profile: Profile::Clean,
        workload: "workloads/deopt-eager.js",
        flags: &["--print-maglev-graphs", "--print-maglev-deopt-verbose"],
    },
    FixtureSpec {
        file: "maglev-graphs+deoptverbose.throw.log",
        profile: Profile::Clean,
        workload: "workloads/throw.js",
        flags: &["--print-maglev-graphs", "--print-maglev-deopt-verbose"],
    },
    FixtureSpec {
        file: "maglev-graphs+tracedeoptverbose.deopt.log",
        profile: Profile::Clean,
        workload: "workloads/deopt-eager.js",
        flags: &["--print-maglev-graphs", "--trace-deopt-verbose"],
    },
    FixtureSpec {
        file: "maglev-graphs+inlining.inlining.log",
        profile: Profile::Clean,
        workload: "workloads/inlining.js",
        flags: &["--print-maglev-graphs", "--trace-maglev-inlining"],
    },
    FixtureSpec {
        file: "maglev-graphs+graphbuilding.truncation.log",
        profile: Profile::Clean,
        workload: "workloads/truncation.js",
        flags: &["--print-maglev-graphs", "--trace-maglev-graph-building"],
    },
    FixtureSpec {
        file: "maglev-graphs+regalloc.simple.log",
        profile: Profile::Clean,
        workload: "workloads/simple.js",
        flags: &["--print-maglev-graphs", "--trace-maglev-regalloc"],
    },
    FixtureSpec {
        file: "maglev-graphs+inlining-ids.inlining.log",
        profile: Profile::RawPredictable,
        workload: "workloads/inlining.js",
        flags: &["--print-maglev-graphs", "--trace-maglev-inlining"],
    },
    FixtureSpec {
        file: "maglev-graphs+code.simple.log",
        profile: Profile::Clean,
        workload: "workloads/simple.js",
        flags: &["--print-maglev-graphs", "--print-opt-code"],
    },
    FixtureSpec {
        file: "maglev-graphs+maglev-code.simple.log",
        profile: Profile::Clean,
        workload: "workloads/simple.js",
        flags: &["--print-maglev-graphs", "--print-maglev-code"],
    },
    FixtureSpec {
        file: "trace-opt+deopt.deopt.log",
        profile: Profile::Clean,
        workload: "workloads/deopt-eager.js",
        flags: &["--trace-opt", "--trace-deopt"],
    },
    FixtureSpec {
        file: "trace-opt+deopt.mixed.log",
        profile: Profile::Clean,
        workload: "workloads/mixed.js",
        flags: &["--trace-opt", "--trace-deopt"],
    },
    FixtureSpec {
        file: "trace-deopt-verbose.deopt.log",
        profile: Profile::Clean,
        workload: "workloads/deopt-eager.js",
        flags: &["--trace-opt", "--trace-deopt-verbose"],
    },
    FixtureSpec {
        file: "trace-osr.osr.log",
        profile: Profile::Clean,
        workload: "workloads/osr.js",
        flags: &["--trace-osr", "--trace-opt"],
    },
    FixtureSpec {
        file: "trace-gc.gc.log",
        profile: Profile::Clean,
        workload: "workloads/gc.js",
        flags: &["--trace-gc"],
    },
    FixtureSpec {
        file: "trace-prototype-users.poly.log",
        profile: Profile::Clean,
        workload: "workloads/poly.js",
        flags: &["--trace-prototype-users"],
    },
    FixtureSpec {
        file: "print-bytecode.simple.log",
        profile: Profile::Clean,
        workload: "workloads/simple.js",
        flags: &["--print-bytecode"],
    },
    FixtureSpec {
        file: "print-opt-code.simple.log",
        profile: Profile::Clean,
        workload: "workloads/simple.js",
        flags: &["--print-opt-code"],
    },
    FixtureSpec {
        file: "turbolev-frontend.simple.log",
        profile: Profile::Clean,
        workload: "workloads/simple.js",
        flags: &["--turbolev", "--print-turbolev-frontend"],
    },
    FixtureSpec {
        file: "turbo-graph.simple.log",
        profile: Profile::Clean,
        workload: "workloads/simple.js",
        flags: &["--trace-turbo-graph"],
    },
    FixtureSpec {
        file: "mvp.deopt.log",
        profile: Profile::Clean,
        workload: "workloads/deopt-eager.js",
        flags: &["--print-maglev-graphs", "--trace-deopt"],
    },
    FixtureSpec {
        file: "mvp.mixed.log",
        profile: Profile::Clean,
        workload: "workloads/mixed.js",
        flags: &["--print-maglev-graphs", "--trace-deopt"],
    },
    FixtureSpec {
        file: "concurrent.mixed.log",
        profile: Profile::Raw,
        workload: "workloads/mixed.js",
        flags: &["--print-maglev-graphs", "--trace-opt", "--trace-deopt"],
    },
];

struct GeneratedFixture {
    file: &'static str,
    bytes: Vec<u8>,
}

fn canonical(path: PathBuf) -> PathBuf {
    std::fs::canonicalize(&path).unwrap_or(path)
}

fn find_d8() -> Option<PathBuf> {
    if let Some(p) = std::env::var_os("GARAGE_D8") {
        let path = PathBuf::from(&p);
        assert!(
            path.is_file(),
            "GARAGE_D8={} does not point to an existing file",
            path.display()
        );
        return Some(canonical(path));
    }
    if let Ok(exe) = std::env::current_exe()
        && let Some(bin_dir) = exe.parent()
    {
        let sibling = bin_dir.join("d8");
        if sibling.is_file() {
            return Some(canonical(sibling));
        }
    }
    // Fallback when invoked via `cargo test` from `experimental/garage/`.
    let manifest = canonical(manifest_dir());
    if let Some(v8_root) = manifest.parent().and_then(Path::parent) {
        for config in [
            "x64.optdebug",
            "x64.release",
            "arm64.optdebug",
            "arm64.release",
            "x64.debug",
            "arm64.debug",
        ] {
            let candidate = v8_root.join("out").join(config).join("d8");
            if candidate.is_file() {
                return Some(canonical(candidate));
            }
        }
    }
    None
}

fn run_d8_fixture(d8: &Path, root: &Path, spec: &FixtureSpec) -> GeneratedFixture {
    let mut cmd = Command::new(d8);
    cmd.current_dir(root);
    match spec.profile {
        Profile::Clean => {
            cmd.args(CLEAN_FLAGS);
        }
        Profile::RawPredictable => {
            cmd.args(RAW_PREDICTABLE_FLAGS);
        }
        Profile::Raw => {}
    }
    cmd.args(spec.flags);
    cmd.arg(spec.workload);

    let out = cmd
        .output()
        .unwrap_or_else(|e| panic!("failed to run {} for {}: {e}", d8.display(), spec.file));
    assert!(
        out.status.success(),
        "{} failed on {} (status {:?}): {}",
        d8.display(),
        spec.file,
        out.status.code(),
        String::from_utf8_lossy(&out.stderr)
    );
    let mut bytes = out.stdout;
    bytes.extend_from_slice(&out.stderr);
    GeneratedFixture {
        file: spec.file,
        bytes,
    }
}

/// Generates all 29 traces in parallel once per test binary run.
/// Returns `None` only when no `d8` binary is available (e.g. a standalone
/// `cargo test` without a V8 build).
fn corpus() -> Option<&'static [GeneratedFixture]> {
    static CORPUS: OnceLock<Option<Vec<GeneratedFixture>>> = OnceLock::new();
    CORPUS
        .get_or_init(|| {
            let d8 = find_d8()?;
            let root = fixtures_root();
            let slots: Vec<OnceLock<GeneratedFixture>> =
                (0..FIXTURE_SPECS.len()).map(|_| OnceLock::new()).collect();
            let next = AtomicUsize::new(0);
            let workers = std::thread::available_parallelism()
                .map(|n| n.get())
                .unwrap_or(4)
                .min(FIXTURE_SPECS.len());

            std::thread::scope(|s| {
                for _ in 0..workers {
                    s.spawn(|| {
                        loop {
                            let i = next.fetch_add(1, Ordering::Relaxed);
                            if i >= FIXTURE_SPECS.len() {
                                break;
                            }
                            let fixture = run_d8_fixture(&d8, &root, &FIXTURE_SPECS[i]);
                            let _ = slots[i].set(fixture);
                        }
                    });
                }
            });

            Some(
                slots
                    .into_iter()
                    .map(|slot| slot.into_inner().expect("fixture generated"))
                    .collect(),
            )
        })
        .as_deref()
}

fn require_corpus() -> Option<&'static [GeneratedFixture]> {
    let Some(c) = corpus() else {
        eprintln!("skipping: no d8 binary found (build //:d8 or set GARAGE_D8)");
        return None;
    };
    Some(c)
}

fn fixture_bytes<'a>(corpus: &'a [GeneratedFixture], file: &str) -> &'a [u8] {
    &corpus
        .iter()
        .find(|f| f.file == file)
        .unwrap_or_else(|| panic!("missing generated fixture {file}"))
        .bytes
}

fn buffer_from(bytes: &[u8]) -> LogBuffer {
    let mut buffer = LogBuffer::new();
    buffer.append(bytes);
    buffer.finish();
    buffer
}

fn index(buffer: &LogBuffer) -> TraceIndex {
    let mut idx = TraceIndex::new(None);
    idx.ingest(buffer, true);
    idx
}

/// Sections must partition the file: every line in exactly one section.
fn assert_partition(idx: &TraceIndex, line_count: usize, name: &str) {
    let mut ranges: Vec<(usize, usize)> = idx
        .compilations
        .iter()
        .map(|c| (c.lines.start, c.lines.end))
        .chain(idx.raw.iter().map(|r| (r.lines.start, r.lines.end)))
        .collect();
    ranges.sort();
    let mut at = 0;
    for (start, end) in ranges {
        assert_eq!(start, at, "{name}: gap or overlap at line {at}");
        assert!(end >= start, "{name}: negative section");
        at = end;
    }
    assert_eq!(at, line_count, "{name}: sections do not cover the file");
}

/// A `d8` built without `V8_ENABLE_MAGLEV_GRAPH_PRINTER` silently ignores
/// `--print-maglev-graphs`; detect that so graph-presence assertions don't
/// false-fail on minimal release builds.
fn has_maglev_graph_printer(corpus: &[GeneratedFixture]) -> bool {
    let buffer = buffer_from(fixture_bytes(corpus, "maglev-graphs.simple.log"));
    !index(&buffer).compilations.is_empty()
}

#[test]
fn generated_corpus_has_no_format_drift() {
    let Some(corpus) = require_corpus() else {
        return;
    };
    let check_anchored = has_maglev_graph_printer(corpus);
    let mut findings = Vec::new();
    for fixture in corpus {
        let stats = drift_for_bytes(&fixture.bytes, fixture.file, check_anchored, &mut findings);
        if fixture.file == "print-bytecode.simple.log" {
            assert!(
                stats.bytecode_rows > 0,
                "{}: expected parsed bytecode rows",
                fixture.file
            );
        }
        if matches!(
            fixture.file,
            "trace-opt+deopt.deopt.log" | "trace-deopt-verbose.deopt.log" | "trace-osr.osr.log"
        ) {
            assert!(
                stats.events > 0,
                "{}: expected parsed timeline events",
                fixture.file
            );
        }
        if stats.listing_phases > 0 {
            assert!(
                stats.disasm_rows > 0,
                "{}: expected parsed disassembly rows in listing phase",
                fixture.file
            );
        }
    }
    findings.sort();
    assert!(
        findings.is_empty(),
        "{} drift finding(s) against live d8:\n{}",
        findings.len(),
        format_findings(&findings)
    );
}

#[test]
fn every_fixture_partitions_and_never_panics() {
    let Some(corpus) = require_corpus() else {
        return;
    };
    for fixture in corpus {
        let buffer = buffer_from(&fixture.bytes);
        let idx = index(&buffer);
        assert_partition(&idx, buffer.line_count(), fixture.file);
        for c in &idx.compilations {
            let parsed = maglev::parse_compilation(&buffer, c);
            // Parsed phases mirror the indexed ones one-to-one.
            assert_eq!(parsed.phases.len(), c.phases.len(), "{}", fixture.file);
            for (section, parsed_phase) in c.phases.iter().zip(&parsed.phases) {
                assert_eq!(
                    parsed_phase.infos.len(),
                    section.lines.len(),
                    "{}: every line of {:?} classified",
                    fixture.file,
                    section.name
                );
            }
        }
    }
}

/// The volatile `[ML:<id>]` prefix must not be load-bearing — the same
/// workload with and without ids yields identical inlining decisions.
#[test]
fn inline_decisions_do_not_depend_on_the_id_prefix() {
    let Some(corpus) = require_corpus() else {
        return;
    };
    if !has_maglev_graph_printer(corpus) {
        return;
    }
    let mut decisions = Vec::new();
    for file in [
        "maglev-graphs+inlining.inlining.log",
        "maglev-graphs+inlining-ids.inlining.log",
    ] {
        let buffer = buffer_from(fixture_bytes(corpus, file));
        let idx = index(&buffer);
        let all: Vec<(bool, String, String)> = idx
            .compilations
            .iter()
            .flat_map(|c| {
                maglev::parse_compilation(&buffer, c)
                    .inline_decisions
                    .into_iter()
                    .map(|d| (d.inlined, d.callee, d.reason))
            })
            .collect();
        assert!(
            !all.is_empty(),
            "{file}: expected inlining decisions in the fixture"
        );
        decisions.push(all);
    }
    assert_eq!(
        decisions[0], decisions[1],
        "decisions must match with and without [ML:] prefixes"
    );
}

/// A bounded random fuzz pass: many seeds, mutations of real
/// fixture data — byte flips, splices, truncations — plus pure noise. Every
/// input must index into a valid partition and parse without panicking.
#[test]
fn fuzz_mutated_fixtures_never_panic() {
    let Some(corpus) = require_corpus() else {
        return;
    };
    let base = fixture_bytes(corpus, "maglev-graphs.throw.log");
    let base = &base[..base.len().min(64 * 1024)];
    assert!(
        !base.is_empty(),
        "generated throw fixture should not be empty"
    );

    let mut state = 0x9e37_79b9_7f4a_7c15_u64;
    let mut rand = move || {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        state
    };

    for round in 0..200 {
        let mut data = base.to_vec();
        // A handful of random mutations per round.
        for _ in 0..(rand() % 8 + 1) {
            match rand() % 4 {
                0 => {
                    // Flip a byte.
                    let at = (rand() as usize) % data.len();
                    data[at] = (rand() & 0xff) as u8;
                }
                1 => {
                    // Truncate.
                    let at = (rand() as usize) % data.len();
                    data.truncate(at.max(1));
                }
                2 => {
                    // Splice a boundary-looking fragment somewhere random.
                    let fragments: [&[u8]; 4] = [
                        b"\nCompiling 0x1 <JSFunction f (sfi = 0x10)> with Maglev\n",
                        b"\n----- Register allocation -----\n",
                        b"\n      \xe2\x86\xb1 eager @2 (5 live vars)\n",
                        b"\n[bailout (kind: deopt-eager, reason: x): begin. deoptimizing ",
                    ];
                    let frag = fragments[(rand() as usize) % fragments.len()];
                    let at = (rand() as usize) % data.len();
                    data.splice(at..at, frag.iter().copied());
                }
                _ => {
                    // Duplicate a random slice (creates weird nesting).
                    let a = (rand() as usize) % data.len();
                    let b = ((rand() as usize) % data.len()).max(a);
                    let slice = data[a..b.min(a + 4096)].to_vec();
                    data.extend_from_slice(&slice);
                }
            }
        }

        let buffer = buffer_from(&data);
        let idx = index(&buffer);
        assert_partition(&idx, buffer.line_count(), &format!("fuzz round {round}"));
        for c in &idx.compilations {
            let _ = maglev::parse_compilation(&buffer, c);
        }
    }
}

/// Truncation and garbage must never panic the indexer or parser.
#[test]
fn truncated_and_garbage_input_is_survivable() {
    if let Some(corpus) = corpus() {
        let bytes = fixture_bytes(corpus, "maglev-graphs.simple.log");
        // Truncate mid-file at several points, including mid-line and mid-escape.
        for cut in [1, 100, 1000, bytes.len() / 2, bytes.len().saturating_sub(1)] {
            if cut == 0 || cut > bytes.len() {
                continue;
            }
            let buffer = buffer_from(&bytes[..cut]);
            let idx = index(&buffer);
            assert_partition(&idx, buffer.line_count(), &format!("truncated@{cut}"));
            for c in &idx.compilations {
                let _ = maglev::parse_compilation(&buffer, c);
            }
        }
    }

    // Deterministic pseudo-random bytes; always runs even without `d8`.
    let mut state = 0x243f_6a88_85a3_08d3_u64;
    let mut garbage = Vec::with_capacity(1 << 16);
    for _ in 0..(1 << 16) {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        garbage.push((state & 0xff) as u8);
    }
    // Splice recognizable boundaries into the noise so the classifier arms run.
    let spliced = [
        &garbage[..1024],
        b"\nCompiling 0x1 <JSFunction f (sfi = 0x10)> with Maglev\n".as_slice(),
        &garbage[1024..2048],
        b"\n----- Maglev graph building -----\n",
        &garbage[2048..4096],
        b"\n   9: CheckedSmiUntag [n2], 1 uses\n      \xe2\x86\xb1 eager @2 (5 live vars)\n",
        &garbage[4096..],
    ]
    .concat();
    let buffer = buffer_from(&spliced);
    let idx = index(&buffer);
    assert_partition(&idx, buffer.line_count(), "garbage");
    for c in &idx.compilations {
        let _ = maglev::parse_compilation(&buffer, c);
    }
}

// ---------------------------------------------------------------------------
// Format-drift report (driven by tools/check-format-drift.sh or live corpus).
// ---------------------------------------------------------------------------

/// A drift finding, ordered by how much it matters.
#[derive(PartialEq, Eq, PartialOrd, Ord)]
enum Drift {
    /// The indexer's core invariant broke: sections no longer tile the file.
    Partition(String),
    /// A fixture that should describe compilations described none.
    NoCompilations(String),
    /// `----- <name> -----` matched no version in assets/markers.toml.
    UnknownBanner { file: String, banner: String },
}

#[derive(Default)]
struct ParseStats {
    bytecode_rows: usize,
    listing_phases: usize,
    disasm_rows: usize,
    events: usize,
}

fn drift_for_bytes(
    bytes: &[u8],
    name: &str,
    check_anchored: bool,
    out: &mut Vec<Drift>,
) -> ParseStats {
    let buffer = buffer_from(bytes);
    let idx = index(&buffer);

    let mut ranges: Vec<(usize, usize)> = idx
        .compilations
        .iter()
        .map(|c| (c.lines.start, c.lines.end))
        .chain(idx.raw.iter().map(|r| (r.lines.start, r.lines.end)))
        .collect();
    ranges.sort();
    let mut at = 0;
    let mut ok = true;
    for (start, end) in ranges {
        if start != at || end < start {
            ok = false;
            break;
        }
        at = end;
    }
    if !ok || at != buffer.line_count() {
        out.push(Drift::Partition(name.to_string()));
    }

    // A graph-printing flag that yields no compilation at all is the loudest
    // possible drift: the compilation anchor itself stopped matching.
    let anchored = name.contains("maglev-graph")
        || name.contains("turbolev")
        || name.contains("turbo-graph")
        || name.starts_with("mvp");
    if check_anchored && anchored && idx.compilations.is_empty() {
        out.push(Drift::NoCompilations(name.to_string()));
    }

    let mut stats = ParseStats {
        events: idx.events.len(),
        ..Default::default()
    };
    let mut seen = std::collections::BTreeSet::new();
    for c in &idx.compilations {
        for p in &c.phases {
            if matches!(p.kind, PhaseKind::Graph { known: false }) && seen.insert(p.name.clone()) {
                out.push(Drift::UnknownBanner {
                    file: name.to_string(),
                    banner: p.name.clone(),
                });
            }
            if matches!(p.kind, PhaseKind::Listing) {
                stats.listing_phases += 1;
            }
        }
        // Parsing must still not panic on anything the new build prints.
        let parsed = maglev::parse_compilation(&buffer, c);
        for phase in &parsed.phases {
            for info in &phase.infos {
                match info {
                    LineInfo::Bytecode(_) => stats.bytecode_rows += 1,
                    LineInfo::Disasm { .. } => stats.disasm_rows += 1,
                    _ => {}
                }
            }
        }
    }
    stats
}

fn format_findings(findings: &[Drift]) -> String {
    let mut lines = Vec::new();
    for f in findings {
        lines.push(match f {
            Drift::Partition(file) => {
                format!("  PARTITION  {file}: sections no longer tile the file")
            }
            Drift::NoCompilations(file) => {
                format!("  EMPTY      {file}: a graph flag produced no compilation")
            }
            Drift::UnknownBanner { file, banner } => {
                format!("  BANNER     {file}: {banner:?} is in no version of assets/markers.toml")
            }
        });
    }
    lines.join("\n")
}

#[test]
#[ignore = "needs a corpus from a fresh d8; run tools/check-format-drift.sh"]
fn format_drift_report() {
    let Some(dir) = std::env::var_os("GARAGE_DRIFT_CORPUS") else {
        panic!("set GARAGE_DRIFT_CORPUS to a directory of .log files");
    };
    let dir = PathBuf::from(dir);

    let mut logs: Vec<PathBuf> = std::fs::read_dir(&dir)
        .unwrap_or_else(|e| panic!("cannot read {}: {e}", dir.display()))
        .filter_map(Result::ok)
        .map(|e| e.path())
        .filter(|p| p.extension().is_some_and(|x| x == "log"))
        .collect();
    logs.sort();
    assert!(!logs.is_empty(), "no .log files in {}", dir.display());

    let mut findings = Vec::new();
    let mut versions = std::collections::BTreeMap::new();
    for path in &logs {
        let name = path
            .file_name()
            .unwrap_or_default()
            .to_string_lossy()
            .into_owned();
        let bytes = std::fs::read(path).unwrap_or_default();
        let buffer = buffer_from(&bytes);
        let detected = index(&buffer)
            .detected_version
            .unwrap_or_else(|| "unknown".to_string());
        *versions.entry(detected).or_insert(0usize) += 1;

        drift_for_bytes(&bytes, &name, true, &mut findings);
    }
    findings.sort();

    println!("\nformat-drift report over {} files", logs.len());
    println!("  detected versions: {versions:?}");
    if findings.is_empty() {
        println!("  no drift: every banner is known, every file partitions\n");
        return;
    }

    let report = format_findings(&findings);
    println!("{report}\n");
    panic!(
        "{} drift finding(s):\n{report}\n\nIf this is a new V8, add its phase \
         names to assets/markers.toml (see the header there for where the \
         table comes from).",
        findings.len()
    );
}
