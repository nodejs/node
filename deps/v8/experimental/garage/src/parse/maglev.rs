// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! The Maglev graph parser.
//!
//! Line-oriented and total: every line of a compilation gets classified into
//! exactly one [`LineInfo`], unmatched lines become positioned annotations
//! (never dropped, never exiled), and no input can panic it. The
//! parser works on ANSI-stripped text (spike-findings.md §1) and records
//! *spans* into that text rather than re-rendered strings, because Phase 4
//! styles the original line, not a reconstruction of it.
//!
//! Grammar facts this encodes, all measured in the corpus:
//!
//! - Node-line syntax varies per phase (§5): `11:` becomes `12/13:` after
//!   scheduling, inputs grow `v0/n9:(x)` decoration, `, N uses` becomes
//!   `→ <loc>` and `live range: [a-b]`. Identity is the `nN` id.
//! - Deopt frames are node-attached structure, not annotations (§6): eager
//!   frames print *before* their node, lazy/throw frames *after* it.
//! - Frames are interned on the `(addr:0x…)` host pointer when present, else
//!   on their canonical payload (§12).
//! - `↳ throw` has its own arms: `@N (bN) : {reg:node, …}` with phis, bare
//!   `(bN)` without; the catch-block id is the exception edge (§12).
//! - Graph lines sit behind a control-flow gutter of box-drawing characters
//!   (`│╭─►Block b4`); classification happens after the gutter.

use std::collections::HashMap;

use crate::ansi;
use crate::model::{
    BlockRef, CompilationSection, DeoptFrame, FrameArrow, IRNode, IcState, InlineDecision,
    LineInfo, NodeId, NodeRef, ParsedCompilation, ParsedPhase, PhaseKind,
};
use crate::source::LogBuffer;

/// Parses one compilation's full line range. Total: never panics, never skips
/// a line.
pub fn parse_compilation(buffer: &LogBuffer, section: &CompilationSection) -> ParsedCompilation {
    let mut parsed = ParsedCompilation::default();
    let mut interner = Interner::default();

    // An opt-code section's preamble is the raw JS source — real content,
    // not trace chatter, so it must not fold away as annotations; it gets
    // JS token styling instead.
    let code_listing = matches!(
        section.phases.first().map(|p| &p.kind),
        Some(PhaseKind::Listing)
    );
    for line in section.preamble.clone() {
        let text = line_text(buffer, line);
        let info = if code_listing {
            if text.trim().is_empty() {
                LineInfo::Control
            } else {
                LineInfo::Source
            }
        } else {
            classify_preamble(&text, line, &mut parsed.inline_decisions)
        };
        parsed.preamble.push(info);
    }

    // Phase 8: the body grammar is decided by tier and phase kind. TurboFan
    // sections carry four grammars of their own (see parse::turbofan);
    // everything else keeps the Maglev grammar — including Turbolev, whose
    // phases print Maglev IR.
    let turbofan = section.key.tier == crate::model::Tier::Turbofan;
    for phase in &section.phases {
        let parsed_phase = match &phase.kind {
            PhaseKind::Listing => {
                super::listing::parse_listing_phase(buffer, phase.lines.clone(), &phase.name)
            }
            // The Turbolev pipeline prints Maglev IR in its early phases and
            // *Turboshaft* IR after lowering, under banner names the marker
            // table may not know (user report: `MERGE B…` headers and `#N`
            // refs went through the Maglev grammar, so folding and def-use
            // were dead there). A bounded body sniff routes each phase to
            // the grammar its lines actually use.
            PhaseKind::Graph { .. } if turbofan || turboshaft_body(buffer, phase.lines.clone()) => {
                super::turbofan::parse_phase(
                    buffer,
                    phase.lines.clone(),
                    &phase.name,
                    &mut interner,
                )
            }
            PhaseKind::Graph { .. } => {
                parse_graph_phase(buffer, phase.lines.clone(), &mut interner, &mut parsed)
            }
            // Bytecode dumps and inlined-callee bytecode share the banner
            // grammar but not the body grammar; their lines are expected dump
            // content, not stray annotations.
            PhaseKind::Bytecode | PhaseKind::Inlining { .. } => {
                parse_dump_phase(buffer, phase.lines.clone(), &mut parsed)
            }
        };
        parsed.phases.push(parsed_phase);
    }

    // An attached code dump's `source_position = N` is the function's char
    // offset in the script — the base that aligns the embedded Raw source
    // fallback. It sits in the first few identity lines of the
    // `Optimized code` phase.
    for section_phase in &section.phases {
        if section_phase.name != "Optimized code" {
            continue;
        }
        for line in section_phase.lines.clone().take(12) {
            let text = line_text(buffer, line);
            if let Some(rest) = text.strip_prefix("source_position = ") {
                parsed.source_position = rest.trim().parse().ok();
                break;
            }
        }
        break;
    }

    parsed.opcodes = interner.opcodes;
    parsed.frames = interner.frames;
    parsed
}

/// The stripped, lossily-decoded text of one line — the exact coordinate
/// space every [`Span`] in the parse refers to.
pub fn line_text(buffer: &LogBuffer, line: usize) -> String {
    let bytes = buffer.line(line).unwrap_or(b"");
    String::from_utf8_lossy(&ansi::strip(bytes)).into_owned()
}

/// Does this graph phase print Turboshaft IR? Decided from the body, not the
/// banner: block headers are the giveaway (`BLOCK B4 <- B3` / `MERGE B9 <-
/// B7, B8` / `LOOP …` vs Maglev's ` Block b4`), and one appears within the
/// first few lines of every Turboshaft dump. Checking a bounded prefix keeps
/// this O(1) per phase.
fn turboshaft_body(buffer: &LogBuffer, lines: std::ops::Range<usize>) -> bool {
    for line in lines.skip(1).take(30) {
        let text = line_text(buffer, line);
        let t = text.trim_start();
        for header in ["BLOCK B", "MERGE B", "LOOP B"] {
            if let Some(rest) = t.strip_prefix(header)
                && rest.as_bytes().first().is_some_and(|b| b.is_ascii_digit())
            {
                return true;
            }
        }
        if t.starts_with("Block b") {
            return false;
        }
    }
    false
}

#[derive(Default)]
pub(crate) struct Interner {
    pub(crate) opcodes: Vec<String>,
    opcode_ids: HashMap<String, u32>,
    pub(crate) frames: Vec<DeoptFrame>,
    frame_ids: HashMap<FrameKey, u32>,
}

#[derive(Debug, Clone, PartialEq, Eq, Hash)]
enum FrameKey {
    /// The `(addr:0x…)` host pointer — the printer's own identity for the
    /// frame, shared across every line that renders it.
    Addr(u64),
    /// No pointer printed: the payload text is the best available key;
    /// identical summaries collapse.
    Payload(String),
}

impl Interner {
    pub(crate) fn opcode(&mut self, name: &str) -> u32 {
        if let Some(&id) = self.opcode_ids.get(name) {
            return id;
        }
        let id = self.opcodes.len() as u32;
        self.opcodes.push(name.to_string());
        self.opcode_ids.insert(name.to_string(), id);
        id
    }

    fn frame(&mut self, frame: DeoptFrame, key: FrameKey) -> u32 {
        if let Some(&id) = self.frame_ids.get(&key) {
            self.frames[id as usize].occurrences += 1;
            return id;
        }
        let id = self.frames.len() as u32;
        self.frames.push(frame);
        self.frame_ids.insert(key, id);
        id
    }
}

/// Which part of a bytecode dump the cursor line sits in. The regions are
/// delimited by V8's own headers, in the order the printer emits them:
/// bytecode rows, then `Constant pool (size = N)`, then the handler / source
/// position tables, then the feedback vector (`--maglev-print-feedback`,
/// default on).
#[derive(PartialEq)]
enum DumpRegion {
    Code,
    Pool,
    Tables,
    Feedback,
}

