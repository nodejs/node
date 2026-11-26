// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! The diff engine: canonicalizer + node-identity diff.
//!
//! PLAN §7.4's core correction stands: a raw Myers line diff of IR marks
//! nearly everything as changed, because registers, schedule ids and value
//! numbers churn between phases. The pipeline here:
//!
//! 1. **Project** each side's display rows into diff keys. Node rows are
//!    keyed by their *identity* — the `nN` id when both sides belong to the
//!    same compilation (Maglev preserves it across phases, the entire point
//!    of `IRNode::id`), else a structural hash over the opcode and
//!    canonically renumbered inputs.
//! 2. **Align** the key sequences with `similar` (Myers). Equal keys pair
//!    rows up; everything else becomes an insert or a delete. Non-node rows
//!    are keyed by canonicalized text, which *is* the line-diff fallback
//!    for unmatched regions — one aligner, two granularities.
//! 3. **Judge** each matched node pair: opcode change, input change (raw or
//!    merely *rerouted* through an `Identity` node), replacement by an
//!    `Identity` — the states a V8 engineer actually asks about when staring
//!    at two phases.
//!
//! The output is a row-aligned two-column model: `rows[i]` names a row on
//! the left, on the right, or both — which is exactly the shape a
//! side-by-side split renders with synced scrolling for free.

use std::collections::HashMap;
use std::sync::Arc;

use crate::myers::{Algorithm, DiffOp, capture_diff_slices};

use crate::model::{CompilationSection, LineInfo, NodeId, ParsedCompilation, SCHEDULE_ONLY};
use crate::parse::maglev::line_text;
use crate::source::LogBuffer;
use crate::view::{ViewRow, model_rows};

/// How far an `Identity` chain is followed before assuming a cycle. Real
/// chains are one or two hops.
const MAX_IDENTITY_HOPS: usize = 64;

// ---------------------------------------------------------------------------
// Canonicalizer
// ---------------------------------------------------------------------------

/// Strips run-volatile content from a line so two dumps of the same thing
/// compare equal: hex addresses, `(addr:0x…)` host pointers, `[ML:n]`
/// compilation-id prefixes, and timing decimals. Node ids (`nN`) survive —
/// within one compilation they are identity, and across compilations node
/// rows never reach text comparison (they are keyed structurally).
pub fn canonicalize(text: &str) -> String {
    // Leading indentation and the control-flow gutter are layout, not
    // content: the schedule-id column appearing from Dead-nodes-sweeping on
    // re-indents every line, and without this trim each one became a false
    // delete+insert pair (found in review).
    let text = text.trim_start_matches([
        ' ', '│', '╭', '╰', '╮', '╯', '─', '►', '◄', '↓', '▼', '▲', '═',
    ]);
    let mut out = String::with_capacity(text.len());
    let bytes = text.as_bytes();
    let mut i = 0;

    // `[ML:1234] ` prefix, stripped entirely (spike-findings.md §8: it must
    // never be load-bearing).
    if let Some(rest) = text.strip_prefix("[ML:")
        && let Some(close) = rest.find("] ")
        && rest[..close].bytes().all(|b| b.is_ascii_digit())
    {
        i = 4 + close + 2;
    }

    while i < bytes.len() {
        // 0x… hex runs → 0x·
        if bytes[i] == b'0' && i + 1 < bytes.len() && bytes[i + 1] == b'x' {
            let mut end = i + 2;
            while end < bytes.len() && bytes[end].is_ascii_hexdigit() {
                end += 1;
            }
            if end > i + 2 {
                out.push_str("0x·");
                i = end;
                continue;
            }
        }
        // Fractional decimals (timings) → ·
        if bytes[i].is_ascii_digit() {
            let mut end = i;
            while end < bytes.len() && bytes[end].is_ascii_digit() {
                end += 1;
            }
            if end < bytes.len()
                && bytes[end] == b'.'
                && end + 1 < bytes.len()
                && bytes[end + 1].is_ascii_digit()
            {
                let mut frac = end + 1;
                while frac < bytes.len() && bytes[frac].is_ascii_digit() {
                    frac += 1;
                }
                out.push('·');
                i = frac;
                continue;
            }
            out.push_str(&text[i..end]);
            i = end;
            continue;
        }
        let ch = text[i..].chars().next().unwrap_or('\u{fffd}');
        out.push(ch);
        i += ch.len_utf8();
    }
    out
}

// ---------------------------------------------------------------------------
// Sides and keys
// ---------------------------------------------------------------------------

/// What one matched or unmatched diff row means. Ordered roughly
/// by how loudly the UI should announce it.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum RowStatus {
    Same,
    /// Present only on the right side.
    Added,
    /// Present only on the left side.
    Deleted,
    /// Same node id on both sides, different body.
    Changed {
        opcode: bool,
        inputs: bool,
        /// The operand payload changed: attached parameters, heap-object
        /// operands (maps, constants), branch targets, the truncation
        /// verdict, or the dead marker — everything a node says beyond its
        /// opcode and input list (found in review: `Int32Constant(0)` →
        /// `Int32Constant(7)` and retargeted branches used to read `Same`).
        operands: bool,
        /// The input lists differ only because values were redirected
        /// through `Identity` nodes — the data flow is unchanged.
        rerouted: bool,
    },
    /// The left side's real node became `nA: Identity [nB]` on the right:
    /// everything it fed now effectively consumes `by`.
    Replaced {
        by: NodeId,
    },
    /// This node id exists on both sides but moved (its aligned position
    /// changed), so the aligner shows it as leave+arrive rather than a
    /// pair. `changed` says its body also changed while moving (found in
    /// review: the change used to vanish under the move).
    Moved {
        changed: bool,
    },
}

