// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! The viewport's display list.
//!
//! Folding means the viewport can no longer be "a contiguous range of buffer
//! lines": a folded block collapses to one marker row, and annotation runs
//! collapse to `[+] N trace lines` until `t` shows them. So the cursor moves
//! over *display rows*, and this module builds them.
//!
//! Two shapes, chosen by what is selected:
//!
//! - [`ViewModel::Plain`] — a 1:1 window over a line range. Used for raw
//!   sections and anything too big to model row-by-row; nothing is
//!   materialised, so selecting a 6.8M-line raw section stays O(1).
//! - [`ViewModel::Modeled`] — one entry per display row, built from the
//!   parsed compilation. Only compilations are modeled, and a compilation is
//!   thousands of lines at worst, so the build is cheap enough to redo
//!   whenever fold state changes.

use std::collections::HashSet;
use std::ops::Range;
use std::sync::Arc;

use crate::model::{BlockId, LineInfo, ParsedCompilation, PhaseKind};
use crate::source::LogBuffer;

/// Sections larger than this are never modeled row-by-row. Far above any real
/// compilation (the corpus maximum is ~1 000 lines); a guard, not a tune.
pub const MODEL_LIMIT: usize = 200_000;

// ---------------------------------------------------------------------------
// Opcode shape predicates (shared by styling and the semantic lenses)
// ---------------------------------------------------------------------------
//
// The opcode vocabulary is open — V8 adds opcodes freely — so these match on
// name shape, not a fixed list. `ui::opcode_class` styles through the same
// predicates so the `:checks` count and the red guard styling can never
// disagree about what a guard is.

pub fn is_guard_opcode(name: &str) -> bool {
    name.starts_with("Check") || name.contains("Deopt") || name.starts_with("Assert")
}

pub fn is_control_opcode(name: &str) -> bool {
    name.starts_with("Jump")
        || name.starts_with("Branch")
        || name.starts_with("Return")
        || name.starts_with("Switch")
        || name.starts_with("CheckpointedJump")
        || name.starts_with("Abort")
        || name.starts_with("Throw")
        // Turboshaft's dominant control op (found in review: the :phi
        // backbone lens showed no control flow on TurboFan dumps).
        || name == "Goto"
}

pub fn is_phi_opcode(name: &str) -> bool {
    // Maglev prints φ/φᵀ; TurboFan and Turboshaft print Phi/phi.
    name.starts_with('φ') || name == "Phi" || name == "phi"
}

/// A semantic viewport lens: the modeled view keeps only the
/// banner/block-header skeleton plus rows the lens matches. Deliberately a
/// *filter* rather than pure highlighting — guards are already styled red by
/// default, so the value of `:checks` is the count and the narrowed view, and
/// the same holds for the others.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Lens {
    /// Guard nodes: `Check*`, `Assert*`, anything with `Deopt` in the name.
    Checks,
    /// The control/phi backbone: block headers, phis, and control flow.
    Phi,
    /// Regalloc spill traffic: `spill:` lines and gap moves touching a stack
    /// slot (both spills and reloads).
    Spill,
    /// Megamorphic feedback and slow-path ICs, matched on the text because it
    /// appears in feedback preambles and in `CallBuiltin(*_Megamorphic)`
    /// nodes alike.
    Megamorphic,
}