/// Parses a `Bytecode array` / `Inlining …` dump phase: bytecode
/// rows with their jump / feedback / constant-pool operands resolved,
/// constant-pool entry rows, and feedback-vector slot headers. Everything
/// else in the dump is expected reference content — Control, not annotation.
fn parse_dump_phase(
    buffer: &LogBuffer,
    lines: std::ops::Range<usize>,
    parsed: &mut ParsedCompilation,
) -> ParsedPhase {
    let mut phase = ParsedPhase::default();
    let mut region = DumpRegion::Code;
    for (offset, line) in lines.enumerate() {
        let text = line_text(buffer, line);
        let t = text.trim_start();
        let info = if offset == 0 {
            LineInfo::Banner
        } else if t.starts_with("Constant pool (size") {
            region = DumpRegion::Pool;
            LineInfo::Control
        } else if t.starts_with("Handler Table (size")
            || t.starts_with("Source Position Table (size")
        {
            region = DumpRegion::Tables;
            LineInfo::Control
        } else if t.starts_with("0x") && t.contains(": [FeedbackVector]") {
            region = DumpRegion::Feedback;
            LineInfo::Control
        } else if region == DumpRegion::Code
            && let Some(info) = parse_bytecode_row(&text, true)
        {
            info
        } else if region == DumpRegion::Pool
            && let Some(info) = parse_pool_entry(&text)
        {
            info
        } else if region == DumpRegion::Feedback
            && let Some(info) = parse_slot_header(&text)
        {
            info
        } else {
            // Inlining-trace lines print between banners, inside the bytecode
            // and inlined-callee sections — not only in the preamble.
            record_inline_decision(&text, line, &mut parsed.inline_decisions);
            LineInfo::Control
        };
        phase.infos.push(info);
    }
    phase
}

/// A full bytecode-array row:
/// `         0x31a0100012c @    0 : 0b 04             Ldar a1`, with an
/// optional `   43 S> ` / `   57 E> ` source-position marker in front
/// (`--print-bytecode` output in some V8 versions).
fn parse_bytecode_row(text: &str, primary: bool) -> Option<LineInfo> {
    let b = text.as_bytes();
    let mut at = space_len(text, 0);
    // Optional source-position marker: digits, space(s), `S>` or `E>` — a
    // character offset into the script, the alignment pane's fine anchor
    // The address itself starts with the digit `0` (`0x…`), so
    // an absent marker falls through rather than failing.
    let mut src_pos = None;
    if b.get(at).is_some_and(u8::is_ascii_digit) {
        let d = digit_len(text, at);
        let m = at + d + space_len(text, at + d);
        if text[m..].starts_with("S>") || text[m..].starts_with("E>") {
            src_pos = text[at..at + d].parse().ok();
            at = m + 2 + space_len(text, m + 2);
        }
    }
    // `0x<addr>`
    if !text[at..].starts_with("0x") {
        return None;
    }
    at += 2;
    let addr_len = text[at..].bytes().take_while(u8::is_ascii_hexdigit).count();
    if addr_len == 0 {
        return None;
    }
    at += addr_len + space_len(text, at + addr_len);
    // `@    <offset>`
    if b.get(at) != Some(&b'@') {
        return None;
    }
    at += 1 + space_len(text, at + 1);
    let digits = text[at..].bytes().take_while(u8::is_ascii_digit).count();
    if digits == 0 {
        return None;
    }
    let offset: u32 = text[at..at + digits].parse().ok()?;
    let offset_span = at as u32..(at + digits) as u32;
    at += digits + space_len(text, at + digits);
    // `: `
    if b.get(at) != Some(&b':') {
        return None;
    }
    at += 1;
    parse_row_tail(text, at, offset, offset_span, primary, src_pos)
}

/// The part of a bytecode row after the `:` — hex encoding bytes, mnemonic,
/// operands. Shared between the dump rows (which lead with an address) and
/// the interleaved graph-context rows (which lead with the offset alone).
fn parse_row_tail(
    text: &str,
    mut at: usize,
    offset: u32,
    offset_span: crate::model::Span,
    primary: bool,
    src_pos: Option<u32>,
) -> Option<LineInfo> {
    let b = text.as_bytes();
    at += space_len(text, at);
    // Encoding bytes: one or more 2-hex-digit tokens. Required — this is
    // what distinguishes a bytecode row from prose containing a colon.
    let mut pairs = 0;
    while at + 2 <= text.len()
        && b[at].is_ascii_hexdigit()
        && b[at + 1].is_ascii_hexdigit()
        && (at + 2 == text.len() || b[at + 2] == b' ')
    {
        pairs += 1;
        at += 2 + space_len(text, at + 2);
    }
    if pairs == 0 {
        return None;
    }
    // Mnemonic: `Ldar`, `LdaSmi.ExtraWide`, … — possibly absent when the
    // encoding bytes end the line.
    let m_start = at;
    if b.get(at).is_some_and(u8::is_ascii_alphabetic) {
        while b
            .get(at)
            .is_some_and(|c| c.is_ascii_alphanumeric() || *c == b'.' || *c == b'_')
        {
            at += 1;
        }
    }
    let mnemonic = m_start as u32..at as u32;

    // Operand scan. Three ref shapes matter (everything else is left as
    // plain text): `FBV[N]` feedback slots, `[N:…]` constant-pool operands
    // (bare `[N]` is an immediate, not a pool index), and jump targets —
    // the `(0x… @ N)` suffix plus `@N` cases inside a switch's `{ … }`
    // jump table.
    // Byte-wise scan (fuzz: mutated input puts multibyte garbage between
    // operands; `text[i..]` at an arbitrary byte offset panics, byte-slice
    // matching cannot). Every range actually sliced out of `text` below
    // starts at a matched ASCII pattern and covers ASCII digits only, so the
    // boundaries are sound.
    let mut targets = Vec::new();
    let mut fbv = Vec::new();
    let mut pool = Vec::new();
    let mut brace_depth = 0u32;
    let mut i = at;
    while i < b.len() {
        let rest = &b[i..];
        if rest.starts_with(b"FBV[") {
            let d = digit_len(text, i + 4);
            if d > 0 && b.get(i + 4 + d) == Some(&b']') {
                let end = i + 4 + d + 1;
                if let Ok(n) = text[i + 4..i + 4 + d].parse() {
                    fbv.push((n, i as u32..end as u32));
                }
                i = end;
                continue;
            }
        } else if b[i] == b'[' {
            let d = digit_len(text, i + 1);
            if d > 0 && b.get(i + 1 + d) == Some(&b':') {
                // Bracket-depth scan: pool values nest brackets
                // (`[0:0x… <FixedArray[2]>]`).
                let mut depth = 1;
                let mut j = i + 1;
                while j < b.len() && depth > 0 {
                    match b[j] {
                        b'[' => depth += 1,
                        b']' => depth -= 1,
                        _ => {}
                    }
                    j += 1;
                }
                // `j` may sit mid-char after scanning past multibyte bytes;
                // only the span (display coordinates) uses it, no slicing.
                if depth == 0
                    && let Ok(n) = text[i + 1..i + 1 + d].parse()
                {
                    pool.push((n, i as u32..j as u32));
                    i = j;
                    continue;
                }
            }
        } else if rest.starts_with(b"(0x") {
            let h = b[i + 3..]
                .iter()
                .take_while(|c| c.is_ascii_hexdigit())
                .count();
            if h > 0 && b[i + 3 + h..].starts_with(b" @ ") {
                let ds = i + 3 + h + 3;
                let d = digit_len(text, ds);
                if d > 0 && b.get(ds + d) == Some(&b')') {
                    let end = ds + d + 1;
                    if let Ok(n) = text[ds..ds + d].parse() {
                        targets.push((n, i as u32..end as u32));
                    }
                    i = end;
                    continue;
                }
            }
        } else if b[i] == b'{' {
            brace_depth += 1;
        } else if b[i] == b'}' {
            brace_depth = brace_depth.saturating_sub(1);
        } else if b[i] == b'@' && brace_depth > 0 {
            let d = digit_len(text, i + 1);
            if d > 0 {
                if let Ok(n) = text[i + 1..i + 1 + d].parse() {
                    targets.push((n, i as u32..(i + 1 + d) as u32));
                }
                i += 1 + d;
                continue;
            }
        }
        i += 1;
    }

    Some(LineInfo::Bytecode(crate::model::BytecodeRow {
        offset,
        offset_span,
        mnemonic,
        targets,
        fbv,
        pool,
        primary,
        src_pos,
    }))
}