/// The diffable projection of one phase view.
pub struct DiffSide {
    /// Display rows in order (always built unfolded, annotations shown: the
    /// diff must not depend on the viewer's fold state).
    pub rows: Vec<ViewRow>,
    pub parsed: Arc<ParsedCompilation>,
    /// Which source this side's rows index into — the two sides of a
    /// dual-run diff read from different buffers.
    pub source: usize,
    keys: Vec<Key>,
    /// The node defined on each row, parallel to `rows` — how a
    /// structurally-matched pair (different ids) finds its two shapes.
    row_nodes: Vec<Option<NodeId>>,
    /// id → shape for every real node row.
    nodes: HashMap<NodeId, NodeShape>,
    /// id → replacement target, from `nA: Identity [nB]` rows.
    identity: HashMap<NodeId, NodeId>,
}

#[derive(Debug, Clone, PartialEq, Eq, Hash, PartialOrd, Ord)]
enum Key {
    /// The phase banner. One per side, always aligned — the two sides'
    /// banners *never* match textually (they name different phases), and
    /// showing that as a delete/insert on every diff would be pure noise.
    Banner,
    Header(u32),
    Node(NodeId),
    Struct(u64),
    Text(String),
}

#[derive(Debug, Clone)]
struct NodeShape {
    opcode: String,
    inputs: Vec<NodeId>,
    /// Branch/jump targets, as block ids.
    targets: Vec<u32>,
    operands: Operands,
}

/// The operand payload of a node line, split by component because phases
/// *drop* components without the node changing (register allocation stops
/// printing truncation clauses and ranges): each component compares only
/// when both sides print it — otherwise decoration-invisibility (PLAN §7.4)
/// would collapse back into a text diff.
#[derive(Debug, Clone, Default, PartialEq, Eq)]
struct Operands {
    /// The group attached directly to the opcode token
    /// (`Int32Constant(0)`, TF's `NumberConstant[0]`), input ids masked.
    attached: String,
    /// Every `<…>` heap-object operand in the detached tail
    /// (`CheckMaps [n1] 0x… <Map…>`).
    heap: String,
    /// Detached `[…]` groups that carry meaning rather than location:
    /// register/location groups (containing `|`) and `live range: […]` are
    /// excluded; value ranges and Turboshaft parameter blocks survive.
    brackets: String,
    /// The `can`/`cannot truncate` verdict, when printed.
    truncate: Option<bool>,
    /// The `🪦` dead marker — only meaningful when the phase prints use
    /// counts at all.
    dead: Option<bool>,
}

impl Operands {
    fn differs(&self, other: &Operands) -> Option<(String, String)> {
        let both = |a: &String, b: &String| !a.is_empty() && !b.is_empty() && a != b;
        if both(&self.attached, &other.attached) {
            return Some((self.attached.clone(), other.attached.clone()));
        }
        if both(&self.heap, &other.heap) {
            return Some((self.heap.clone(), other.heap.clone()));
        }
        if both(&self.brackets, &other.brackets) {
            return Some((self.brackets.clone(), other.brackets.clone()));
        }
        if let (Some(a), Some(b)) = (self.truncate, other.truncate)
            && a != b
        {
            let word = |v: bool| if v { "can truncate" } else { "cannot truncate" }.to_string();
            return Some((word(a), word(b)));
        }
        if let (Some(a), Some(b)) = (self.dead, other.dead)
            && a != b
        {
            let word = |v: bool| if v { "dead 🪦" } else { "live" }.to_string();
            return Some((word(a), word(b)));
        }
        None
    }
}

impl NodeShape {
    /// Everything except the input list: opcode, targets, operands.
    fn body_change(&self, other: &NodeShape) -> Option<(String, String)> {
        if self.targets != other.targets {
            let fmt = |t: &[u32]| {
                t.iter()
                    .map(|b| format!("b{b}"))
                    .collect::<Vec<_>>()
                    .join(" ")
            };
            return Some((fmt(&self.targets), fmt(&other.targets)));
        }
        self.operands.differs(&other.operands)
    }
}

/// Extracts the [`Operands`] of one node line (see the field docs for what
/// is kept and what is decoration). All hex is masked through
/// [`canonicalize`], so heap addresses compare by shape, not by run.
fn operand_shape(text: &str, node: &crate::model::IRNode) -> Operands {
    let mut shape = Operands::default();
    let bytes = text.as_bytes();
    let tail_start = node.opcode_span.end as usize;
    if tail_start >= bytes.len() {
        return shape;
    }

    let in_input_span = |at: usize| {
        node.inputs
            .iter()
            .any(|r| (r.span.start as usize..r.span.end as usize).contains(&at))
    };
    let masked = |group_start: usize, group_end: usize| {
        let mut piece = String::new();
        let mut at = group_start;
        while at < group_end {
            if in_input_span(at) {
                if !piece.ends_with('·') {
                    piece.push('·');
                }
                at += 1;
                continue;
            }
            let ch = text[at..].chars().next().unwrap_or('\u{fffd}');
            piece.push(ch);
            at += ch.len_utf8();
        }
        canonicalize(&piece)
    };
    let balanced = |from: usize, open: u8, close: u8| {
        let mut depth = 0usize;
        let mut i = from;
        while i < bytes.len() {
            if bytes[i] == open {
                depth += 1;
            } else if bytes[i] == close {
                depth -= 1;
                if depth == 0 {
                    return i + 1;
                }
            }
            i += 1;
        }
        i
    };

    let mut i = tail_start;
    if bytes[i] == b'(' || bytes[i] == b'[' {
        let close = if bytes[i] == b'(' { b')' } else { b']' };
        let end = balanced(i, bytes[i], close);
        shape.attached = masked(i, end);
        i = end;
    }

    while i < bytes.len() {
        match bytes[i] {
            b'<' => {
                let end = balanced(i, b'<', b'>');
                shape.heap.push_str(&masked(i, end));
                shape.heap.push(' ');
                i = end;
            }
            b'[' => {
                let end = balanced(i, b'[', b']');
                let group = &text[i..end];
                let liveness = text[..i].ends_with("live range: ");
                let overlaps_input = (i..end).any(&in_input_span);
                if !group.contains('|') && !liveness && !overlaps_input {
                    shape.brackets.push_str(&masked(i, end));
                    shape.brackets.push(' ');
                }
                i = end;
            }
            _ => i += 1,
        }
    }

    if text.contains("cannot truncate") {
        shape.truncate = Some(false);
    } else if text.contains("can truncate") {
        shape.truncate = Some(true);
    }
    shape.dead = node.uses.is_some().then_some(node.dead);
    shape
}

