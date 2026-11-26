// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Core data model.
//!
//! Two layers, deliberately separate:
//!
//! - **Index-time types** ([`CompilationSection`], [`RawSection`],
//!   [`TimelineEvent`]) hold only what one streaming pass can find without
//!   parsing bodies: byte/line ranges and the boundary-line fields. This is
//!   what makes a 1 GB file open instantly (PLAN §3.3).
//! - **Parse-time types** ([`ParsedCompilation`], [`ParsedPhase`], [`IRNode`],
//!   [`DeoptFrame`]) exist only for compilations someone actually viewed.
//!
//! `IRNode` separates *identity* (`nN`, stable across phases — what makes the
//! phase diff feasible without Turbolizer JSON) from *per-phase rendering*
//! (schedule position, input decoration, registers, live ranges), per
//! spike-findings.md §5. Deopt frames are node-attached structure with interned
//! identity, not annotations (§6, §12).

use std::ops::Range;

/// A V8 heap address as printed (`0x9b80101e2cd`). Stored numerically so that
/// `(sfi = 0x09b8…)` and `sfi = 0x9b8…` — same address, different zero padding
/// — compare equal.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, PartialOrd, Ord)]
pub struct Addr(pub u64);

impl Addr {
    pub fn parse(hex: &str) -> Option<Self> {
        let digits = hex.strip_prefix("0x").unwrap_or(hex);
        u64::from_str_radix(digits, 16).ok().map(Addr)
    }
}

impl std::fmt::Display for Addr {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "0x{:x}", self.0)
    }
}

/// Compilation tier, from `with <Tier>` (graph dumps) or `MAGLEV` /
/// `TURBOFAN_JS` (timeline lines). Both spellings map here.
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub enum Tier {
    Ignition,
    Sparkplug,
    Maglev,
    Turbofan,
    /// The Turbolev frontend (`--turbolev --print-turbolev-frontend`):
    /// Maglev-style graphs feeding the TurboFan backend. Its dumps have no
    /// `Compiling … with <Tier>` line; the indexer anchors them on the
    /// `Bytecode before MaglevGraphBuilding` banner instead.
    Turbolev,
    /// Never guess: an unrecognised tier is kept verbatim.
    Other(String),
}

impl Tier {
    pub fn parse(word: &str) -> Self {
        // The graph printer says "Maglev"; the tiering/deopt printers say
        // "MAGLEV" and "TURBOFAN_JS". One tier, three spellings.
        match word.to_ascii_uppercase().as_str() {
            "IGNITION" => Tier::Ignition,
            "SPARKPLUG" | "BASELINE" => Tier::Sparkplug,
            "MAGLEV" => Tier::Maglev,
            "TURBOFAN" | "TURBOFAN_JS" => Tier::Turbofan,
            "TURBOLEV" => Tier::Turbolev,
            _ => Tier::Other(word.to_string()),
        }
    }

    pub fn label(&self) -> &str {
        match self {
            Tier::Ignition => "Ignition",
            Tier::Sparkplug => "Sparkplug",
            Tier::Maglev => "Maglev",
            Tier::Turbofan => "Turbofan",
            Tier::Turbolev => "Turbolev",
            Tier::Other(s) => s,
        }
    }
}

/// Identity of one compilation instance, per docs/correlation-keys.md: the SFI
/// address is the only stable identity (function names can be empty or
/// duplicated), and the ordinal counts instances of `(sfi, tier)` in stream
/// order.
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct CompilationKey {
    pub sfi: Addr,
    pub tier: Tier,
    /// 1-based instance count of this `(sfi, tier)` pair within the source.
    pub ordinal: u32,
}

/// How confident the indexer is that OSR data attached to a compilation
/// actually belongs to it (correlation-keys.md §3: never invent a link).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Confidence {
    /// Same-key bracket lines adjacent in a clean stream.
    Exact,
    /// Matched through a weak key (function name) or across interleaving.
    Heuristic,
}