/// `           0: 0x09b8000034d5 <String[1]: #v>` — a constant-pool entry
/// row inside the `Constant pool (size = N)` region.
pub(crate) fn parse_pool_entry(text: &str) -> Option<LineInfo> {
    let start = space_len(text, 0);
    if start == 0 {
        // Entries are always indented; an unindented `0:` is something else.
        return None;
    }
    let d = digit_len(text, start);
    if d == 0 || text.as_bytes().get(start + d) != Some(&b':') {
        return None;
    }
    let index = text[start..start + d].parse().ok()?;
    Some(LineInfo::PoolEntry {
        index,
        def_span: start as u32..(start + d + 1) as u32,
    })
}

/// ` - slot #0 LoadProperty MONOMORPHIC` / ` - slot #1 BinaryOp
/// BinaryOp:SignedSmall {` / ` - slot #0 Literal  {` — a feedback-vector
/// slot header. Kind and state are recorded as spans; the state word is
/// classified for styling.
fn parse_slot_header(text: &str) -> Option<LineInfo> {
    let b = text.as_bytes();
    let start = space_len(text, 0);
    let rest = text[start..].strip_prefix("- slot #")?;
    let d_start = start + "- slot #".len();
    let d = digit_len(text, d_start);
    if d == 0 {
        return None;
    }
    let index = rest[..d].parse().ok()?;
    // `slot #N` is the definition token, without the `- ` bullet.
    let def_span = (start + 2) as u32..(d_start + d) as u32;

    let k_start = d_start + d + space_len(text, d_start + d);
    let mut k_end = k_start;
    while b.get(k_end).is_some_and(|c| !c.is_ascii_whitespace()) {
        k_end += 1;
    }
    let s_start = k_end + space_len(text, k_end);
    // The state runs to the end of the line, minus a trailing ` {`.
    let mut s_end = text.trim_end().len();
    if text[..s_end].ends_with('{') {
        s_end = text[..s_end - 1].trim_end().len();
    }
    let s_end = s_end.max(s_start);
    let state = &text[s_start..s_end];

    let ic = if state.contains("MEGAMORPHIC") {
        IcState::Megamorphic
    } else if state.contains("POLYMORPHIC") {
        IcState::Polymorphic
    } else if state.contains("MONOMORPHIC") {
        IcState::Monomorphic
    } else if state.contains("UNINITIALIZED") || state.contains("PREMONOMORPHIC") {
        IcState::Uninitialized
    } else {
        IcState::Other
    };

    Some(LineInfo::FeedbackSlot {
        index,
        def_span,
        kind: k_start as u32..k_end as u32,
        state: s_start as u32..s_end as u32,
        ic,
    })
}

fn space_len(text: &str, at: usize) -> usize {
    text.as_bytes()
        .get(at..)
        .map(|rest| rest.iter().take_while(|b| **b == b' ').count())
        .unwrap_or(0)
}

fn digit_len(text: &str, at: usize) -> usize {
    text.as_bytes()
        .get(at..)
        .map(|rest| rest.iter().take_while(|b| b.is_ascii_digit()).count())
        .unwrap_or(0)
}

fn parse_graph_phase(
    buffer: &LogBuffer,
    lines: std::ops::Range<usize>,
    interner: &mut Interner,
    parsed: &mut ParsedCompilation,
) -> ParsedPhase {
    let mut phase = ParsedPhase::default();
    // Once a phase prints `sched/node:` ids, a bare `N:` is a schedule-only
    // line (GapMove and friends), not a node id (spike-findings.md §5).
    let mut saw_dual_id = false;
    let mut last_node: Option<NodeId> = None;
    let mut last_arrow = FrameArrow::Eager;
    // Eager frames print before the node they belong to; their attachment is
    // patched when that node line arrives.
    let mut pending_eager: Vec<usize> = Vec::new();

    for (offset, line) in lines.clone().enumerate() {
        let text = line_text(buffer, line);
        let mut info = if offset == 0 {
            LineInfo::Banner
        } else {
            classify_graph_line(
                &text,
                interner,
                &mut saw_dual_id,
                &mut last_node,
                &mut last_arrow,
            )
        };

        let at = phase.infos.len();
        match &info {
            LineInfo::Node(node) => {
                phase.node_count += 1;
                if node.id != SCHEDULE_ONLY {
                    phase.defs.insert(node.id, at as u32);
                }
                for input in &node.inputs {
                    phase.users.entry(input.node).or_default().push(at as u32);
                }
                for pending in pending_eager.drain(..) {
                    if let Some(LineInfo::Frame { after_node, .. }) = phase.infos.get_mut(pending) {
                        *after_node = Some(node.id);
                    }
                }
            }
            LineInfo::Frame { refs, .. } | LineInfo::PhiMove { refs } => {
                for r in refs {
                    phase.users.entry(r.node).or_default().push(at as u32);
                }
                if matches!(
                    &info,
                    LineInfo::Frame {
                        after_node: None,
                        ..
                    }
                ) {
                    pending_eager.push(at);
                }
            }
            LineInfo::BlockHeader { .. } => phase.block_count += 1,
            LineInfo::SfiContext { .. } => {
                let script = parse_script(&text);
                if parsed.script.is_none() {
                    parsed.script = script.clone();
                }
                if let LineInfo::SfiContext {
                    line: l,
                    same_script,
                    ..
                } = &mut info
                {
                    *same_script = script.is_some() && script == parsed.script;
                    // The main function's own anchor: first same-script
                    // context with a real position.
                    if *same_script && *l > 0 && parsed.script_line.is_none() {
                        parsed.script_line = Some(*l);
                    }
                }
            }
            LineInfo::Annotation { .. } => {
                phase.annotation_count += 1;
                record_inline_decision(&text, line, &mut parsed.inline_decisions);
            }
            _ => {}
        }
        phase.infos.push(info);
    }
    phase
}

/// Splits off the control-flow gutter (`│╭─►`, arrows, indentation) and
/// returns the content start byte offset.
fn content_start(text: &str) -> usize {
    for (i, ch) in text.char_indices() {
        match ch {
            ' ' | '│' | '╭' | '╰' | '╮' | '╯' | '─' | '►' | '◄' | '↓' | '▼' | '▲' | '═' =>
                {}
            _ => return i,
        }
    }
    text.len()
}

