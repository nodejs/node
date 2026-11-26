// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! TurboFan / Turboshaft phase parsers (`--trace-turbo-graph`).
//!
//! One `Begin compiling method … using TurboFan` section carries **four**
//! distinct body grammars, all measured in the corpus (both versions print
//! identical shapes):
//!
//! - **Sea of nodes** (`----- Graph after V8.TF… -----`):
//!   `#17:SpeculativeSmallIntegerAdd[SignedSmall](#2:Parameter, …)` — id and
//!   opcode glued with `:`, bracketed parameters attached, inputs as
//!   `#N:Opcode` references. Flat: no blocks.
//! - **Schedule** (`----- schedule -----`): `--- BLOCK B1 id1 <- B0 ---`
//!   headers, then `  17: CheckedInt32Add(33, 34, 34, 9) : Type` rows with
//!   *bare-number* inputs and optional `-> B1` jump targets.
//! - **Turboshaft** (`----- V8.TFTurboshaft… -----`): `BLOCK B4 <- B3` /
//!   `MERGE B9 <- B7, B8` / `LOOP …` headers, `   47: Op(#42)[params]` rows,
//!   memory forms (`Load *(#57 + #11) […]`, `Store *(#85 + #86) = #84 […]`)
//!   and block targets inside the parameter brackets
//!   (`Branch(#65)[B8, B7, True]`).
//! - **Instruction sequences** (`----- Instruction sequence … -----`):
//!   `B0: AO#0  instructions: [0, 7)` headers, `IMM#`/`CST#` pools,
//!   numbered `gap` rows and `v28(=x0) = ArchCallCodeObject …` rows whose
//!   `vN` virtual registers get def/use tracking like nodes.
//!
//! Everything unmatched degrades to a positioned annotation, exactly like
//! the Maglev parser — total, never panicking, nothing dropped.

use std::ops::Range;

use super::maglev::{Interner, line_text};
use crate::model::{BlockRef, IRNode, LineInfo, NodeId, NodeRef, ParsedPhase, SCHEDULE_ONLY};
use crate::source::LogBuffer;

#[derive(Clone, Copy, PartialEq)]
enum Grammar {
    Sea,
    Schedule,
    Turboshaft,
    Instructions,
}

/// Parses one TurboFan-tier phase, dispatching on the banner name.
pub(crate) fn parse_phase(
    buffer: &LogBuffer,
    lines: Range<usize>,
    phase_name: &str,
    interner: &mut Interner,
) -> ParsedPhase {
    let grammar = if phase_name.starts_with("Graph after ") {
        Grammar::Sea
    } else if phase_name == "schedule" {
        Grammar::Schedule
    } else if phase_name.starts_with("Instruction sequence") {
        Grammar::Instructions
    } else {
        // The V8.TFTurboshaft* family; also the honest default for a new
        // TF banner shape — its rows will mostly degrade to annotations.
        Grammar::Turboshaft
    };

    let mut phase = ParsedPhase::default();
    let mut last_node: Option<NodeId> = None;

    for (offset, line) in lines.enumerate() {
        let text = line_text(buffer, line);
        let info = if offset == 0 {
            LineInfo::Banner
        } else {
            match grammar {
                Grammar::Sea => classify_sea(&text, interner, &mut last_node),
                Grammar::Schedule => classify_schedule(&text, interner, &mut last_node),
                Grammar::Turboshaft => classify_turboshaft(&text, interner, &mut last_node),
                Grammar::Instructions => classify_instructions(&text, interner, &mut last_node),
            }
        };
        record(&mut phase, info);
    }
    phase
}

/// The same def/use bookkeeping the Maglev parser does per line.
fn record(phase: &mut ParsedPhase, info: LineInfo) {
    let at = phase.infos.len() as u32;
    match &info {
        LineInfo::Node(node) => {
            phase.node_count += 1;
            if node.id != SCHEDULE_ONLY {
                phase.defs.insert(node.id, at);
            }
            for input in &node.inputs {
                phase.users.entry(input.node).or_default().push(at);
            }
        }
        LineInfo::BlockHeader { .. } => phase.block_count += 1,
        LineInfo::Annotation { .. } => phase.annotation_count += 1,
        _ => {}
    }
    phase.infos.push(info);
}

// ---------------------------------------------------------------------------
// Token scanners
// ---------------------------------------------------------------------------