/// One `Compiling 0x… <JSFunction …> with <Tier>` section: byte/line ranges
/// plus everything the anchor line itself says. Bodies are not parsed here.
#[derive(Debug, Clone)]
pub struct CompilationSection {
    pub key: CompilationKey,
    /// From the anchor line; empty for the toplevel script function
    /// (spike-findings.md §11). Display code must fall back, not this field.
    pub name: String,
    pub function_addr: Option<Addr>,
    /// Line range, including the `Begin compiling`/rule preamble when present
    /// and the `Finished compiling` trailer when present.
    pub lines: Range<usize>,
    /// OSR compilations are first-class (PLAN §5.1). Filled from bracketing
    /// `[compiling method … OSR]` events when present.
    pub osr: Option<OsrInfo>,
    /// Phase sections in order, found by the same streaming pass.
    pub phases: Vec<PhaseSection>,
    /// Line range between the anchor and the first banner: where
    /// `--trace-maglev-inlining` lines land, before any phase opens (2.8).
    pub preamble: Range<usize>,
    /// True when `--function` filtered this compilation out: still indexed (the
    /// section boundaries must stay a partition), but hidden and never parsed.
    pub filtered_out: bool,
}

impl CompilationSection {
    /// Sidebar display name with the toplevel fallback.
    pub fn display_name(&self) -> &str {
        if self.name.is_empty() {
            "<toplevel>"
        } else {
            &self.name
        }
    }
}

#[derive(Debug, Clone)]
pub struct OsrInfo {
    /// From `[OSR - compilation started … osr offset: N]`, correlated by
    /// function name — a weak key, hence the confidence field.
    pub offset: Option<u32>,
    pub confidence: Confidence,
}

/// What a `----- … -----` banner opened.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum PhaseKind {
    /// A real pipeline phase. `known` says the name is in the marker table for
    /// some version (or matches the stable TurboFan banner shapes); an unknown
    /// name still opens a phase (journey J5).
    Graph { known: bool },
    /// `----- Bytecode array -----`: shares the grammar, is not a phase.
    Bytecode,
    /// `----- Inlining 0x… <SharedFunctionInfo …> with bytecode -----`: one per
    /// inlining site, before the first real phase (spike-findings.md §9).
    Inlining { sfi: Option<Addr>, callee: String },
    /// A non-graph listing (Phase 8): `--print-opt-code` sub-sections
    /// (Instructions, Deoptimization Input Data, Safepoints, RelocInfo) and
    /// `--print-bytecode` tables (Constant pool, Handler Table, Source
    /// Position Table). Not diffable, no block structure.
    Listing,
}

#[derive(Debug, Clone)]
pub struct PhaseSection {
    pub name: String,
    pub kind: PhaseKind,
    /// Lines from the banner (inclusive) to the next boundary (exclusive).
    pub lines: Range<usize>,
}

/// Interstitial top-level content: the fallback that guarantees no line is
/// ever dropped (PLAN principle 1). Byte ranges into the source buffer keep
/// d8's own ANSI colours available to the raw view; matching happens on
/// stripped text.
#[derive(Debug, Clone)]
pub struct RawSection {
    pub lines: Range<usize>,
    /// First non-blank line, stripped — the sidebar label.
    pub label: String,
}

/// A single-line lifecycle event (`--trace-opt`, `--trace-deopt`,
/// `--trace-osr`). Timeline *UI* is Phase 6; the data is cheap to keep now.
#[derive(Debug, Clone)]
pub struct TimelineEvent {
    pub line: usize,
    pub kind: EventKind,
}

#[derive(Debug, Clone, PartialEq)]
pub enum EventKind {
    /// `[marking <fn> for optimization to <TIER>, …, reason: <r>]`
    Marking {
        name: String,
        sfi: Option<Addr>,
        target: Tier,
        reason: String,
    },
    /// `[compiling method <fn> (target <TIER>)[ OSR], mode: …]`
    CompileStart {
        name: String,
        sfi: Option<Addr>,
        target: Tier,
        osr: bool,
    },
    /// `[completed compiling <fn> (target <TIER>)[ OSR] - took …]`
    CompileDone {
        name: String,
        sfi: Option<Addr>,
        target: Tier,
        osr: bool,
    },
    /// `[bailout (kind: <k>, reason: <r>): begin. deoptimizing <fn>, <Code T>,
    ///  opt id N, bytecode offset N, …]`
    DeoptBegin {
        kind: String,
        reason: String,
        name: String,
        sfi: Option<Addr>,
        tier: Tier,
        opt_id: Option<u32>,
        bytecode_offset: Option<u32>,
    },
    /// `[bailout end. code_invalidation: <invalidated|unaffected>…]`
    DeoptEnd { invalidated: bool },
    /// `[OSR - <what>. function: <name>[, osr offset: N]…]`
    Osr {
        what: String,
        name: String,
        osr_offset: Option<u32>,
    },
}