fn classify_graph_line(
    text: &str,
    interner: &mut Interner,
    saw_dual_id: &mut bool,
    last_node: &mut Option<NodeId>,
    last_arrow: &mut FrameArrow,
) -> LineInfo {
    let start = content_start(text);
    let content = &text[start..];

    if content.is_empty() || content == "Graph" {
        return LineInfo::Control;
    }

    if let Some(rest) = content.strip_prefix("Block b")
        && let Some(block) = leading_number(rest)
    {
        return LineInfo::BlockHeader { block };
    }

    if content.starts_with("↱") || content.starts_with("↳") || content.starts_with('@') {
        return parse_frame_line(text, start, interner, *last_node, last_arrow);
    }

    if content.starts_with("VOs ") || content == "VOs" {
        return LineInfo::VirtualObjects;
    }

    if content.starts_with("- ") && content.contains('→') && content.contains('φ') {
        return LineInfo::PhiMove {
            refs: node_refs(text, start),
        };
    }

    if content.starts_with("0x") && content.contains("<SharedFunctionInfo") {
        let (line, col) = parse_sfi_position(content);
        // `same_script` is provisional; the phase loop corrects it against
        // the compilation's main script.
        return LineInfo::SfiContext {
            line,
            col,
            same_script: true,
        };
    }

    if let Some(node) = parse_node_line(text, start, interner, saw_dual_id) {
        if node.id != SCHEDULE_ONLY {
            *last_node = Some(node.id);
            return LineInfo::Node(node);
        }
        return LineInfo::Node(node);
    }

    if let Some(info) = parse_interleaved_bytecode(text, start) {
        return info;
    }

    // Section furniture that lands inside the last phase's range — the
    // closing `----------` rule and the Begin/Finished trailers (Turbolev
    // prints them around every dump). Expected structure, not annotations:
    // classifying them as annotations made every section end in dim-italic
    // noise and swept the trailer into `t`'s annotation folds.
    if content.starts_with("Finished compiling method ")
        || content.starts_with("Begin compiling method ")
        || (content.trim_end().len() >= 20 && content.trim_end().bytes().all(|b| b == b'-'))
    {
        return LineInfo::Control;
    }

    // The attachment rule (2.8): anything unmatched is an annotation on the
    // node it follows — regalloc traces, graph-building traces, whatever a
    // future flag prints. Attached in place, dimmed later, never dropped.
    LineInfo::Annotation {
        after_node: *last_node,
    }
}

/// Bare `N : hh hh …` source-bytecode lines interleaved into graph blocks
/// (offset, space, colon — node lines have no space before the colon).
/// `start` is the content offset after the control-flow gutter; spans are
/// absolute into `text`. The rows share the dump grammar's operand scan, so
/// jump targets and `FBV[N]` refs navigate here too — but `primary` is
/// false: interleaved context stays uniformly dim.
fn parse_interleaved_bytecode(text: &str, start: usize) -> Option<LineInfo> {
    let content = &text[start..];
    let (num, _) = content.split_once(" : ")?;
    let offset = num.parse().ok()?;
    let offset_span = start as u32..(start + num.len()) as u32;
    // `num` ends right before ` : `; the tail parser resumes after the colon.
    let colon = start + num.len() + 1;
    parse_row_tail(text, colon + 1, offset, offset_span, false, None)
}

pub use crate::model::SCHEDULE_ONLY;

fn parse_node_line(
    text: &str,
    start: usize,
    interner: &mut Interner,
    saw_dual_id: &mut bool,
) -> Option<IRNode> {
    let content = &text[start..];

    // `11:` / `12/13:` — digits (optionally slash digits) then a colon with
    // no space before it, then a space.
    let mut it = content.char_indices().peekable();
    let mut first_end = 0;
    while let Some(&(i, c)) = it.peek() {
        if c.is_ascii_digit() {
            first_end = i + 1;
            it.next();
        } else {
            break;
        }
    }
    if first_end == 0 {
        return None;
    }
    let first: u32 = content[..first_end].parse().ok()?;
    let (second, ids_end) = match content[first_end..].strip_prefix('/') {
        Some(rest) => {
            let digits = rest.bytes().take_while(|b| b.is_ascii_digit()).count();
            if digits == 0 {
                return None;
            }
            let second: u32 = rest[..digits].parse().ok()?;
            (Some(second), first_end + 1 + digits)
        }
        None => (None, first_end),
    };
    let after_ids = &content[ids_end..];
    if !after_ids.starts_with(": ") {
        return None;
    }
    let opcode_start = ids_end + 2;

    let (schedule_pos, id) = match second {
        Some(node_id) => {
            *saw_dual_id = true;
            (Some(first), node_id)
        }
        None => (None, first),
    };

    // Opcode token: up to the first space or attached `(`; an *attached*
    // `[Tag]` (`Int32Compare[LessThan]`) is part of the opcode, a detached
    // ` [n1, n2]` is the input list.
    let opcode_text = &content[opcode_start..];
    let mut opcode_end = opcode_text.len();
    let mut depth = 0usize;
    for (i, c) in opcode_text.char_indices() {
        match c {
            '[' => depth += 1,
            ']' => depth = depth.saturating_sub(1),
            ' ' | '(' if depth == 0 => {
                opcode_end = i;
                break;
            }
            _ => {}
        }
    }
    let opcode_name = &opcode_text[..opcode_end];
    if opcode_name.is_empty() {
        return None;
    }

    let id = if second.is_none() && *saw_dual_id {
        // Late-phase bare id: schedule position only (GapMove & co).
        SCHEDULE_ONLY
    } else if opcode_name.starts_with("GapMove") || opcode_name.starts_with("ConstantGapMove") {
        SCHEDULE_ONLY
    } else {
        id
    };

    let abs = |o: usize| (start + o) as u32;
    let tail_start = opcode_start + opcode_end;
    let tail = &content[tail_start..];

    Some(IRNode {
        id,
        schedule_pos: if id == SCHEDULE_ONLY && second.is_none() {
            Some(first)
        } else {
            schedule_pos
        },
        def_span: abs(0)..abs(ids_end),
        opcode: interner.opcode(opcode_name),
        opcode_span: abs(opcode_start)..abs(tail_start),
        inputs: node_refs(text, start + tail_start),
        uses: parse_uses(tail),
        dead: tail.contains('🪦'),
        targets: block_refs(tail, start + tail_start),
    })
}

fn parse_uses(tail: &str) -> Option<u32> {
    let digits_end = tail.find(" uses")?;
    let bytes = tail.as_bytes();
    let mut digits_start = digits_end;
    while digits_start > 0 && bytes[digits_start - 1].is_ascii_digit() {
        digits_start -= 1;
    }
    if digits_start == 0 || digits_start == digits_end {
        return None;
    }
    tail[digits_start..digits_end].parse().ok()
}

/// Every `nN` token from `from` to the end of the line, with absolute spans.
///
/// Tokens inside `<…>` heap-object descriptors and quoted strings are
/// skipped: `<JSFunction n1 (sfi = …)>` names a *JS function called `n1`*
/// (minified code does this constantly), not a reference to node 1, and
/// counting it corrupted the def/use maps (found in review).
fn node_refs(text: &str, from: usize) -> Vec<NodeRef> {
    let mut refs = Vec::new();
    let bytes = text.as_bytes();
    let mut i = from;
    let mut angle_depth = 0usize;
    let mut in_string = false;
    while i < bytes.len() {
        match bytes[i] {
            b'<' if !in_string => {
                angle_depth += 1;
                i += 1;
                continue;
            }
            b'>' if !in_string => {
                angle_depth = angle_depth.saturating_sub(1);
                i += 1;
                continue;
            }
            b'"' => {
                in_string = !in_string;
                i += 1;
                continue;
            }
            _ => {}
        }
        if angle_depth > 0 || in_string {
            i += 1;
            continue;
        }
        if bytes[i] == b'n'
            && i + 1 < bytes.len()
            && bytes[i + 1].is_ascii_digit()
            && (i == 0 || !bytes[i - 1].is_ascii_alphanumeric())
        {
            let mut end = i + 1;
            while end < bytes.len() && bytes[end].is_ascii_digit() {
                end += 1;
            }
            if (end >= bytes.len() || !bytes[end].is_ascii_alphabetic())
                && let Ok(node) = text[i + 1..end].parse::<NodeId>()
            {
                refs.push(NodeRef {
                    node,
                    span: i as u32..end as u32,
                });
            }
            i = end;
        } else {
            i += 1;
        }
    }
    refs
}