impl Lens {
    pub fn name(self) -> &'static str {
        match self {
            Lens::Checks => "checks",
            Lens::Phi => "phi",
            Lens::Spill => "spill",
            Lens::Megamorphic => "megamorphic",
        }
    }

    /// Does the lens keep this row? Structure rows (banner, block header) are
    /// the caller's business; this judges content only.
    pub fn matches(
        self,
        info: Option<&LineInfo>,
        parsed: &ParsedCompilation,
        buffer: &LogBuffer,
        line: usize,
    ) -> bool {
        let opcode_name = |node: &crate::model::IRNode| {
            parsed
                .opcodes
                .get(node.opcode as usize)
                .map(String::as_str)
                .unwrap_or("")
        };
        match self {
            Lens::Checks => match info {
                Some(LineInfo::Node(node)) => is_guard_opcode(opcode_name(node)),
                _ => false,
            },
            Lens::Phi => match info {
                Some(LineInfo::Node(node)) => {
                    let name = opcode_name(node);
                    is_phi_opcode(name) || is_control_opcode(name)
                }
                Some(LineInfo::PhiMove { .. }) => true,
                _ => false,
            },
            Lens::Spill => {
                let text = crate::parse::maglev::line_text(buffer, line);
                text.contains("spill: ")
                    || (text.contains("GapMove") && text.contains("[stack:"))
                    // TurboFan instruction sequences spell spills as gap
                    // moves into stack slots (found in review).
                    || (text.contains("gap (") && text.contains("[stack:"))
            }
            Lens::Megamorphic => {
                let text = crate::parse::maglev::line_text(buffer, line);
                text.to_ascii_lowercase().contains("megamorphic")
            }
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum RowKind {
    Text,
    /// A folded block header: `[+] b4 — 14 lines hidden`.
    BlockFold {
        block: BlockId,
        hidden: usize,
    },
    /// A collapsed annotation run: `[+] 3 trace lines`.
    AnnotationFold {
        count: usize,
    },
}

#[derive(Debug, Clone)]
pub struct ViewRow {
    /// Buffer line this row shows (the header line for folds).
    pub line: usize,
    /// `(phase index, info index)` into the parsed compilation, when modeled.
    pub info: Option<(usize, usize)>,
    pub kind: RowKind,
}

pub enum ViewModel {
    Plain {
        range: Range<usize>,
    },
    Modeled {
        rows: Vec<ViewRow>,
        parsed: Arc<ParsedCompilation>,
        /// Compilation index within the source, for fold-state keys.
        comp: usize,
    },
}

impl ViewModel {
    pub fn len(&self) -> usize {
        match self {
            ViewModel::Plain { range } => range.len(),
            ViewModel::Modeled { rows, .. } => rows.len(),
        }
    }

    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }

    /// The buffer line shown at a display row.
    pub fn line_at(&self, row: usize) -> Option<usize> {
        match self {
            ViewModel::Plain { range } => {
                let line = range.start + row;
                (line < range.end).then_some(line)
            }
            ViewModel::Modeled { rows, .. } => rows.get(row).map(|r| r.line),
        }
    }

    pub fn row(&self, row: usize) -> Option<ViewRow> {
        match self {
            ViewModel::Plain { range: _ } => self.line_at(row).map(|line| ViewRow {
                line,
                info: None,
                kind: RowKind::Text,
            }),
            ViewModel::Modeled { rows, .. } => rows.get(row).cloned(),
        }
    }

    /// The display row showing a buffer line, if it is visible (used to land
    /// jumps; a jump into a folded block first unfolds it, so a `None` here is
    /// always resolvable by the caller).
    pub fn row_of_line(&self, line: usize) -> Option<usize> {
        match self {
            ViewModel::Plain { range } => (range.contains(&line)).then(|| line - range.start),
            ViewModel::Modeled { rows, .. } => rows
                .iter()
                .position(|r| r.line == line && r.kind == RowKind::Text),
        }
    }

    pub fn parsed(&self) -> Option<&Arc<ParsedCompilation>> {
        match self {
            ViewModel::Plain { .. } => None,
            ViewModel::Modeled { parsed, .. } => Some(parsed),
        }
    }
}

/// Fold state, keyed so it survives navigating away and back (
/// per-block persisted state).
pub type FoldKey = (usize, usize, usize, BlockId); // (source, comp, phase, block)

/// Builds the modeled rows for one compilation section (or a sub-range of it,
/// for a phase selection). With a lens, only the skeleton (anchor lines,
/// banners, block headers) and matching rows survive; annotation folding is
/// bypassed because a matching annotation is exactly what the user asked to
/// see.
#[allow(clippy::too_many_arguments)]
pub fn model_rows(
    parsed: &Arc<ParsedCompilation>,
    section: &crate::model::CompilationSection,
    only_lines: &Range<usize>,
    source: usize,
    comp: usize,
    folded: &HashSet<FoldKey>,
    show_annotations: bool,
    buffer: &LogBuffer,
    lens: Option<Lens>,
) -> Vec<ViewRow> {
    let mut rows = Vec::new();

    // The anchor line and any rule/`Begin compiling` prefix live in the
    // section but before the preamble; they still have to render.
    for line in section.lines.start..section.preamble.start {
        if only_lines.contains(&line) {
            rows.push(ViewRow {
                line,
                info: None,
                kind: RowKind::Text,
            });
        }
    }

    // Preamble lines (annotations fold there too).
    let mut pending_annotations: Vec<(usize, Option<(usize, usize)>)> = Vec::new();

    let flush_annotations =
        |rows: &mut Vec<ViewRow>, pending: &mut Vec<(usize, Option<(usize, usize)>)>| {
            if pending.is_empty() {
                return;
            }
            if show_annotations {
                for (line, info) in pending.drain(..) {
                    rows.push(ViewRow {
                        line,
                        info,
                        kind: RowKind::Text,
                    });
                }
            } else {
                let count = pending.len();
                let (line, info) = pending[0];
                rows.push(ViewRow {
                    line,
                    info,
                    kind: RowKind::AnnotationFold { count },
                });
                pending.clear();
            }
        };

    for (line, info) in section.preamble.clone().zip_longest_infos(&parsed.preamble) {
        if !only_lines.contains(&line) {
            continue;
        }
        if let Some(lens) = lens {
            if lens.matches(info, parsed, buffer, line) {
                rows.push(ViewRow {
                    line,
                    info: None,
                    kind: RowKind::Text,
                });
            }
            continue;
        }
        match info {
            Some(LineInfo::Annotation { .. }) => pending_annotations.push((line, None)),
            _ => {
                flush_annotations(&mut rows, &mut pending_annotations);
                rows.push(ViewRow {
                    line,
                    info: None,
                    kind: RowKind::Text,
                });
            }
        }
    }
    flush_annotations(&mut rows, &mut pending_annotations);

    for (p, (phase_section, phase)) in section.phases.iter().zip(&parsed.phases).enumerate() {
        // Which block each info line belongs to, tracked while walking.
        let mut current_fold: Option<(BlockId, usize)> = None; // (block, hidden count)
        let is_graph = matches!(phase_section.kind, PhaseKind::Graph { .. });

        for (i, line) in phase_section.lines.clone().enumerate() {
            if !only_lines.contains(&line) {
                continue;
            }
            let info = phase.infos.get(i);

            if is_graph && let Some(LineInfo::BlockHeader { block }) = info {
                flush_annotations(&mut rows, &mut pending_annotations);
                // Close any previous fold run.
                if let Some((folded_block, hidden)) = current_fold.take() {
                    patch_fold_count(&mut rows, folded_block, hidden);
                }
                if folded.contains(&(source, comp, p, *block)) {
                    current_fold = Some((*block, 0));
                    rows.push(ViewRow {
                        line,
                        info: Some((p, i)),
                        kind: RowKind::BlockFold {
                            block: *block,
                            hidden: 0, // patched when the run closes
                        },
                    });
                    continue;
                }
            }

            if let Some((_, hidden)) = &mut current_fold {
                *hidden += 1;
                continue;
            }

            if let Some(lens) = lens {
                let structure = matches!(
                    info,
                    Some(LineInfo::Banner) | Some(LineInfo::BlockHeader { .. })
                );
                if structure || lens.matches(info, parsed, buffer, line) {
                    rows.push(ViewRow {
                        line,
                        info: Some((p, i)),
                        kind: RowKind::Text,
                    });
                }
                continue;
            }

            match info {
                Some(LineInfo::Annotation { .. }) => {
                    pending_annotations.push((line, Some((p, i))));
                }
                _ => {
                    flush_annotations(&mut rows, &mut pending_annotations);
                    rows.push(ViewRow {
                        line,
                        info: Some((p, i)),
                        kind: RowKind::Text,
                    });
                }
            }
        }
        flush_annotations(&mut rows, &mut pending_annotations);
        if let Some((folded_block, hidden)) = current_fold.take() {
            patch_fold_count(&mut rows, folded_block, hidden);
        }
    }

    rows
}

fn patch_fold_count(rows: &mut [ViewRow], block: BlockId, hidden: usize) {
    if let Some(row) = rows
        .iter_mut()
        .rev()
        .find(|r| matches!(r.kind, RowKind::BlockFold { block: b, .. } if b == block))
    {
        row.kind = RowKind::BlockFold { block, hidden };
    }
}

/// Pairs preamble line numbers with their parsed infos, tolerating a parse
/// that is shorter than the section (a still-streaming compilation).
trait ZipInfos {
    fn zip_longest_infos<'a>(
        self,
        infos: &'a [LineInfo],
    ) -> Box<dyn Iterator<Item = (usize, Option<&'a LineInfo>)> + 'a>;
}

impl ZipInfos for Range<usize> {
    fn zip_longest_infos<'a>(
        self,
        infos: &'a [LineInfo],
    ) -> Box<dyn Iterator<Item = (usize, Option<&'a LineInfo>)> + 'a> {
        Box::new(self.enumerate().map(move |(i, line)| (line, infos.get(i))))
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::index::TraceIndex;
    use crate::source::LogBuffer;

    fn setup(trace: &str) -> (LogBuffer, TraceIndex) {
        let mut buffer = LogBuffer::new();
        buffer.append(trace.as_bytes());
        buffer.finish();
        let mut idx = TraceIndex::new(None);
        idx.ingest(&buffer, true);
        (buffer, idx)
    }

    const TRACE: &str = "\
Compiling 0x1 <JSFunction f (sfi = 0x10)> with Maglev
----- Maglev graph building -----
 Block b0
   1: Foo
   2: Bar [n1]
 Block b1
   3: Baz [n2]
   some regalloc trace line
   another trace line
   4: Quux [n3]
";

    fn build(
        buffer: &LogBuffer,
        idx: &TraceIndex,
        folded: &HashSet<FoldKey>,
        show_annotations: bool,
    ) -> Vec<ViewRow> {
        let section = &idx.compilations[0];
        let parsed = Arc::new(crate::parse::maglev::parse_compilation(buffer, section));
        model_rows(
            &parsed,
            section,
            &section.lines.clone(),
            0,
            0,
            folded,
            show_annotations,
            buffer,
            None,
        )
    }

    #[test]
    fn lens_keeps_skeleton_and_matches_only() {
        let trace = "\
Compiling 0x1 <JSFunction f (sfi = 0x10)> with Maglev
----- Maglev graph building -----
 Block b0
   1: CheckMaps [n0]
   2: Int32Add [n1, n1]
 Block b1
   3: Jump b0
";
        let (buffer, idx) = setup(trace);
        let section = &idx.compilations[0];
        let parsed = Arc::new(crate::parse::maglev::parse_compilation(&buffer, section));
        let rows = model_rows(
            &parsed,
            section,
            &section.lines.clone(),
            0,
            0,
            &HashSet::new(),
            false,
            &buffer,
            Some(Lens::Checks),
        );
        // Anchor + banner + b0 + CheckMaps + b1: Int32Add and Jump are gone.
        let lines: Vec<usize> = rows.iter().map(|r| r.line).collect();
        assert_eq!(lines, vec![0, 1, 2, 3, 5]);

        let rows = model_rows(
            &parsed,
            section,
            &section.lines.clone(),
            0,
            0,
            &HashSet::new(),
            false,
            &buffer,
            Some(Lens::Phi),
        );
        let lines: Vec<usize> = rows.iter().map(|r| r.line).collect();
        assert_eq!(lines, vec![0, 1, 2, 5, 6], "backbone: headers + Jump");
    }

    #[test]
    fn annotations_fold_into_one_marker_by_default() {
        let (buffer, idx) = setup(TRACE);
        let rows = build(&buffer, &idx, &HashSet::new(), false);
        let fold = rows
            .iter()
            .find(|r| matches!(r.kind, RowKind::AnnotationFold { .. }))
            .expect("annotation fold marker");
        assert_eq!(fold.kind, RowKind::AnnotationFold { count: 2 });
        // 10 section lines - 2 annotations + 1 marker = 9 rows.
        assert_eq!(rows.len(), 9);

        let shown = build(&buffer, &idx, &HashSet::new(), true);
        assert_eq!(shown.len(), 10, "t shows the annotations inline");
    }

    #[test]
    fn folding_a_block_hides_its_lines() {
        let (buffer, idx) = setup(TRACE);
        let mut folded = HashSet::new();
        folded.insert((0usize, 0usize, 0usize, 0u32)); // block b0 in phase 0
        let rows = build(&buffer, &idx, &folded, false);
        let fold = rows
            .iter()
            .find(|r| matches!(r.kind, RowKind::BlockFold { .. }))
            .expect("block fold marker");
        assert_eq!(
            fold.kind,
            RowKind::BlockFold {
                block: 0,
                hidden: 2
            }
        );
        // b0's two node lines are gone; b1's lines are still there.
        assert!(rows.iter().all(|r| r.line != 3 && r.line != 4));
        assert!(rows.iter().any(|r| r.line == 6));
    }

    #[test]
    fn row_of_line_skips_hidden_lines() {
        let (buffer, idx) = setup(TRACE);
        let mut folded = HashSet::new();
        folded.insert((0usize, 0usize, 0usize, 0u32));
        let rows = build(&buffer, &idx, &folded, false);
        let parsed = Arc::new(crate::parse::maglev::parse_compilation(
            &buffer,
            &idx.compilations[0],
        ));
        let vm = ViewModel::Modeled {
            rows,
            parsed,
            comp: 0,
        };
        assert!(vm.row_of_line(3).is_none(), "line inside the fold");
        assert!(vm.row_of_line(6).is_some(), "line in the open block");
    }

    #[test]
    fn plain_model_is_a_window() {
        let vm = ViewModel::Plain { range: 10..20 };
        assert_eq!(vm.len(), 10);
        assert_eq!(vm.line_at(0), Some(10));
        assert_eq!(vm.line_at(9), Some(19));
        assert_eq!(vm.line_at(10), None);
        assert_eq!(vm.row_of_line(15), Some(5));
        assert_eq!(vm.row_of_line(25), None);
    }
}