impl EventKind {
    /// Short badge for telemetry and goldens.
    pub fn label(&self) -> &'static str {
        match self {
            EventKind::Marking { .. } => "marking",
            EventKind::CompileStart { .. } => "compile-start",
            EventKind::CompileDone { .. } => "compile-done",
            EventKind::DeoptBegin { .. } => "deopt",
            EventKind::DeoptEnd { .. } => "deopt-end",
            EventKind::Osr { .. } => "osr",
        }
    }
}

// ---------------------------------------------------------------------------
// Parse-time types (lazy, per compilation viewed)
// ---------------------------------------------------------------------------

/// A node id `nN` — the stable identity across phases.
pub type NodeId = u32;

/// Sentinel id for schedule-only lines (`16: GapMove(…)`): they occupy a
/// schedule position but define nothing another line can reference. Anything
/// resolving "the node here" must treat this as "no node" — several lines can
/// share it (found in review: the cursor once highlighted every gap move at
/// once).
pub const SCHEDULE_ONLY: NodeId = u32::MAX;

/// A basic-block id `bN`.
pub type BlockId = u32;

/// A reference to a node inside a line, with the span of the `nN` token in the
/// *stripped* text of that line — the coordinates the styled view works in.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct NodeRef {
    pub node: NodeId,
    pub span: Span,
}

/// A `bN` block reference inside a line (branch/jump targets), with its span
/// for styling.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct BlockRef {
    pub block: BlockId,
    pub span: Span,
}

/// Byte range within one stripped line.
pub type Span = Range<u32>;

/// Per-phase rendering of one node occurrence (spike-findings.md §5): the
/// identity is `id`; everything else varies phase to phase and is recorded as
/// spans over the stripped line, not re-parsed strings.
#[derive(Debug, Clone)]
pub struct IRNode {
    pub id: NodeId,
    /// The `A/` in `A/B:` — present from Dead-nodes-sweeping onwards.
    pub schedule_pos: Option<u32>,
    /// Span of the id token(s) up to the colon: the definition site.
    pub def_span: Span,
    /// Interned opcode index into [`ParsedCompilation::opcodes`].
    pub opcode: u32,
    pub opcode_span: Span,
    pub inputs: Vec<NodeRef>,
    /// `, N uses` when the phase prints it. Do not assume it exists (§5).
    pub uses: Option<u32>,
    /// `🪦` — zero uses, kept only for rendering.
    pub dead: bool,
    /// Branch/jump targets (`Jump b1`, `BranchIf… b2 b8`).
    pub targets: Vec<BlockRef>,
}

/// One interpreter-bytecode row, from a `----- Bytecode array -----` dump or
/// interleaved as source context inside a graph phase:
/// `   0x31a010001b0 @    0 : 33 03 00 00   GetNamedProperty a0, [0:"v"], FBV[0]`.
///
/// The refs give bytecode listings the same def-use navigation graphs have:
/// jump operands point at other offsets, `FBV[N]` operands point at the
/// feedback-vector slots printed right below the array, `[N:…]` operands
/// point at constant-pool entries.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct BytecodeRow {
    pub offset: u32,
    /// Span of the offset token — what a jump into this row highlights.
    pub offset_span: Span,
    pub mnemonic: Span,
    /// Jump-target offsets: the `(0x… @ N)` suffix on jumps, plus every
    /// `@N` case of a switch's `{ 0: @44, 1: @48 }` table.
    pub targets: Vec<(u32, Span)>,
    /// `FBV[N]` feedback-vector slot operands.
    pub fbv: Vec<(u32, Span)>,
    /// `[N:…]` constant-pool operands (the bare `[N]` form is *not* a pool
    /// ref — for `AddSmi [1]` it is an immediate — so only the annotated
    /// form counts).
    pub pool: Vec<(u32, Span)>,
    /// True in bytecode-array dumps, where the rows are the content and get
    /// full styling; false for the interleaved context rows inside graph
    /// phases, which stay uniformly dim.
    pub primary: bool,
    /// The `NNN S>` / `NNN E>` source marker, when the dump prints one: a
    /// character offset into the script — the fine-grained anchor the
    /// alignment pane uses. Absent when the function's source
    /// position table was never collected.
    pub src_pos: Option<u32>,
}