/// Every `bN` block token in a tail (`Jump b1`, `BranchIf… b2 b8`), with
/// absolute spans (`base` is the tail's offset within the line).
fn block_refs(tail: &str, base: usize) -> Vec<BlockRef> {
    let mut out = Vec::new();
    let bytes = tail.as_bytes();
    let mut i = 0;
    while i < bytes.len() {
        if bytes[i] == b'b'
            && i + 1 < bytes.len()
            && bytes[i + 1].is_ascii_digit()
            && (i == 0 || !bytes[i - 1].is_ascii_alphanumeric())
        {
            let mut end = i + 1;
            while end < bytes.len() && bytes[end].is_ascii_digit() {
                end += 1;
            }
            if (end >= bytes.len() || !bytes[end].is_ascii_alphanumeric())
                && let Ok(block) = tail[i + 1..end].parse()
            {
                out.push(BlockRef {
                    block,
                    span: (base + i) as u32..(base + end) as u32,
                });
            }
            i = end;
        } else {
            i += 1;
        }
    }
    out
}

fn leading_number<T: std::str::FromStr>(s: &str) -> Option<T> {
    let digits = s.bytes().take_while(|b| b.is_ascii_digit()).count();
    if digits == 0 {
        return None;
    }
    s[..digits].parse().ok()
}

/// Parses the three deopt-frame arrows and their continuation lines.
///
/// Payload syntaxes (spike-findings.md §12): `(N live vars)` summary,
/// `{reg:node:loc, …} (addr:0x…)` verbose (loc phase-dependent, possibly
/// empty), `{reg:node, …}` for `↳ throw`, plus bare `↳ throw (bN)`.
fn parse_frame_line(
    text: &str,
    start: usize,
    interner: &mut Interner,
    last_node: Option<NodeId>,
    last_arrow: &mut FrameArrow,
) -> LineInfo {
    let content = &text[start..];

    let (arrow, rest, attach_backwards) = if let Some(r) = content.strip_prefix("↱ eager") {
        (FrameArrow::Eager, r, false)
    } else if let Some(r) = content.strip_prefix("↳ lazy") {
        (FrameArrow::Lazy, r, true)
    } else if let Some(r) = content.strip_prefix("↳ throw") {
        (FrameArrow::Throw, r, true)
    } else {
        // A continuation line (`@2 (5 live vars)` behind depth bars): part of
        // the innermost preceding arrow's frame chain.
        (
            *last_arrow,
            content,
            !matches!(*last_arrow, FrameArrow::Eager),
        )
    };
    *last_arrow = arrow;

    let rest = rest.trim_start();
    let bytecode_offset = rest.strip_prefix('@').and_then(|r| {
        let end = r
            .bytes()
            .take_while(|b| b.is_ascii_digit() || *b == b'-')
            .count();
        r[..end].parse::<i32>().ok()
    });

    // `(bN)` — the catch block; only throw frames print one, and looking for
    // it elsewhere would false-positive on payload text.
    let catch_block = if arrow == FrameArrow::Throw {
        rest.find("(b").and_then(|i| leading_number(&rest[i + 2..]))
    } else {
        None
    };

    let addr = rest.find("(addr:0x").and_then(|i| {
        let hex = &rest[i + 8..];
        let end = hex.bytes().take_while(|b| b.is_ascii_hexdigit()).count();
        u64::from_str_radix(&hex[..end], 16).ok()
    });

    let key = match addr {
        Some(a) => FrameKey::Addr(a),
        None => FrameKey::Payload(format!("{arrow:?} {rest}")),
    };
    let frame = interner.frame(
        DeoptFrame {
            arrow,
            bytecode_offset,
            catch_block,
            addr,
            occurrences: 1,
        },
        key,
    );

    LineInfo::Frame {
        frame,
        after_node: if attach_backwards { last_node } else { None },
        refs: node_refs(text, start),
    }
}

/// `0x… <SharedFunctionInfo name> (0x… <String[N]: "path">:line:col)`
/// The `…">:5:14)` tail of an SFI context line: the function definition
/// anchor as a 0-based (line, column). `(0, 0)` when V8 printed none.
fn parse_sfi_position(content: &str) -> (u32, u32) {
    let Some(end) = content.trim_end().strip_suffix(')') else {
        return (0, 0);
    };
    let mut it = end.rsplitn(3, ':');
    let col = it.next().and_then(|s| s.parse().ok());
    let line = it.next().and_then(|s| s.parse().ok());
    match (line, col) {
        (Some(l), Some(c)) => (l, c),
        _ => (0, 0),
    }
}

fn parse_script(text: &str) -> Option<String> {
    let at = text.find("<String[")?;
    let rest = &text[at..];
    let open = rest.find('"')?;
    let close = rest[open + 1..].find('"')?;
    Some(rest[open + 1..open + 1 + close].to_string())
}

fn classify_preamble(text: &str, line: usize, decisions: &mut Vec<InlineDecision>) -> LineInfo {
    let content = text.trim_start();
    if content.is_empty() {
        return LineInfo::Control;
    }
    record_inline_decision(text, line, decisions);
    LineInfo::Annotation { after_node: None }
}