/// Everything needed to build one side of a diff.
pub struct SideInput<'a> {
    pub buffer: &'a LogBuffer,
    pub parsed: &'a Arc<ParsedCompilation>,
    pub section: &'a CompilationSection,
    /// Phase index within the section.
    pub phase: usize,
    /// `(source, compilation)` — decides whether node ids are comparable.
    pub comp_id: (usize, usize),
}

impl DiffSide {
    fn build(input: &SideInput, by_id: bool) -> DiffSide {
        let phase_lines = input.section.phases[input.phase].lines.clone();
        let rows = model_rows(
            input.parsed,
            input.section,
            &phase_lines,
            input.comp_id.0,
            input.comp_id.1,
            &Default::default(),
            true, // annotations inline: the diff sees everything
            input.buffer,
            None,
        );

        let mut side = DiffSide {
            rows,
            parsed: Arc::clone(input.parsed),
            source: input.comp_id.0,
            keys: Vec::new(),
            row_nodes: Vec::new(),
            nodes: HashMap::new(),
            identity: HashMap::new(),
        };

        let node_at = |row: &ViewRow| {
            let (p, i) = row.info?;
            match input.parsed.phases.get(p)?.infos.get(i)? {
                LineInfo::Node(node) if node.id != SCHEDULE_ONLY => Some(node.clone()),
                _ => None,
            }
        };

        // Canonical renumbering (7.2): definition order within the phase.
        // Assigned in a full first pass so that forward references — loop
        // phis consuming their back edge — renumber too; assigning while
        // emitting keys left them on raw ids that never match across
        // compilations (found in review).
        let mut canon: HashMap<NodeId, u32> = HashMap::new();
        for row in &side.rows {
            if let Some(node) = node_at(row) {
                let n = canon.len() as u32;
                canon.entry(node.id).or_insert(n);
            }
        }

        for row in &side.rows {
            let info = row
                .info
                .and_then(|(p, i)| input.parsed.phases.get(p)?.infos.get(i));
            let node = node_at(row);
            side.row_nodes.push(node.as_ref().map(|n| n.id));
            let key = match (info, node) {
                (Some(LineInfo::Banner), _) => Key::Banner,
                (Some(LineInfo::BlockHeader { block }), _) => Key::Header(*block),
                (_, Some(node)) => {
                    let opcode = input
                        .parsed
                        .opcodes
                        .get(node.opcode as usize)
                        .cloned()
                        .unwrap_or_default();
                    let inputs: Vec<NodeId> = node.inputs.iter().map(|r| r.node).collect();
                    if opcode == "Identity"
                        && let [target] = inputs.as_slice()
                    {
                        side.identity.insert(node.id, *target);
                    }
                    let text = line_text(input.buffer, row.line);
                    side.nodes.insert(
                        node.id,
                        NodeShape {
                            opcode: opcode.clone(),
                            inputs: inputs.clone(),
                            targets: node.targets.iter().map(|t| t.block).collect(),
                            operands: operand_shape(&text, &node),
                        },
                    );
                    if by_id {
                        Key::Node(node.id)
                    } else {
                        // Structural key: opcode + canonically renumbered
                        // inputs.
                        use std::hash::{Hash, Hasher};
                        let mut h = std::collections::hash_map::DefaultHasher::new();
                        opcode.hash(&mut h);
                        for input in &inputs {
                            match canon.get(input) {
                                Some(c) => (0u8, *c).hash(&mut h),
                                None => (1u8, *input).hash(&mut h),
                            }
                        }
                        Key::Struct(h.finish())
                    }
                }
                _ => {
                    let mut t = canonicalize(&line_text(input.buffer, row.line));
                    // Schedule-only rows (gap moves, destination-less
                    // instructions) carry a leading instruction index that
                    // renumbers when anything is inserted; keying on it
                    // turned before/after-RA diffs into walls of
                    // delete+insert (found in review). Strip it.
                    if matches!(info, Some(LineInfo::Node(_))) {
                        let digits = t.bytes().take_while(|b| b.is_ascii_digit()).count();
                        if digits > 0 && t[digits..].starts_with(": ") {
                            t = t[digits + 2..].to_string();
                        }
                    }
                    Key::Text(t)
                }
            };
            side.keys.push(key);
        }
        side
    }

    fn resolve_identity(&self, id: NodeId) -> NodeId {
        let mut at = id;
        for _ in 0..MAX_IDENTITY_HOPS {
            match self.identity.get(&at) {
                Some(&next) => at = next,
                None => return at,
            }
        }
        at
    }
}

// ---------------------------------------------------------------------------
// The aligned model
// ---------------------------------------------------------------------------