/// Every `#N` node reference from `from`, with absolute spans (the `#` is
/// part of the span — it is what the eye finds).
fn hash_refs(text: &str, from: usize) -> Vec<NodeRef> {
    let bytes = text.as_bytes();
    let mut refs = Vec::new();
    let mut i = from;
    while i < bytes.len() {
        if bytes[i] == b'#'
            && i + 1 < bytes.len()
            && bytes[i + 1].is_ascii_digit()
            && (i == 0 || !bytes[i - 1].is_ascii_alphanumeric())
        {
            let mut end = i + 1;
            while end < bytes.len() && bytes[end].is_ascii_digit() {
                end += 1;
            }
            if let Ok(node) = text[i + 1..end].parse::<NodeId>() {
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

/// Every `vN` virtual-register reference from `from` (instruction sequences).
fn vreg_refs(text: &str, from: usize) -> Vec<NodeRef> {
    let bytes = text.as_bytes();
    let mut refs = Vec::new();
    let mut i = from;
    while i < bytes.len() {
        if bytes[i] == b'v'
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

/// Every `BN` block reference from `from` (Turboshaft targets, schedule
/// `-> B1`, successor lists).
fn upper_block_refs(text: &str, from: usize) -> Vec<BlockRef> {
    let bytes = text.as_bytes();
    let mut out = Vec::new();
    let mut i = from;
    while i < bytes.len() {
        if bytes[i] == b'B'
            && i + 1 < bytes.len()
            && bytes[i + 1].is_ascii_digit()
            && (i == 0 || !bytes[i - 1].is_ascii_alphanumeric())
        {
            let mut end = i + 1;
            while end < bytes.len() && bytes[end].is_ascii_digit() {
                end += 1;
            }
            if (end >= bytes.len() || !bytes[end].is_ascii_alphanumeric())
                && let Ok(block) = text[i + 1..end].parse()
            {
                out.push(BlockRef {
                    block,
                    span: i as u32..end as u32,
                });
            }
            i = end;
        } else {
            i += 1;
        }
    }
    out
}

/// The `Begin`/`Finished compiling` trailer lines land inside the last
/// phase's range when a section closes; they are separators, not IR.
fn is_trailer(t: &str) -> bool {
    t.starts_with("Finished compiling ")
        || t.starts_with("Begin compiling ")
        || (!t.is_empty() && t.bytes().all(|b| b == b'-'))
}

fn leading_digits(s: &str) -> usize {
    s.bytes().take_while(|b| b.is_ascii_digit()).count()
}

/// An opcode-name run: alphanumeric plus the few chars TF opcode names use.
fn opcode_len(s: &str) -> usize {
    s.bytes()
        .take_while(|b| b.is_ascii_alphanumeric() || *b == b'_' || *b == b'.')
        .count()
}

// ---------------------------------------------------------------------------
// Sea of nodes: `#17:SpeculativeSmallIntegerAdd[SignedSmall](#2:Parameter, …)`
// ---------------------------------------------------------------------------

fn classify_sea(text: &str, interner: &mut Interner, last_node: &mut Option<NodeId>) -> LineInfo {
    let t = text.trim_end();
    if t.is_empty() || is_trailer(t) {
        return LineInfo::Control;
    }
    if let Some(node) = parse_sea_node(t, interner) {
        *last_node = Some(node.id);
        return LineInfo::Node(node);
    }
    LineInfo::Annotation {
        after_node: *last_node,
    }
}

fn parse_sea_node(t: &str, interner: &mut Interner) -> Option<IRNode> {
    let rest = t.strip_prefix('#')?;
    let id_len = leading_digits(rest);
    if id_len == 0 {
        return None;
    }
    let id: NodeId = rest[..id_len].parse().ok()?;
    let after_id = &rest[id_len..];
    let after_colon = after_id.strip_prefix(':')?;
    let name_len = opcode_len(after_colon);
    if name_len == 0 {
        return None;
    }
    let name = &after_colon[..name_len];

    let opcode_start = 1 + id_len + 1;
    let opcode_end = opcode_start + name_len;
    Some(IRNode {
        id,
        schedule_pos: None,
        def_span: 0..(1 + id_len) as u32,
        opcode: interner.opcode(name),
        opcode_span: opcode_start as u32..opcode_end as u32,
        // Parameters in `[…]` never contain `#N` tokens; the input list in
        // `(…)` does — scanning the whole tail catches exactly the inputs.
        inputs: hash_refs(t, opcode_end),
        uses: None,
        dead: false,
        targets: Vec::new(),
    })
}

// ---------------------------------------------------------------------------
// Schedule: `--- BLOCK B1 id1 <- B0 ---` + `  17: Op(33, 34) : Type -> B1`
// ---------------------------------------------------------------------------

fn classify_schedule(
    text: &str,
    interner: &mut Interner,
    last_node: &mut Option<NodeId>,
) -> LineInfo {
    let t = text.trim_end();
    if t.is_empty() || is_trailer(t) {
        return LineInfo::Control;
    }
    if let Some(rest) = t.strip_prefix("--- BLOCK B") {
        let digits = leading_digits(rest);
        if digits > 0
            && let Ok(block) = rest[..digits].parse()
        {
            return LineInfo::BlockHeader { block };
        }
    }
    if let Some(node) = parse_schedule_node(t, interner) {
        *last_node = Some(node.id);
        return LineInfo::Node(node);
    }
    LineInfo::Annotation {
        after_node: *last_node,
    }
}

fn parse_schedule_node(t: &str, interner: &mut Interner) -> Option<IRNode> {
    let start = t.len() - t.trim_start().len();
    let content = &t[start..];
    let id_len = leading_digits(content);
    if id_len == 0 {
        return None;
    }
    let after_id = &content[id_len..];
    let after_colon = after_id.strip_prefix(": ")?;
    let id: NodeId = content[..id_len].parse().ok()?;
    let name_len = opcode_len(after_colon);
    if name_len == 0 {
        return None;
    }
    let name = &after_colon[..name_len];
    let opcode_start = start + id_len + 2;
    let opcode_end = opcode_start + name_len;

    // Skip an attached `[…]` parameter block, then read the bare-number
    // input list out of the following `(…)`.
    let tail = &t[opcode_end..];
    let mut at = 0;
    if tail[at..].starts_with('[') {
        let mut depth = 0usize;
        for (j, c) in tail[at..].char_indices() {
            match c {
                '[' => depth += 1,
                ']' => {
                    depth -= 1;
                    if depth == 0 {
                        at += j + 1;
                        break;
                    }
                }
                _ => {}
            }
        }
    }
    let mut inputs = Vec::new();
    if tail[at..].starts_with('(') {
        let inner_start = opcode_end + at + 1;
        if let Some(close) = t[inner_start..].find(')') {
            let inner = &t[inner_start..inner_start + close];
            let bytes = inner.as_bytes();
            let mut i = 0;
            while i < bytes.len() {
                if bytes[i].is_ascii_digit() && (i == 0 || !bytes[i - 1].is_ascii_alphanumeric()) {
                    let mut end = i;
                    while end < bytes.len() && bytes[end].is_ascii_digit() {
                        end += 1;
                    }
                    if let Ok(node) = inner[i..end].parse::<NodeId>() {
                        inputs.push(NodeRef {
                            node,
                            span: (inner_start + i) as u32..(inner_start + end) as u32,
                        });
                    }
                    i = end;
                } else {
                    i += 1;
                }
            }
        }
    }

    // `-> B1` jump target after the body.
    let targets = match t.find(" -> ") {
        Some(arrow) => upper_block_refs(t, arrow),
        None => Vec::new(),
    };

    Some(IRNode {
        id,
        schedule_pos: None,
        def_span: start as u32..(start + id_len) as u32,
        opcode: interner.opcode(name),
        opcode_span: opcode_start as u32..opcode_end as u32,
        inputs,
        uses: None,
        dead: false,
        targets,
    })
}

// ---------------------------------------------------------------------------
// Turboshaft: `BLOCK B4 <- B3` + `   47: Op(#42)[B8, B7, True]`
// ---------------------------------------------------------------------------

fn classify_turboshaft(
    text: &str,
    interner: &mut Interner,
    last_node: &mut Option<NodeId>,
) -> LineInfo {
    let t = text.trim_end();
    if t.is_empty() || is_trailer(t) {
        return LineInfo::Control;
    }
    for header in ["BLOCK B", "MERGE B", "LOOP B"] {
        if let Some(rest) = t.strip_prefix(header) {
            let digits = leading_digits(rest);
            if digits > 0
                && let Ok(block) = rest[..digits].parse()
            {
                return LineInfo::BlockHeader { block };
            }
        }
    }
    if let Some(node) = parse_turboshaft_op(t, interner) {
        *last_node = Some(node.id);
        return LineInfo::Node(node);
    }
    LineInfo::Annotation {
        after_node: *last_node,
    }
}

fn parse_turboshaft_op(t: &str, interner: &mut Interner) -> Option<IRNode> {
    let start = t.len() - t.trim_start().len();
    let content = &t[start..];
    let id_len = leading_digits(content);
    if id_len == 0 {
        return None;
    }
    let after_colon = content[id_len..].strip_prefix(": ")?;
    let id: NodeId = content[..id_len].parse().ok()?;
    let name_len = opcode_len(after_colon);
    if name_len == 0 {
        return None;
    }
    let name = &after_colon[..name_len];
    let opcode_start = start + id_len + 2;
    let opcode_end = opcode_start + name_len;

    Some(IRNode {
        id,
        schedule_pos: None,
        def_span: start as u32..(start + id_len) as u32,
        opcode: interner.opcode(name),
        opcode_span: opcode_start as u32..opcode_end as u32,
        // Memory forms put refs outside the parens (`Store *(#85 + #86) =
        // #84`), so scan the whole tail.
        inputs: hash_refs(t, opcode_end),
        uses: None,
        dead: false,
        // `Goto()[B6, 0]`, `Branch(#65)[B8, B7, False]`: targets live in the
        // bracket block; `BN` never appears in parameter text otherwise.
        targets: upper_block_refs(t, opcode_end),
    })
}

// ---------------------------------------------------------------------------
// Instruction sequences: `B0: AO#0 …` + `v28(=x0) = ArchCallCodeObject …`
// ---------------------------------------------------------------------------

fn classify_instructions(
    text: &str,
    interner: &mut Interner,
    last_node: &mut Option<NodeId>,
) -> LineInfo {
    let t = text.trim_end();
    if t.is_empty() || is_trailer(t) {
        return LineInfo::Control;
    }
    // `B0: AO#0  instructions: [0, 7)` — a block header at column 0.
    if let Some(rest) = t.strip_prefix('B') {
        let digits = leading_digits(rest);
        if digits > 0
            && rest[digits..].starts_with(':')
            && let Ok(block) = rest[..digits].parse()
        {
            return LineInfo::BlockHeader { block };
        }
    }
    let trimmed = t.trim_start();
    if t.starts_with("IMM#")
        || t.starts_with("CST#")
        || trimmed.starts_with("predecessors")
        || trimmed.starts_with("successors")
    {
        return LineInfo::Control;
    }

    let start = t.len() - trimmed.len();

    // `      7: gap () ()` — an instruction slot; the moves inside the gaps
    // reference vregs after register allocation.
    let id_len = leading_digits(trimmed);
    if id_len > 0 && trimmed[id_len..].starts_with(": ") {
        let opcode_start = start + id_len + 2;
        let name_len = opcode_len(&trimmed[id_len + 2..]);
        let name = &trimmed[id_len + 2..id_len + 2 + name_len];
        return LineInfo::Node(IRNode {
            id: SCHEDULE_ONLY,
            schedule_pos: trimmed[..id_len].parse().ok(),
            def_span: start as u32..(start + id_len) as u32,
            opcode: interner.opcode(if name.is_empty() { "gap" } else { name }),
            opcode_span: opcode_start as u32..(opcode_start + name_len.max(1)) as u32,
            inputs: vreg_refs(t, opcode_start),
            uses: None,
            dead: false,
            targets: Vec::new(),
        });
    }

    let refs = vreg_refs(t, start);

    // `phi: v6 = v8 v9` / `phi: [r9|R|w64] = v28 v29` — parallel phi rows;
    // the destination is a def when it is a vreg.
    if let Some(rest) = trimmed.strip_prefix("phi:") {
        let (id, def_span, inputs) = match refs.split_first() {
            Some((def, uses)) if rest.trim_start().starts_with('v') => {
                (def.node, def.span.clone(), uses.to_vec())
            }
            _ => (SCHEDULE_ONLY, start as u32..start as u32, refs),
        };
        if id != SCHEDULE_ONLY {
            *last_node = Some(id);
        }
        return LineInfo::Node(IRNode {
            id,
            schedule_pos: None,
            def_span,
            opcode: interner.opcode("phi"),
            opcode_span: start as u32..(start + 3) as u32,
            inputs,
            uses: None,
            dead: false,
            targets: Vec::new(),
        });
    }

    // Def rows: `<dst> = Opcode …`. The destination is a vreg (`v28(=x0)`),
    // a physical location (`[x3|R|w32]`), or a constant slot
    // (`[constant:v32]`); the opcode is always right of the ` = ` (found in
    // review: bracket destinations lost their opcode on ~14 % of post-RA
    // rows and interned a bogus "insn").
    if (trimmed.starts_with('v') || trimmed.starts_with('['))
        && let Some(eq) = t.find(" = ")
    {
        let opcode_start = eq + 3;
        let name_len = opcode_len(&t[opcode_start..]);
        if name_len > 0 {
            let name = &t[opcode_start..opcode_start + name_len];
            // The destination defs only when it is itself a vreg.
            let (id, def_span) = match refs.first() {
                Some(r) if trimmed.starts_with('v') && (r.span.start as usize) == start => {
                    (r.node, r.span.clone())
                }
                _ => (SCHEDULE_ONLY, start as u32..start as u32),
            };
            let inputs: Vec<NodeRef> = refs
                .iter()
                .filter(|r| id == SCHEDULE_ONLY || r.span != def_span)
                .cloned()
                .collect();
            if id != SCHEDULE_ONLY {
                *last_node = Some(id);
            }
            return LineInfo::Node(IRNode {
                id,
                schedule_pos: None,
                def_span,
                opcode: interner.opcode(name),
                opcode_span: opcode_start as u32..(opcode_start + name_len) as u32,
                inputs,
                uses: None,
                dead: false,
                targets: Vec::new(),
            });
        }
    }

    // Destination-less instructions: `Arm64Poke #0 #1`, `Arm64Claim #2`,
    // `ArchJmp [rpo_immediate:3]`, `Arm64Tst32 && deoptimize if not equal …`.
    // Corroborating structure is required — an architecture-prefixed opcode,
    // a `&& branch/deoptimize` combo, or vreg operands behind an
    // opcode-shaped token — because "any uppercase word" swallowed stray
    // program output as fake IR (found in review).
    let name_len = opcode_len(trimmed);
    let opcode_like = name_len >= 2
        && trimmed
            .chars()
            .next()
            .is_some_and(|c| c.is_ascii_uppercase());
    let arch_like = [
        "Arch", "Arm64", "X64", "IA32", "Riscv", "Mips", "Loong", "Ppc", "S390",
    ]
    .iter()
    .any(|p| trimmed.starts_with(p));
    if arch_like || t.contains(" && ") || (!refs.is_empty() && opcode_like) {
        return LineInfo::Node(IRNode {
            id: SCHEDULE_ONLY,
            schedule_pos: None,
            def_span: start as u32..start as u32,
            opcode: interner.opcode(if name_len > 0 {
                &trimmed[..name_len]
            } else {
                "insn"
            }),
            opcode_span: start as u32..(start + name_len) as u32,
            inputs: refs,
            uses: None,
            dead: false,
            targets: Vec::new(),
        });
    }
    LineInfo::Annotation {
        after_node: *last_node,
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::model::ParsedCompilation;

    fn parse(name: &str, body: &[&str]) -> (ParsedPhase, ParsedCompilation) {
        let mut text = format!("----- {name} -----\n");
        for l in body {
            text.push_str(l);
            text.push('\n');
        }
        let mut buffer = LogBuffer::new();
        buffer.append(text.as_bytes());
        buffer.finish();
        let mut interner = Interner::default();
        let phase = parse_phase(&buffer, 0..buffer.line_count(), name, &mut interner);
        let parsed = ParsedCompilation {
            opcodes: interner.opcodes,
            ..Default::default()
        };
        (phase, parsed)
    }

    fn node(info: &LineInfo) -> &IRNode {
        match info {
            LineInfo::Node(n) => n,
            other => panic!("expected node, got {other:?}"),
        }
    }

    #[test]
    fn sea_of_nodes_line() {
        let (phase, parsed) = parse(
            "Graph after V8.TFTyper",
            &[
                "#17:SpeculativeSmallIntegerAdd[SignedSmall](#2:Parameter, #3:Parameter, #15:Checkpoint, #9:JSStackCheck)",
                "#18:NumberConstant[0]()",
            ],
        );
        let n = node(&phase.infos[1]);
        assert_eq!(n.id, 17);
        assert_eq!(
            parsed.opcodes[n.opcode as usize],
            "SpeculativeSmallIntegerAdd"
        );
        assert_eq!(
            n.inputs.iter().map(|r| r.node).collect::<Vec<_>>(),
            vec![2, 3, 15, 9]
        );
        let c = node(&phase.infos[2]);
        assert_eq!(c.id, 18);
        assert!(c.inputs.is_empty(), "the [0] parameter is not an input");
        assert_eq!(phase.defs[&17], 1);
        assert_eq!(phase.users[&2], vec![1]);
    }

    #[test]
    fn schedule_block_and_node() {
        let (phase, parsed) = parse(
            "schedule",
            &[
                "--- BLOCK B0 id0 ---",
                "  5: Parameter[6, debug name: %context](0) : OtherInternal",
                "  17: CheckedInt32Add(33, 34, 34, 9) : (MinusZero | Range(-1, 1))",
                "  19: Return(35, 36, 17, 9) -> B1",
                "--- BLOCK B1 id1 <- B0 ---",
            ],
        );
        assert!(matches!(phase.infos[1], LineInfo::BlockHeader { block: 0 }));
        assert!(matches!(phase.infos[5], LineInfo::BlockHeader { block: 1 }));
        let p = node(&phase.infos[2]);
        assert_eq!(parsed.opcodes[p.opcode as usize], "Parameter");
        assert_eq!(p.inputs.iter().map(|r| r.node).collect::<Vec<_>>(), vec![0]);
        let add = node(&phase.infos[3]);
        assert_eq!(
            add.inputs.iter().map(|r| r.node).collect::<Vec<_>>(),
            vec![33, 34, 34, 9],
            "type annotation numbers must not become inputs"
        );
        let ret = node(&phase.infos[4]);
        assert_eq!(
            ret.targets.iter().map(|t| t.block).collect::<Vec<_>>(),
            vec![1]
        );
    }

    #[test]
    fn turboshaft_blocks_ops_and_targets() {
        let (phase, parsed) = parse(
            "V8.TFTurboshaftMachineLowering",
            &[
                "BLOCK B3 <- B2",
                "   47: OverflowCheckedBinop(#42, #42)[signed add, Word32]",
                "   51: Branch(#49)[B5, B4, False]",
                "MERGE B9 <- B7, B8",
                "   79: Store *(#85 + #86) = #84 [raw, TaggedPointer, NoWriteBarrier]",
                "   61: Goto()[B6, 0]",
            ],
        );
        assert!(matches!(phase.infos[1], LineInfo::BlockHeader { block: 3 }));
        assert!(matches!(phase.infos[4], LineInfo::BlockHeader { block: 9 }));
        let binop = node(&phase.infos[2]);
        assert_eq!(
            parsed.opcodes[binop.opcode as usize],
            "OverflowCheckedBinop"
        );
        assert_eq!(
            binop.inputs.iter().map(|r| r.node).collect::<Vec<_>>(),
            vec![42, 42]
        );
        let branch = node(&phase.infos[3]);
        assert_eq!(
            branch.targets.iter().map(|t| t.block).collect::<Vec<_>>(),
            vec![5, 4]
        );
        let store = node(&phase.infos[5]);
        assert_eq!(
            store.inputs.iter().map(|r| r.node).collect::<Vec<_>>(),
            vec![85, 86, 84],
            "memory-form refs outside the parens are collected"
        );
        let goto = node(&phase.infos[6]);
        assert_eq!(
            goto.targets.iter().map(|t| t.block).collect::<Vec<_>>(),
            vec![6]
        );
    }

    #[test]
    fn instruction_sequence_defs_and_uses() {
        let (phase, parsed) = parse(
            "Instruction sequence after instruction selection",
            &[
                "IMM#0: 0x09b8001a9659 <Code BUILTIN AllocateInYoungGeneration>",
                "B0: AO#0  instructions: [0, 7)",
                " predecessors:",
                "       5: gap () ()",
                "          v35(R) = Arm64Ldr : Root #-224",
                "          v28(=x0) = ArchCallCodeObject [immediate:4] v23(S) v30(=x1)",
                "          ArchJmp [rpo_immediate:3]",
                " successors: B2 B1",
            ],
        );
        assert!(matches!(phase.infos[1], LineInfo::Control));
        assert!(matches!(phase.infos[2], LineInfo::BlockHeader { block: 0 }));
        assert!(matches!(phase.infos[3], LineInfo::Control));
        let gap = node(&phase.infos[4]);
        assert_eq!(gap.id, SCHEDULE_ONLY);
        let ldr = node(&phase.infos[5]);
        assert_eq!(ldr.id, 35);
        assert_eq!(parsed.opcodes[ldr.opcode as usize], "Arm64Ldr");
        let call = node(&phase.infos[6]);
        assert_eq!(call.id, 28);
        assert_eq!(
            call.inputs.iter().map(|r| r.node).collect::<Vec<_>>(),
            vec![23, 30]
        );
        assert_eq!(phase.defs[&28], 6);
        assert_eq!(phase.users[&23], vec![6]);
        let jmp = node(&phase.infos[7]);
        assert_eq!(jmp.id, SCHEDULE_ONLY);
        assert_eq!(parsed.opcodes[jmp.opcode as usize], "ArchJmp");
    }

    /// Review C1/M3: bracket destinations carry the opcode right of ` = `;
    /// phi rows parse; arbitrary prose does *not* become IR.
    #[test]
    fn instruction_edge_shapes() {
        let (phase, parsed) = parse(
            "Instruction sequence after register allocation",
            &[
                "          [x3|R|w32] = Arm64Asr32 [x2|R|t] #1",
                "          [constant:v32] = ArchNop",
                "          phi: v6 = v8 v9",
                "          phi: [r9|R|w64] = v28 v29",
                "Process memory usage:",
                "Uncaught TypeError: x is not a function",
            ],
        );
        let asr = node(&phase.infos[1]);
        assert_eq!(asr.id, SCHEDULE_ONLY);
        assert_eq!(
            parsed.opcodes[asr.opcode as usize], "Arm64Asr32",
            "bracket destinations must not lose the opcode"
        );
        let nop = node(&phase.infos[2]);
        assert_eq!(parsed.opcodes[nop.opcode as usize], "ArchNop");
        assert_eq!(
            nop.inputs.iter().map(|r| r.node).collect::<Vec<_>>(),
            vec![32]
        );
        let phi = node(&phase.infos[3]);
        assert_eq!(phi.id, 6, "vreg destination is a def");
        assert_eq!(parsed.opcodes[phi.opcode as usize], "phi");
        assert_eq!(
            phi.inputs.iter().map(|r| r.node).collect::<Vec<_>>(),
            vec![8, 9]
        );
        assert_eq!(phase.defs[&6], 3);
        let loc_phi = node(&phase.infos[4]);
        assert_eq!(
            loc_phi.id, SCHEDULE_ONLY,
            "location destination defs nothing"
        );
        assert_eq!(
            loc_phi.inputs.iter().map(|r| r.node).collect::<Vec<_>>(),
            vec![28, 29]
        );
        assert!(
            matches!(phase.infos[5], LineInfo::Annotation { .. }),
            "prose is not IR: {:?}",
            phase.infos[5]
        );
        assert!(matches!(phase.infos[6], LineInfo::Annotation { .. }));
    }

    #[test]
    fn turboshaft_loop_header_and_schedule_parenless_rows() {
        let (phase, _) = parse(
            "V8.TFTurboshaftLoopUnrolling",
            &["LOOP B2 <- B1, B4", "   12: JSStackCheck(#9, #7)[loop]"],
        );
        assert!(matches!(phase.infos[1], LineInfo::BlockHeader { block: 2 }));

        let (phase, parsed) = parse(
            "schedule",
            &["  0: Start : Internal", "  33: Int32Constant[0]"],
        );
        let start = node(&phase.infos[1]);
        assert_eq!(parsed.opcodes[start.opcode as usize], "Start");
        assert!(start.inputs.is_empty());
        let c = node(&phase.infos[2]);
        assert_eq!(c.id, 33);
        assert!(c.inputs.is_empty());
    }

    #[test]
    fn garbage_degrades_to_annotations() {
        let (phase, _) = parse(
            "Graph after V8.TFTyper",
            &[
                "#:broken",
                "#12",
                "not a node at all",
                "#999999999999999999:Op()",
            ],
        );
        for info in &phase.infos[1..] {
            assert!(matches!(info, LineInfo::Annotation { .. }), "got {info:?}");
        }
    }
}