/// `⚡ INLINE small   reason…` / `❌ SKIP big   reason…`, optionally behind a
/// `[ML:38564] ` prefix — which is volatile and stripped entirely by
/// `--no-trace-with-compilation-id`, so it must never be load-bearing
/// (spike-findings.md §8).
fn record_inline_decision(text: &str, line: usize, decisions: &mut Vec<InlineDecision>) {
    let content = text.trim_start();
    let body = match content.strip_prefix("[ML:") {
        Some(rest) => rest.split_once("] ").map(|(_, b)| b).unwrap_or(content),
        None => content,
    };
    let verdict = if let Some(rest) = body.strip_prefix("⚡ INLINE") {
        Some((true, rest))
    } else {
        body.strip_prefix("❌ SKIP").map(|rest| (false, rest))
    };
    if let Some((inlined, rest)) = verdict {
        let rest = rest.trim_start();
        let (callee, reason) = match rest.split_once("  ") {
            Some((c, r)) => (c.trim(), r.trim()),
            None => (rest.trim(), ""),
        };
        decisions.push(InlineDecision {
            line,
            inlined,
            callee: callee.to_string(),
            reason: reason.to_string(),
        });
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn parse_lines(lines: &[&str]) -> (ParsedPhase, ParsedCompilation) {
        let mut text = String::from("----- Maglev graph building -----\n");
        for l in lines {
            text.push_str(l);
            text.push('\n');
        }
        let mut buffer = LogBuffer::new();
        buffer.append(text.as_bytes());
        buffer.finish();
        let mut parsed = ParsedCompilation::default();
        let mut interner = Interner::default();
        let phase = parse_graph_phase(&buffer, 0..buffer.line_count(), &mut interner, &mut parsed);
        parsed.opcodes = interner.opcodes;
        parsed.frames = interner.frames;
        (phase, parsed)
    }

    fn parse_dump_lines(lines: &[&str]) -> ParsedPhase {
        let mut text = String::from("----- Bytecode array -----\n");
        for l in lines {
            text.push_str(l);
            text.push('\n');
        }
        let mut buffer = LogBuffer::new();
        buffer.append(text.as_bytes());
        buffer.finish();
        let mut parsed = ParsedCompilation::default();
        parse_dump_phase(&buffer, 0..buffer.line_count(), &mut parsed)
    }

    fn bc(info: &LineInfo) -> &crate::model::BytecodeRow {
        match info {
            LineInfo::Bytecode(bc) => bc,
            other => panic!("expected bytecode row, got {other:?}"),
        }
    }

    #[test]
    fn bytecode_rows_parse_offset_mnemonic_and_refs() {
        let phase = parse_dump_lines(&[
            "Parameter count 2",
            "         0x31a010001b0 @    0 : 33 03 00 00       GetNamedProperty a0, [0:\"v\"], FBV[0]",
            "         0x31a010001fa @   14 : a8 11             JumpIfFalse [17] (0x31a0100020b @ 31)",
            "         0x31a0100013f @   67 : 97 33 00 09       JumpLoop [51], [0], FBV[9] (0x31a0100010c @ 16)",
            "         0x31a010001f4 @    8 : d3                Star1",
        ]);
        assert!(matches!(phase.infos[1], LineInfo::Control));

        let get = bc(&phase.infos[2]);
        assert_eq!(get.offset, 0);
        assert!(get.primary);
        assert_eq!(get.fbv.len(), 1, "FBV[0] operand: {:?}", get.fbv);
        assert_eq!(get.fbv[0].0, 0);
        assert_eq!(get.pool.len(), 1, "[0:\"v\"] operand: {:?}", get.pool);
        assert_eq!(get.pool[0].0, 0);
        assert!(get.targets.is_empty());

        let jif = bc(&phase.infos[3]);
        assert_eq!(jif.offset, 14);
        assert_eq!(jif.targets.len(), 1);
        assert_eq!(jif.targets[0].0, 31);
        // The bare `[17]` relative operand is an immediate, not a pool ref.
        assert!(jif.pool.is_empty());

        let loop_ = bc(&phase.infos[4]);
        assert_eq!(loop_.targets[0].0, 16);
        assert_eq!(loop_.fbv[0].0, 9);

        let star = bc(&phase.infos[5]);
        assert_eq!(star.offset, 8);
        assert!(star.targets.is_empty() && star.fbv.is_empty() && star.pool.is_empty());
    }

    #[test]
    fn bytecode_wide_mnemonics_and_embedded_feedback() {
        let phase = parse_dump_lines(&[
            "         0x31a010001ee @    2 : 00 57 ff 03 00 00 BitwiseAndSmi.Wide [1023], FBV[0]",
            "         0x31a010000d0 @   16 : 01 0d a0 86 01 00 LdaSmi.ExtraWide [100000]",
            "         0x31a010001f6 @   10 : 78 f8 01 00       TestEqualStrict r1, EmbeddedFeedback[0]",
        ]);
        let and = bc(&phase.infos[1]);
        assert_eq!(and.fbv.len(), 1);
        let smi = bc(&phase.infos[2]);
        assert!(smi.fbv.is_empty() && smi.pool.is_empty());
        // `EmbeddedFeedback[0]` is not a feedback-vector slot.
        let test = bc(&phase.infos[3]);
        assert!(test.fbv.is_empty(), "{:?}", test.fbv);
    }

    #[test]
    fn bytecode_switch_jump_table_targets() {
        let phase = parse_dump_lines(&[
            "         0x31a01000115 @   25 : ab 00 03 00       SwitchOnSmiNoFeedback [0], [3], [0] { 0: @44, 1: @48, 2: @52 }",
        ]);
        let sw = bc(&phase.infos[1]);
        let ids: Vec<u32> = sw.targets.iter().map(|(t, _)| *t).collect();
        assert_eq!(ids, vec![44, 48, 52]);
    }

    #[test]
    fn bytecode_source_position_markers_are_tolerated() {
        let phase = parse_dump_lines(&[
            "   45 S> 0x2f9d0829f26e @    0 : 0b 03             Ldar a0",
            "   57 E> 0x2f9d0829f270 @    2 : 46 03 00          MulSmi [3], [0]",
            "         0x2f9d0829f273 @    5 : ab                Return",
        ]);
        assert_eq!(bc(&phase.infos[1]).offset, 0);
        assert_eq!(bc(&phase.infos[1]).src_pos, Some(45), "S> char offset");
        assert_eq!(bc(&phase.infos[2]).src_pos, Some(57), "E> char offset");
        assert_eq!(bc(&phase.infos[3]).src_pos, None, "markerless row");
    }

    #[test]
    fn constant_pool_entries_and_regions() {
        let phase = parse_dump_lines(&[
            "         0x31a010001b0 @    0 : 33 03 00 00       GetNamedProperty a0, [0:\"v\"], FBV[0]",
            "Constant pool (size = 1)",
            "0x31a0100017d: [TrustedFixedArray]",
            " - map: 0x09b800000605 <Map(TRUSTED_FIXED_ARRAY_TYPE)>",
            " - length: 1",
            "           0: 0x09b8000034d5 <String[1]: #v>",
            "Handler Table (size = 0)",
            "Source Position Table (size = 0)",
        ]);
        assert!(matches!(phase.infos[2], LineInfo::Control), "pool header");
        assert!(matches!(phase.infos[3], LineInfo::Control), "array object");
        let LineInfo::PoolEntry { index, .. } = &phase.infos[6] else {
            panic!("expected pool entry, got {:?}", phase.infos[6]);
        };
        assert_eq!(*index, 0);
        assert!(matches!(phase.infos[7], LineInfo::Control), "handler table");
    }

    #[test]
    fn feedback_vector_slots_classify_states() {
        let phase = parse_dump_lines(&[
            "0x9b80101e9c1: [FeedbackVector] in OldSpace",
            " - map: 0x09b800000895 <Map(FEEDBACK_VECTOR_TYPE)>",
            " - slot #0 LoadProperty MONOMORPHIC",
            "   [weak] 0x09b80101e519 <Map[16](HOLEY_ELEMENTS)>: LoadHandler(Smi)(kind = kField) {",
            "     [0]: [weak] 0x09b80101e519 <Map[16](HOLEY_ELEMENTS)>",
            "  }",
            " - slot #1 LoadProperty POLYMORPHIC",
            " - slot #2 LoadProperty MEGAMORPHIC {",
            " - slot #3 BinaryOp BinaryOp:SignedSmall {",
            " - slot #4 Literal  {",
            " - slot #5 LoadGlobalNotInsideTypeof UNINITIALIZED {",
        ]);
        let slot = |i: usize| match &phase.infos[i] {
            LineInfo::FeedbackSlot { index, ic, .. } => (*index, *ic),
            other => panic!("expected slot, got {other:?}"),
        };
        assert_eq!(slot(3), (0, IcState::Monomorphic));
        assert!(matches!(phase.infos[4], LineInfo::Control), "payload");
        assert_eq!(slot(7), (1, IcState::Polymorphic));
        assert_eq!(slot(8), (2, IcState::Megamorphic));
        assert_eq!(slot(9), (3, IcState::Other));
        assert_eq!(slot(10), (4, IcState::Other));
        assert_eq!(slot(11), (5, IcState::Uninitialized));
    }

    #[test]
    fn slot_kind_and_state_spans_cover_the_tokens() {
        let text = " - slot #3 BinaryOp BinaryOp:SignedSmall {";
        let Some(LineInfo::FeedbackSlot {
            def_span,
            kind,
            state,
            ..
        }) = parse_slot_header(text)
        else {
            panic!("no slot");
        };
        assert_eq!(
            &text[def_span.start as usize..def_span.end as usize],
            "slot #3"
        );
        assert_eq!(&text[kind.start as usize..kind.end as usize], "BinaryOp");
        assert_eq!(
            &text[state.start as usize..state.end as usize],
            "BinaryOp:SignedSmall"
        );
    }

    fn node(info: &LineInfo) -> &IRNode {
        match info {
            LineInfo::Node(n) => n,
            other => panic!("expected node, got {other:?}"),
        }
    }

    #[test]
    fn early_phase_node_line() {
        let (phase, parsed) = parse_lines(&[
            "  11: Int32AddWithOverflow [n9, n10], 1 uses, can truncate to int32 [-2147483648, 2147483647]",
        ]);
        let n = node(&phase.infos[1]);
        assert_eq!(n.id, 11);
        assert_eq!(n.schedule_pos, None);
        assert_eq!(parsed.opcodes[n.opcode as usize], "Int32AddWithOverflow");
        assert_eq!(
            n.inputs.iter().map(|r| r.node).collect::<Vec<_>>(),
            vec![9, 10]
        );
        assert_eq!(n.uses, Some(1));
        assert!(!n.dead);
    }

    #[test]
    fn scheduled_node_line_with_decorated_inputs() {
        let (phase, parsed) = parse_lines(&[
            "    9/9: CheckedSmiUntag [v5/n2:[x0|R|t]] → [x0|R|w32], live range: [9-11]",
        ]);
        let n = node(&phase.infos[1]);
        assert_eq!(n.id, 9);
        assert_eq!(n.schedule_pos, Some(9));
        assert_eq!(parsed.opcodes[n.opcode as usize], "CheckedSmiUntag");
        assert_eq!(n.inputs.iter().map(|r| r.node).collect::<Vec<_>>(), vec![2]);
        assert_eq!(n.uses, None);
    }

    #[test]
    fn opcode_tag_is_attached_bracket_not_inputs() {
        let (phase, parsed) =
            parse_lines(&["  14: Int32ToNumber[kCanonicalizeSmi] [v0/n11:(x)] → (x)"]);
        let n = node(&phase.infos[1]);
        assert_eq!(
            parsed.opcodes[n.opcode as usize],
            "Int32ToNumber[kCanonicalizeSmi]"
        );
        assert_eq!(
            n.inputs.iter().map(|r| r.node).collect::<Vec<_>>(),
            vec![11]
        );
    }

    #[test]
    fn dead_marker_and_zero_uses() {
        let (phase, _) = parse_lines(&["   6: RootConstant(undefined_value), 0 uses 🪦"]);
        let n = node(&phase.infos[1]);
        assert_eq!(n.uses, Some(0));
        assert!(n.dead);
    }

    #[test]
    fn branch_targets_are_collected() {
        let (phase, _) = parse_lines(&["╭────16: BranchIfInt32Compare(LessThan) [n13, n14] b2 b8"]);
        let n = node(&phase.infos[1]);
        assert_eq!(
            n.targets.iter().map(|t| t.block).collect::<Vec<_>>(),
            vec![2, 8]
        );
        assert_eq!(
            n.inputs.iter().map(|r| r.node).collect::<Vec<_>>(),
            vec![13, 14]
        );
    }

    #[test]
    fn phi_line_with_paren_inputs() {
        let (phase, parsed) = parse_lines(&["││   30: φᵀ r0 (n28, n44), 3 uses"]);
        let n = node(&phase.infos[1]);
        assert_eq!(parsed.opcodes[n.opcode as usize], "φᵀ");
        assert_eq!(
            n.inputs.iter().map(|r| r.node).collect::<Vec<_>>(),
            vec![28, 44]
        );
        assert_eq!(n.uses, Some(3));
    }

    #[test]
    fn gap_moves_define_no_node() {
        let (phase, _) = parse_lines(&[
            "    1/12: HeapConstant(0x1 <FeedbackCell>) → v-1, live range: [1-12]",
            "     16: GapMove([stack:-7|t] → [x0|R|t])",
        ]);
        let dual = node(&phase.infos[1]);
        assert_eq!(dual.id, 12);
        let gap = node(&phase.infos[2]);
        assert_eq!(gap.id, SCHEDULE_ONLY);
        assert!(!phase.defs.contains_key(&SCHEDULE_ONLY));
        // But its input reference (none here — locations only) is fine.
        assert_eq!(phase.node_count, 2);
    }

    #[test]
    fn block_headers_behind_gutter() {
        let (phase, _) = parse_lines(&["│╭─►Block b4 peeled (effects: c1)", " Block b0"]);
        assert!(matches!(phase.infos[1], LineInfo::BlockHeader { block: 4 }));
        assert!(matches!(phase.infos[2], LineInfo::BlockHeader { block: 0 }));
        assert_eq!(phase.block_count, 2);
    }

    #[test]
    fn eager_frame_attaches_forward_lazy_backward() {
        let (phase, parsed) = parse_lines(&[
            "   7: FunctionEntryStackCheck",
            "      ↳ lazy @-1 (4 live vars)",
            "      ↱ eager @2 (5 live vars)",
            "   9: CheckedSmiUntag [n2], 1 uses, cannot truncate to int32",
        ]);
        let LineInfo::Frame {
            after_node: lazy_at,
            frame: lazy_frame,
            ..
        } = &phase.infos[2]
        else {
            panic!("lazy frame expected");
        };
        assert_eq!(*lazy_at, Some(7));
        assert_eq!(parsed.frames[*lazy_frame as usize].arrow, FrameArrow::Lazy);
        assert_eq!(
            parsed.frames[*lazy_frame as usize].bytecode_offset,
            Some(-1)
        );
        let LineInfo::Frame {
            after_node: eager_at,
            ..
        } = &phase.infos[3]
        else {
            panic!("eager frame expected");
        };
        assert_eq!(
            *eager_at,
            Some(9),
            "eager frames belong to the node after them"
        );
    }

    #[test]
    fn verbose_frames_intern_on_addr() {
        let (phase, parsed) = parse_lines(&[
            "   1: A",
            "      ↱ eager @0 : {<closure>:n3:, <this>:n1:, a0:n2:} (addr:0x124011b0fb8)",
            "   2: B",
            "      ↳ lazy @0 : {<closure>:n3:[stack:-2|t], <this>:n1:[stack:-6|t], a0:n2:[x1|R|t]} (addr:0x124011b0fb8)",
        ]);
        let LineInfo::Frame {
            frame: f1, refs, ..
        } = &phase.infos[2]
        else {
            panic!()
        };
        let LineInfo::Frame { frame: f2, .. } = &phase.infos[4] else {
            panic!()
        };
        assert_eq!(f1, f2, "same (addr:…) is the same frame");
        assert_eq!(parsed.frames.len(), 1);
        assert_eq!(parsed.frames[0].occurrences, 2);
        assert_eq!(
            refs.iter().map(|r| r.node).collect::<Vec<_>>(),
            vec![3, 1, 2],
            "verbose payload node refs are collected"
        );
    }

    #[test]
    fn throw_frames_both_forms() {
        let (phase, parsed) = parse_lines(&[
            "   5: CallRuntime",
            "      ↳ throw @26 (b2) : {<this>:n1, a0:n2, <context>:n4, r0:n10}",
            "      ↳ throw (b3)",
        ]);
        let LineInfo::Frame { frame: f1, .. } = &phase.infos[2] else {
            panic!()
        };
        let LineInfo::Frame { frame: f2, .. } = &phase.infos[3] else {
            panic!()
        };
        assert_eq!(parsed.frames[*f1 as usize].catch_block, Some(2));
        assert_eq!(parsed.frames[*f1 as usize].bytecode_offset, Some(26));
        assert_eq!(parsed.frames[*f2 as usize].catch_block, Some(3));
        assert_eq!(parsed.frames[*f2 as usize].bytecode_offset, None);
    }

    #[test]
    fn continuation_frame_lines_join_the_chain() {
        let (phase, _) = parse_lines(&[
            "        ↱ eager @46 (4 live vars)",
            "        │       @24 (6 live vars)",
            "   38: Int32AddWithOverflow [n34, n37], 2 uses",
        ]);
        assert!(matches!(phase.infos[1], LineInfo::Frame { .. }));
        assert!(matches!(phase.infos[2], LineInfo::Frame { .. }));
        // Both belong to node 38 (they print before it).
        for i in [1, 2] {
            let LineInfo::Frame { after_node, .. } = &phase.infos[i] else {
                panic!()
            };
            assert_eq!(*after_node, Some(38));
        }
    }

    #[test]
    fn interleaved_bytecode_and_sfi_context() {
        let (phase, parsed) = parse_lines(&[
            "   2 : 42 03 01          Add a0, EmbeddedFeedback[1]",
            "0x09b80101e2cd <SharedFunctionInfo add> (0x09b80104a3e5 <String[19]: \"workloads/simple.js\">:2:12)",
        ]);
        assert!(matches!(&phase.infos[1], LineInfo::Bytecode(bc) if bc.offset == 2 && !bc.primary));
        let LineInfo::SfiContext {
            line,
            col,
            same_script,
        } = &phase.infos[2]
        else {
            panic!("expected sfi context, got {:?}", phase.infos[2]);
        };
        assert_eq!((*line, *col), (2, 12), "0-based definition anchor");
        assert!(*same_script);
        assert_eq!(parsed.script.as_deref(), Some("workloads/simple.js"));
    }

    #[test]
    fn section_trailers_inside_a_phase_are_structure_not_annotations() {
        let (phase, _) = parse_lines(&[
            "  10: Foo [n2]",
            "---------------------------------------------------",
            "Finished compiling method bnpFromString using TurboFan",
        ]);
        assert!(matches!(phase.infos[2], LineInfo::Control), "closing rule");
        assert!(matches!(phase.infos[3], LineInfo::Control), "trailer");
        assert_eq!(phase.annotation_count, 0);
    }

    #[test]
    fn unmatched_lines_attach_to_the_preceding_node() {
        let (phase, _) = parse_lines(&[
            "  10: Foo [n2]",
            "     allocating v5 to x0 (regalloc trace)",
        ]);
        let LineInfo::Annotation { after_node } = &phase.infos[2] else {
            panic!("expected annotation, got {:?}", phase.infos[2]);
        };
        assert_eq!(*after_node, Some(10));
        assert_eq!(phase.annotation_count, 1);
    }

    #[test]
    fn def_use_maps_are_built() {
        let (phase, _) = parse_lines(&[
            "   9: CheckedSmiUntag [n2], 1 uses",
            "  11: Int32AddWithOverflow [n9, n10], 1 uses",
        ]);
        assert_eq!(phase.defs[&9], 1);
        assert_eq!(phase.defs[&11], 2);
        assert_eq!(phase.users[&9], vec![2]);
        assert_eq!(phase.users[&2], vec![1]);
    }

    #[test]
    fn spans_index_the_stripped_text() {
        let (phase, _) = parse_lines(&["  11: Int32AddWithOverflow [n9, n10], 1 uses"]);
        let n = node(&phase.infos[1]);
        let text = "  11: Int32AddWithOverflow [n9, n10], 1 uses";
        assert_eq!(
            &text[n.def_span.start as usize..n.def_span.end as usize],
            "11"
        );
        assert_eq!(
            &text[n.opcode_span.start as usize..n.opcode_span.end as usize],
            "Int32AddWithOverflow"
        );
        let spans: Vec<&str> = n
            .inputs
            .iter()
            .map(|r| &text[r.span.start as usize..r.span.end as usize])
            .collect();
        assert_eq!(spans, vec!["n9", "n10"]);
    }

    /// Turboshaft IR appears inside Turbolev sections after lowering, under
    /// banner names the marker table may not know; the body sniffer must
    /// route those phases to the Turboshaft grammar so `MERGE B…` headers
    /// fold and `#N` refs drive def-use and `i`/`u` (user report: both were
    /// dead because the Maglev grammar saw those lines).
    #[test]
    fn turboshaft_phase_inside_turbolev_routes_to_turboshaft_grammar() {
        let trace = "\
----- Bytecode before MaglevGraphBuilding -----
Function: 0x9 <JSFunction add (sfi = 0x77)>
----- Maglev graph building -----
   1: Foo
----- Turboshaft lowering -----
MERGE B2 <- B0, B1
   47: OverflowCheckedBinop(#42, #42)[signed add, Word32]
   51: Branch(#49)[B5, B4, False]
";
        let mut buffer = LogBuffer::new();
        buffer.append(trace.as_bytes());
        buffer.finish();
        let mut idx = crate::index::TraceIndex::new(None);
        idx.ingest(&buffer, true);
        let section = &idx.compilations[0];
        assert_eq!(section.phases.len(), 3);

        let parsed = parse_compilation(&buffer, section);
        // The Maglev phase still parses as Maglev.
        assert!(matches!(
            parsed.phases[1].infos[1],
            LineInfo::Node(ref n) if n.id == 1
        ));
        // The Turboshaft phase gets block headers, hash-ref inputs, and
        // block targets — folding and def-use material.
        let ts = &parsed.phases[2];
        assert!(matches!(ts.infos[1], LineInfo::BlockHeader { block: 2 }));
        let LineInfo::Node(ref binop) = ts.infos[2] else {
            panic!("expected node, got {:?}", ts.infos[2]);
        };
        assert_eq!(binop.id, 47);
        assert_eq!(
            binop.inputs.iter().map(|r| r.node).collect::<Vec<_>>(),
            vec![42, 42]
        );
        let LineInfo::Node(ref branch) = ts.infos[3] else {
            panic!()
        };
        assert_eq!(
            branch.targets.iter().map(|t| t.block).collect::<Vec<_>>(),
            vec![5, 4]
        );
        assert_eq!(ts.defs[&47], 2, "defs feed i/u jumps");
        assert_eq!(ts.users[&42], vec![2, 2], "one entry per reference");
        assert_eq!(ts.block_count, 1);
    }

    #[test]
    fn preamble_inline_decisions_with_and_without_ids() {
        let mut decisions = Vec::new();
        classify_preamble(
            "⚡ INLINE small                          Small function, skipping max-depth",
            5,
            &mut decisions,
        );
        classify_preamble(
            "[ML:38612] ❌ SKIP   big                            big function, size (174) >= max-size (100)",
            6,
            &mut decisions,
        );
        assert_eq!(
            decisions,
            vec![
                InlineDecision {
                    line: 5,
                    inlined: true,
                    callee: "small".into(),
                    reason: "Small function, skipping max-depth".into(),
                },
                InlineDecision {
                    line: 6,
                    inlined: false,
                    callee: "big".into(),
                    reason: "big function, size (174) >= max-size (100)".into(),
                },
            ]
        );
    }

    /// Review regression: `n<digits>` inside a heap-object descriptor is a
    /// JS identifier, not a node reference.
    #[test]
    fn object_descriptor_text_is_not_a_node_ref() {
        let (phase, _) = parse_lines(&[
            "  12: CallKnownJSFunction(0x1 <JSFunction n1 (sfi = 0x2)>) [n3, n4], 1 uses",
            "  13: LoadField(0x3 <String[4]: \"n9x\">) [n12], 1 uses",
        ]);
        let call = node(&phase.infos[1]);
        assert_eq!(
            call.inputs.iter().map(|r| r.node).collect::<Vec<_>>(),
            vec![3, 4],
            "the callee named n1 must not become an input"
        );
        let load = node(&phase.infos[2]);
        assert_eq!(
            load.inputs.iter().map(|r| r.node).collect::<Vec<_>>(),
            vec![12],
            "quoted string content must not become an input"
        );
    }

    #[test]
    fn garbage_is_annotations_not_panics() {
        let (phase, _) = parse_lines(&[
            "\u{fffd}\u{fffd}\u{fffd}",
            "1234567890123456789012345678901234567890: ",
            "-9/x: huh",
            "↱",
            "@@",
            ": no ids",
        ]);
        assert_eq!(phase.infos.len(), 7);
    }
}