/// One aligned row of the side-by-side view.
#[derive(Debug, Clone)]
pub struct DiffRow {
    /// Index into the left side's `rows`, if this row has a left half.
    pub left: Option<usize>,
    pub right: Option<usize>,
    pub status: RowStatus,
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Summary {
    pub nodes_added: usize,
    pub nodes_deleted: usize,
    pub nodes_changed: usize,
    pub nodes_replaced: usize,
    pub nodes_moved: usize,
    pub lines_added: usize,
    pub lines_deleted: usize,
}

impl Summary {
    /// `+3 −1 ~2 →1 nodes · +4 −4 lines`, omitting zero terms.
    pub fn describe(&self) -> String {
        let mut node_parts = Vec::new();
        if self.nodes_added > 0 {
            node_parts.push(format!("+{}", self.nodes_added));
        }
        if self.nodes_deleted > 0 {
            node_parts.push(format!("−{}", self.nodes_deleted));
        }
        if self.nodes_changed > 0 {
            node_parts.push(format!("~{}", self.nodes_changed));
        }
        if self.nodes_replaced > 0 {
            node_parts.push(format!("→{}", self.nodes_replaced));
        }
        if self.nodes_moved > 0 {
            node_parts.push(format!("≈{}", self.nodes_moved));
        }
        let mut out = String::new();
        if !node_parts.is_empty() {
            out.push_str(&format!("{} nodes", node_parts.join(" ")));
        }
        if self.lines_added > 0 || self.lines_deleted > 0 {
            if !out.is_empty() {
                out.push_str(" · ");
            }
            out.push_str(&format!(
                "+{} −{} lines",
                self.lines_added, self.lines_deleted
            ));
        }
        if out.is_empty() {
            out.push_str("no changes");
        }
        out
    }
}

pub struct DiffModel {
    pub left: DiffSide,
    pub right: DiffSide,
    pub rows: Vec<DiffRow>,
    pub summary: Summary,
    /// Pane titles when the panes cannot describe themselves — the
    /// dual-run diff, whose left side is another source.
    pub titles: Option<(String, String)>,
}

impl DiffModel {
    /// Human description of what happened to the node pair on one aligned
    /// row — the status line's detail text.
    pub fn describe_row(&self, at: usize) -> Option<String> {
        let row = self.rows.get(at)?;
        let node_id = |side: &DiffSide, idx: Option<usize>| {
            let (p, i) = side.rows.get(idx?)?.info?;
            match side.parsed.phases.get(p)?.infos.get(i)? {
                LineInfo::Node(n) if n.id != SCHEDULE_ONLY => Some(n.id),
                _ => None,
            }
        };
        match &row.status {
            RowStatus::Same => None,
            RowStatus::Added => Some(match node_id(&self.right, row.right) {
                Some(id) => format!("n{id} added"),
                None => "line added".to_string(),
            }),
            RowStatus::Deleted => Some(match node_id(&self.left, row.left) {
                Some(id) => format!("n{id} deleted"),
                None => "line deleted".to_string(),
            }),
            RowStatus::Moved { changed } => {
                let id = node_id(&self.left, row.left).or(node_id(&self.right, row.right))?;
                if *changed {
                    let detail = self
                        .change_detail(id, id)
                        .map(|d| format!(" · {d}"))
                        .unwrap_or_default();
                    Some(format!("n{id} moved and changed{detail}"))
                } else {
                    Some(format!("n{id} moved"))
                }
            }
            RowStatus::Replaced { by } => {
                let id = node_id(&self.left, row.left)?;
                Some(format!("n{id} replaced by n{by} (Identity)"))
            }
            RowStatus::Changed { .. } => {
                // Struct-matched pairs carry different ids per side.
                let l_id = node_id(&self.left, row.left)?;
                let r_id = node_id(&self.right, row.right).unwrap_or(l_id);
                let detail = self.change_detail(l_id, r_id)?;
                Some(format!("n{l_id} changed: {detail}"))
            }
        }
    }