/// InlineCacheState of a feedback slot, classified from the printed word for
/// styling. `Other` covers the non-IC feedback kinds (`BinaryOp:SignedSmall`,
/// `JumpLoop`, …) whose "state" is a type lattice, not an IC state.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum IcState {
    Uninitialized,
    Monomorphic,
    Polymorphic,
    Megamorphic,
    Other,
}

/// Eager / lazy / throw — the three deopt-frame arrows (§6, §12).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum FrameArrow {
    Eager,
    Lazy,
    Throw,
}

impl FrameArrow {
    pub fn label(self) -> &'static str {
        match self {
            FrameArrow::Eager => "eager",
            FrameArrow::Lazy => "lazy",
            FrameArrow::Throw => "throw",
        }
    }
}

/// One distinct deopt frame. Interned: under `--print-maglev-deopt-verbose`
/// frames carry an `(addr:0x…)` host pointer and are heavily shared (215
/// rendered lines → 39 distinct frames in the corpus); keying on the address
/// recovers that sharing (§12). Without the address, the payload text is the
/// key — identical summaries collapse, which is the best available.
#[derive(Debug, Clone)]
pub struct DeoptFrame {
    pub arrow: FrameArrow,
    /// `@N` bytecode offset. `@-1` is real output; absent for bare `↳ throw (bN)`.
    pub bytecode_offset: Option<i32>,
    /// `↳ throw @26 (b2)`: the catch block — the only place the exception edge
    /// appears in the output (§12).
    pub catch_block: Option<BlockId>,
    /// The `(addr:0x…)` host pointer when printed. Volatile across runs; used
    /// as intern key, masked in goldens.
    pub addr: Option<u64>,
    /// How many lines rendered this frame.
    pub occurrences: u32,
}

/// One classified line of a parsed phase. Every line of the phase's range gets
/// exactly one of these — nothing is dropped, unmatched lines become
/// [`LineInfo::Annotation`].
#[derive(Debug, Clone)]
pub enum LineInfo {
    /// The `----- … -----` banner itself.
    Banner,
    /// `Graph`, box-drawing-only lines, blank lines.
    Control,
    /// ` Block bN` header (possibly behind a control-flow gutter).
    BlockHeader { block: BlockId },
    /// `0x… <SharedFunctionInfo …> (0x… <String…: "file">:line:col)` —
    /// switches the current inlined-function context inside a block. The
    /// position is the *function definition* anchor (0-based line, column
    /// of its parameter list); `0:0` means none was printed. `same_script`
    /// is false for callees inlined from another file.
    SfiContext {
        line: u32,
        col: u32,
        same_script: bool,
    },
    /// A bytecode row: primary content of a `Bytecode array` dump, or
    /// interleaved source context in a graph phase (see [`BytecodeRow`]).
    Bytecode(BytecodeRow),
    /// ` - slot #N Kind STATE {` — a feedback-vector slot header. The slot
    /// is what a bytecode row's `FBV[N]` operand refers to.
    FeedbackSlot {
        index: u32,
        /// Span of `slot #N` — the definition site.
        def_span: Span,
        kind: Span,
        /// The state word(s); empty for kinds that print none (`Literal`).
        state: Span,
        ic: IcState,
    },
    /// `           N: 0x… <String[1]: #v>` — a constant-pool entry, inside
    /// the `Constant pool (size = N)` region of a bytecode dump.
    PoolEntry {
        index: u32,
        /// Span of the `N:` index token.
        def_span: Span,
    },
    /// A node definition line.
    Node(IRNode),
    /// A deopt-frame line (arrow or continuation), pointing at its interned
    /// frame. `after_node` is the node the frame hangs off.
    Frame {
        frame: u32,
        after_node: Option<NodeId>,
        /// Node refs inside a verbose payload (`a0:n2:[stack:-7|t]`).
        refs: Vec<NodeRef>,
    },
    /// `VOs : { … }` virtual-objects line (only under `--trace-deopt-verbose`).
    VirtualObjects,
    /// `- nA → B: φ… rN …` phi-input move printed at block edges.
    PhiMove { refs: Vec<NodeRef> },
    /// One `--print-opt-code` disassembly row (Phase 8.2):
    /// `0x1700001bc  1c  540002c9  b.ls #+0x58 (addr 0x170000214)  ;; comment`.
    /// Spans style what V8 itself emitted — no decoding is promised beyond
    /// that (PLAN §7.6 feasibility note). The resolved target fields give
    /// assembly the same def-use navigation as graphs: arm64
    /// prints absolute `(addr 0x…)` targets, x64 prints `<+0x…>` code
    /// offsets, so both forms are kept and either resolves a row.
    Disasm {
        /// The code offset (second column, hex).
        offset: u32,
        /// Span of the offset column — the label a branch jump lands on.
        offset_span: Span,
        /// The instruction's own address (first column) — how absolute
        /// branch targets resolve to rows.
        addr: u64,
        mnemonic: Span,
        /// The `(addr 0x…)` / `<+0x…>` branch-target token, when present.
        target: Option<Span>,
        /// Branch destination as a code offset (`<+0x…>`), when printed.
        target_offset: Option<u32>,
        /// Branch destination as an absolute address (`(addr 0x…)`).
        target_addr: Option<u64>,
        /// Some branch in this listing lands here — a de-facto block
        /// leader, labelled in the view and a stop for `[`/`]`.
        labeled: bool,
        /// The `;; …` reloc comment, when present.
        comment: Option<Span>,
    },
    /// A line of JavaScript source — the `--- Raw source ---` block of a
    /// code dump. Painted with a small JS tokenizer at render time; no
    /// spans are recorded here because the text is the structure.
    Source,
    /// Anything unmatched: attached in place, never exiled.
    Annotation { after_node: Option<NodeId> },
}