    /// The concrete story of a changed pair, recomputed from the shapes so
    /// it works for id-matched and struct-matched pairs alike.
    fn change_detail(&self, l_id: NodeId, r_id: NodeId) -> Option<String> {
        let l = self.left.nodes.get(&l_id)?;
        let r = self.right.nodes.get(&r_id)?;
        let mut parts = Vec::new();
        if l.opcode != r.opcode {
            parts.push(format!("opcode {} → {}", l.opcode, r.opcode));
        }
        if l.inputs != r.inputs && l_id == r_id {
            let list = |shape: &NodeShape| {
                shape
                    .inputs
                    .iter()
                    .map(|n| format!("n{n}"))
                    .collect::<Vec<_>>()
                    .join(", ")
            };
            let rerouted = {
                let via = |side: &DiffSide| {
                    let a: Vec<NodeId> =
                        l.inputs.iter().map(|&n| side.resolve_identity(n)).collect();
                    let b: Vec<NodeId> =
                        r.inputs.iter().map(|&n| side.resolve_identity(n)).collect();
                    a == b
                };
                via(&self.right) || via(&self.left)
            };
            if rerouted {
                parts.push(format!(
                    "inputs rerouted via Identity: [{}] → [{}]",
                    list(l),
                    list(r)
                ));
            } else {
                parts.push(format!("inputs [{}] → [{}]", list(l), list(r)));
            }
        }
        if let Some((from, to)) = l.body_change(r) {
            let clip = |s: String| {
                if s.chars().count() > 48 {
                    let mut t: String = s.chars().take(47).collect();
                    t.push('…');
                    t
                } else {
                    s
                }
            };
            parts.push(format!("{} → {}", clip(from), clip(to)));
        }
        if parts.is_empty() {
            return None;
        }
        Some(parts.join(" · "))
    }
}

/// Builds the aligned diff of two phase views. Node identity is used when
/// both sides are the same compilation; structural hashing otherwise
/// (docs: PLAN §7.4 step 2).
pub fn diff_phases(left: &SideInput, right: &SideInput) -> DiffModel {
    let by_id = left.comp_id == right.comp_id;
    let left_side = DiffSide::build(left, by_id);
    let right_side = DiffSide::build(right, by_id);

    let ops = capture_diff_slices(Algorithm::Myers, &left_side.keys, &right_side.keys);

    let mut rows = Vec::new();
    let mut summary = Summary::default();

    for op in &ops {
        match op {
            DiffOp::Equal {
                old_index,
                new_index,
                len,
            } => {
                for k in 0..*len {
                    let l = old_index + k;
                    let r = new_index + k;
                    let status = match &left_side.keys[l] {
                        Key::Node(id) => judge_pair(*id, &left_side, &right_side, &mut summary),
                        // Structurally matched (different ids): opcode and
                        // canonical inputs are equal by construction, but
                        // the operand payload is not in the hash — compare
                        // it (found in review: SmiConstant(3) vs
                        // SmiConstant(7) matched and read Same).
                        Key::Struct(_) => {
                            judge_struct_pair((&left_side, l), (&right_side, r), &mut summary)
                        }
                        _ => RowStatus::Same,
                    };
                    rows.push(DiffRow {
                        left: Some(l),
                        right: Some(r),
                        status,
                    });
                }
            }
            DiffOp::Delete {
                old_index, old_len, ..
            } => {
                for l in *old_index..old_index + old_len {
                    let status = leave_status(
                        &left_side.keys[l],
                        &left_side,
                        &right_side,
                        &mut summary,
                        false,
                    );
                    rows.push(DiffRow {
                        left: Some(l),
                        right: None,
                        status,
                    });
                }
            }
            DiffOp::Insert {
                new_index, new_len, ..
            } => {
                for r in *new_index..new_index + new_len {
                    let status = leave_status(
                        &right_side.keys[r],
                        &right_side,
                        &left_side,
                        &mut summary,
                        true,
                    );
                    rows.push(DiffRow {
                        left: None,
                        right: Some(r),
                        status,
                    });
                }
            }
            DiffOp::Replace {
                old_index,
                old_len,
                new_index,
                new_len,
            } => {
                // similar's Replace is Delete + Insert; keep the two runs
                // adjacent so the eye can pair them.
                for l in *old_index..old_index + old_len {
                    let status = leave_status(
                        &left_side.keys[l],
                        &left_side,
                        &right_side,
                        &mut summary,
                        false,
                    );
                    rows.push(DiffRow {
                        left: Some(l),
                        right: None,
                        status,
                    });
                }
                for r in *new_index..new_index + new_len {
                    let status = leave_status(
                        &right_side.keys[r],
                        &right_side,
                        &left_side,
                        &mut summary,
                        true,
                    );
                    rows.push(DiffRow {
                        left: None,
                        right: Some(r),
                        status,
                    });
                }
            }
        }
    }

    DiffModel {
        left: left_side,
        right: right_side,
        rows,
        summary,
        titles: None,
    }
}

/// Status of a matched node pair (same key on both sides).
fn judge_pair(id: NodeId, left: &DiffSide, right: &DiffSide, summary: &mut Summary) -> RowStatus {
    let (Some(l), Some(r)) = (left.nodes.get(&id), right.nodes.get(&id)) else {
        return RowStatus::Same;
    };

    // A real node that became an Identity: replaced, and the interesting
    // fact is what it now points at (chains resolved).
    if r.opcode == "Identity" && l.opcode != "Identity" {
        summary.nodes_replaced += 1;
        return RowStatus::Replaced {
            by: right.resolve_identity(id),
        };
    }

    let opcode_changed = l.opcode != r.opcode;
    let inputs_changed = l.inputs != r.inputs;
    let operands_changed = l.body_change(r).is_some() && !opcode_changed;
    if !opcode_changed && !inputs_changed && !operands_changed {
        return RowStatus::Same;
    }

    // Rerouted-only: the lists differ, but resolving both through either
    // side's Identity map makes them agree — the values flowing in are the
    // same ones under new names. Cross-side resolution is sound *because*
    // judge_pair only ever runs when both sides are the same compilation
    // (Key::Node keying): the two id spaces are one space, and an Identity
    // on either side genuinely describes the other side's value. Do not
    // reuse this test across compilations, where the same id can name
    // unrelated nodes.
    let rerouted = inputs_changed && !opcode_changed && {
        let via = |side: &DiffSide| {
            let l: Vec<NodeId> = l.inputs.iter().map(|&n| side.resolve_identity(n)).collect();
            let r: Vec<NodeId> = r.inputs.iter().map(|&n| side.resolve_identity(n)).collect();
            l == r
        };
        via(right) || via(left)
    };

    summary.nodes_changed += 1;
    RowStatus::Changed {
        opcode: opcode_changed,
        inputs: inputs_changed,
        operands: operands_changed,
        rerouted,
    }
}

/// A structurally-matched pair: the hash already equates opcode and
/// canonical inputs, so only the operand payload can differ.
fn judge_struct_pair(
    left: (&DiffSide, usize),
    right: (&DiffSide, usize),
    summary: &mut Summary,
) -> RowStatus {
    let (Some(&Some(l_id)), Some(&Some(r_id))) =
        (left.0.row_nodes.get(left.1), right.0.row_nodes.get(right.1))
    else {
        return RowStatus::Same;
    };
    let (Some(l), Some(r)) = (left.0.nodes.get(&l_id), right.0.nodes.get(&r_id)) else {
        return RowStatus::Same;
    };
    if l.operands.differs(&r.operands).is_none() && l.targets == r.targets {
        return RowStatus::Same;
    }
    summary.nodes_changed += 1;
    RowStatus::Changed {
        opcode: false,
        inputs: false,
        operands: true,
        rerouted: false,
    }
}

/// Status of an unmatched row: `Added`/`Deleted`, unless it is a node whose
/// id still exists on the other side — then the node merely *moved* and
/// saying "deleted" would be a lie. A move can carry a body change too; it
/// must not swallow it (found in review).
fn leave_status(
    key: &Key,
    own: &DiffSide,
    other: &DiffSide,
    summary: &mut Summary,
    added: bool,
) -> RowStatus {
    match key {
        Key::Node(id) if other.nodes.contains_key(id) => {
            let changed = match (own.nodes.get(id), other.nodes.get(id)) {
                (Some(a), Some(b)) => {
                    a.opcode != b.opcode || a.inputs != b.inputs || a.body_change(b).is_some()
                }
                _ => false,
            };
            // Count each moved node once, on its left-side (leave) row.
            if !added {
                summary.nodes_moved += 1;
                if changed {
                    summary.nodes_changed += 1;
                }
            }
            RowStatus::Moved { changed }
        }
        Key::Node(_) | Key::Struct(_) => {
            if added {
                summary.nodes_added += 1;
                RowStatus::Added
            } else {
                summary.nodes_deleted += 1;
                RowStatus::Deleted
            }
        }
        _ => {
            if added {
                summary.lines_added += 1;
                RowStatus::Added
            } else {
                summary.lines_deleted += 1;
                RowStatus::Deleted
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::index::TraceIndex;

    fn setup(trace: &str) -> (LogBuffer, TraceIndex) {
        let mut buffer = LogBuffer::new();
        buffer.append(trace.as_bytes());
        buffer.finish();
        let mut idx = TraceIndex::new(None);
        idx.ingest(&buffer, true);
        (buffer, idx)
    }

    /// Diffs phase `a` against phase `b` of compilation 0.
    fn diff(trace: &str, a: usize, b: usize) -> DiffModel {
        let (buffer, idx) = setup(trace);
        let section = &idx.compilations[0];
        let parsed = Arc::new(crate::parse::maglev::parse_compilation(&buffer, section));
        diff_phases(
            &SideInput {
                buffer: &buffer,
                parsed: &parsed,
                section,
                phase: a,
                comp_id: (0, 0),
            },
            &SideInput {
                buffer: &buffer,
                parsed: &parsed,
                section,
                phase: b,
                comp_id: (0, 0),
            },
        )
    }

    #[test]
    fn canonicalizer_masks_volatile_content() {
        assert_eq!(
            canonicalize("Compiling 0x09b80101e371 <JSFunction add (sfi = 0x9b80101e2cd)>"),
            "Compiling 0x· <JSFunction add (sfi = 0x·)>"
        );
        assert_eq!(
            canonicalize("[ML:38612] ⚡ INLINE small"),
            "⚡ INLINE small"
        );
        assert_eq!(
            canonicalize("took 0.000, 1.110, 0.025 ms"),
            "took ·, ·, · ms"
        );
        // Leading indentation and gutter glyphs are layout, not content: the
        // schedule column re-indents whole phases (review finding M5).
        assert_eq!(
            canonicalize("  11: Foo [n9], 2 uses"),
            "11: Foo [n9], 2 uses"
        );
        assert_eq!(
            canonicalize("      ↳ lazy @-1 (4 live vars)"),
            canonicalize("         ↳ lazy @-1 (4 live vars)")
        );
        assert_eq!(canonicalize("│   ↓"), canonicalize("      ↓"));
        assert_eq!(canonicalize("(addr:0x124011b0fb8)"), "(addr:0x·)");
    }

    const TWO_PHASES: &str = "\
Compiling 0x1 <JSFunction f (sfi = 0x10)> with Maglev
----- Maglev graph building -----
 Block b0
   1: InitialValue(a0)
   2: SmiConstant(3)
   5: Int32Add [n1, n2], 1 uses
   6: CheckSmth [n5]
   9: Foo [n5]
 Block b1
   7: Jump b0
----- Phi untagging -----
 Block b0
   1: InitialValue(a0)
   2: SmiConstant(3)
   5: Identity [n12], 1 uses
  12: Float64Add [n1, n2], 1 uses
   9: Foo [n5]
 Block b1
   7: Jump b0
";

    #[test]
    fn identity_replacement_and_additions_are_identified() {
        let m = diff(TWO_PHASES, 0, 1);
        // n5: real op → Identity [n12]: replaced-by, with the target named.
        let replaced: Vec<&DiffRow> = m
            .rows
            .iter()
            .filter(|r| matches!(r.status, RowStatus::Replaced { .. }))
            .collect();
        assert_eq!(replaced.len(), 1);
        assert_eq!(replaced[0].status, RowStatus::Replaced { by: 12 });
        assert!(
            m.describe_row(
                m.rows
                    .iter()
                    .position(|r| r.status == RowStatus::Replaced { by: 12 })
                    .unwrap()
            )
            .unwrap()
            .contains("n5 replaced by n12"),
        );

        // n12 is new; n6 (the check) is gone.
        assert_eq!(m.summary.nodes_added, 1);
        assert_eq!(m.summary.nodes_deleted, 1);
        assert_eq!(m.summary.nodes_replaced, 1);
        // n9's inputs are textually identical, so it is unchanged.
        assert_eq!(m.summary.nodes_changed, 0);

        // Alignment: matched rows carry both sides; the banner and headers
        // pair up.
        assert!(m.rows.iter().any(|r| r.left.is_some() && r.right.is_some()));
    }

    const REROUTE: &str = "\
Compiling 0x1 <JSFunction f (sfi = 0x10)> with Maglev
----- Maglev graph building -----
 Block b0
   5: Int32Add [n1, n2]
   9: Foo [n5]
----- Phi untagging -----
 Block b0
   5: Identity [n12]
  12: Float64Add [n1, n2]
   9: Foo [n12]
";

    #[test]
    fn rerouted_inputs_are_distinguished_from_real_input_changes() {
        let m = diff(REROUTE, 0, 1);
        // n9 consumed n5 before and n12 after — but n5 *is* n12 through the
        // Identity, so this is a reroute, not a semantic input change.
        let changed: Vec<&DiffRow> = m
            .rows
            .iter()
            .filter(|r| matches!(r.status, RowStatus::Changed { .. }))
            .collect();
        assert_eq!(changed.len(), 1);
        assert_eq!(
            changed[0].status,
            RowStatus::Changed {
                opcode: false,
                inputs: true,
                operands: false,
                rerouted: true
            }
        );
        let at = m
            .rows
            .iter()
            .position(|r| matches!(r.status, RowStatus::Changed { .. }))
            .unwrap();
        let detail = m.describe_row(at).unwrap();
        assert!(detail.contains("rerouted"), "{detail}");
    }

    const MOVED: &str = "\
Compiling 0x1 <JSFunction f (sfi = 0x10)> with Maglev
----- Maglev graph building -----
 Block b0
   3: Alpha
   4: Beta
 Block b1
   5: Gamma
----- Phi untagging -----
 Block b0
   4: Beta
 Block b1
   5: Gamma
   3: Alpha
";

    #[test]
    fn a_node_that_changes_blocks_is_moved_not_deleted() {
        let m = diff(MOVED, 0, 1);
        let moved: Vec<&RowStatus> = m
            .rows
            .iter()
            .filter(|r| matches!(r.status, RowStatus::Moved { .. }))
            .map(|r| &r.status)
            .collect();
        assert_eq!(moved.len(), 2, "one leave row, one arrive row");
        assert_eq!(m.summary.nodes_moved, 1, "counted once");
        assert_eq!(m.summary.nodes_deleted, 0);
        assert_eq!(m.summary.nodes_added, 0);
    }

    #[test]
    fn opcode_and_input_changes_are_reported_precisely() {
        let m = diff(
            "\
Compiling 0x1 <JSFunction f (sfi = 0x10)> with Maglev
----- A -----
   5: Int32Add [n1, n2]
   6: Baz [n1]
----- B -----
   5: Float64Add [n1, n2]
   6: Baz [n1, n5]
",
            0,
            1,
        );
        let statuses: Vec<&RowStatus> = m
            .rows
            .iter()
            .filter(|r| !matches!(r.status, RowStatus::Same))
            .map(|r| &r.status)
            .collect();
        assert_eq!(
            statuses,
            vec![
                &RowStatus::Changed {
                    opcode: true,
                    inputs: false,
                    operands: false,
                    rerouted: false
                },
                &RowStatus::Changed {
                    opcode: false,
                    inputs: true,
                    operands: false,
                    rerouted: false
                },
            ]
        );
        assert_eq!(m.summary.nodes_changed, 2);
        assert_eq!(m.summary.describe(), "~2 nodes");
    }

    #[test]
    fn cross_compilation_diff_matches_structurally() {
        let trace = "\
Compiling 0x1 <JSFunction f (sfi = 0x10)> with Maglev
----- Maglev graph building -----
 Block b0
   4: InitialValue(a0)
   7: CheckSmth [n4]
   9: Foo [n4]
Compiling 0x1 <JSFunction f (sfi = 0x10)> with Maglev
----- Maglev graph building -----
 Block b0
   3: InitialValue(a0)
   8: Foo [n3]
";
        let (buffer, idx) = setup(trace);
        let a = &idx.compilations[0];
        let b = &idx.compilations[1];
        let pa = Arc::new(crate::parse::maglev::parse_compilation(&buffer, a));
        let pb = Arc::new(crate::parse::maglev::parse_compilation(&buffer, b));
        let m = diff_phases(
            &SideInput {
                buffer: &buffer,
                parsed: &pa,
                section: a,
                phase: 0,
                comp_id: (0, 0),
            },
            &SideInput {
                buffer: &buffer,
                parsed: &pb,
                section: b,
                phase: 0,
                comp_id: (0, 1),
            },
        );
        // Different ids (n4/n9 vs n3/n8) but the same structure: the
        // InitialValue and the Foo consuming it match; the check is deleted.
        assert_eq!(m.summary.nodes_deleted, 1);
        assert_eq!(m.summary.nodes_added, 0);
        let matched = m
            .rows
            .iter()
            .filter(|r| r.left.is_some() && r.right.is_some())
            .count();
        assert!(matched >= 3, "banner, header, and two structural matches");
    }

    /// Review C1: changes carried only in the operand payload — constant
    /// values, heap operands, branch targets, truncation verdicts — must
    /// not read `Same`.
    #[test]
    fn payload_only_changes_are_reported() {
        let m = diff(
            "\
Compiling 0x1 <JSFunction f (sfi = 0x10)> with Maglev
----- A -----
 Block b0
  25: Int32Constant(0), 1 uses
  30: RootConstant(undefined_value), 1 uses
  31: CheckMaps [n25] 0xaaaa <Map[HeapNumber]>, 1 uses
  40: BranchIfToBooleanTrue [n25] b1 b2
  26: Int32BitwiseOr [n25, n25], 1 uses, cannot truncate to int32
----- B -----
 Block b0
  25: Int32Constant(7), 1 uses
  30: RootConstant(null_value), 1 uses
  31: CheckMaps [n25] 0xbbbb <Map[String]>, 1 uses
  40: BranchIfToBooleanTrue [n25] b3 b2
  26: Int32BitwiseOr [n25, n25], 1 uses, can truncate to int32 [-1, 1]
",
            0,
            1,
        );
        let changed: Vec<_> = m
            .rows
            .iter()
            .filter(|r| matches!(r.status, RowStatus::Changed { operands: true, .. }))
            .collect();
        assert_eq!(
            changed.len(),
            5,
            "constant, root, map, branch target, truncation verdict all reported"
        );
        assert_eq!(m.summary.nodes_changed, 5);
        // The details say what actually changed.
        let details: Vec<String> = (0..m.rows.len())
            .filter_map(|i| m.describe_row(i))
            .collect();
        assert!(
            details.iter().any(|d| d.contains("(0) → (7)")),
            "{details:?}"
        );
        assert!(
            details.iter().any(|d| d.contains("b1 b2 → b3 b2")),
            "{details:?}"
        );
        assert!(
            details
                .iter()
                .any(|d| d.contains("cannot truncate → can truncate")),
            "{details:?}"
        );
        // Map addresses compare canonicalized: same shape, different text —
        // both are `<Map[…]>` payloads with masked hex, so the *names*
        // differ, which is the change.
        assert!(
            details.iter().any(|d| d.contains("Map[HeapNumber]")),
            "{details:?}"
        );
    }

    /// Review M5 + PLAN §7.4: per-phase decorations — schedule ids, register
    /// locations, live ranges, use counts, dropped truncation clauses,
    /// indentation — must stay invisible.
    #[test]
    fn decorations_stay_invisible_to_the_diff() {
        let m = diff(
            "\
Compiling 0x1 <JSFunction f (sfi = 0x10)> with Maglev
----- A -----
 Block b0
   9: CheckedSmiUntag [n2], 1 uses, cannot truncate to int32
      ↳ lazy @-1 (4 live vars)
  12: HeapConstant(0xaaaa <FeedbackCell>), 1 uses
   ↓
----- B -----
 Block b0
    9/9: CheckedSmiUntag [v5/n2:[x0|R|t]] → [x0|R|w32], live range: [9-11]
         ↳ lazy @-1 (4 live vars)
    1/12: HeapConstant(0xbbbb <FeedbackCell>) → v-1, live range: [1-12]
      ↓
",
            0,
            1,
        );
        assert_eq!(
            m.summary,
            Summary::default(),
            "schedule ids, registers, live ranges, indentation: no changes"
        );
        assert!(m.rows.iter().all(|r| r.status == RowStatus::Same));
    }

    /// Review M2: a node that moved *and* changed reports both facts.
    #[test]
    fn moved_and_changed_reports_both() {
        let m = diff(
            "\
Compiling 0x1 <JSFunction f (sfi = 0x10)> with Maglev
----- A -----
 Block b0
   3: Alpha [n1]
   4: Beta
 Block b1
   5: Gamma
----- B -----
 Block b0
   4: Beta
 Block b1
   5: Gamma
   3: Alpha [n2]
",
            0,
            1,
        );
        let moved: Vec<_> = m
            .rows
            .iter()
            .filter(|r| matches!(r.status, RowStatus::Moved { changed: true }))
            .collect();
        assert_eq!(moved.len(), 2, "leave and arrive rows both say changed");
        assert_eq!(m.summary.nodes_moved, 1);
        assert_eq!(m.summary.nodes_changed, 1, "the input change is counted");
        let details: Vec<String> = (0..m.rows.len())
            .filter_map(|i| m.describe_row(i))
            .collect();
        assert!(
            details.iter().any(|d| d.contains("moved and changed")),
            "{details:?}"
        );
    }

    /// Review m6: canonical renumbering must cover forward references (loop
    /// phis consuming their back edge), or the same loop never matches
    /// across compilations.
    #[test]
    fn cross_compilation_loop_phis_match_structurally() {
        let trace = "\
Compiling 0x1 <JSFunction f (sfi = 0x10)> with Maglev
----- Maglev graph building -----
 Block b0
   1: InitialValue(a0)
 Block b1
   2: φ r0 (n1, n3)
   3: Int32Add [n2, n1]
Compiling 0x1 <JSFunction f (sfi = 0x10)> with Maglev
----- Maglev graph building -----
 Block b0
  11: InitialValue(a0)
 Block b1
  12: φ r0 (n11, n13)
  13: Int32Add [n12, n11]
";
        let (buffer, idx) = setup(trace);
        let a = &idx.compilations[0];
        let b = &idx.compilations[1];
        let pa = Arc::new(crate::parse::maglev::parse_compilation(&buffer, a));
        let pb = Arc::new(crate::parse::maglev::parse_compilation(&buffer, b));
        let m = diff_phases(
            &SideInput {
                buffer: &buffer,
                parsed: &pa,
                section: a,
                phase: 0,
                comp_id: (0, 0),
            },
            &SideInput {
                buffer: &buffer,
                parsed: &pb,
                section: b,
                phase: 0,
                comp_id: (0, 1),
            },
        );
        assert_eq!(m.summary.nodes_added, 0, "{:?}", m.summary);
        assert_eq!(m.summary.nodes_deleted, 0, "the phi matches despite ids");
    }

    /// Review C1 (structural half): structurally matched nodes with
    /// different payloads report the payload change.
    #[test]
    fn cross_compilation_payload_change_is_reported() {
        let trace = "\
Compiling 0x1 <JSFunction f (sfi = 0x10)> with Maglev
----- Maglev graph building -----
 Block b0
   4: SmiConstant(3)
Compiling 0x1 <JSFunction f (sfi = 0x10)> with Maglev
----- Maglev graph building -----
 Block b0
   9: SmiConstant(7)
";
        let (buffer, idx) = setup(trace);
        let a = &idx.compilations[0];
        let b = &idx.compilations[1];
        let pa = Arc::new(crate::parse::maglev::parse_compilation(&buffer, a));
        let pb = Arc::new(crate::parse::maglev::parse_compilation(&buffer, b));
        let m = diff_phases(
            &SideInput {
                buffer: &buffer,
                parsed: &pa,
                section: a,
                phase: 0,
                comp_id: (0, 0),
            },
            &SideInput {
                buffer: &buffer,
                parsed: &pb,
                section: b,
                phase: 0,
                comp_id: (0, 1),
            },
        );
        assert_eq!(m.summary.nodes_changed, 1, "{:?}", m.summary);
        assert!(
            m.rows
                .iter()
                .any(|r| matches!(r.status, RowStatus::Changed { operands: true, .. })),
        );
    }

    #[test]
    fn identity_chains_resolve_with_cycle_guard() {
        let m = diff(
            "\
Compiling 0x1 <JSFunction f (sfi = 0x10)> with Maglev
----- A -----
   5: Foo [n1]
----- B -----
   5: Identity [n6]
   6: Identity [n7]
   7: Bar
",
            0,
            1,
        );
        assert!(
            m.rows
                .iter()
                .any(|r| r.status == RowStatus::Replaced { by: 7 }),
            "chain n5→n6→n7 resolves to the end"
        );

        // A cycle must not hang.
        let m = diff(
            "\
Compiling 0x1 <JSFunction f (sfi = 0x10)> with Maglev
----- A -----
   5: Foo [n1]
----- B -----
   5: Identity [n6]
   6: Identity [n5]
",
            0,
            1,
        );
        assert!(
            m.rows
                .iter()
                .any(|r| matches!(r.status, RowStatus::Replaced { .. })),
        );
    }
}