/// One parsed phase: classification of every line, plus def/use maps for
/// cursor highlighting (4.3/4.4).
#[derive(Debug, Default)]
pub struct ParsedPhase {
    /// Parallel to the phase's line range: `infos[i]` describes line
    /// `lines.start + i`.
    pub infos: Vec<LineInfo>,
    /// node id → index into `infos` of its definition line.
    pub defs: std::collections::HashMap<NodeId, u32>,
    /// node id → indices into `infos` of lines that consume it.
    pub users: std::collections::HashMap<NodeId, Vec<u32>>,
    pub node_count: u32,
    pub block_count: u32,
    pub annotation_count: u32,
}

/// A parsed inlining decision from `--trace-maglev-inlining` output
/// (`⚡ INLINE small …`, `❌ SKIP big …`), with or without the volatile
/// `[ML:<id>]` prefix — the prefix must never be load-bearing
/// (spike-findings.md §8).
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct InlineDecision {
    pub line: usize,
    pub inlined: bool,
    pub callee: String,
    pub reason: String,
}

/// Everything lazy parsing produced for one compilation.
#[derive(Debug, Default)]
pub struct ParsedCompilation {
    /// Interned opcode strings; `IRNode::opcode` indexes this.
    pub opcodes: Vec<String>,
    /// Interned deopt frames; `LineInfo::Frame::frame` indexes this.
    pub frames: Vec<DeoptFrame>,
    /// One entry per phase section, in the same order.
    pub phases: Vec<ParsedPhase>,
    /// Classification of the preamble lines (annotations, inline decisions).
    pub preamble: Vec<LineInfo>,
    pub inline_decisions: Vec<InlineDecision>,
    /// Script and position from the first SFI-context line, when present.
    pub script: Option<String>,
    /// The main function's definition anchor (0-based script line), from the
    /// first same-script SFI context with a real position.
    pub script_line: Option<u32>,
    /// `source_position = N` from an attached code dump: the function's
    /// character offset in the script — the base that aligns the embedded
    /// `Raw source` text when the `.js` file cannot be resolved.
    pub source_position: Option<u32>,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn addr_ignores_zero_padding() {
        assert_eq!(
            Addr::parse("0x09b80101e2cd").unwrap(),
            Addr::parse("0x9b80101e2cd").unwrap()
        );
        assert!(Addr::parse("not-hex").is_none());
    }

    #[test]
    fn tier_maps_both_spellings() {
        assert_eq!(Tier::parse("Maglev"), Tier::parse("MAGLEV"));
        assert_eq!(Tier::parse("TURBOFAN_JS"), Tier::parse("Turbofan"));
        assert_eq!(Tier::parse("Weird"), Tier::Other("Weird".into()));
    }

    #[test]
    fn toplevel_display_name_falls_back() {
        let c = CompilationSection {
            key: CompilationKey {
                sfi: Addr(1),
                tier: Tier::Maglev,
                ordinal: 1,
            },
            name: String::new(),
            function_addr: None,
            lines: 0..0,
            osr: None,
            phases: vec![],
            preamble: 0..0,
            filtered_out: false,
        };
        assert_eq!(c.display_name(), "<toplevel>");
    }
}
