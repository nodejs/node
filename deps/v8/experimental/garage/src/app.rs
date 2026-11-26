// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Application state and the event loop (Phases 3–4).
//!
//! Two panes — sidebar and viewport — with a focus model instead of modes:
//! `h`/`l` move focus, movement keys act on the focused pane, and every
//! binding resolves through the remappable [`Keymap`]. The one real
//! mode is the input line (`/` search, `f` filter), which captures keys until
//! Enter or Esc.
//!
//! The viewport cursor is a **display row**, not a buffer line: folding (4.2)
//! and annotation collapsing (4.8) mean the visible rows are not a contiguous
//! line range. [`App::view_model`] builds the row list for the selected
//! section — and, for compilations, triggers the lazy parse whose
//! results drive styling, def-use highlighting, and node jumps.

use std::collections::{HashMap, HashSet};
use std::sync::mpsc::Receiver;

use crate::tracing;
use crate::tui::term::event::{
    Event as TermEvent, KeyCode, KeyEvent, KeyEventKind, KeyModifiers, MouseButton, MouseEvent,
    MouseEventKind,
};
use anyhow::Result;
use regex::Regex;

use crate::config::{Action, Keymap};
use crate::event::Event;
use crate::index::TraceIndex;
use crate::model::{Addr, BlockId, EventKind, IRNode, LineInfo, NodeId, PhaseKind, SCHEDULE_ONLY};
use crate::parse::ParseCache;
use crate::parse::maglev::line_text;
use crate::source::{LogBuffer, LogSource, SourceEvent};
use crate::terminal::TerminalGuard;
use crate::view::{FoldKey, Lens, MODEL_LIMIT, RowKind, ViewModel, model_rows};

/// How many queued events one loop iteration absorbs before redrawing.
/// A fast producer can deliver chunks faster than the terminal repaints;
/// draining a bounded batch keeps it at one redraw per batch while never
/// starving keystrokes.
const MAX_EVENTS_PER_DRAW: usize = 512;

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum SourceState {
    Loading,
    Complete,
    Failed(String),
}

pub struct Source {
    pub label: String,
    pub buffer: LogBuffer,
    pub state: SourceState,
    pub index: TraceIndex,
    pub parses: ParseCache,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Pane {
    Sidebar,
    Viewport,
    /// The JS source alignment pane, when open.
    Source,
}

/// Screen-space rectangle of a pane, recorded at render time — the frame is
/// the only thing that knows the layout, and mouse events arrive in screen
/// coordinates.
#[derive(Debug, Clone, Copy, Default)]
pub struct PaneRect {
    pub x: u16,
    pub y: u16,
    pub width: u16,
    pub height: u16,
}

impl PaneRect {
    pub fn contains(&self, x: u16, y: u16) -> bool {
        x >= self.x && x < self.x + self.width && y >= self.y && y < self.y + self.height
    }
}

/// One sidebar row. Rebuilt per frame from the index — identity is positional
/// within the *current* row list, which is why selection is clamped after
/// every rebuild rather than stored as an id.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Row {
    /// Grouped mode: one header per function (keyed on SFI — names can be
    /// empty or duplicated, spike-findings.md §11).
    Function {
        sfi: Addr,
        name_comp: usize,
        count: usize,
    },
    Compilation(usize),
    Phase {
        comp: usize,
        phase: usize,
    },
    Raw(usize),
    /// Timeline mode: one row per [`TimelineEvent`].
    Event(usize),
}

/// What the input line is collecting.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Prompt {
    Search,
    Filter,
    /// `E`: filename to export the current view to.
    Export,
    /// `:` — the command palette.
    Command,
}

/// The palette's command table, in completion order. The second column is the
/// help/candidate hint. Kept as data so completion, dispatch, and the help
/// modal can never drift apart.
pub const COMMANDS: &[(&str, &str)] = &[
    ("checks", "lens: guard nodes, with a count"),
    ("clear", "clear the lens and timeline filter"),
    ("copy", "copy the visible section"),
    ("deopts", "timeline, deopt events only"),
    ("export", "export the view to <file>"),
    ("function", "filter the sidebar to <regex>"),
    ("megamorphic", "lens: megamorphic feedback and ICs"),
    ("phi", "lens: control/phi backbone"),
    ("q", "quit"),
    ("quit", "quit"),
    ("spill", "lens: regalloc spills and reloads"),
    ("timeline", "toggle the timeline view"),
];

#[derive(Debug)]
pub struct InputLine {
    pub prompt: Prompt,
    pub buffer: String,
}

/// What a `u`/`i` cycle is anchored to: a graph node, or one of the
/// bytecode-listing identities — a bytecode offset, a
/// feedback-vector slot, a constant-pool entry.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum Anchor {
    Node(NodeId),
    Offset(u32),
    Slot(u32),
    Pool(u32),
    /// A basic block, anchored at its header — `u` cycles its predecessors.
    Block(BlockId),
    /// A schedule-only row with block targets (`Gap`/parenless schedule
    /// rows): no node id to anchor on, so the buffer line is the identity.
    Line(usize),
    /// A disassembly row, anchored on its code offset.
    Insn(u32),
}

impl std::fmt::Display for Anchor {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Anchor::Node(n) => write!(f, "n{n}"),
            Anchor::Offset(o) => write!(f, "@{o}"),
            Anchor::Slot(s) => write!(f, "FBV[{s}]"),
            Anchor::Pool(p) => write!(f, "pool[{p}]"),
            Anchor::Block(b) => write!(f, "b{b}"),
            Anchor::Line(l) => write!(f, "L{}", l + 1),
            Anchor::Insn(o) => write!(f, "+0x{o:x}"),
        }
    }
}

/// `u`/`i` cycling state: anchored to the identity the cycle started from, so
/// jumping to a consumer does not re-anchor the cycle on the consumer.
#[derive(Debug, Clone)]
struct Cycle {
    anchor: Anchor,
    /// Which action built the cycle (`input`/`ref`/`target`/`consumer`/…):
    /// a repeat of the *same* action continues it; a different action
    /// re-derives from the row the cursor landed on. Without this, `u`
    /// after `i` asked about the jump's anchor instead of the row under
    /// the cursor (found dogfooding the disassembly nav).
    what: &'static str,
    targets: Vec<usize>, // buffer lines
    at: usize,
}

/// One jump-history entry. Stores the sidebar *mode* alongside the position:
/// a deopt→graph jump leaves the timeline, and Ctrl+O must land back on the
/// event row, not interpret a timeline row index against the compilation list.
#[derive(Debug, Clone, Copy)]
struct Jump {
    timeline: bool,
    selected: usize,
    line: usize,
}

/// Split orientation. `Vertical` = side by side (the diff shape),
/// `Horizontal` = stacked.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SplitDir {
    Vertical,
    Horizontal,
}

/// One viewport's complete navigation state. The *active* pane lives in the
/// App's flat fields (`selected`/`cursor`/`top`/`scroll_x`) so every existing
/// keybinding keeps operating on "the current view"; the inactive pane is a
/// parked snapshot, swapped in by [`App::activate_pane`] — the same pattern
/// the timeline uses for its selection.
#[derive(Debug, Clone, Copy, Default)]
pub struct ViewState {
    pub selected: usize,
    pub cursor: usize,
    pub top: usize,
    pub scroll_x: usize,
}

/// Where the alignment pane's source text came from, per row: a character
/// offset in the script (`NNN S>` bytecode markers) or a function definition
/// anchor line (SFI context lines). Chars are precise; anchors are the
/// fallback grain.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SrcRef {
    Char(u32),
    FnLine(u32),
}

/// buffer line → source ref, for one compilation.
pub struct SourceMap {
    pub of_row: HashMap<usize, SrcRef>,
}

/// The JS source alignment pane: the resolved `.js` file, or
/// the compilation's embedded `Raw source` block as the fallback.
pub struct SourcePane {
    pub title: String,
    pub lines: Vec<String>,
    /// 0-based script line of `lines[0]` (0 for a resolved file).
    pub base_line: u32,
    /// Script-absolute char offset of each line start; `u32::MAX` base means
    /// char refs cannot be mapped (embedded text without `source_position`).
    line_starts: Vec<u32>,
    chars_usable: bool,
    pub top: usize,
    pub cursor: usize,
    /// (source, compilation) the pane was built for.
    pub key: (usize, usize),
}

impl SourcePane {
    /// The pane line a source ref lands on, if it is inside the pane's text.
    pub fn display_line(&self, r: &SrcRef) -> Option<usize> {
        let d = match r {
            SrcRef::Char(c) => {
                if !self.chars_usable || self.line_starts.first().is_none_or(|f| c < f) {
                    return None;
                }
                self.line_starts
                    .partition_point(|s| *s <= *c)
                    .saturating_sub(1)
            }
            SrcRef::FnLine(l) => (*l).checked_sub(self.base_line)? as usize,
        };
        (d < self.lines.len()).then_some(d)
    }
}

pub struct App {
    pub sources: Vec<Source>,
    pub active: usize,
    pub focus: Pane,
    /// Sidebar mode: chronological ⇄ grouped-by-function.
    pub grouped: bool,
    pub selected: usize,
    pub sidebar_scroll: usize,
    /// Expanded compilations, per (source, compilation).
    expanded: HashSet<(usize, usize)>,
    /// Expanded function groups, per (source, sfi).
    expanded_groups: HashSet<(usize, u64)>,
    /// Viewport cursor: a display-row index into the current view model.
    pub cursor: usize,
    /// First visible display row.
    pub top: usize,
    pub scroll_x: usize,
    pub wrap: bool,
    pub follow: bool,
    pub help: bool,
    pub quit: bool,
    pub status: String,
    pub viewport_height: usize,
    pub sidebar_height: usize,
    pub keys: Keymap,
    pub function_filter: Option<Regex>,

    // Phase 4 state.
    /// Folded basic blocks; persisted across navigation.
    folded_blocks: HashSet<FoldKey>,
    /// `t`: show trace annotations inline instead of folded.
    pub show_annotations: bool,
    /// The committed search pattern.
    pub search: Option<Regex>,
    /// Sidebar quick filter.
    pub sidebar_filter: Option<Regex>,
    /// Active input line, if any.
    pub input: Option<InputLine>,
    /// Jump history for Ctrl+O / Ctrl+I.
    jumps: Vec<Jump>,
    jump_at: usize,
    cycle: Option<Cycle>,

    // Phase 6 state.
    /// Timeline mode: the sidebar lists events instead of sections.
    pub timeline: bool,
    /// The other mode's selection, restored when toggling back.
    timeline_selected: usize,
    /// `:deopts`: timeline narrowed to deopt events.
    pub timeline_deopts_only: bool,
    /// Active semantic lens.
    pub lens: Option<Lens>,

    // Phase 7 state.
    /// Open split, if any.
    pub split: Option<SplitDir>,
    /// The inactive pane's parked view state (meaningful only when split).
    pub other_view: ViewState,
    /// Which physical pane (0 = left/top, 1 = right/bottom) the flat fields
    /// currently drive.
    pub active_pane: usize,
    /// Phase diff mode; requires a split.
    pub diff: bool,
    /// Aligned model cache: recomputed only when a side changes.
    diff_cache: Option<(DiffKey, std::sync::Arc<crate::diff::DiffModel>)>,
    /// Dual-run diff: `((src, comp, phase), (src, comp, phase))`
    /// pinned by `D`, overriding the pane-derived pair while set.
    dual: Option<(DualSide, DualSide)>,

    /// Sidebar visibility (`b`): hiding it hands its columns to the
    /// viewport — wide graphs are the whole point. A one-column strip stays
    /// behind as the visual clue that it exists.
    pub sidebar_visible: bool,

    /// The inlining-decisions panel (`I`): selected entry when open.
    pub inline_panel: Option<usize>,
    /// The JS source alignment pane (`S`), and its map cache.
    pub source_pane: Option<SourcePane>,
    src_cache: Option<((usize, usize), std::sync::Arc<SourceMap>)>,

    /// Pane geometry from the last frame, for mouse routing.
    pub sidebar_rect: PaneRect,
    pub viewport_rect: PaneRect,
    /// The second pane's rect; zero-sized (never matching) without a split.
    pub viewport2_rect: PaneRect,
    pub source_rect: PaneRect,
}

/// `(source, comp, phase, section end line)` per side: any content change on
/// either side changes the key.
type DiffKey = ((usize, usize, usize, usize), (usize, usize, usize, usize));
/// One dual-run diff side: `(source, compilation, phase)`.
type DualSide = (usize, usize, usize);

impl App {
    pub fn new(sources: &[LogSource], function_filter: Option<Regex>, keys: Keymap) -> Self {
        Self {
            sources: sources
                .iter()
                .map(|s| Source {
                    label: s.label(),
                    buffer: LogBuffer::new(),
                    state: SourceState::Loading,
                    index: TraceIndex::new(function_filter.clone()),
                    parses: ParseCache::default(),
                })
                .collect(),
            active: 0,
            focus: Pane::Sidebar,
            grouped: false,
            selected: 0,
            sidebar_scroll: 0,
            expanded: HashSet::new(),
            expanded_groups: HashSet::new(),
            cursor: 0,
            top: 0,
            scroll_x: 0,
            wrap: false,
            // Following by default only makes sense for a live stream; files
            // open at the top.
            follow: matches!(sources.first(), Some(LogSource::Stdin)),
            help: false,
            quit: false,
            status: "? help".to_string(),
            viewport_height: 1,
            sidebar_height: 1,
            keys,
            function_filter,
            folded_blocks: HashSet::new(),
            show_annotations: false,
            search: None,
            sidebar_filter: None,
            input: None,
            jumps: Vec::new(),
            jump_at: 0,
            cycle: None,
            timeline: false,
            timeline_selected: 0,
            timeline_deopts_only: false,
            lens: None,
            split: None,
            other_view: ViewState::default(),
            active_pane: 0,
            diff: false,
            diff_cache: None,
            dual: None,
            sidebar_visible: true,
            sidebar_rect: PaneRect::default(),
            viewport_rect: PaneRect::default(),
            viewport2_rect: PaneRect::default(),
            source_rect: PaneRect::default(),
            inline_panel: None,
            source_pane: None,
            src_cache: None,
        }
    }

    pub fn active_source(&self) -> &Source {
        &self.sources[self.active]
    }

    pub fn compilation_expanded(&self, comp: usize) -> bool {
        self.expanded.contains(&(self.active, comp))
    }

    pub fn group_expanded(&self, sfi: u64) -> bool {
        self.expanded_groups.contains(&(self.active, sfi))
    }

    fn filter_matches(&self, text: &str) -> bool {
        match &self.sidebar_filter {
            Some(re) => re.is_match(text),
            None => true,
        }
    }

    /// The sidebar rows for the active source, in display order.
    pub fn rows(&self) -> Vec<Row> {
        self.rows_in(self.timeline)
    }

    /// The row list of either sidebar mode — parked selections (the other
    /// mode's, the split's inactive pane's) re-locate through this.
    fn rows_in(&self, timeline: bool) -> Vec<Row> {
        let source = &self.sources[self.active];
        let idx = &source.index;
        let mut rows = Vec::new();

        if timeline {
            // Timeline mode: one row per event, ordinal order —
            // which is stream order, because the indexer records them in one
            // pass.
            for (i, e) in idx.events.iter().enumerate() {
                if self.timeline_deopts_only && !matches!(e.kind, EventKind::DeoptBegin { .. }) {
                    continue;
                }
                rows.push(Row::Event(i));
            }
            return rows;
        }

        let comp_visible = |i: usize| {
            let c = &idx.compilations[i];
            !c.filtered_out && self.filter_matches(c.display_name())
        };

        if self.grouped {
            // First-seen order of SFIs; compilations nested under each,
            // collapsed by default — this is what scales to thousands of
            // compilations (PLAN §5.1). Sections whose identity never
            // arrived (sfi = 0: opt-code dumps, unidentified TF sections)
            // are NOT grouped — keying them all on the placeholder merged
            // unrelated functions into one bogus group (found in review) —
            // they list individually after the groups.
            let mut seen: Vec<Addr> = Vec::new();
            let mut members_of: std::collections::HashMap<Addr, Vec<usize>> =
                std::collections::HashMap::new();
            let mut ungrouped: Vec<usize> = Vec::new();
            for (i, c) in idx.compilations.iter().enumerate() {
                if comp_visible(i) {
                    if c.key.sfi == Addr(0) {
                        ungrouped.push(i);
                        continue;
                    }
                    let members = members_of.entry(c.key.sfi).or_default();
                    if members.is_empty() {
                        seen.push(c.key.sfi);
                    }
                    members.push(i);
                }
            }
            for sfi in seen {
                let members = members_of.remove(&sfi).unwrap_or_default();
                rows.push(Row::Function {
                    sfi,
                    name_comp: members[0],
                    count: members.len(),
                });
                if self.expanded_groups.contains(&(self.active, sfi.0)) {
                    for i in members {
                        self.push_compilation_rows(&mut rows, i);
                    }
                }
            }
            for i in ungrouped {
                self.push_compilation_rows(&mut rows, i);
            }
            for (i, r) in idx.raw.iter().enumerate() {
                if self.filter_matches(&r.label) {
                    rows.push(Row::Raw(i));
                }
            }
        } else {
            // Chronological: compilations and raw sections merged by start
            // line, which is simply their interleaved order in the stream.
            let mut comps = idx.compilations.iter().enumerate().peekable();
            let mut raws = idx.raw.iter().enumerate().peekable();
            loop {
                let next_comp = comps.peek().map(|(_, c)| c.lines.start);
                let next_raw = raws.peek().map(|(_, r)| r.lines.start);
                match (next_comp, next_raw) {
                    (Some(c), Some(r)) if c <= r => {
                        let (i, _) = comps.next().unwrap();
                        if comp_visible(i) {
                            self.push_compilation_rows(&mut rows, i);
                        }
                    }
                    (Some(_), Some(_)) | (None, Some(_)) => {
                        let (i, r) = raws.next().unwrap();
                        if self.filter_matches(&r.label) {
                            rows.push(Row::Raw(i));
                        }
                    }
                    (Some(_), None) => {
                        let (i, _) = comps.next().unwrap();
                        if comp_visible(i) {
                            self.push_compilation_rows(&mut rows, i);
                        }
                    }
                    (None, None) => break,
                }
            }
        }
        rows
    }

    fn push_compilation_rows(&self, rows: &mut Vec<Row>, comp: usize) {
        rows.push(Row::Compilation(comp));
        if self.expanded.contains(&(self.active, comp)) {
            let n = self.sources[self.active].index.compilations[comp]
                .phases
                .len();
            for phase in 0..n {
                rows.push(Row::Phase { comp, phase });
            }
        }
    }

    /// Builds the display rows for the selected section. Compilations get the
    /// modeled (parsed, foldable) form; raw sections and oversized sections
    /// stay a plain O(1) window.
    pub fn view_model(&mut self) -> ViewModel {
        self.view_model_for(self.selected)
    }

    /// The view model for an arbitrary sidebar row — the split's inactive
    /// pane renders through this without disturbing the active view.
    pub fn view_model_for(&mut self, selected: usize) -> ViewModel {
        let rows = self.rows();
        let (comp, only_phase) = match rows.get(selected) {
            Some(Row::Compilation(i)) => (*i, None),
            Some(Row::Phase { comp, phase }) => (*comp, Some(*phase)),
            Some(Row::Function { name_comp, .. }) => (*name_comp, None),
            Some(Row::Raw(i)) => {
                let range = self.sources[self.active].index.raw[*i].lines.clone();
                return ViewModel::Plain { range };
            }
            Some(Row::Event(i)) => {
                return ViewModel::Plain {
                    range: self.event_view_range(*i),
                };
            }
            None => {
                return ViewModel::Plain {
                    range: 0..self.sources[self.active].buffer.line_count(),
                };
            }
        };

        let source = &mut self.sources[self.active];
        let section = &source.index.compilations[comp];
        if section.lines.len() > MODEL_LIMIT {
            // Oversized: no row modeling, but a phase selection must still
            // show *that phase*, not the whole compilation.
            let range = match only_phase {
                Some(p) => section.phases[p].lines.clone(),
                None => section.lines.clone(),
            };
            return ViewModel::Plain { range };
        }
        let parsed = source.parses.get_or_parse(&source.buffer, section, comp);
        let only_lines = match only_phase {
            Some(p) => section.phases[p].lines.clone(),
            None => section.lines.clone(),
        };
        let rows = model_rows(
            &parsed,
            section,
            &only_lines,
            self.active,
            comp,
            &self.folded_blocks,
            self.show_annotations,
            &source.buffer,
            self.lens,
        );
        ViewModel::Modeled { rows, parsed, comp }
    }

    /// What the viewport shows for a selected timeline event: for a deopt,
    /// the whole bailout block (through the matching `[bailout end]` — under
    /// `--trace-deopt-verbose` that is the frame-unwinding dump);
    /// for anything else, the enclosing section as context. Ranges are capped
    /// to the enclosing section so an interleaved stream cannot leak another
    /// section into the panel.
    fn event_view_range(&self, ev: usize) -> std::ops::Range<usize> {
        let idx = &self.sources[self.active].index;
        let Some(event) = idx.events.get(ev) else {
            return 0..self.sources[self.active].buffer.line_count();
        };
        // A still-streaming source's open tail is not in the index yet
        // (`close_all` only runs at the next boundary or EOF), so an event in
        // the tail has no enclosing section — but it does have every line
        // indexed so far, and capping at the event line itself would shrink
        // the 6.5 panel to one line exactly when the user is watching a live
        // deopt (found in review).
        let section = self
            .enclosing_section_range(event.line)
            .unwrap_or_else(|| event.line..idx.lines_indexed().max(event.line + 1));

        if matches!(event.kind, EventKind::DeoptBegin { .. }) {
            let end = idx.events[ev + 1..]
                .iter()
                .find(|e| matches!(e.kind, EventKind::DeoptEnd { .. }))
                .map(|e| e.line + 1)
                .unwrap_or(section.end)
                .min(section.end);
            return event.line..end.max(event.line + 1);
        }
        section
    }

    /// The section (raw or compilation) containing a buffer line. Both lists
    /// are sorted by start line and the sections partition the file, so in
    /// each list only the last section starting at or before the line can
    /// contain it.
    fn enclosing_section_range(&self, line: usize) -> Option<std::ops::Range<usize>> {
        let idx = &self.sources[self.active].index;
        let at = idx.raw.partition_point(|r| r.lines.start <= line);
        if let Some(r) = at.checked_sub(1).map(|i| &idx.raw[i])
            && r.lines.contains(&line)
        {
            return Some(r.lines.clone());
        }
        let at = idx.compilations.partition_point(|c| c.lines.start <= line);
        if let Some(c) = at.checked_sub(1).map(|i| &idx.compilations[i])
            && c.lines.contains(&line)
        {
            return Some(c.lines.clone());
        }
        None
    }

    /// The node defined on the cursor row, for tracking and jumps.
    /// Schedule-only lines (GapMove & co) share the [`SCHEDULE_ONLY`]
    /// sentinel and define nothing — treating them as "a node" once
    /// highlighted every gap move on screen at the same time.
    pub fn cursor_node(&self, vm: &ViewModel) -> Option<IRNode> {
        node_at(vm, self.cursor)
    }

    /// The node under the shared diff cursor, on one side of the diff.
    pub fn diff_cursor_node(&self, model: &crate::diff::DiffModel, pane: usize) -> Option<IRNode> {
        let row = model.rows.get(self.cursor)?;
        let (side, idx) = if pane == 0 {
            (&model.left, row.left?)
        } else {
            (&model.right, row.right?)
        };
        let (p, i) = side.rows.get(idx)?.info?;
        match side.parsed.phases.get(p)?.infos.get(i)? {
            LineInfo::Node(node) if node.id != SCHEDULE_ONLY => Some(node.clone()),
            _ => None,
        }
    }

    pub fn handle(&mut self, event: Event) {
        match event {
            Event::Input(TermEvent::Key(key)) => self.handle_key(key),
            Event::Input(TermEvent::Mouse(mouse)) => self.handle_mouse(mouse),
            // Resize needs no state change; the redraw that follows is enough.
            Event::Input(_) => {}
            Event::Source(e) => self.handle_source(e),
            Event::InputClosed => {
                tracing::warn!("terminal input closed; quitting");
                self.quit = true;
            }
        }
    }

    /// Mouse: the wheel scrolls the pane under the pointer (three rows, like
    /// vim's default), a left click focuses the pane and places the
    /// selection/cursor — which is what lights up def-use highlighting on the
    /// clicked node. Coordinates route through the pane rects the last frame
    /// recorded. With wrap on, one logical row can occupy several screen
    /// rows, so a click lands on the row *starting* at that screen line —
    /// approximate, and documented as such.
    fn handle_mouse(&mut self, mouse: MouseEvent) {
        if self.help {
            self.help = false;
            return;
        }
        // The input line owns the interaction; a stray click must not move
        // the cursor out from under an incremental search.
        if self.input.is_some() {
            return;
        }

        // Viewport hits carry the pane index; a hit on the inactive pane
        // activates it first (focus follows the pointer).
        let at_pane = if self.sidebar_rect.contains(mouse.column, mouse.row) {
            Some((Pane::Sidebar, 0))
        } else if self.viewport_rect.contains(mouse.column, mouse.row) {
            Some((Pane::Viewport, 0))
        } else if self.source_pane.is_some() && self.source_rect.contains(mouse.column, mouse.row) {
            Some((Pane::Source, 0))
        } else if self.viewport2_rect.contains(mouse.column, mouse.row) {
            Some((Pane::Viewport, 1))
        } else {
            None
        };

        match mouse.kind {
            MouseEventKind::ScrollUp | MouseEventKind::ScrollDown => {
                let Some((pane, viewport)) = at_pane else {
                    return;
                };
                if pane == Pane::Sidebar && !self.sidebar_visible {
                    return;
                }
                self.focus = pane;
                if pane == Pane::Viewport {
                    self.activate_pane(viewport);
                }
                let delta = if mouse.kind == MouseEventKind::ScrollUp {
                    -3
                } else {
                    3
                };
                self.move_by(delta);
            }
            MouseEventKind::Down(MouseButton::Left) => match at_pane {
                Some((Pane::Sidebar, _)) => {
                    // A click on the collapsed strip reopens the sidebar.
                    if !self.sidebar_visible {
                        self.sidebar_visible = true;
                        self.focus = Pane::Sidebar;
                        return;
                    }
                    self.focus = Pane::Sidebar;
                    let offset = (mouse.row - self.sidebar_rect.y) as usize;
                    let rows = self.rows().len();
                    if rows == 0 {
                        return;
                    }
                    let target = (self.sidebar_scroll + offset).min(rows - 1);
                    if target != self.selected {
                        self.selected = target;
                        self.reset_view();
                    }
                }
                Some((Pane::Source, _)) => {
                    self.focus = Pane::Source;
                    // The pane renders a title row; content starts one below.
                    let offset = (mouse.row - self.source_rect.y) as usize;
                    if let Some(pane) = &mut self.source_pane {
                        let offset = offset.saturating_sub(1);
                        if !pane.lines.is_empty() {
                            pane.cursor = (pane.top + offset).min(pane.lines.len() - 1);
                        }
                    }
                }
                Some((Pane::Viewport, viewport)) => {
                    self.focus = Pane::Viewport;
                    self.activate_pane(viewport);
                    let rect = if viewport == 0 {
                        self.viewport_rect
                    } else {
                        self.viewport2_rect
                    };
                    // Split panes render a title row (Borders::TOP), so the
                    // first content row is one below the rect's origin
                    // (found in review: clicks landed one row off).
                    let offset = (mouse.row - rect.y) as usize;
                    let offset = if self.split.is_some() {
                        offset.saturating_sub(1)
                    } else {
                        offset
                    };
                    let len = self.viewport_len();
                    if len == 0 {
                        return;
                    }
                    self.cursor = (self.top + offset).min(len - 1);
                    self.follow = self.follow && self.cursor + 1 == len;
                    self.cycle = None;
                }
                None => {}
            },
            _ => {}
        }
    }

    fn handle_key(&mut self, key: KeyEvent) {
        // Windows reports press and release; without this every key acts twice.
        if key.kind == KeyEventKind::Release {
            return;
        }

        if self.input.is_some() {
            self.handle_input_key(key);
            return;
        }

        // Before the keymap lookup, so that genuinely *any* key closes the
        // modal — unbound keys included, and `q` closes rather than quits.
        if self.help {
            self.help = false;
            return;
        }

        // The inlining panel owns navigation while open; any
        // other key closes it.
        if let Some(sel) = self.inline_panel {
            let decisions = self.inline_decisions();
            match self.keys.lookup(&key) {
                Some(Action::Down) => {
                    self.inline_panel = Some((sel + 1).min(decisions.len().saturating_sub(1)));
                }
                Some(Action::Up) => {
                    self.inline_panel = Some(sel.saturating_sub(1));
                }
                Some(Action::Select) => {
                    self.inline_panel = None;
                    if let Some(d) = decisions.get(sel) {
                        self.push_history();
                        self.goto_line(d.line);
                        self.status = format!(
                            "{} {} — {}",
                            if d.inlined { "INLINE" } else { "SKIP" },
                            d.callee,
                            d.reason
                        );
                    }
                }
                _ => self.inline_panel = None,
            }
            return;
        }

        let Some(action) = self.keys.lookup(&key) else {
            return;
        };

        match action {
            Action::Quit => self.quit = true,
            Action::Back => {
                if self.focus == Pane::Source {
                    self.focus = Pane::Viewport;
                } else if self.focus == Pane::Viewport {
                    self.focus = Pane::Sidebar;
                }
            }
            Action::Help => self.help = true,
            Action::FocusSidebar => {
                // `h` at the left edge also brings a hidden sidebar back —
                // the natural "where did it go" gesture.
                self.sidebar_visible = true;
                self.focus = Pane::Sidebar;
            }
            Action::FocusViewport => {
                // `l` walks right: sidebar → viewport → source pane.
                if self.focus == Pane::Viewport && self.source_pane.is_some() {
                    self.focus = Pane::Source;
                    self.sync_source_cursor();
                } else {
                    self.focus = Pane::Viewport;
                }
            }
            Action::NextSource => {
                if self.sources.len() > 1 {
                    self.active = (self.active + 1) % self.sources.len();
                    self.selected = 0;
                    self.sidebar_scroll = 0;
                    self.reset_view();
                }
            }
            Action::Select => {
                // Enter in the viewport follows control-flow refs (a
                // branch's targets, cycling); in the source pane it jumps
                // to the rows aligned with the cursor line; in the sidebar
                // it keeps its expand/collapse/jump role.
                match self.focus {
                    Pane::Viewport => self.follow_targets(),
                    Pane::Source => self.source_enter(),
                    Pane::Sidebar => self.toggle_expand(),
                }
            }
            Action::ToggleGrouping => {
                self.grouped = !self.grouped;
                self.selected = 0;
                self.sidebar_scroll = 0;
                // Follow means "pin to the newest section", and grouped mode
                // does not order rows by recency — the pin has no honest
                // target there, so switching modes breaks follow.
                self.follow = false;
                self.status = format!(
                    "sidebar: {}",
                    if self.grouped {
                        "grouped by function"
                    } else {
                        "chronological"
                    }
                );
            }
            Action::ToggleWrap => {
                self.wrap = !self.wrap;
                self.scroll_x = 0;
                self.status = format!("wrap {}", if self.wrap { "on" } else { "off" });
            }
            Action::ToggleFollow => {
                self.follow = !self.follow;
                if self.follow {
                    let len = self.viewport_len();
                    self.cursor = len.saturating_sub(1);
                }
                self.status = format!("follow {}", if self.follow { "on" } else { "off" });
            }
            Action::ScrollLeft => self.scroll_x = self.scroll_x.saturating_sub(8),
            Action::ScrollRight => self.scroll_x += 8,

            Action::Up => self.move_by(-1),
            Action::Down => self.move_by(1),
            Action::HalfPageDown => {
                if self.focus == Pane::Sidebar {
                    self.focus = Pane::Viewport;
                }
                self.move_by(self.half_page());
            }
            Action::HalfPageUp => {
                if self.focus == Pane::Sidebar {
                    self.focus = Pane::Viewport;
                }
                self.move_by(-self.half_page());
            }
            Action::PageDown => {
                if self.focus == Pane::Sidebar {
                    self.focus = Pane::Viewport;
                }
                self.move_by(self.page());
            }
            Action::PageUp => {
                if self.focus == Pane::Sidebar {
                    self.focus = Pane::Viewport;
                }
                self.move_by(-self.page());
            }
            Action::Top => self.jump_top(),
            Action::Bottom => self.jump_bottom(),

            Action::FoldBlock => self.fold_block(),
            Action::ToggleAnnotations => {
                self.show_annotations = !self.show_annotations;
                self.status = format!(
                    "annotations {}",
                    if self.show_annotations {
                        "inline"
                    } else {
                        "folded"
                    }
                );
            }
            Action::Search => {
                self.input = Some(InputLine {
                    prompt: Prompt::Search,
                    buffer: String::new(),
                });
            }
            Action::Filter => {
                self.input = Some(InputLine {
                    prompt: Prompt::Filter,
                    buffer: self
                        .sidebar_filter
                        .as_ref()
                        .map(|r| r.as_str().to_string())
                        .unwrap_or_default(),
                });
            }
            Action::SearchNext => self.search_step(1),
            Action::SearchPrev => self.search_step(-1),
            Action::JumpToInput => self.jump_to_input(),
            Action::CycleConsumers => self.cycle_consumers(),
            Action::JumpBack => self.history_step(-1),
            Action::JumpForward => self.history_step(1),

            Action::Yank => self.yank_line(),
            Action::YankSection => self.yank_section(),
            Action::Export => {
                self.input = Some(InputLine {
                    prompt: Prompt::Export,
                    buffer: String::new(),
                });
            }
            Action::CommandPalette => {
                self.input = Some(InputLine {
                    prompt: Prompt::Command,
                    buffer: String::new(),
                });
            }
            Action::ToggleTimeline => self.toggle_timeline(),
            Action::SplitVertical => self.toggle_split(SplitDir::Vertical),
            Action::SplitHorizontal => self.toggle_split(SplitDir::Horizontal),
            Action::OtherPane => {
                if self.source_pane.is_some() {
                    if self.focus == Pane::Source {
                        self.focus = Pane::Viewport;
                    } else {
                        self.focus = Pane::Source;
                        self.sync_source_cursor();
                    }
                } else if self.split.is_some() {
                    self.activate_pane(1 - self.active_pane);
                    self.focus = Pane::Viewport;
                } else {
                    self.status = "no split — v or s opens one".to_string();
                }
            }
            Action::Diff => self.toggle_diff(),
            Action::FoldAllBlocks => self.fold_all_blocks(),
            Action::PrevBlock => self.step_block(-1),
            Action::NextBlock => self.step_block(1),
            Action::ToggleSidebar => self.toggle_sidebar(),
            Action::ToggleSourcePane => self.toggle_source_pane(),
            Action::InliningPanel => self.toggle_inline_panel(),
            Action::DualDiff => self.dual_diff_run(),
        }
    }

    /// The current compilation's inlining decisions, parsed on demand.
    pub fn inline_decisions(&mut self) -> Vec<crate::model::InlineDecision> {
        let Some(comp) = self.current_comp() else {
            return Vec::new();
        };
        let source = &mut self.sources[self.active];
        let section = source.index.compilations[comp].clone();
        let parsed = source.parses.get_or_parse(&source.buffer, &section, comp);
        parsed.inline_decisions.clone()
    }

    /// `I`: the inlining-decisions panel for the current compilation
    /// — every ⚡ INLINE / ❌ SKIP the trace recorded, with its
    /// reason; Enter jumps to the decision line.
    fn toggle_inline_panel(&mut self) {
        if self.inline_panel.take().is_some() {
            return;
        }
        if self.current_comp().is_none() {
            self.status = "inlining decisions need a compilation in view".to_string();
            return;
        }
        let n = self.inline_decisions().len();
        if n == 0 {
            self.status =
                "no inlining decisions here (run with --trace-maglev-inlining)".to_string();
            return;
        }
        self.inline_panel = Some(0);
        self.status = format!("{n} inlining decisions — Enter jumps, Esc closes");
    }

    /// `b`: show / hide the sidebar. Hidden, its columns go to the viewport
    /// and a one-column `▸` strip remains as the clue; `h` or a click on the
    /// strip also bring it back.
    fn toggle_sidebar(&mut self) {
        self.sidebar_visible = !self.sidebar_visible;
        if self.sidebar_visible {
            self.status = "sidebar shown".to_string();
        } else {
            if self.focus == Pane::Sidebar {
                self.focus = Pane::Viewport;
            }
            let hint = self
                .keys
                .chord_hint(crate::config::Action::ToggleSidebar)
                .unwrap_or_else(|| "toggle-sidebar".to_string());
            self.status = format!("sidebar hidden ({hint} or h shows it)");
        }
    }

    fn handle_input_key(&mut self, key: KeyEvent) {
        let Some(input) = &mut self.input else { return };
        match key.code {
            KeyCode::Esc => {
                let prompt = input.prompt;
                self.input = None;
                if prompt == Prompt::Search {
                    self.search = None;
                }
            }
            KeyCode::Enter => {
                let input = self.input.take().expect("checked above");
                match input.prompt {
                    Prompt::Search => {
                        // The pattern was compiled incrementally; Enter just
                        // commits and jumps to the first match.
                        if self.search.is_some() {
                            self.search_step(1);
                        }
                    }
                    Prompt::Filter => {
                        self.sidebar_filter = if input.buffer.is_empty() {
                            None
                        } else {
                            match Regex::new(&input.buffer) {
                                Ok(re) => Some(re),
                                Err(_) => {
                                    self.status = format!("bad filter regex: {}", input.buffer);
                                    None
                                }
                            }
                        };
                        self.selected = 0;
                        self.reset_view();
                    }
                    Prompt::Export => {
                        if input.buffer.is_empty() {
                            self.status = "export cancelled (empty filename)".to_string();
                        } else {
                            self.export_to(std::path::PathBuf::from(&input.buffer));
                        }
                    }
                    Prompt::Command => self.run_command(input.buffer.trim().to_string()),
                }
            }
            KeyCode::Tab if input.prompt == Prompt::Command => {
                // Completion: extend to the longest common prefix
                // of the matching commands; unique match completes fully. The
                // candidate list itself is rendered live in the status line.
                let typed = input.buffer.clone();
                if typed.contains(' ') {
                    return; // arguments have no completion
                }
                let matches: Vec<&str> = COMMANDS
                    .iter()
                    .map(|(name, _)| *name)
                    .filter(|name| name.starts_with(&typed))
                    .collect();
                match matches.as_slice() {
                    [] => {}
                    [only] => input.buffer = only.to_string(),
                    several => {
                        let mut prefix = several[0].to_string();
                        for name in &several[1..] {
                            let common = prefix
                                .chars()
                                .zip(name.chars())
                                .take_while(|(a, b)| a == b)
                                .count();
                            // Truncate at a char boundary, not a char count:
                            // the command table is ASCII today, but truncate()
                            // takes bytes and would panic the day it isn't.
                            let bytes = prefix
                                .char_indices()
                                .nth(common)
                                .map_or(prefix.len(), |(b, _)| b);
                            prefix.truncate(bytes);
                        }
                        input.buffer = prefix;
                    }
                }
            }
            KeyCode::Backspace => {
                input.buffer.pop();
                self.recompile_input();
            }
            KeyCode::Char(c) if !key.modifiers.contains(KeyModifiers::CONTROL) => {
                input.buffer.push(c);
                self.recompile_input();
            }
            _ => {}
        }
    }

    /// Incremental search: recompile on each keystroke; invalid patterns are
    /// simply "no match yet" while typing.
    fn recompile_input(&mut self) {
        let Some(input) = &self.input else { return };
        if input.prompt == Prompt::Search {
            self.search = Regex::new(&input.buffer)
                .ok()
                .filter(|_| !input.buffer.is_empty());
        }
    }

    fn page(&self) -> isize {
        match self.focus {
            Pane::Sidebar => self.sidebar_height.saturating_sub(1).max(1) as isize,
            Pane::Viewport | Pane::Source => self.viewport_height.saturating_sub(2).max(1) as isize,
        }
    }

    fn half_page(&self) -> isize {
        (self.page() / 2).max(1)
    }

    fn move_by(&mut self, delta: isize) {
        match self.focus {
            Pane::Sidebar => {
                let rows = self.rows().len();
                if rows == 0 {
                    return;
                }
                let target = self.selected as isize + delta;
                self.selected = target.clamp(0, rows as isize - 1) as usize;
                self.reset_view();
            }
            Pane::Viewport => {
                let len = self.viewport_len();
                if len == 0 {
                    return;
                }
                let target = self.cursor as isize + delta;
                self.cursor = target.clamp(0, len as isize - 1) as usize;
                self.follow = self.follow && self.cursor + 1 == len;
                self.cycle = None;
            }
            Pane::Source => {
                if let Some(pane) = &mut self.source_pane {
                    let len = pane.lines.len();
                    if len == 0 {
                        return;
                    }
                    let target = pane.cursor as isize + delta;
                    pane.cursor = target.clamp(0, len as isize - 1) as usize;
                }
            }
        }
    }

    fn jump_top(&mut self) {
        match self.focus {
            Pane::Sidebar => {
                self.selected = 0;
                self.reset_view();
            }
            Pane::Viewport => {
                self.cursor = 0;
                self.follow = false;
                self.cycle = None;
            }
            Pane::Source => {
                if let Some(pane) = &mut self.source_pane {
                    pane.cursor = 0;
                }
            }
        }
    }

    fn jump_bottom(&mut self) {
        match self.focus {
            Pane::Sidebar => {
                self.selected = self.rows().len().saturating_sub(1);
                self.reset_view();
            }
            Pane::Viewport => {
                self.cursor = self.viewport_len().saturating_sub(1);
                self.cycle = None;
            }
            Pane::Source => {
                if let Some(pane) = &mut self.source_pane {
                    pane.cursor = pane.lines.len().saturating_sub(1);
                }
            }
        }
    }

    fn toggle_expand(&mut self) {
        let rows = self.rows();
        match rows.get(self.selected) {
            Some(Row::Compilation(i)) => {
                let key = (self.active, *i);
                if !self.expanded.remove(&key) {
                    self.expanded.insert(key);
                }
            }
            Some(Row::Function { sfi, .. }) => {
                let key = (self.active, sfi.0);
                if !self.expanded_groups.remove(&key) {
                    self.expanded_groups.insert(key);
                }
            }
            Some(Row::Phase { .. } | Row::Raw(_)) => self.focus = Pane::Viewport,
            Some(Row::Event(i)) => self.event_jump(*i),
            None => {}
        }
    }

    /// Selection moved: pin the cursor into the new section. Deliberate
    /// sidebar navigation also breaks follow — otherwise the next chunk from a
    /// live stream yanks the selection straight back to the newest section.
    fn reset_view(&mut self) {
        self.cursor = 0;
        self.top = 0;
        self.scroll_x = 0;
        self.follow = false;
        self.cycle = None;
        self.sync_event_cursor();
    }

    /// In timeline mode the view is the event's context, and the cursor
    /// belongs on the event's own line, not on the top of that context.
    fn sync_event_cursor(&mut self) {
        if !self.timeline {
            return;
        }
        let rows = self.rows();
        let Some(Row::Event(i)) = rows.get(self.selected) else {
            return;
        };
        let line = self.sources[self.active].index.events[*i].line;
        let vm = self.view_model();
        if let Some(row) = row_showing(&vm, line) {
            self.cursor = row;
        }
    }

    // -----------------------------------------------------------------------
    // Folding
    // -----------------------------------------------------------------------

    /// `Space`: fold/unfold the block containing the cursor.
    fn fold_block(&mut self) {
        if self.diff {
            self.status = "folding is off in diff view (d to leave)".to_string();
            return;
        }
        let vm = self.view_model();
        let Some(row) = vm.row(self.cursor) else {
            return;
        };
        let (Some((p, i)), Some(parsed), ViewModel::Modeled { comp, .. }) =
            (row.info, vm.parsed(), &vm)
        else {
            return;
        };
        let comp = *comp;

        // Walk back from the cursor to the enclosing block header.
        let infos = &parsed.phases[p].infos;
        let header = (0..=i.min(infos.len().saturating_sub(1)))
            .rev()
            .find_map(|j| match infos.get(j) {
                Some(LineInfo::BlockHeader { block }) => Some((j, *block)),
                _ => None,
            });
        let Some((header_idx, block)) = header else {
            self.status = "no block here".to_string();
            return;
        };

        let key = (self.active, comp, p, block);
        let folded = if self.folded_blocks.remove(&key) {
            false
        } else {
            self.folded_blocks.insert(key);
            true
        };

        // Land the cursor on the header row of the toggled block.
        let header_line = self.sources[self.active].index.compilations[comp].phases[p]
            .lines
            .start
            + header_idx;
        let vm = self.view_model();
        if let Some(row) = row_showing(&vm, header_line) {
            self.cursor = row;
        }
        self.status = format!("b{block} {}", if folded { "folded" } else { "unfolded" });
    }

    /// `z`: fold every block in the current view, or — when they already
    /// all are — unfold them all. Scoped to what is on screen: a phase
    /// selection folds only that phase's blocks.
    fn fold_all_blocks(&mut self) {
        if self.diff {
            self.status = "folding is off in diff view (d to leave)".to_string();
            return;
        }
        let vm = self.view_model();
        let ViewModel::Modeled { comp, .. } = &vm else {
            self.status = "no blocks here (raw view)".to_string();
            return;
        };
        let comp = *comp;

        // Every block with a header row in the current view — fold markers
        // included, so a partially folded view still collects everything.
        let mut keys: HashSet<FoldKey> = HashSet::new();
        for at in 0..vm.len() {
            let Some(row) = vm.row(at) else { break };
            let block = match row.kind {
                RowKind::BlockFold { block, .. } => Some(block),
                RowKind::Text => match row.info.and_then(|(p, i)| {
                    vm.parsed()
                        .and_then(|parsed| parsed.phases.get(p))
                        .and_then(|phase| phase.infos.get(i))
                        .map(|info| (p, info))
                }) {
                    Some((p, LineInfo::BlockHeader { block })) => {
                        keys.insert((self.active, comp, p, *block));
                        None
                    }
                    _ => None,
                },
                _ => None,
            };
            // BlockFold rows carry their phase in `info` too.
            if let (Some(block), Some((p, _))) = (block, row.info) {
                keys.insert((self.active, comp, p, block));
            }
        }
        if keys.is_empty() {
            self.status = "no blocks in this view".to_string();
            return;
        }

        let cursor_line = vm.line_at(self.cursor);
        let any_unfolded = keys.iter().any(|k| !self.folded_blocks.contains(k));
        let n = keys.len();
        if any_unfolded {
            self.folded_blocks.extend(keys);
            self.status = format!("{n} blocks folded (z unfolds)");
        } else {
            for k in &keys {
                self.folded_blocks.remove(k);
            }
            self.status = format!("{n} blocks unfolded");
        }

        // Keep the cursor in context: same line if still visible, else the
        // nearest row at or before it (its block's fold marker).
        let vm = self.view_model();
        if let Some(line) = cursor_line {
            self.cursor = match row_showing(&vm, line) {
                Some(row) => row,
                None => (0..vm.len())
                    .take_while(|&r| vm.line_at(r).is_some_and(|l| l <= line))
                    .last()
                    .unwrap_or(0),
            };
        }
        self.cursor = self.cursor.min(vm.len().saturating_sub(1));
        self.cycle = None;
    }

    // -----------------------------------------------------------------------
    // Search
    // -----------------------------------------------------------------------

    /// `n`/`N`: move the cursor to the next/previous display row whose text
    /// matches the search. Works in parsed and raw sections alike, wraps.
    fn search_step(&mut self, direction: isize) {
        let Some(re) = self.search.clone() else {
            self.status = "no search — press /".to_string();
            return;
        };
        self.focus = Pane::Viewport;

        // In diff view, search walks the aligned rows and matches either
        // side — a hit only on the left is still a hit.
        if self.diff
            && let Some(model) = self.diff_model()
        {
            let len = model.rows.len();
            if len == 0 {
                return;
            }
            let left_buffer = &self.sources[model.left.source].buffer;
            let right_buffer = &self.sources[model.right.source].buffer;
            let mut at = self.cursor as isize;
            for _ in 0..len {
                at += direction;
                if at < 0 {
                    at = len as isize - 1;
                } else if at >= len as isize {
                    at = 0;
                }
                let row = &model.rows[at as usize];
                let left_hit = row
                    .left
                    .and_then(|i| model.left.rows.get(i))
                    .is_some_and(|r| line_matches(left_buffer, r.line, &re));
                let right_hit = row
                    .right
                    .and_then(|i| model.right.rows.get(i))
                    .is_some_and(|r| line_matches(right_buffer, r.line, &re));
                if left_hit || right_hit {
                    self.cursor = at as usize;
                    self.cycle = None;
                    self.status = format!("match at diff row {}", at + 1);
                    return;
                }
            }
            self.status = format!("no match for {}", re.as_str());
            return;
        }

        let vm = self.view_model();
        let len = vm.len();
        if len == 0 {
            return;
        }

        let buffer = &self.sources[self.active].buffer;
        let mut at = self.cursor as isize;
        for _ in 0..len {
            at += direction;
            // Wrap around.
            if at < 0 {
                at = len as isize - 1;
            } else if at >= len as isize {
                at = 0;
            }
            let Some(line) = vm.line_at(at as usize) else {
                continue;
            };
            if line_matches(buffer, line, &re) {
                self.cursor = at as usize;
                self.follow = false;
                // A search jump re-targets any i/u cycle to the new cursor.
                self.cycle = None;
                self.status = format!("match at L{}", line + 1);
                return;
            }
        }
        self.status = format!("no match for {}", re.as_str());
    }

    // -----------------------------------------------------------------------
    // Command palette
    // -----------------------------------------------------------------------

    /// Resolves a command name against `COMMANDS` by exact match or unique prefix.
    ///
    /// Returns `Ok(canonical_name)` when the prefix is unique (or an exact match),
    /// `Err(status_message)` when unknown or ambiguous.
    fn resolve_command(&self, name: &str) -> Result<&'static str, String> {
        if name.is_empty() {
            return Ok("");
        }
        if let Some(&(cmd, _)) = COMMANDS.iter().find(|(cmd, _)| *cmd == name) {
            return Ok(cmd);
        }
        let matches: Vec<&str> = COMMANDS
            .iter()
            .map(|(cmd, _)| *cmd)
            .filter(|cmd| cmd.starts_with(name))
            .collect();
        match matches.as_slice() {
            [] => Err(format!("unknown command :{name} (Tab lists commands)")),
            [only] => Ok(only),
            several => Err(format!(
                "ambiguous command :{name} (matches: {})",
                several.join(", ")
            )),
        }
    }

    /// Executes one committed `:` command. The status line is the message
    /// line: every branch ends in a message, including the unknown-command
    /// case.
    fn run_command(&mut self, text: String) {
        let (name, arg) = match text.split_once(char::is_whitespace) {
            Some((n, a)) => (n, a.trim()),
            None => (text.as_str(), ""),
        };
        let canonical = match self.resolve_command(name) {
            Ok(cmd) => cmd,
            Err(msg) => {
                self.status = msg;
                return;
            }
        };
        match canonical {
            "" => self.status = "empty command".to_string(),
            "q" | "quit" => self.quit = true,
            "checks" => self.toggle_lens(Lens::Checks),
            "phi" => self.toggle_lens(Lens::Phi),
            "spill" => self.toggle_lens(Lens::Spill),
            "megamorphic" => self.toggle_lens(Lens::Megamorphic),
            "deopts" => {
                if !self.timeline {
                    self.toggle_timeline();
                }
                self.timeline_deopts_only = true;
                let rows = self.rows();
                self.selected = self.selected.min(rows.len().saturating_sub(1));
                self.reset_view();
                self.status = match rows.len() {
                    0 => "no deopt events in this trace".to_string(),
                    1 => "1 deopt event — Enter jumps to the compilation".to_string(),
                    n => format!("{n} deopt events — Enter jumps to the compilation"),
                };
            }
            "function" => {
                if arg.is_empty() {
                    self.sidebar_filter = None;
                    self.status = "sidebar filter cleared".to_string();
                } else {
                    match Regex::new(arg) {
                        Ok(re) => {
                            self.status = format!("sidebar filtered to /{arg}/");
                            self.sidebar_filter = Some(re);
                            self.selected = 0;
                            self.reset_view();
                        }
                        Err(_) => self.status = format!("bad regex: {arg}"),
                    }
                }
            }
            "copy" => self.yank_section(),
            "export" => {
                if arg.is_empty() {
                    self.input = Some(InputLine {
                        prompt: Prompt::Export,
                        buffer: String::new(),
                    });
                } else {
                    self.export_to(std::path::PathBuf::from(arg));
                }
            }
            "timeline" => self.toggle_timeline(),
            "clear" => {
                self.lens = None;
                if self.timeline_deopts_only {
                    // Widening the timeline shifts every row; keep the same
                    // *event* selected, not the same row index (found in
                    // review).
                    let kept = match self.rows().get(self.selected) {
                        Some(Row::Event(i)) => Some(*i),
                        _ => None,
                    };
                    self.timeline_deopts_only = false;
                    if let Some(event) = kept
                        && let Some(at) = self
                            .rows()
                            .iter()
                            .position(|r| matches!(r, Row::Event(i) if *i == event))
                    {
                        self.selected = at;
                    }
                    self.reset_view();
                }
                self.status = "lens and timeline filter cleared".to_string();
            }
            other => self.status = format!("unknown command :{other} (Tab lists commands)"),
        }
    }

    /// `:checks` & friends: same lens toggles off, different lens replaces.
    /// The match count reported is for the *current* view, which is the count
    /// the user is looking at.
    fn toggle_lens(&mut self, lens: Lens) {
        if self.lens == Some(lens) {
            self.lens = None;
            self.status = format!("lens :{} cleared", lens.name());
            return;
        }
        self.lens = Some(lens);
        self.cursor = 0;
        self.top = 0;
        self.cycle = None;
        let vm = self.view_model();
        let matches = match &vm {
            ViewModel::Modeled { rows, parsed, .. } => {
                let buffer = &self.sources[self.active].buffer;
                rows.iter()
                    .filter(|row| {
                        row.kind == RowKind::Text && {
                            let info = row
                                .info
                                .and_then(|(p, i)| parsed.phases.get(p)?.infos.get(i));
                            lens.matches(info, parsed, buffer, row.line)
                        }
                    })
                    .count()
            }
            ViewModel::Plain { .. } => {
                self.status = format!(
                    "lens :{} set — applies to parsed compilations (this view is raw)",
                    lens.name()
                );
                return;
            }
        };
        self.status = format!(
            "lens :{} — {matches} match{} in this view (:clear resets)",
            lens.name(),
            if matches == 1 { "" } else { "es" }
        );
    }

    // -----------------------------------------------------------------------
    // Timeline & the deopt→graph jump
    // -----------------------------------------------------------------------

    /// `Tab`: compilation list ⇄ timeline. Each mode keeps its own selection.
    fn toggle_timeline(&mut self) {
        // The timeline lives in the sidebar; toggling it while hidden would
        // otherwise change nothing visible.
        self.sidebar_visible = true;
        self.timeline = !self.timeline;
        std::mem::swap(&mut self.selected, &mut self.timeline_selected);
        self.timeline_deopts_only = false;
        self.sidebar_scroll = 0;
        self.focus = Pane::Sidebar;
        if self.split.is_some() {
            // Pane selections are row indices of the *current* sidebar mode;
            // the parked one would silently point at an arbitrary row of the
            // new mode (found in review). Re-sync it and drop the diff.
            self.diff = false;
            self.other_view = ViewState {
                selected: self.selected,
                ..Default::default()
            };
        }
        // Clamp *before* reset_view: sync_event_cursor bails on an
        // out-of-range selection, and the clamp coming later left the panel
        // cursor unsynced (found in review).
        let n = self.rows().len();
        self.selected = self.selected.min(n.saturating_sub(1));
        self.reset_view();
        if self.timeline {
            self.status = match n {
                0 => "timeline — no events (needs --trace-opt / --trace-deopt)".to_string(),
                n => format!("timeline — {n} events · Enter jumps to the compilation"),
            };
        } else {
            self.status = "compilation list".to_string();
        }
    }

    /// Enter on a timeline event: jump to the correlated compilation, per
    /// docs/correlation-keys.md — `(sfi, tier)` + stream position, never a
    /// guess. Unresolvable events stay timeline entries with a message.
    fn event_jump(&mut self, ev: usize) {
        /// Which way an event binds to its dump in stream order.
        enum Bind {
            /// Deopts: the code being torn down was compiled earlier, and a
            /// forward guess would be an invented link (rule 2).
            BackwardOnly,
            /// Marking / compile-start: these *trigger* the dump, which
            /// follows them — the indexer's own OSR binding relies on
            /// exactly that. An earlier instance of the same key is the old,
            /// possibly dead code (found in review: it used to win).
            ForwardFirst,
            /// Compile-done: follows its dump.
            BackwardFirst,
        }

        let event = self.sources[self.active].index.events[ev].clone();
        let (sfi, tier, offset, bind) = match &event.kind {
            EventKind::DeoptBegin {
                sfi: Some(sfi),
                tier,
                bytecode_offset,
                ..
            } => (*sfi, tier.clone(), *bytecode_offset, Bind::BackwardOnly),
            EventKind::Marking {
                sfi: Some(sfi),
                target,
                ..
            }
            | EventKind::CompileStart {
                sfi: Some(sfi),
                target,
                ..
            } => (*sfi, target.clone(), None, Bind::ForwardFirst),
            EventKind::CompileDone {
                sfi: Some(sfi),
                target,
                ..
            } => (*sfi, target.clone(), None, Bind::BackwardFirst),
            _ => {
                self.status =
                    "this event has no SFI to correlate on — timeline entry only".to_string();
                return;
            }
        };

        let comps = &self.sources[self.active].index.compilations;
        let matching = |c: &crate::model::CompilationSection| {
            c.key.sfi == sfi && c.key.tier == tier && !c.filtered_out
        };
        // Most recent matching instance opened before the event line…
        let backward = || {
            comps
                .iter()
                .enumerate()
                .filter(|(_, c)| matching(c) && c.lines.start < event.line)
                .map(|(i, _)| i)
                .next_back()
        };
        // …or the first one after it.
        let forward = || {
            comps
                .iter()
                .enumerate()
                .filter(|(_, c)| matching(c) && c.lines.start >= event.line)
                .map(|(i, _)| i)
                .next()
        };
        let found = match bind {
            Bind::BackwardOnly => backward(),
            Bind::ForwardFirst => forward().or_else(backward),
            Bind::BackwardFirst => backward().or_else(forward),
        };
        let Some(comp) = found else {
            // Distinguish "not in the trace" from "hidden by --function":
            // claiming absence when the dump is right there would be a lie.
            let hidden = comps
                .iter()
                .any(|c| c.key.sfi == sfi && c.key.tier == tier && c.filtered_out);
            self.status = if hidden {
                format!(
                    "the {} graph for sfi {sfi} is hidden by --function — timeline entry only",
                    tier.label()
                )
            } else {
                format!(
                    "unresolved: no {} graph for sfi {sfi} in this trace — timeline entry only",
                    tier.label()
                )
            };
            return;
        };

        self.push_history();
        self.open_compilation(comp);
        {
            let c = &self.sources[self.active].index.compilations[comp];
            self.status = format!(
                "jumped to {} · {} #{}",
                c.display_name(),
                c.key.tier.label(),
                c.key.ordinal
            );
        }
        if let Some(offset) = offset {
            self.jump_to_bytecode_offset(comp, offset);
        }
    }

    /// Leaves the timeline (if on) and selects a compilation row, expanding
    /// its group in grouped mode so the row exists.
    fn open_compilation(&mut self, comp: usize) {
        if self.timeline {
            self.timeline = false;
            std::mem::swap(&mut self.selected, &mut self.timeline_selected);
        }
        let sfi = self.sources[self.active].index.compilations[comp].key.sfi;
        if self.grouped {
            self.expanded_groups.insert((self.active, sfi.0));
        }
        let mut rows = self.rows();
        let mut at = rows
            .iter()
            .position(|r| matches!(r, Row::Compilation(c) if *c == comp));
        if at.is_none() && self.sidebar_filter.is_some() {
            // The target exists but the quick filter hides it. Jumping
            // somewhere else would be a lie; clearing a display filter is the
            // honest resolution, and the status says so.
            self.sidebar_filter = None;
            rows = self.rows();
            at = rows
                .iter()
                .position(|r| matches!(r, Row::Compilation(c) if *c == comp));
            self.status = "sidebar filter cleared to show the jump target".to_string();
        }
        if let Some(at) = at {
            self.selected = at;
        }
        self.reset_view();
        self.focus = Pane::Viewport;
    }

    /// Lands the cursor on `bytecode offset N` inside a compilation, per the
    /// correlation spec: prefer the earliest *graph* phase containing the
    /// offset (later phases drop dead bytecode), then a deopt frame at that
    /// offset, then the bytecode-array dump, then stay at the top.
    fn jump_to_bytecode_offset(&mut self, comp: usize, offset: u32) {
        let section = &self.sources[self.active].index.compilations[comp];
        if section.lines.len() > MODEL_LIMIT {
            self.status = format!("@{offset}: section too large to model — cursor at the top");
            return;
        }
        let section = section.clone();
        let source = &mut self.sources[self.active];
        let parsed = source.parses.get_or_parse(&source.buffer, &section, comp);

        let mut target: Option<(usize, &'static str)> = None;
        let find_bytecode = |p: usize| {
            parsed.phases.get(p).and_then(|phase| {
                phase
                    .infos
                    .iter()
                    .position(|info| matches!(info, LineInfo::Bytecode(bc) if bc.offset == offset))
            })
        };
        for (p, phase_section) in section.phases.iter().enumerate() {
            if !matches!(phase_section.kind, PhaseKind::Graph { .. }) {
                continue;
            }
            if let Some(i) = find_bytecode(p) {
                target = Some((phase_section.lines.start + i, "graph"));
                break;
            }
        }
        if target.is_none() {
            'frames: for (p, phase_section) in section.phases.iter().enumerate() {
                let Some(phase) = parsed.phases.get(p) else {
                    continue;
                };
                for (i, info) in phase.infos.iter().enumerate() {
                    if let LineInfo::Frame { frame, .. } = info
                        && parsed
                            .frames
                            .get(*frame as usize)
                            .is_some_and(|f| f.bytecode_offset == Some(offset as i32))
                    {
                        target = Some((phase_section.lines.start + i, "deopt frame"));
                        break 'frames;
                    }
                }
            }
        }
        if target.is_none() {
            for (p, phase_section) in section.phases.iter().enumerate() {
                if !matches!(phase_section.kind, PhaseKind::Bytecode) {
                    continue;
                }
                if let Some(i) = find_bytecode(p) {
                    target = Some((phase_section.lines.start + i, "bytecode array"));
                    break;
                }
            }
        }

        match target {
            Some((line, where_)) => {
                self.goto_line(line);
                self.status = format!("bytecode offset {offset} ({where_})");
            }
            None => {
                self.status =
                    format!("bytecode offset {offset} not in this dump — cursor at the top");
            }
        }
    }

    // -----------------------------------------------------------------------
    // Splits & phase diff
    // -----------------------------------------------------------------------

    /// Snapshot of the live (active-pane) view fields.
    pub fn view_state(&self) -> ViewState {
        ViewState {
            selected: self.selected,
            cursor: self.cursor,
            top: self.top,
            scroll_x: self.scroll_x,
        }
    }

    pub fn set_view_state(&mut self, v: ViewState) {
        self.selected = v.selected;
        self.cursor = v.cursor;
        self.top = v.top;
        self.scroll_x = v.scroll_x;
    }

    /// The state of a physical pane, active or parked.
    pub fn pane_state(&self, pane: usize) -> ViewState {
        if pane == self.active_pane {
            self.view_state()
        } else {
            self.other_view
        }
    }

    /// Makes `pane` the one the flat fields (and thus every key) drive.
    pub fn activate_pane(&mut self, pane: usize) {
        if self.split.is_none() || pane == self.active_pane {
            return;
        }
        let (cursor, top, scroll_x) = (self.cursor, self.top, self.scroll_x);
        let current = self.view_state();
        let parked = std::mem::replace(&mut self.other_view, current);
        self.set_view_state(parked);
        if self.diff {
            // The diff cursor and scroll range over the *shared* aligned
            // rows; pane switching changes which side yank/status read, not
            // the position.
            self.cursor = cursor;
            self.top = top;
            self.scroll_x = scroll_x;
        }
        self.active_pane = pane;
        self.cycle = None;
    }

    /// `v` / `s`: open a split, flip its orientation, or close it (same key
    /// again). Closing keeps the active pane's view.
    fn toggle_split(&mut self, dir: SplitDir) {
        // Splits and the source pane use the same screen estate.
        if self.source_pane.take().is_some() && self.focus == Pane::Source {
            self.focus = Pane::Viewport;
        }
        match self.split {
            None => {
                self.split = Some(dir);
                self.other_view = self.view_state();
                self.status = format!(
                    "{} split — Ctrl+W switches panes, d diffs two graph phases",
                    if dir == SplitDir::Vertical {
                        "vertical"
                    } else {
                        "horizontal"
                    }
                );
            }
            Some(d) if d == dir => {
                self.split = None;
                self.diff = false;
                self.active_pane = 0;
                // Mouse events drained in the same batch as the close must
                // not route through the stale second-pane rect (found in
                // review).
                self.viewport2_rect = PaneRect::default();
                self.status = "split closed".to_string();
            }
            Some(_) => {
                self.split = Some(dir);
                self.status = "split direction switched".to_string();
            }
        }
    }

    /// Resolves a sidebar row to `(comp, graph phase)` — what the diff can
    /// consume.
    fn pane_phase(&self, selected: usize) -> Option<(usize, usize)> {
        match self.rows().get(selected)? {
            Row::Phase { comp, phase } => {
                let section = &self.sources[self.active].index.compilations[*comp];
                matches!(section.phases.get(*phase)?.kind, PhaseKind::Graph { .. })
                    .then_some((*comp, *phase))
            }
            _ => None,
        }
    }

    /// `d`: toggle phase diff mode. Without a split it picks the
    /// obvious pair itself: on a phase row, the previous graph phase vs this
    /// one; on a compilation row, first vs last graph phase — J1's "what did
    /// the pipeline do" in one keystroke.
    fn toggle_diff(&mut self) {
        if self.diff {
            self.diff = false;
            self.dual = None;
            self.status = "diff off".to_string();
            return;
        }

        if self.split.is_none() && !self.auto_split_for_diff() {
            return;
        }

        let left = self.pane_phase(self.pane_state(0).selected);
        let right = self.pane_phase(self.pane_state(1).selected);
        if left.is_none() || right.is_none() {
            self.status =
                "diff needs a graph phase selected in each pane (Enter expands a compilation)"
                    .to_string();
            return;
        }
        self.diff = true;
        self.cursor = 0;
        self.top = 0;
        self.cycle = None;
        self.follow = false;
        self.focus = Pane::Viewport;
        if let Some(model) = self.diff_model() {
            self.status = format!("diff: {}", model.summary.describe());
        }
    }

    /// Picks the diff pair when `d` is pressed without a split. Returns false
    /// (with a status message) when the selection offers no pair.
    fn auto_split_for_diff(&mut self) -> bool {
        let rows = self.rows();
        let graph_phases = |comp: usize| -> Vec<usize> {
            self.sources[self.active].index.compilations[comp]
                .phases
                .iter()
                .enumerate()
                .filter(|(_, p)| matches!(p.kind, PhaseKind::Graph { .. }))
                .map(|(i, _)| i)
                .collect()
        };
        let (comp, left_phase, right_phase) = match rows.get(self.selected) {
            Some(Row::Phase { comp, phase }) => {
                let graphs = graph_phases(*comp);
                let Some(at) = graphs.iter().position(|p| p == phase) else {
                    self.status = "diff needs a graph phase (this is a dump section)".to_string();
                    return false;
                };
                if at == 0 {
                    self.status = "no earlier graph phase to diff against".to_string();
                    return false;
                }
                (*comp, graphs[at - 1], *phase)
            }
            Some(Row::Compilation(comp))
            | Some(Row::Function {
                name_comp: comp, ..
            }) => {
                let graphs = graph_phases(*comp);
                let (Some(&first), Some(&last)) = (graphs.first(), graphs.last()) else {
                    self.status = "no graph phases in this compilation".to_string();
                    return false;
                };
                if first == last {
                    self.status = "only one graph phase — nothing to diff against".to_string();
                    return false;
                }
                self.expanded.insert((self.active, *comp));
                // Grouped mode gates phase rows behind the *group* too
                // (found in review: d on a collapsed group found no rows).
                let sfi = self.sources[self.active].index.compilations[*comp].key.sfi;
                if self.grouped {
                    self.expanded_groups.insert((self.active, sfi.0));
                }
                (*comp, first, last)
            }
            _ => {
                self.status = "diff works on compilations and their graph phases".to_string();
                return false;
            }
        };

        let rows = self.rows();
        let row_of = |phase: usize| {
            rows.iter().position(
                |r| matches!(r, Row::Phase { comp: c, phase: p } if *c == comp && *p == phase),
            )
        };
        let (Some(left_row), Some(right_row)) = (row_of(left_phase), row_of(right_phase)) else {
            self.status = "could not locate the phase rows".to_string();
            return false;
        };

        // Left pane parked on the earlier phase, live fields = right pane.
        self.split = Some(SplitDir::Vertical);
        self.other_view = ViewState {
            selected: left_row,
            ..Default::default()
        };
        self.selected = right_row;
        self.active_pane = 1;
        true
    }

    /// The aligned diff of the two panes — or of the dual-run pair `D`
    /// pinned — cached until either side changes.
    pub fn diff_model(&mut self) -> Option<std::sync::Arc<crate::diff::DiffModel>> {
        if !self.diff {
            return None;
        }
        let ((lsrc, lc, lp), (rsrc, rc, rp)) = match self.dual {
            Some(pair) => pair,
            None => {
                let (lc, lp) = self.pane_phase(self.pane_state(0).selected)?;
                let (rc, rp) = self.pane_phase(self.pane_state(1).selected)?;
                ((self.active, lc, lp), (self.active, rc, rp))
            }
        };
        let key: DiffKey = (
            (
                lsrc,
                lc,
                lp,
                self.sources[lsrc].index.compilations[lc].lines.end,
            ),
            (
                rsrc,
                rc,
                rp,
                self.sources[rsrc].index.compilations[rc].lines.end,
            ),
        );
        if let Some((cached_key, model)) = &self.diff_cache
            && *cached_key == key
        {
            return Some(std::sync::Arc::clone(model));
        }

        // Oversized compilations are never modeled (5.1 guard); the diff
        // path inherits that.
        let too_big = |src: usize, c: usize| {
            self.sources[src].index.compilations[c].lines.len() > MODEL_LIMIT
        };
        if too_big(lsrc, lc) || too_big(rsrc, rc) {
            self.status = "section too large to diff".to_string();
            return None;
        }

        let left_section = self.sources[lsrc].index.compilations[lc].clone();
        let right_section = self.sources[rsrc].index.compilations[rc].clone();
        let left_parsed = {
            let source = &mut self.sources[lsrc];
            source
                .parses
                .get_or_parse(&source.buffer, &left_section, lc)
        };
        let right_parsed = {
            let source = &mut self.sources[rsrc];
            source
                .parses
                .get_or_parse(&source.buffer, &right_section, rc)
        };
        let mut model = crate::diff::diff_phases(
            &crate::diff::SideInput {
                buffer: &self.sources[lsrc].buffer,
                parsed: &left_parsed,
                section: &left_section,
                phase: lp,
                comp_id: (lsrc, lc),
            },
            &crate::diff::SideInput {
                buffer: &self.sources[rsrc].buffer,
                parsed: &right_parsed,
                section: &right_section,
                phase: rp,
                comp_id: (rsrc, rc),
            },
        );
        if self.dual.is_some() {
            let title = |src: usize, section: &crate::model::CompilationSection, p: usize| {
                format!(
                    "{} · {} · {}",
                    self.sources[src].label,
                    section.display_name(),
                    section.phases[p].name
                )
            };
            model.titles = Some((
                title(lsrc, &left_section, lp),
                title(rsrc, &right_section, rp),
            ));
        }
        let model = std::sync::Arc::new(model);
        self.diff_cache = Some((key, std::sync::Arc::clone(&model)));
        Some(model)
    }

    /// `D`: diff this function against its compilation in the *other* run
    /// Matching is by name and tier (SFI addresses are not
    /// comparable across runs; the node-identity diff already matches
    /// cross-compilation by structural hash). On a phase row the same-named
    /// phase is preferred on the other side; otherwise both sides use their
    /// last graph phase.
    fn dual_diff_run(&mut self) {
        if self.diff {
            self.diff = false;
            self.dual = None;
            self.status = "diff off".to_string();
            return;
        }
        if self.sources.len() < 2 {
            self.status = "dual-run diff needs two sources (garage a.log b.log)".to_string();
            return;
        }
        let Some(comp) = self.current_comp() else {
            self.status = "dual-run diff needs a compilation in view".to_string();
            return;
        };
        let other = (self.active + 1) % self.sources.len();
        let last_graph = |section: &crate::model::CompilationSection| {
            section
                .phases
                .iter()
                .rposition(|p| matches!(p.kind, PhaseKind::Graph { .. }))
        };
        let section = self.sources[self.active].index.compilations[comp].clone();
        // Phase-row selection wins; a compilation row means "the end state".
        let phase = self
            .pane_phase(self.selected)
            .filter(|(c, _)| *c == comp)
            .map(|(_, p)| p)
            .or_else(|| last_graph(&section));
        let Some(phase) = phase else {
            self.status = "no graph phase to diff in this compilation".to_string();
            return;
        };

        let index = &self.sources[other].index;
        let matched = index
            .compilations
            .iter()
            .enumerate()
            .filter(|(_, c)| {
                c.name == section.name && c.key.tier == section.key.tier && !c.filtered_out
            })
            .map(|(i, _)| i)
            .max_by_key(|&i| {
                // Prefer the same ordinal; otherwise the last instance.
                let c = &index.compilations[i];
                (c.key.ordinal == section.key.ordinal, c.key.ordinal)
            });
        let Some(other_comp) = matched else {
            self.status = format!(
                "{} has no {} {} compilation",
                self.sources[other].label,
                section.display_name(),
                section.key.tier.label()
            );
            return;
        };
        let other_section = index.compilations[other_comp].clone();
        let phase_name = &section.phases[phase].name;
        let other_phase = other_section
            .phases
            .iter()
            .position(|p| matches!(p.kind, PhaseKind::Graph { .. }) && p.name == *phase_name)
            .or_else(|| last_graph(&other_section));
        let Some(other_phase) = other_phase else {
            self.status = format!(
                "{}'s {} has no graph phase to diff",
                self.sources[other].label,
                other_section.display_name()
            );
            return;
        };

        self.source_pane = None;
        self.split = Some(SplitDir::Vertical);
        self.diff = true;
        self.dual = Some(((other, other_comp, other_phase), (self.active, comp, phase)));
        self.cursor = 0;
        self.top = 0;
        self.status = format!(
            "dual-run diff: {} ⇄ {} — d closes",
            self.sources[other].label, self.sources[self.active].label
        );
    }

    /// Rows the viewport cursor ranges over — the aligned diff when it is
    /// on, the normal view model otherwise.
    fn viewport_len(&mut self) -> usize {
        if self.diff
            && let Some(model) = self.diff_model()
        {
            return model.rows.len();
        }
        self.view_model().len()
    }

    // -----------------------------------------------------------------------
    // Node jumps
    // -----------------------------------------------------------------------

    /// The `u`/`i` anchor under the cursor: a node, or — in bytecode
    /// listings — a bytecode row, a feedback-vector slot, or a constant-pool
    /// entry.
    fn cursor_anchor(&self, vm: &ViewModel) -> Option<Anchor> {
        if let Some(node) = self.cursor_node(vm) {
            return Some(Anchor::Node(node.id));
        }
        let (p, i) = vm.row(self.cursor)?.info?;
        match vm.parsed()?.phases.get(p)?.infos.get(i)? {
            LineInfo::Bytecode(bc) => Some(Anchor::Offset(bc.offset)),
            LineInfo::FeedbackSlot { index, .. } => Some(Anchor::Slot(*index)),
            LineInfo::PoolEntry { index, .. } => Some(Anchor::Pool(*index)),
            LineInfo::BlockHeader { block } => Some(Anchor::Block(*block)),
            LineInfo::Disasm { offset, .. } => Some(Anchor::Insn(*offset)),
            _ => None,
        }
    }

    /// `i`: jump to the definition of the cursor node's inputs, cycling
    /// through them on repeat. Like `u`, the cycle stays anchored to the node
    /// it started from — the first jump moves the cursor onto an input, and
    /// without the anchor a second `i` would ask for *that* node's inputs
    /// instead of the next input of the original.
    ///
    /// On a bytecode row the "inputs" are its operand refs: jump-target
    /// offsets, `FBV[N]` feedback slots, `[N:…]` constant-pool entries.
    fn jump_to_input(&mut self) {
        if self.diff {
            self.status = "node jumps are off in diff view (d to leave)".to_string();
            return;
        }
        let vm = self.view_model();
        let Some(row) = vm.row(self.cursor) else {
            return;
        };
        let Some((p, _)) = row.info else { return };
        let Some(parsed) = vm.parsed() else { return };

        let continued = self
            .cycle
            .as_ref()
            .filter(|c| matches!(c.what, "input" | "ref") && !matches!(c.anchor, Anchor::Insn(_)))
            .map(|c| c.anchor);
        let anchor = match (continued, self.cursor_anchor(&vm)) {
            (Some(anchor), _) => anchor,
            (None, Some(anchor)) => anchor,
            (None, None) => {
                self.status = "no node under cursor".to_string();
                return;
            }
        };

        let (targets, what) = match anchor {
            Anchor::Node(id) => {
                let phase = &parsed.phases[p];
                // The anchor's inputs come from its definition line, which
                // may not be the cursor line any more.
                let Some(anchor_node) = phase
                    .defs
                    .get(&id)
                    .and_then(|&idx| phase.infos.get(idx as usize))
                    .and_then(|info| match info {
                        LineInfo::Node(node) => Some(node),
                        _ => None,
                    })
                else {
                    self.status = format!("n{id} is not defined in this phase");
                    return;
                };
                let phase_start = self.phase_start(&vm, p);
                let targets: Vec<usize> = anchor_node
                    .inputs
                    .iter()
                    .filter_map(|r| phase.defs.get(&r.node))
                    .map(|&info_idx| phase_start + info_idx as usize)
                    .collect();
                if targets.is_empty() {
                    self.status = format!("n{id} has no inputs defined in this phase");
                    return;
                }
                (targets, "input")
            }
            Anchor::Offset(offset) => {
                let targets = self.bytecode_ref_targets(&vm, p, offset);
                if targets.is_empty() {
                    self.status = format!("@{offset} has no resolvable refs here");
                    return;
                }
                (targets, "ref")
            }
            Anchor::Block(_) => {
                self.status = format!("{anchor} has no inputs — u cycles its predecessors");
                return;
            }
            Anchor::Insn(o) => {
                // The branch destination of this disassembly row, resolved
                // through either target form.
                let phase = &parsed.phases[p];
                let Some((to_off, to_addr)) = phase.infos.iter().find_map(|info| match info {
                    LineInfo::Disasm {
                        offset,
                        target_offset,
                        target_addr,
                        ..
                    } if *offset == o => Some((*target_offset, *target_addr)),
                    _ => None,
                }) else {
                    self.status = format!("{anchor}: no such row in this phase");
                    return;
                };
                if to_off.is_none() && to_addr.is_none() {
                    self.status = format!("{anchor} has no branch target — u cycles its sources");
                    return;
                }
                let phase_start = self.phase_start(&vm, p);
                let found = phase.infos.iter().position(|info| {
                    matches!(info, LineInfo::Disasm { offset, addr, .. }
                        if to_off == Some(*offset) || to_addr == Some(*addr))
                });
                let Some(idx) = found else {
                    self.status = format!("{anchor}: target is outside this listing");
                    return;
                };
                (vec![phase_start + idx], "target")
            }
            Anchor::Slot(_) | Anchor::Pool(_) | Anchor::Line(_) => {
                self.status = format!("{anchor} has no inputs — u cycles its uses");
                return;
            }
        };
        self.cycle_jump(anchor, targets, what);
    }

    /// `u`: cycle through the consumers of the cursor anchor. For
    /// a node these are its users; for a bytecode offset, the jumps that
    /// target it; for a feedback slot or pool entry, the bytecode rows whose
    /// operands reference it. The cycle stays anchored to where it started.
    fn cycle_consumers(&mut self) {
        if self.diff {
            self.status = "node jumps are off in diff view (d to leave)".to_string();
            return;
        }
        let vm = self.view_model();
        let Some(row) = vm.row(self.cursor) else {
            return;
        };
        let Some((p, _)) = row.info else { return };
        let Some(parsed) = vm.parsed() else { return };

        // Continue an existing `u` cycle even though the cursor moved off
        // the anchor; otherwise `u u` would cycle the first consumer's
        // users. A cycle built by `i`/Enter does not continue here — `u`
        // then asks about the row the jump landed on.
        let continued = self
            .cycle
            .as_ref()
            .filter(|c| matches!(c.what, "consumer" | "predecessor"))
            .map(|c| c.anchor);
        let anchor = match (continued, self.cursor_anchor(&vm)) {
            (Some(anchor), _) => anchor,
            (None, Some(anchor)) => anchor,
            (None, None) => {
                self.status = "no node under cursor".to_string();
                return;
            }
        };

        let phase_start = self.phase_start(&vm, p);
        let targets: Vec<usize> = match anchor {
            Anchor::Node(id) => parsed.phases[p]
                .users
                .get(&id)
                .map(|users| {
                    users
                        .iter()
                        .map(|&info_idx| phase_start + info_idx as usize)
                        .collect()
                })
                .unwrap_or_default(),
            Anchor::Offset(o) => bytecode_rows_where(&parsed.phases[p], phase_start, |bc| {
                bc.targets.iter().any(|(t, _)| *t == o)
            }),
            Anchor::Slot(s) => bytecode_rows_where(&parsed.phases[p], phase_start, |bc| {
                bc.fbv.iter().any(|(n, _)| *n == s)
            }),
            Anchor::Pool(c) => bytecode_rows_where(&parsed.phases[p], phase_start, |bc| {
                bc.pool.iter().any(|(n, _)| *n == c)
            }),
            // Predecessors: every node whose branch/jump targets include
            // this block — the "who jumps here" question at loop headers
            // and merges.
            Anchor::Block(b) => parsed.phases[p]
                .infos
                .iter()
                .enumerate()
                .filter_map(|(idx, info)| match info {
                    LineInfo::Node(node) if node.targets.iter().any(|t| t.block == b) => {
                        Some(phase_start + idx)
                    }
                    _ => None,
                })
                .collect(),
            Anchor::Line(_) => Vec::new(),
            // Branch sources: every row in the listing that jumps here,
            // through either target form.
            Anchor::Insn(o) => {
                let phase = &parsed.phases[p];
                let own_addr = phase.infos.iter().find_map(|info| match info {
                    LineInfo::Disasm { offset, addr, .. } if *offset == o => Some(*addr),
                    _ => None,
                });
                phase
                    .infos
                    .iter()
                    .enumerate()
                    .filter_map(|(idx, info)| match info {
                        LineInfo::Disasm {
                            target_offset,
                            target_addr,
                            ..
                        } if *target_offset == Some(o)
                            || (own_addr.is_some() && *target_addr == own_addr) =>
                        {
                            Some(phase_start + idx)
                        }
                        _ => None,
                    })
                    .collect()
            }
        };
        let what = if matches!(anchor, Anchor::Block(_)) {
            "predecessor"
        } else {
            "consumer"
        };
        if targets.is_empty() {
            self.status = format!("{anchor} has no {what}s in this phase");
            return;
        }
        self.cycle_jump(anchor, targets, what);
    }

    /// Buffer lines of the rows a bytecode row's operands point at: its
    /// jump-target offsets (same phase — every graph phase re-interleaves
    /// the same offsets), then its feedback slots and pool entries, looked
    /// up in the cursor phase first and then the rest of the compilation —
    /// an Ignition dump keeps its constant pool in a separate phase, and a
    /// graph phase's rows resolve their slots in the bytecode dump above.
    fn bytecode_ref_targets(&self, vm: &ViewModel, p: usize, offset: u32) -> Vec<usize> {
        let Some(parsed) = vm.parsed() else {
            return Vec::new();
        };
        let phase = &parsed.phases[p];
        let Some(bc) = phase.infos.iter().find_map(|info| match info {
            LineInfo::Bytecode(bc) if bc.offset == offset => Some(bc),
            _ => None,
        }) else {
            return Vec::new();
        };
        let phase_start = self.phase_start(vm, p);
        let mut out = Vec::new();
        let push = |line: usize, out: &mut Vec<usize>| {
            if !out.contains(&line) {
                out.push(line);
            }
        };
        for (t, _) in &bc.targets {
            let found = phase
                .infos
                .iter()
                .position(|info| matches!(info, LineInfo::Bytecode(row) if row.offset == *t));
            if let Some(idx) = found {
                push(phase_start + idx, &mut out);
            }
        }
        for (s, _) in &bc.fbv {
            if let Some(line) = self.find_in_phases(
                vm,
                p,
                |info| matches!(info, LineInfo::FeedbackSlot { index, .. } if index == s),
            ) {
                push(line, &mut out);
            }
        }
        for (c, _) in &bc.pool {
            if let Some(line) = self.find_in_phases(
                vm,
                p,
                |info| matches!(info, LineInfo::PoolEntry { index, .. } if index == c),
            ) {
                push(line, &mut out);
            }
        }
        out
    }

    /// First line matching `pred`, searching the cursor phase first and then
    /// every other phase of the compilation in order.
    fn find_in_phases(
        &self,
        vm: &ViewModel,
        p: usize,
        pred: impl Fn(&LineInfo) -> bool,
    ) -> Option<usize> {
        let parsed = vm.parsed()?;
        let order = std::iter::once(p).chain((0..parsed.phases.len()).filter(|q| *q != p));
        for q in order {
            if let Some(idx) = parsed.phases[q].infos.iter().position(&pred) {
                return Some(self.phase_start(vm, q) + idx);
            }
        }
        None
    }

    /// Shared cycle mechanics: same anchor advances, new anchor restarts.
    fn cycle_jump(&mut self, anchor: Anchor, targets: Vec<usize>, what: &'static str) {
        let at = match &self.cycle {
            Some(c) if c.anchor == anchor && c.targets == targets => (c.at + 1) % targets.len(),
            _ => 0,
        };
        let line = targets[at];
        self.push_history();
        self.goto_line(line);
        self.cycle = Some(Cycle {
            anchor,
            what,
            targets: targets.clone(),
            at,
        });
        self.status = format!("{anchor} {what} {}/{}", at + 1, targets.len());
    }

    /// `[`/`]`: walk the listing block by block — unconditional, no history,
    /// `j`/`k` at block granularity. Folded blocks count: the cursor lands
    /// on the fold marker.
    fn step_block(&mut self, dir: isize) {
        if self.diff {
            self.status = "block navigation is off in diff view (d to leave)".to_string();
            return;
        }
        let vm = self.view_model();
        let len = vm.len();
        // In a disassembly listing the branch destinations are the block
        // leaders — `[`/`]` stop on them the way they stop on `Block bN`
        // headers in a graph.
        let block_at = |idx: usize| -> Option<String> {
            let row = vm.row(idx)?;
            if let RowKind::BlockFold { block, .. } = row.kind {
                return Some(format!("b{block}"));
            }
            let (p, i) = row.info?;
            match vm.parsed()?.phases.get(p)?.infos.get(i)? {
                LineInfo::BlockHeader { block } => Some(format!("b{block}")),
                LineInfo::Disasm {
                    offset,
                    labeled: true,
                    ..
                } => Some(format!("+0x{offset:x}")),
                _ => None,
            }
        };
        let mut idx = self.cursor as isize;
        loop {
            idx += dir;
            if idx < 0 || idx >= len as isize {
                self.status = format!("no {} block", if dir > 0 { "next" } else { "previous" });
                return;
            }
            if let Some(label) = block_at(idx as usize) {
                self.cursor = idx as usize;
                self.focus = Pane::Viewport;
                self.follow = false;
                self.cycle = None;
                self.status = label;
                return;
            }
        }
    }

    /// Enter in the viewport: follow the control-flow refs of the cursor
    /// node — a `Jump`'s successor, a branch's two targets, a switch's
    /// cases — cycling through the corresponding block headers with the
    /// same anchored-cycle machinery as `i`/`u`.
    fn follow_targets(&mut self) {
        if self.diff {
            self.status = "node jumps are off in diff view (d to leave)".to_string();
            return;
        }
        let vm = self.view_model();
        let Some(row) = vm.row(self.cursor) else {
            return;
        };
        let Some((p, i)) = row.info else {
            self.status = "no block refs under cursor".to_string();
            return;
        };
        let Some(parsed) = vm.parsed() else { return };
        let Some(phase) = parsed.phases.get(p) else {
            return;
        };
        let phase_start = self.phase_start(&vm, p);

        // Repeat-Enter continues the cycle even though the cursor now sits
        // on the target header; other actions' cycles re-derive from the row.
        let anchor = match &self.cycle {
            Some(c)
                if c.what == "target" && matches!(c.anchor, Anchor::Node(_) | Anchor::Line(_)) =>
            {
                c.anchor
            }
            _ => match phase.infos.get(i) {
                Some(LineInfo::Node(node)) if node.id != SCHEDULE_ONLY => Anchor::Node(node.id),
                Some(LineInfo::Node(_)) => Anchor::Line(row.line),
                _ => {
                    self.status = "no block refs under cursor".to_string();
                    return;
                }
            },
        };

        let node = match anchor {
            Anchor::Node(id) => phase
                .defs
                .get(&id)
                .and_then(|&idx| phase.infos.get(idx as usize)),
            Anchor::Line(line) => line
                .checked_sub(phase_start)
                .and_then(|idx| phase.infos.get(idx)),
            _ => None,
        };
        let Some(LineInfo::Node(node)) = node else {
            self.status = "no block refs under cursor".to_string();
            return;
        };

        let mut targets = Vec::new();
        for t in &node.targets {
            let header = phase.infos.iter().position(
                |info| matches!(info, LineInfo::BlockHeader { block } if *block == t.block),
            );
            if let Some(idx) = header {
                let line = phase_start + idx;
                if !targets.contains(&line) {
                    targets.push(line);
                }
            }
        }
        if targets.is_empty() {
            self.status = if node.targets.is_empty() {
                "no block refs under cursor".to_string()
            } else {
                format!("{anchor}: target block has no header in this phase")
            };
            return;
        }
        self.cycle_jump(anchor, targets, "target");
    }

    // -----------------------------------------------------------------------
    // Source alignment
    // -----------------------------------------------------------------------

    /// The compilation the current view shows, if it is a modeled one.
    fn current_comp(&mut self) -> Option<usize> {
        match self.view_model() {
            ViewModel::Modeled { comp, .. } => Some(comp),
            _ => None,
        }
    }

    /// buffer line → source ref for one compilation, cached. Fine anchors
    /// come from `NNN S>` char-offset markers (interleaved graph rows
    /// inherit them through their bytecode offset); SFI context lines
    /// contribute function-definition anchors as the coarse fallback.
    pub fn source_map(&mut self, comp: usize) -> std::sync::Arc<SourceMap> {
        let cache_key = (self.active, comp);
        if let Some((k, m)) = &self.src_cache
            && *k == cache_key
        {
            return m.clone();
        }
        let source = &mut self.sources[self.active];
        let section = source.index.compilations[comp].clone();
        let parsed = source.parses.get_or_parse(&source.buffer, &section, comp);

        let mut pos_of_offset: HashMap<u32, u32> = HashMap::new();
        for phase in &parsed.phases {
            for info in &phase.infos {
                if let LineInfo::Bytecode(bc) = info
                    && let Some(p) = bc.src_pos
                {
                    pos_of_offset.entry(bc.offset).or_insert(p);
                }
            }
        }

        let mut of_row = HashMap::new();
        for (p, phase) in parsed.phases.iter().enumerate() {
            let Some(section_phase) = section.phases.get(p) else {
                break;
            };
            let start = section_phase.lines.start;
            let mut current: Option<SrcRef> = None;
            for (i, info) in phase.infos.iter().enumerate() {
                match info {
                    LineInfo::SfiContext {
                        line, same_script, ..
                    } => {
                        current = (*same_script && *line > 0).then_some(SrcRef::FnLine(*line));
                        if let Some(r) = current {
                            of_row.insert(start + i, r);
                        }
                    }
                    LineInfo::Bytecode(bc) => {
                        if let Some(pos) = bc
                            .src_pos
                            .or_else(|| pos_of_offset.get(&bc.offset).copied())
                        {
                            current = Some(SrcRef::Char(pos));
                        }
                        if let Some(r) = current {
                            of_row.insert(start + i, r);
                        }
                    }
                    LineInfo::Node(_) | LineInfo::Frame { .. } | LineInfo::PhiMove { .. } => {
                        if let Some(r) = current {
                            of_row.insert(start + i, r);
                        }
                    }
                    _ => {}
                }
            }
        }
        let map = std::sync::Arc::new(SourceMap { of_row });
        self.src_cache = Some((cache_key, map.clone()));
        map
    }

    /// 9.1: resolve the compilation's script — the printed path as-is, then
    /// relative to the trace file's directory, then by basename there —
    /// falling back to the dump's own `Raw source` block when no file
    /// resolves, and failing with an honest message when neither exists.
    fn build_source_pane(&mut self, comp: usize) -> Result<SourcePane, String> {
        let key = (self.active, comp);
        let label = self.sources[self.active].label.clone();
        let source = &mut self.sources[self.active];
        let section = source.index.compilations[comp].clone();
        let parsed = source.parses.get_or_parse(&source.buffer, &section, comp);
        let script = parsed.script.clone();
        let script_line = parsed.script_line;
        let source_position = parsed.source_position;

        if let Some(script) = &script {
            let mut candidates = vec![std::path::PathBuf::from(script)];
            let trace_dir = std::path::Path::new(&label).parent();
            if let Some(dir) = trace_dir {
                candidates.push(dir.join(script));
                // d8 often runs one directory up from where the trace was
                // saved (the fixture corpus does exactly this).
                if let Some(up) = dir.parent() {
                    candidates.push(up.join(script));
                }
                if let Some(base) = std::path::Path::new(script).file_name() {
                    candidates.push(dir.join(base));
                }
            }
            for cand in candidates {
                let Ok(text) = std::fs::read_to_string(&cand) else {
                    continue;
                };
                if text.len() > 8 * 1024 * 1024 {
                    continue;
                }
                let mut line_starts = vec![0u32];
                for (i, b) in text.bytes().enumerate() {
                    if b == b'\n' {
                        line_starts.push(i as u32 + 1);
                    }
                }
                line_starts.truncate(text.lines().count().max(1));
                return Ok(SourcePane {
                    title: cand.display().to_string(),
                    lines: text.lines().map(str::to_string).collect(),
                    base_line: 0,
                    line_starts,
                    chars_usable: true,
                    top: 0,
                    cursor: 0,
                    key,
                });
            }
        }

        // Fallback: the compilation's own `Raw source` block — a merged
        // dump's phase, or a standalone dump's preamble.
        let range = section
            .phases
            .iter()
            .find(|p| p.name == "Raw source")
            .map(|p| (p.lines.start + 1)..p.lines.end)
            .or_else(|| {
                (matches!(
                    section.phases.first().map(|p| &p.kind),
                    Some(PhaseKind::Listing)
                ) && !section.preamble.is_empty())
                .then(|| section.preamble.clone())
            })
            .ok_or_else(|| match &script {
                Some(s) => format!("cannot open {s}, and the dump has no Raw source block"),
                None => "no script path and no Raw source block in this dump".to_string(),
            })?;
        let source = &self.sources[self.active];
        let mut lines: Vec<String> = range
            .clone()
            .map(|l| crate::parse::maglev::line_text(&source.buffer, l))
            .collect();
        while lines.last().is_some_and(|l| l.trim().is_empty()) {
            lines.pop();
        }
        // `source_position = N` (from an attached code dump) is the script
        // char offset of the block's first byte — without it, char refs
        // cannot be placed and only function anchors align.
        let (chars_usable, base_char) = match source_position {
            Some(p) => (true, p),
            None => (false, 0),
        };
        let mut line_starts = Vec::with_capacity(lines.len());
        let mut at = base_char;
        for l in &lines {
            line_starts.push(at);
            at += l.len() as u32 + 1;
        }
        Ok(SourcePane {
            title: match &script {
                Some(s) => format!("{s} (embedded raw source; file not found)"),
                None => "Raw source (embedded)".to_string(),
            },
            lines,
            base_line: script_line.unwrap_or(0),
            line_starts,
            chars_usable,
            top: 0,
            cursor: 0,
            key,
        })
    }

    /// `S`: toggle the source alignment pane for the current compilation.
    fn toggle_source_pane(&mut self) {
        if self.source_pane.take().is_some() {
            if self.focus == Pane::Source {
                self.focus = Pane::Viewport;
            }
            self.status = "source pane closed".to_string();
            return;
        }
        if self.diff {
            self.status = "source pane is off in diff view (d to leave)".to_string();
            return;
        }
        let Some(comp) = self.current_comp() else {
            self.status = "source alignment needs a compilation in view".to_string();
            return;
        };
        self.split = None;
        match self.build_source_pane(comp) {
            Ok(pane) => {
                let aligned = self.source_map(comp).of_row.len();
                self.status = format!("{} — {} aligned rows", pane.title, aligned);
                self.source_pane = Some(pane);
            }
            Err(e) => self.status = e,
        }
    }

    /// Focusing the pane parks its cursor on the line aligned with the
    /// trace cursor, when there is one.
    fn sync_source_cursor(&mut self) {
        let Some(pane) = &self.source_pane else {
            return;
        };
        let key = pane.key;
        if key.0 != self.active || self.current_comp() != Some(key.1) {
            return;
        }
        let vm = self.view_model();
        let Some(row) = vm.row(self.cursor) else {
            return;
        };
        let line = row.line;
        let map = self.source_map(key.1);
        let display = map
            .of_row
            .get(&line)
            .and_then(|r| self.source_pane.as_ref().expect("checked").display_line(r));
        if let Some(d) = display {
            self.source_pane.as_mut().expect("checked").cursor = d;
        }
    }

    /// Enter in the source pane: cycle through the trace rows aligned with
    /// the cursor line.
    fn source_enter(&mut self) {
        let Some(pane) = &self.source_pane else {
            return;
        };
        let key = pane.key;
        let cursor = pane.cursor;
        if self.current_comp() != Some(key.1) || key.0 != self.active {
            self.status = "view moved to another compilation — S reopens the pane".to_string();
            return;
        }
        let map = self.source_map(key.1);
        let pane = self.source_pane.as_ref().expect("checked above");
        let display = cursor;
        let mut targets: Vec<usize> = map
            .of_row
            .iter()
            .filter(|(_, r)| pane.display_line(r) == Some(display))
            .map(|(row, _)| *row)
            .collect();
        targets.sort_unstable();
        targets.dedup();
        if targets.is_empty() {
            self.status = format!(
                "no aligned trace rows for source line {}",
                pane.base_line as usize + display + 1
            );
            return;
        }
        self.cycle_jump(Anchor::Line(display), targets, "aligned row");
    }

    /// Buffer line of a phase's first line within the current compilation.
    fn phase_start(&self, vm: &ViewModel, p: usize) -> usize {
        match vm {
            ViewModel::Modeled { comp, .. } => {
                self.sources[self.active].index.compilations[*comp].phases[p]
                    .lines
                    .start
            }
            ViewModel::Plain { .. } => 0,
        }
    }

    /// Moves the cursor to a buffer line, unfolding whatever hides it.
    fn goto_line(&mut self, line: usize) {
        let vm = self.view_model();
        if let Some(row) = row_showing(&vm, line) {
            self.cursor = row;
            self.follow = false;
            return;
        }
        // Hidden: inside a folded block, or in a collapsed annotation run.
        if let ViewModel::Modeled { comp, parsed, .. } = &vm {
            let comp = *comp;
            let section = &self.sources[self.active].index.compilations[comp];
            for (p, phase_section) in section.phases.iter().enumerate() {
                if !phase_section.lines.contains(&line) {
                    continue;
                }
                let idx = line - phase_section.lines.start;
                let infos = &parsed.phases[p].infos;
                if let Some((_, block)) = (0..=idx.min(infos.len().saturating_sub(1)))
                    .rev()
                    .find_map(|j| match infos.get(j) {
                        Some(LineInfo::BlockHeader { block }) => Some((j, *block)),
                        _ => None,
                    })
                {
                    self.folded_blocks.remove(&(self.active, comp, p, block));
                }
            }
        }
        // Unfolding the block may already be enough; only reach for the wider
        // hammers if the target is still hidden.
        let vm = self.view_model();
        if let Some(row) = row_showing(&vm, line) {
            self.cursor = row;
            self.follow = false;
            return;
        }
        // A lens can hide the target (e.g. `i` from a guard to a plain value
        // node under `:checks`); the jump wins over the lens.
        if self.lens.is_some() {
            self.lens = None;
            self.status = "lens cleared by jump".to_string();
            let vm = self.view_model();
            if let Some(row) = row_showing(&vm, line) {
                self.cursor = row;
                self.follow = false;
                return;
            }
        }
        self.show_annotations = true;
        let vm = self.view_model();
        if let Some(row) = row_showing(&vm, line) {
            self.cursor = row;
            self.follow = false;
        }
    }

    // -----------------------------------------------------------------------
    // Jump history
    // -----------------------------------------------------------------------

    /// History entries store **buffer lines**, not display rows: a jump can
    /// unfold blocks or expand annotations, which shifts every later display
    /// row — a stored row index would land Ctrl+O off by the hidden count
    /// (found in review).
    fn push_history(&mut self) {
        let here = self.history_position();
        self.jumps.truncate(self.jump_at);
        self.jumps.push(here);
        self.jump_at = self.jumps.len();
    }

    fn history_position(&mut self) -> Jump {
        let line = self.view_model().line_at(self.cursor).unwrap_or(0);
        Jump {
            timeline: self.timeline,
            selected: self.selected,
            line,
        }
    }

    fn history_step(&mut self, direction: isize) {
        if self.diff {
            // History stores view-model positions; reinterpreting them as
            // aligned diff rows re-pairs the diff and mislands the cursor
            // (found in review). Same rule as folds and node jumps.
            self.status = "jump history is off in diff view (d to leave)".to_string();
            return;
        }
        if direction < 0 {
            if self.jump_at == 0 {
                self.status = "at oldest jump".to_string();
                return;
            }
            // Record where we are so Ctrl+I can come back.
            if self.jump_at == self.jumps.len() {
                let here = self.history_position();
                self.jumps.push(here);
            }
            self.jump_at -= 1;
        } else {
            if self.jump_at + 1 >= self.jumps.len() {
                self.status = "at newest jump".to_string();
                return;
            }
            self.jump_at += 1;
        }
        let jump = self.jumps[self.jump_at];
        // Restore the sidebar mode the entry was recorded in; the stored
        // selection is only meaningful in that mode. Park the selection
        // being left, or the next Tab would interpret this mode's row index
        // against the other mode's list (found in review).
        if jump.timeline != self.timeline {
            self.timeline_selected = self.selected;
            self.timeline = jump.timeline;
        }
        self.selected = jump.selected.min(self.rows().len().saturating_sub(1));
        self.goto_line(jump.line);
        self.follow = false;
        self.cycle = None;
    }

    // -----------------------------------------------------------------------
    // Clipboard and export
    // -----------------------------------------------------------------------

    /// A short description of what the viewport is showing, for export
    /// headers and messages.
    pub fn view_title(&self) -> String {
        self.view_title_for(self.selected)
    }

    /// Title for an arbitrary sidebar row (split panes have two).
    pub fn view_title_for(&self, selected: usize) -> String {
        let source = &self.sources[self.active];
        match self.rows().get(selected) {
            Some(Row::Compilation(i)) | Some(Row::Function { name_comp: i, .. }) => {
                let c = &source.index.compilations[*i];
                format!(
                    "{} · {} #{}",
                    c.display_name(),
                    c.key.tier.label(),
                    c.key.ordinal
                )
            }
            Some(Row::Phase { comp, phase }) => {
                let c = &source.index.compilations[*comp];
                format!(
                    "{} · {} #{} · {}",
                    c.display_name(),
                    c.key.tier.label(),
                    c.key.ordinal,
                    c.phases[*phase].name
                )
            }
            Some(Row::Raw(i)) => format!("raw · {}", source.index.raw[*i].label),
            Some(Row::Event(i)) => {
                let event = &source.index.events[*i];
                format!("event #{} · {}", i + 1, event_summary(&event.kind))
            }
            None => source.label.clone(),
        }
    }

    /// The visible rows of the current view as plain text — folds render as
    /// their markers, exactly like the screen (PLAN §7.9: "current view").
    /// In diff view: the aligned rows with their `+`/`−`/`~`/`→` gutters,
    /// showing the changed side (unified-ish, ready for a ticket).
    fn visible_view_text(&mut self) -> Vec<String> {
        if self.diff
            && let Some(model) = self.diff_model()
        {
            return model
                .rows
                .iter()
                .map(|row| {
                    let side_text = |side: &crate::diff::DiffSide, idx: Option<usize>| {
                        let buffer = &self.sources[side.source].buffer;
                        idx.and_then(|i| side.rows.get(i))
                            .map(|r| line_text(buffer, r.line))
                    };
                    let left = side_text(&model.left, row.left);
                    let right = side_text(&model.right, row.right);
                    let text = right.or(left).unwrap_or_default();
                    format!("{} {text}", diff_gutter(&row.status))
                })
                .collect();
        }
        let vm = self.view_model();
        let buffer = &self.sources[self.active].buffer;
        (0..vm.len())
            .filter_map(|r| vm.row(r))
            .map(|row| match row.kind {
                RowKind::Text => line_text(buffer, row.line),
                RowKind::BlockFold { block, hidden } => {
                    format!("[+] b{block} — {hidden} lines hidden")
                }
                RowKind::AnnotationFold { count } => format!("[+] {count} trace lines"),
            })
            .collect()
    }

    fn yank_line_text(&mut self) -> Option<String> {
        if self.diff {
            let model = self.diff_model()?;
            let row = model.rows.get(self.cursor)?;
            let (side, idx) = if self.active_pane == 0 {
                (&model.left, row.left?)
            } else {
                (&model.right, row.right?)
            };
            let r = side.rows.get(idx)?;
            return Some(line_text(&self.sources[side.source].buffer, r.line));
        }
        let vm = self.view_model();
        let line = vm.line_at(self.cursor)?;
        Some(line_text(&self.sources[self.active].buffer, line))
    }

    fn yank_line(&mut self) {
        let Some(text) = self.yank_line_text() else {
            return;
        };
        match crate::clipboard::copy(&text) {
            Ok(how) => self.status = format!("line {how}"),
            Err(e) => self.status = e.to_string(),
        }
    }

    fn yank_section(&mut self) {
        // Refuse oversized sections *before* materialising anything: the size
        // of a plain window is O(1) from the line-offset index, and without
        // this check a Y on a multi-million-line raw section froze the UI to
        // build a gigabyte of Strings whose only possible fate was the same
        // refusal (found in review). Diff views are always modeled (bounded),
        // so the guard only applies to the plain path.
        let vm = self.view_model();
        if let (false, ViewModel::Plain { range }) = (self.diff, &vm) {
            let bytes = self.sources[self.active].buffer.span_bytes(range.clone());
            let cap = crate::clipboard::max_copy();
            if bytes > cap {
                self.status = format!(
                    "section is {} KB; this clipboard route tops out at {} KB — use export instead",
                    bytes / 1024,
                    cap / 1024
                );
                return;
            }
        }
        let text = self.visible_view_text().join("\n");
        match crate::clipboard::copy(&text) {
            Ok(how) => self.status = format!("section {how}"),
            Err(e) => self.status = e.to_string(),
        }
    }

    fn export_to(&mut self, path: std::path::PathBuf) {
        let title = self.view_title();
        let lines = self.visible_view_text();
        match crate::clipboard::export(&path, &title, &lines) {
            Ok(()) => self.status = format!("exported {} lines to {}", lines.len(), path.display()),
            Err(e) => self.status = e.to_string(),
        }
    }

    fn handle_source(&mut self, event: SourceEvent) {
        let index = match &event {
            SourceEvent::Mapped { source, .. }
            | SourceEvent::Chunk { source, .. }
            | SourceEvent::Eof { source }
            | SourceEvent::Failed { source, .. } => *source,
        };

        if self.sources.get(index).is_none() {
            tracing::error!(index, "event for an unknown source");
            return;
        }
        // Captured before ingest mutates the index, so the selections can be
        // re-located by identity afterwards. The parked ones — the other
        // sidebar mode's selection and the split's inactive pane — shift with
        // the row list exactly like the live one (found in review).
        let prev_row = (index == self.active)
            .then(|| self.rows().get(self.selected).cloned())
            .flatten();
        let prev_parked = (index == self.active)
            .then(|| {
                self.rows_in(!self.timeline)
                    .get(self.timeline_selected)
                    .cloned()
            })
            .flatten();
        let prev_pane = (index == self.active && self.split.is_some())
            .then(|| self.rows().get(self.other_view.selected).cloned())
            .flatten();
        // Alignment data is derived from the parse; stream growth can extend
        // the very compilation it was built from.
        if index == self.active {
            self.src_cache = None;
        }
        let target = &mut self.sources[index];

        match event {
            SourceEvent::Mapped { map, .. } => {
                target.buffer.adopt_map(map);
                target.index.ingest(&target.buffer, false);
            }
            SourceEvent::Chunk { bytes, .. } => {
                target.buffer.append(&bytes);
                target.index.ingest(&target.buffer, false);
            }
            SourceEvent::Eof { .. } => {
                target.buffer.finish();
                target.index.ingest(&target.buffer, true);
                target.state = SourceState::Complete;
            }
            SourceEvent::Failed { error, .. } => {
                // `SourceError` already names the path or "stdin"; prefixing
                // the label here would print it twice.
                target.state = SourceState::Failed(error.clone());
                self.status = error;
            }
        }

        if index == self.active {
            let rows = self.rows();
            // Re-locate the selection by identity, not by index: grouped mode
            // inserts new Function rows *before* the raw rows, so a kept index
            // silently lands on a different item as the stream grows (found in
            // review). Chronological mode is append-only, where this is a
            // no-op.
            if let Some(prev) = prev_row
                && let Some(at) = rows.iter().position(|r| same_row(r, &prev))
            {
                self.selected = at;
            }
            self.selected = self.selected.min(rows.len().saturating_sub(1));

            let other_rows = self.rows_in(!self.timeline);
            if let Some(prev) = prev_parked
                && let Some(at) = other_rows.iter().position(|r| same_row(r, &prev))
            {
                self.timeline_selected = at;
            }
            self.timeline_selected = self
                .timeline_selected
                .min(other_rows.len().saturating_sub(1));

            if let Some(prev) = prev_pane
                && let Some(at) = rows.iter().position(|r| same_row(r, &prev))
            {
                self.other_view.selected = at;
            }
            self.other_view.selected = self.other_view.selected.min(rows.len().saturating_sub(1));
            if self.follow && !self.grouped && !self.timeline {
                // Streaming, chronological: the last row is the newest
                // section (the merge is ordered by start line). The cursor
                // pin to the last display row happens at render time. In
                // grouped mode there is no "newest" row to pin — toggling
                // grouping breaks follow instead.
                self.selected = rows.len().saturating_sub(1);
            }
        }
    }
}

/// Buffer lines of the bytecode rows in one phase matching `pred` — the
/// consumer side of bytecode-listing navigation (jump sources of an offset,
/// referents of a feedback slot or pool entry).
fn bytecode_rows_where(
    phase: &crate::model::ParsedPhase,
    phase_start: usize,
    pred: impl Fn(&crate::model::BytecodeRow) -> bool,
) -> Vec<usize> {
    phase
        .infos
        .iter()
        .enumerate()
        .filter_map(|(idx, info)| match info {
            LineInfo::Bytecode(bc) if pred(bc) => Some(phase_start + idx),
            _ => None,
        })
        .collect()
}

/// The node defined on a display row (see [`App::cursor_node`]).
pub fn node_at(vm: &ViewModel, at: usize) -> Option<IRNode> {
    let row = vm.row(at)?;
    let (p, i) = row.info?;
    let parsed = vm.parsed()?;
    match parsed.phases.get(p)?.infos.get(i)? {
        LineInfo::Node(node) if node.id != SCHEDULE_ONLY => Some(node.clone()),
        _ => None,
    }
}

/// The two-character diff gutter, shared by rendering and export.
pub fn diff_gutter(status: &crate::diff::RowStatus) -> &'static str {
    use crate::diff::RowStatus;
    match status {
        RowStatus::Same => "  ",
        RowStatus::Added => "+ ",
        RowStatus::Deleted => "− ",
        RowStatus::Changed { .. } => "~ ",
        RowStatus::Replaced { .. } => "→ ",
        RowStatus::Moved { .. } => "≈ ",
    }
}

/// One-line human summary of an event, shared by the timeline sidebar and
/// the view title.
pub fn event_summary(kind: &EventKind) -> String {
    let display = |name: &str| {
        if name.is_empty() {
            "<toplevel>".to_string()
        } else {
            name.to_string()
        }
    };
    match kind {
        EventKind::Marking {
            name,
            target,
            reason,
            ..
        } => format!("mark {} → {} ({reason})", display(name), target.label()),
        EventKind::CompileStart {
            name, target, osr, ..
        } => format!(
            "compile {} {}{}",
            display(name),
            target.label(),
            if *osr { " OSR" } else { "" }
        ),
        EventKind::CompileDone {
            name, target, osr, ..
        } => format!(
            "done {} {}{}",
            display(name),
            target.label(),
            if *osr { " OSR" } else { "" }
        ),
        EventKind::DeoptBegin {
            kind,
            reason,
            name,
            tier,
            bytecode_offset,
            ..
        } => {
            let at = bytecode_offset
                .map(|o| format!(" @{o}"))
                .unwrap_or_default();
            format!("{kind} {} {}{at} — {reason}", display(name), tier.label())
        }
        EventKind::DeoptEnd { invalidated } => format!(
            "bailout end ({})",
            if *invalidated {
                "code invalidated"
            } else {
                "code unaffected"
            }
        ),
        EventKind::Osr {
            what,
            name,
            osr_offset,
            ..
        } => {
            let at = osr_offset.map(|o| format!(" @{o}")).unwrap_or_default();
            format!("OSR {what} {}{at}", display(name))
        }
    }
}

/// Row identity across rebuilds: `Function` rows compare by SFI only — their
/// `count` grows as the stream does, which is exactly when re-location
/// matters.
fn same_row(a: &Row, b: &Row) -> bool {
    match (a, b) {
        (Row::Function { sfi: a, .. }, Row::Function { sfi: b, .. }) => a == b,
        _ => a == b,
    }
}

/// Regex match on one line without materialising a String when avoidable:
/// escape-free valid-UTF-8 lines (all of a plain-text trace) match borrowed.
/// d8's colored graph lines still pay the strip, but `n` over a large raw
/// section stops allocating per line (found in review: ~56 ms per keypress
/// on a 2M-line section).
fn line_matches(buffer: &LogBuffer, line: usize, re: &Regex) -> bool {
    let bytes = buffer.line(line).unwrap_or(b"");
    if !bytes.contains(&0x1b)
        && let Ok(text) = std::str::from_utf8(bytes)
    {
        return re.is_match(text);
    }
    re.is_match(&line_text(buffer, line))
}

/// The display row showing a buffer line, regardless of row kind (fold
/// markers included).
fn row_showing(vm: &ViewModel, line: usize) -> Option<usize> {
    match vm {
        ViewModel::Plain { range } => range.contains(&line).then(|| line - range.start),
        ViewModel::Modeled { rows, .. } => rows.iter().position(|r| r.line == line),
    }
}

/// Blocks on the event channel, updates, redraws. One redraw per batch of
/// events, and no CPU at all while nothing is happening.
/// The caller must keep its own `Sender` alive for the duration, otherwise
/// `recv` starts failing as soon as the last reader finishes.
pub fn run(guard: &mut TerminalGuard, app: &mut App, rx: Receiver<Event>) -> Result<()> {
    guard.tui().draw(|frame| crate::ui::render(frame, app))?;

    while !app.quit {
        // Blocking: a `RecvError` means every sender is gone, which cannot
        // normally happen because the caller holds one, so treat it as quit.
        let Ok(first) = rx.recv() else { break };
        app.handle(first);

        let mut drained = 0;
        while drained < MAX_EVENTS_PER_DRAW {
            match rx.try_recv() {
                Ok(next) => {
                    app.handle(next);
                    drained += 1;
                }
                Err(_) => break,
            }
        }

        if app.quit {
            break;
        }
        let started = std::time::Instant::now();
        guard.tui().draw(|frame| crate::ui::render(frame, app))?;
        // The PLAN §12 cursor-latency budget, made observable: any frame that
        // blows it lands in the debug log with what was on screen.
        let elapsed = started.elapsed();
        if elapsed.as_millis() >= 16 {
            tracing::debug!(
                millis = elapsed.as_millis(),
                selected = app.selected,
                "slow frame"
            );
        }
    }

    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::tui::term::event::{KeyCode, KeyModifiers};

    fn app_with(trace: &str) -> App {
        let sources = vec![LogSource::Stdin];
        let keys = Keymap::build(&std::collections::HashMap::new()).unwrap();
        let mut app = App::new(&sources, None, keys);
        app.handle(Event::Source(SourceEvent::Chunk {
            source: 0,
            bytes: trace.as_bytes().to_vec(),
        }));
        app.handle(Event::Source(SourceEvent::Eof { source: 0 }));
        app
    }

    fn key(app: &mut App, code: KeyCode) {
        app.handle(Event::Input(TermEvent::Key(KeyEvent::new(
            code,
            KeyModifiers::NONE,
        ))));
    }

    fn ctrl(app: &mut App, c: char) {
        app.handle(Event::Input(TermEvent::Key(KeyEvent::new(
            KeyCode::Char(c),
            KeyModifiers::CONTROL,
        ))));
    }

    const TRACE: &str = "\
warmup line
Compiling 0x1 <JSFunction f (sfi = 0x10)> with Maglev
----- Maglev graph building -----
 Block b0
   1: Foo
   2: Bar [n1]
 Block b1
   3: Baz [n2]
   regalloc trace line
   4: Quux [n1, n3]
Compiling 0x2 <JSFunction g (sfi = 0x20)> with Maglev
----- Maglev graph building -----
   5: Zap
";

    #[test]
    fn rows_interleave_raw_and_compilations_chronologically() {
        let app = app_with(TRACE);
        let rows = app.rows();
        assert_eq!(
            rows,
            vec![Row::Raw(0), Row::Compilation(0), Row::Compilation(1)]
        );
    }

    #[test]
    fn compilation_view_models_and_folds_annotations() {
        let mut app = app_with(TRACE);
        app.follow = false;
        app.selected = 1;
        let vm = app.view_model();
        // 9 section lines (anchor + banner + 2 headers + 4 nodes + 1
        // annotation-as-marker): 9 rows.
        assert_eq!(vm.len(), 9);
        assert!(matches!(
            vm.row(7).unwrap().kind,
            RowKind::AnnotationFold { count: 1 }
        ));
        key(&mut app, KeyCode::Char('t'));
        let vm = app.view_model();
        assert_eq!(vm.len(), 9, "single annotation, same count");
        assert!(matches!(vm.row(7).unwrap().kind, RowKind::Text));
        assert!(app.show_annotations);
    }

    #[test]
    fn space_folds_the_block_under_the_cursor() {
        let mut app = app_with(TRACE);
        app.follow = false;
        app.selected = 1;
        app.focus = Pane::Viewport;
        // Cursor onto node `2: Bar` (display row: 0 anchor,1 banner,2 b0,3 Foo,4 Bar).
        app.cursor = 4;
        key(&mut app, KeyCode::Char(' '));
        let vm = app.view_model();
        assert!(
            matches!(
                vm.row(2).unwrap().kind,
                RowKind::BlockFold {
                    block: 0,
                    hidden: 2
                }
            ),
            "b0 folded with its two nodes hidden"
        );
        assert_eq!(app.cursor, 2, "cursor landed on the fold marker");
        key(&mut app, KeyCode::Char(' '));
        assert_eq!(app.view_model().len(), 9, "unfolded again");
    }

    #[test]
    fn search_moves_the_cursor_and_wraps() {
        let mut app = app_with(TRACE);
        app.follow = false;
        app.selected = 1;
        app.focus = Pane::Viewport;
        key(&mut app, KeyCode::Char('/'));
        for c in "Ba[rz]".chars() {
            key(&mut app, KeyCode::Char(c));
        }
        key(&mut app, KeyCode::Enter);
        let vm = app.view_model();
        assert_eq!(vm.line_at(app.cursor), Some(5), "first match: Bar");
        key(&mut app, KeyCode::Char('n'));
        let vm = app.view_model();
        assert_eq!(vm.line_at(app.cursor), Some(7), "next match: Baz");
        key(&mut app, KeyCode::Char('n'));
        let vm = app.view_model();
        assert_eq!(vm.line_at(app.cursor), Some(5), "wrapped");
    }

    #[test]
    fn input_jump_and_consumer_cycle_with_history() {
        let mut app = app_with(TRACE);
        app.follow = false;
        app.selected = 1;
        app.focus = Pane::Viewport;
        // Row 8: `4: Quux [n1, n3]` (the annotation above it is a marker row).
        app.cursor = 8;
        key(&mut app, KeyCode::Char('i'));
        let vm = app.view_model();
        assert_eq!(vm.line_at(app.cursor), Some(4), "def of n1 (first input)");
        // The cycle stays anchored to Quux, so the second `i` reaches the
        // *second* input's definition (review finding: it used to restart
        // from the node under the cursor and report "no inputs").
        key(&mut app, KeyCode::Char('i'));
        let vm = app.view_model();
        assert_eq!(vm.line_at(app.cursor), Some(7), "def of n3 (second input)");
        // History unwinds jump by jump, in buffer-line coordinates.
        ctrl(&mut app, 'o');
        let vm = app.view_model();
        assert_eq!(vm.line_at(app.cursor), Some(4), "back to the first jump");
        ctrl(&mut app, 'o');
        let vm = app.view_model();
        assert_eq!(vm.line_at(app.cursor), Some(9), "back to the origin");

        // Consumers of n1: Bar (line 5) and Quux (line 9). Move onto Foo via
        // keys so the stale cycle is cleared the way real navigation clears
        // it.
        key(&mut app, KeyCode::Char('g'));
        for _ in 0..3 {
            key(&mut app, KeyCode::Char('j'));
        }
        let vm = app.view_model();
        assert_eq!(vm.line_at(app.cursor), Some(4), "on `1: Foo`");
        key(&mut app, KeyCode::Char('u'));
        let vm = app.view_model();
        assert_eq!(vm.line_at(app.cursor), Some(5), "first consumer");
        key(&mut app, KeyCode::Char('u'));
        let vm = app.view_model();
        assert_eq!(
            vm.line_at(app.cursor),
            Some(9),
            "cycle stays anchored to n1"
        );
    }

    const BRANCH_TRACE: &str = "\
warmup line
Compiling 0x1 <JSFunction f (sfi = 0x10)> with Maglev
----- Maglev graph building -----
 Block b0
   1: Foo
   2: BranchIfRootConstant [n1] b1 b2
 Block b1
   3: Jump b2
 Block b2
   4: Return
";

    #[test]
    fn brackets_walk_blocks_and_enter_follows_edges() {
        let mut app = app_with(BRANCH_TRACE);
        app.follow = false;
        app.selected = 1;
        app.focus = Pane::Viewport;
        app.cursor = 0;

        // `]` walks headers; `[` walks back; the ends report honestly.
        for expected in [3usize, 6, 8] {
            key(&mut app, KeyCode::Char(']'));
            let vm = app.view_model();
            assert_eq!(vm.line_at(app.cursor), Some(expected));
        }
        key(&mut app, KeyCode::Char(']'));
        assert_eq!(app.status, "no next block");
        key(&mut app, KeyCode::Char('['));
        let vm = app.view_model();
        assert_eq!(vm.line_at(app.cursor), Some(6), "back to b1");

        // Enter on the branch cycles its two targets, staying anchored.
        app.cursor = 4; // `2: BranchIfRootConstant [n1] b1 b2` (line 5)
        key(&mut app, KeyCode::Enter);
        let vm = app.view_model();
        assert_eq!(vm.line_at(app.cursor), Some(6), "left target b1");
        key(&mut app, KeyCode::Enter);
        let vm = app.view_model();
        assert_eq!(vm.line_at(app.cursor), Some(8), "right target b2");
        assert!(app.status.contains("n2 target"), "status: {}", app.status);
        key(&mut app, KeyCode::Enter);
        let vm = app.view_model();
        assert_eq!(vm.line_at(app.cursor), Some(6), "cycle wraps");

        // `u` on a block header cycles its predecessors.
        key(&mut app, KeyCode::Char('j'));
        key(&mut app, KeyCode::Char('k')); // movement clears the cycle
        app.cursor = 7; // ` Block b2` (line 8)
        key(&mut app, KeyCode::Char('u'));
        let vm = app.view_model();
        assert_eq!(vm.line_at(app.cursor), Some(5), "the branch targets b2");
        key(&mut app, KeyCode::Char('u'));
        let vm = app.view_model();
        assert_eq!(vm.line_at(app.cursor), Some(7), "so does b1's Jump");
        assert!(
            app.status.contains("b2 predecessor 2/2"),
            "status: {}",
            app.status
        );

        // `]` still finds folded blocks: it lands on the fold marker.
        key(&mut app, KeyCode::Char('g'));
        key(&mut app, KeyCode::Char('z'));
        key(&mut app, KeyCode::Char(']'));
        let vm = app.view_model();
        assert!(matches!(
            vm.row(app.cursor).unwrap().kind,
            RowKind::BlockFold { block: 0, .. }
        ));
    }

    #[test]
    fn paging_keys_switch_focus_from_sidebar_to_viewport() {
        let mut app = app_with(BRANCH_TRACE);
        app.follow = false;
        app.selected = 1;
        app.focus = Pane::Sidebar;

        key(&mut app, KeyCode::PageDown);
        assert_eq!(app.focus, Pane::Viewport);
        assert_eq!(app.selected, 1, "sidebar selection unchanged");

        app.focus = Pane::Sidebar;
        ctrl(&mut app, 'd'); // HalfPageDown
        assert_eq!(app.focus, Pane::Viewport);
        assert_eq!(app.selected, 1);
    }

    const DISASM_TRACE: &str = "\
--- Optimized code ---
kind = TURBOFAN_JS
name = f
Instructions (size = 40)
0x100    0  aa  jmp 0x108  <+0x8>
0x104    4  bb  nop
0x108    8  cc  ret
--- End code ---
";

    #[test]
    fn disasm_branches_navigate_and_label() {
        let mut app = app_with(DISASM_TRACE);
        app.follow = false;
        app.selected = 0;
        app.focus = Pane::Viewport;

        // `i` on the jump lands on its destination row.
        app.cursor = 4; // the jmp (line 5)
        key(&mut app, KeyCode::Char('i'));
        let vm = app.view_model();
        assert_eq!(vm.line_at(app.cursor), Some(6), "ret row (+0x8)");
        assert!(app.status.contains("+0x0 target"), "{}", app.status);

        // `u` on the destination cycles the branches that land on it.
        key(&mut app, KeyCode::Char('j'));
        key(&mut app, KeyCode::Char('k'));
        key(&mut app, KeyCode::Char('u'));
        let vm = app.view_model();
        assert_eq!(vm.line_at(app.cursor), Some(4), "the jmp is the source");
        assert!(app.status.contains("+0x8 consumer"), "{}", app.status);

        // `]` walks the labelled rows — branch destinations are the block
        // leaders of a listing.
        key(&mut app, KeyCode::Char('g'));
        key(&mut app, KeyCode::Char(']'));
        let vm = app.view_model();
        assert_eq!(vm.line_at(app.cursor), Some(6));
        assert_eq!(app.status, "+0x8");
    }

    #[test]
    fn dual_diff_pairs_the_same_function_across_runs() {
        let run_a = "\
Compiling 0x1 <JSFunction f (sfi = 0x10)> with Maglev
----- Maglev graph building -----
 Block b0
   1: Foo
";
        let run_b = "\
Compiling 0x9 <JSFunction f (sfi = 0x90)> with Maglev
----- Maglev graph building -----
 Block b0
   1: Foo
   2: Bar [n1]
";
        let sources = vec![LogSource::Stdin, LogSource::Stdin];
        let keys = Keymap::build(&std::collections::HashMap::new()).unwrap();
        let mut app = App::new(&sources, None, keys);
        for (i, trace) in [run_a, run_b].iter().enumerate() {
            app.handle(Event::Source(SourceEvent::Chunk {
                source: i,
                bytes: trace.as_bytes().to_vec(),
            }));
            app.handle(Event::Source(SourceEvent::Eof { source: i }));
        }
        app.follow = false;
        app.active = 0;
        app.selected = 0;
        app.focus = Pane::Viewport;

        key(&mut app, KeyCode::Char('D'));
        assert!(app.diff, "{}", app.status);
        let model = app.diff_model().expect("dual model");
        let (lt, rt) = model.titles.clone().expect("dual titles");
        assert!(lt.contains('f') && rt.contains('f'), "{lt} / {rt}");
        assert_eq!(model.left.source, 1, "left = the other run");
        assert_eq!(model.right.source, 0);
        // Active run on the right: run B's extra node sits on the left
        // side, so it reads as deleted relative to the view.
        assert_eq!(model.summary.nodes_deleted, 1, "{:?}", model.summary);

        // `d` (or `D` again) drops back out.
        key(&mut app, KeyCode::Char('d'));
        assert!(!app.diff);
    }

    #[test]
    fn dual_diff_search_and_yank_use_each_pane_buffer() {
        let run_a = "\
raw preamble 0
raw preamble 1
raw preamble 2
raw preamble 3
OnlyInRunARaw
Compiling 0x1 <JSFunction f (sfi = 0x10)> with Maglev
----- Maglev graph building -----
 Block b0
   1: Foo
";
        let run_b = "\
Compiling 0x9 <JSFunction f (sfi = 0x90)> with Maglev
----- Maglev graph building -----
 Block b0
   1: Foo
   2: Bar [n1]
";
        let sources = vec![LogSource::Stdin, LogSource::Stdin];
        let keys = Keymap::build(&std::collections::HashMap::new()).unwrap();
        let mut app = App::new(&sources, None, keys);
        for (i, trace) in [run_a, run_b].iter().enumerate() {
            app.handle(Event::Source(SourceEvent::Chunk {
                source: i,
                bytes: trace.as_bytes().to_vec(),
            }));
            app.handle(Event::Source(SourceEvent::Eof { source: i }));
        }
        app.follow = false;
        app.active = 0;
        // Row 0 is the raw preamble section in `run_a`; row 1 is `f`.
        app.selected = 1;
        app.focus = Pane::Viewport;

        key(&mut app, KeyCode::Char('D'));
        assert!(app.diff, "{}", app.status);

        key(&mut app, KeyCode::Char('/'));
        for c in "Bar".chars() {
            key(&mut app, KeyCode::Char(c));
        }
        key(&mut app, KeyCode::Enter);
        assert_eq!(app.cursor, 3, "{}", app.status);
        assert_eq!(app.status, "match at diff row 4");

        // Left pane (`active_pane == 0`) belongs to `run_b` (`source 1`), where line 4 is `Bar`,
        // not `run_a` (`source 0`), where line 4 is `OnlyInRunARaw`.
        app.active_pane = 0;
        assert_eq!(app.yank_line_text().as_deref(), Some("   2: Bar [n1]"));
        // Right pane (`active_pane == 1`) has no line on this deleted row, and reads `run_a` on row 2.
        app.active_pane = 1;
        assert_eq!(app.yank_line_text(), None);
        app.cursor = 2;
        assert_eq!(app.yank_line_text().as_deref(), Some("   1: Foo"));

        key(&mut app, KeyCode::Char('/'));
        for c in "OnlyInRunARaw".chars() {
            key(&mut app, KeyCode::Char(c));
        }
        key(&mut app, KeyCode::Enter);
        assert_eq!(app.status, "no match for OnlyInRunARaw");
    }

    #[test]
    fn inlining_panel_lists_and_jumps() {
        let trace = "\
Compiling 0x1 <JSFunction f (sfi = 0x10)> with Maglev
\u{26a1} INLINE small                          Small function
\u{274c} SKIP   big                            too big
----- Maglev graph building -----
 Block b0
   1: Foo
";
        let mut app = app_with(trace);
        app.follow = false;
        app.selected = 0;
        app.focus = Pane::Viewport;

        key(&mut app, KeyCode::Char('I'));
        assert_eq!(app.inline_panel, Some(0), "{}", app.status);
        key(&mut app, KeyCode::Char('j'));
        assert_eq!(app.inline_panel, Some(1));
        key(&mut app, KeyCode::Enter);
        assert!(app.inline_panel.is_none());
        let vm = app.view_model();
        assert_eq!(vm.line_at(app.cursor), Some(2), "on the SKIP line");
        assert!(app.status.contains("SKIP big"), "{}", app.status);
    }

    const ALIGN_TRACE: &str = "\
Compiling 0x1 <JSFunction f (sfi = 0x10)> with Maglev
----- Bytecode array -----
   45 S> 0x100 @    0 : 0b 03             Ldar a0
   57 E> 0x102 @    2 : 46 03 00          MulSmi [3], [0]
----- Maglev graph building -----
 Block b0
0x10 <SharedFunctionInfo f> (0x2 <String[6]: \"add.js\">:1:10)
   0 : 0b 03             Ldar a0
   1: Foo
   2 : 46 03 00          MulSmi [3], [0]
   2: Bar [n1]
Finished compiling method f using Maglev
--- Raw source ---
(a) {
  return a * 3;
}
--- Optimized code ---
optimization_id = 0
source_position = 40
kind = MAGLEV
name = f
Instructions (size = 8)
0x200    0  aa  ret
--- End code ---
";

    #[test]
    fn source_pane_falls_back_to_raw_source_and_aligns() {
        let mut app = app_with(ALIGN_TRACE);
        app.follow = false;
        app.selected = 0;
        app.focus = Pane::Viewport;

        key(&mut app, KeyCode::Char('S'));
        let pane = app.source_pane.as_ref().expect("pane open");
        assert!(pane.title.contains("add.js"), "{}", pane.title);
        assert!(pane.title.contains("not found"), "{}", pane.title);
        assert_eq!(pane.lines.len(), 3, "{:?}", pane.lines);
        assert_eq!(pane.base_line, 1, "function anchor from the SFI context");
        // Char refs place through `source_position = 40`.
        assert_eq!(pane.display_line(&SrcRef::Char(45)), Some(0));
        assert_eq!(pane.display_line(&SrcRef::Char(57)), Some(1));
        assert_eq!(pane.display_line(&SrcRef::FnLine(1)), Some(0));

        let map = app.source_map(0);
        assert_eq!(map.of_row.get(&2), Some(&SrcRef::Char(45)), "S> marker");
        assert_eq!(
            map.of_row.get(&9),
            Some(&SrcRef::Char(57)),
            "interleaved row inherits through its offset"
        );
        assert_eq!(
            map.of_row.get(&10),
            Some(&SrcRef::Char(57)),
            "node under it too"
        );
        assert_eq!(map.of_row.get(&6), Some(&SrcRef::FnLine(1)));

        // JS → trace: Enter cycles the rows aligned with the cursor line.
        app.focus = Pane::Source;
        app.source_pane.as_mut().unwrap().cursor = 1;
        key(&mut app, KeyCode::Enter);
        let vm = app.view_model();
        assert_eq!(vm.line_at(app.cursor), Some(3), "first aligned row");
        key(&mut app, KeyCode::Enter);
        let vm = app.view_model();
        assert_eq!(vm.line_at(app.cursor), Some(9), "cycle continues");

        // S again closes and returns focus to the viewport.
        key(&mut app, KeyCode::Char('S'));
        assert!(app.source_pane.is_none());
        assert_eq!(app.focus, Pane::Viewport);
    }

    #[test]
    fn sidebar_toggles_and_reopens_from_the_left_edge() {
        let mut app = app_with(TRACE);
        assert!(app.sidebar_visible);
        app.focus = Pane::Sidebar;

        key(&mut app, KeyCode::Char('b'));
        assert!(!app.sidebar_visible);
        assert_eq!(app.focus, Pane::Viewport, "focus leaves a hidden pane");
        assert!(app.status.contains("b or h shows it"), "{}", app.status);

        // `h` brings it back and focuses it.
        key(&mut app, KeyCode::Char('h'));
        assert!(app.sidebar_visible);
        assert_eq!(app.focus, Pane::Sidebar);

        // The timeline lives in the sidebar: Tab reopens a hidden one.
        key(&mut app, KeyCode::Char('b'));
        assert!(!app.sidebar_visible);
        key(&mut app, KeyCode::Tab);
        assert!(app.sidebar_visible && app.timeline);
    }

    const BYTECODE_TRACE: &str = "\
warmup line
Compiling 0x1 <JSFunction f (sfi = 0x10)> with Maglev
----- Bytecode array -----
Parameter count 1
         0x100 @    0 : 23 00 01          LdaGlobal [0:\"x\"], FBV[1]
         0x103 @    3 : 97 33 00 01       JumpLoop [51], [0], FBV[1] (0x100 @ 0)
         0x105 @   20 : b9                Return
Constant pool (size = 1)
           0: 0x2 <String[1]: #x>
Handler Table (size = 0)
0x3: [FeedbackVector] in OldSpace
 - slot #1 LoadGlobalNotInsideTypeof MONOMORPHIC
----- Maglev graph building -----
 Block b0
   1: Foo
";

    #[test]
    fn bytecode_refs_navigate_like_node_inputs() {
        let mut app = app_with(BYTECODE_TRACE);
        app.follow = false;
        app.selected = 1;
        app.focus = Pane::Viewport;
        // Cursor on the JumpLoop row (buffer line 5).
        app.cursor = 4;
        let vm = app.view_model();
        assert_eq!(vm.line_at(app.cursor), Some(5), "on JumpLoop");

        // `i` walks the row's refs: jump target first…
        key(&mut app, KeyCode::Char('i'));
        let vm = app.view_model();
        assert_eq!(
            vm.line_at(app.cursor),
            Some(4),
            "jump target @0 (LdaGlobal)"
        );
        // …then FBV[1], anchored to the original row, not the one landed on.
        key(&mut app, KeyCode::Char('i'));
        let vm = app.view_model();
        assert_eq!(vm.line_at(app.cursor), Some(11), "slot #1");
        assert!(app.status.contains("@3"), "anchor label: {}", app.status);

        // `u` on the slot cycles the rows that reference it.
        key(&mut app, KeyCode::Char('g'));
        for _ in 0..10 {
            key(&mut app, KeyCode::Char('j'));
        }
        let vm = app.view_model();
        assert_eq!(vm.line_at(app.cursor), Some(11), "on slot #1");
        key(&mut app, KeyCode::Char('u'));
        let vm = app.view_model();
        assert_eq!(vm.line_at(app.cursor), Some(4), "first FBV[1] use");
        key(&mut app, KeyCode::Char('u'));
        let vm = app.view_model();
        assert_eq!(vm.line_at(app.cursor), Some(5), "second FBV[1] use");
        assert!(app.status.contains("FBV[1]"), "status: {}", app.status);

        // `i` on LdaGlobal reaches its pool entry too (slot, then pool).
        key(&mut app, KeyCode::Char('g'));
        for _ in 0..3 {
            key(&mut app, KeyCode::Char('j'));
        }
        let vm = app.view_model();
        assert_eq!(vm.line_at(app.cursor), Some(4), "on LdaGlobal");
        key(&mut app, KeyCode::Char('i'));
        let vm = app.view_model();
        assert_eq!(vm.line_at(app.cursor), Some(11), "FBV ref first");
        key(&mut app, KeyCode::Char('i'));
        let vm = app.view_model();
        assert_eq!(vm.line_at(app.cursor), Some(8), "pool entry second");

        // `u` on the pool entry finds its referencing row.
        key(&mut app, KeyCode::Char('g'));
        for _ in 0..7 {
            key(&mut app, KeyCode::Char('j'));
        }
        let vm = app.view_model();
        assert_eq!(vm.line_at(app.cursor), Some(8), "on pool entry");
        key(&mut app, KeyCode::Char('u'));
        let vm = app.view_model();
        assert_eq!(vm.line_at(app.cursor), Some(4), "pool consumer");
        assert!(app.status.contains("pool[0]"), "status: {}", app.status);
    }

    #[test]
    fn z_folds_and_unfolds_all_blocks_preserving_context() {
        let mut app = app_with(TRACE);
        app.follow = false;
        app.selected = 1;
        app.focus = Pane::Viewport;
        // Cursor on `4: Quux` (line 9), inside b1.
        app.cursor = 8;

        key(&mut app, KeyCode::Char('z'));
        let vm = app.view_model();
        // anchor + banner + two fold markers; all body lines hidden.
        assert_eq!(vm.len(), 4);
        assert!(matches!(
            vm.row(2).unwrap().kind,
            RowKind::BlockFold { block: 0, .. }
        ));
        assert!(matches!(
            vm.row(3).unwrap().kind,
            RowKind::BlockFold { block: 1, .. }
        ));
        assert_eq!(app.cursor, 3, "cursor lands on its block's fold marker");
        assert!(app.status.contains("2 blocks folded"), "{}", app.status);

        key(&mut app, KeyCode::Char('z'));
        assert_eq!(app.view_model().len(), 9, "all unfolded again");
        assert!(app.status.contains("2 blocks unfolded"), "{}", app.status);

        // Partially folded (one block by hand) → z finishes the job.
        app.cursor = 3;
        key(&mut app, KeyCode::Char(' ')); // fold b0 only
        key(&mut app, KeyCode::Char('z'));
        assert_eq!(app.view_model().len(), 4, "z folded the rest");
    }

    #[test]
    fn z_on_a_phase_selection_scopes_to_that_phase() {
        let mut app = app_with(DIFF_TRACE);
        app.follow = false;
        key(&mut app, KeyCode::Enter); // expand: rows comp, phase0, phase1
        app.selected = 2; // Phi untagging
        app.focus = Pane::Viewport;
        key(&mut app, KeyCode::Char('z'));
        assert!(app.status.contains("1 blocks folded"), "{}", app.status);

        // The other phase's identical block id is untouched.
        app.selected = 1;
        app.cursor = 0;
        let vm = app.view_model();
        assert!(
            (0..vm.len()).all(|r| !matches!(vm.row(r).unwrap().kind, RowKind::BlockFold { .. })),
            "phase 0's b0 stays open"
        );
    }

    #[test]
    fn jump_into_a_folded_block_unfolds_it() {
        let mut app = app_with(TRACE);
        app.follow = false;
        app.selected = 1;
        app.focus = Pane::Viewport;
        // Fold b0 (rows: 0 anchor, 1 banner, 2 b0, ...).
        app.cursor = 2;
        key(&mut app, KeyCode::Char(' '));
        // From Quux, i → def of n1 which is inside folded b0.
        let vm = app.view_model();
        let quux_row = (0..vm.len())
            .find(|&r| vm.line_at(r) == Some(9))
            .expect("Quux visible");
        app.cursor = quux_row;
        key(&mut app, KeyCode::Char('i'));
        let vm = app.view_model();
        assert_eq!(vm.line_at(app.cursor), Some(4), "unfolded and landed");
    }

    #[test]
    fn sidebar_filter_narrows_rows() {
        let mut app = app_with(TRACE);
        app.follow = false;
        key(&mut app, KeyCode::Char('f'));
        key(&mut app, KeyCode::Char('g'));
        key(&mut app, KeyCode::Enter);
        let rows = app.rows();
        assert_eq!(rows, vec![Row::Compilation(1)], "only g matches");
        // Clearing the filter restores everything.
        key(&mut app, KeyCode::Char('f'));
        key(&mut app, KeyCode::Backspace);
        key(&mut app, KeyCode::Enter);
        assert_eq!(app.rows().len(), 3);
    }

    #[test]
    fn grouping_creates_function_headers() {
        let mut app = app_with(TRACE);
        app.follow = false;
        key(&mut app, KeyCode::Char('c'));
        let rows = app.rows();
        // Two functions (f, g) collapsed + one raw section.
        assert_eq!(rows.len(), 3);
        assert!(matches!(rows[0], Row::Function { count: 1, .. }));
        key(&mut app, KeyCode::Enter);
        assert_eq!(app.rows().len(), 4, "group expanded");
    }

    #[test]
    fn follow_sticks_to_the_newest_section() {
        let app = app_with(TRACE);
        assert!(app.follow, "stdin defaults to follow");
        let rows = app.rows();
        assert_eq!(app.selected, rows.len() - 1);
    }

    #[test]
    fn help_modal_opens_and_any_key_closes() {
        let mut app = app_with(TRACE);
        key(&mut app, KeyCode::Char('?'));
        assert!(app.help);
        key(&mut app, KeyCode::Char('q'));
        assert!(!app.help);
        assert!(!app.quit, "q inside help closes the modal, not the app");
    }

    #[test]
    fn esc_backs_out_of_viewport_but_does_not_quit() {
        let mut app = app_with(TRACE);
        app.focus = Pane::Viewport;
        key(&mut app, KeyCode::Esc);
        assert_eq!(app.focus, Pane::Sidebar);
        assert!(!app.quit);
        key(&mut app, KeyCode::Esc);
        assert_eq!(app.focus, Pane::Sidebar);
        assert!(!app.quit);
    }

    fn mouse(app: &mut App, kind: MouseEventKind, column: u16, row: u16) {
        app.handle(Event::Input(TermEvent::Mouse(MouseEvent {
            kind,
            column,
            row,
            modifiers: KeyModifiers::NONE,
        })));
    }

    /// Lay out panes the way a frame would: sidebar columns 0..30, viewport
    /// 30..120, both rows 1..41.
    fn with_layout(app: &mut App) {
        app.sidebar_rect = PaneRect {
            x: 0,
            y: 1,
            width: 30,
            height: 40,
        };
        app.viewport_rect = PaneRect {
            x: 30,
            y: 1,
            width: 90,
            height: 40,
        };
        app.sidebar_height = 40;
        app.viewport_height = 40;
    }

    #[test]
    fn wheel_scrolls_the_pane_under_the_pointer() {
        let mut app = app_with(TRACE);
        app.follow = false;
        app.selected = 1;
        app.focus = Pane::Sidebar;
        with_layout(&mut app);

        // Wheel over the viewport moves the cursor, not the selection…
        app.cursor = 0;
        mouse(&mut app, MouseEventKind::ScrollDown, 60, 5);
        assert_eq!(app.focus, Pane::Viewport);
        assert_eq!(app.cursor, 3, "three rows per tick");
        assert_eq!(app.selected, 1, "selection untouched");
        mouse(&mut app, MouseEventKind::ScrollUp, 60, 5);
        assert_eq!(app.cursor, 0);

        // …and over the sidebar it moves the selection.
        mouse(&mut app, MouseEventKind::ScrollUp, 5, 5);
        assert_eq!(app.focus, Pane::Sidebar);
        assert_eq!(app.selected, 0);

        // Outside both panes: ignored.
        let before = app.selected;
        mouse(&mut app, MouseEventKind::ScrollDown, 60, 0);
        assert_eq!(app.selected, before);
    }

    #[test]
    fn click_places_selection_and_cursor() {
        let mut app = app_with(TRACE);
        app.follow = false;
        with_layout(&mut app);

        // Click the second sidebar row (screen row 2 = y 1 + offset 1).
        mouse(&mut app, MouseEventKind::Down(MouseButton::Left), 5, 2);
        assert_eq!(app.focus, Pane::Sidebar);
        assert_eq!(app.selected, 1);

        // Click viewport screen row 5 → display row top+4 → its node lights
        // up as the cursor node.
        mouse(&mut app, MouseEventKind::Down(MouseButton::Left), 60, 5);
        assert_eq!(app.focus, Pane::Viewport);
        assert_eq!(app.cursor, 4);
        let vm = app.view_model();
        assert_eq!(app.cursor_node(&vm).map(|n| n.id), Some(2), "clicked Bar");

        // A click past the end of the section clamps to the last row.
        mouse(&mut app, MouseEventKind::Down(MouseButton::Left), 60, 39);
        let vm = app.view_model();
        assert_eq!(app.cursor, vm.len() - 1);
    }

    #[test]
    fn mouse_ignored_during_prompts_and_closes_help() {
        let mut app = app_with(TRACE);
        app.follow = false;
        with_layout(&mut app);

        key(&mut app, KeyCode::Char('?'));
        mouse(&mut app, MouseEventKind::ScrollDown, 60, 5);
        assert!(!app.help, "any mouse action closes the modal");

        key(&mut app, KeyCode::Char('/'));
        let cursor = app.cursor;
        mouse(&mut app, MouseEventKind::Down(MouseButton::Left), 60, 20);
        assert_eq!(app.cursor, cursor, "prompt owns the interaction");
        assert!(app.input.is_some());
    }

    #[test]
    fn cursor_node_tracks_the_cursor_row() {
        let mut app = app_with(TRACE);
        app.follow = false;
        app.selected = 1;
        app.cursor = 4;
        let vm = app.view_model();
        let node = app.cursor_node(&vm).expect("node on row 4");
        assert_eq!(node.id, 2);
    }

    /// A trace with lifecycle events, a graph with an interleaved bytecode
    /// line, and a verbose deopt block — the Phase 6 material.
    const EVENTS_TRACE: &str = "\
[marking 0x09b8 <JSFunction f (sfi = 0x10)> for optimization to MAGLEV, ConcurrencyMode::kConcurrent, reason: hot and stable]
Compiling 0x1 <JSFunction f (sfi = 0x10)> with Maglev
----- Maglev graph building -----
 Block b0
   2 : 0b 04             Ldar a1
   1: CheckSmth [n0]
[bailout (kind: deopt-eager, reason: not a Smi): begin. deoptimizing 0x09b8 <JSFunction f (sfi = 0x10)>, 0x031a <Code MAGLEV>, opt id 1, bytecode offset 2, deopt exit 0, FP to SP delta 32, caller SP 0x0001, pc 0x0002]
            ;;; deoptimize at <test.js:14:1>
  reading input frame  => bytecode_offset=2, args=1, height=6, retval=0(#0); inputs:
      0: 0x09b8 ;  [fp -  16]  0x09b8 <JSFunction (sfi = 0x10)>
[bailout end. code_invalidation: unaffected, took 0.024 ms]
";

    #[test]
    fn command_palette_completes_and_toggles_a_lens() {
        let mut app = app_with(EVENTS_TRACE);
        app.follow = false;
        app.selected = 1; // the compilation

        key(&mut app, KeyCode::Char(':'));
        for c in "che".chars() {
            key(&mut app, KeyCode::Char(c));
        }
        key(&mut app, KeyCode::Tab);
        assert_eq!(app.input.as_ref().unwrap().buffer, "checks");
        key(&mut app, KeyCode::Enter);
        assert_eq!(app.lens, Some(Lens::Checks));
        assert!(app.status.contains("1 match"), "{}", app.status);

        // The lens keeps the skeleton + the guard; the bytecode line is gone.
        let vm = app.view_model();
        let lines: Vec<usize> = (0..vm.len()).filter_map(|r| vm.line_at(r)).collect();
        assert_eq!(lines, vec![1, 2, 3, 5]);

        // Ambiguous prefix completes to the common prefix and stays open.
        key(&mut app, KeyCode::Char(':'));
        key(&mut app, KeyCode::Char('c'));
        key(&mut app, KeyCode::Tab);
        assert_eq!(
            app.input.as_ref().unwrap().buffer,
            "c",
            "checks/clear/copy share only c"
        );
        for c in "lear".chars() {
            key(&mut app, KeyCode::Char(c));
        }
        key(&mut app, KeyCode::Enter);
        assert_eq!(app.lens, None);
    }

    #[test]
    fn unknown_command_reports_and_does_nothing() {
        let mut app = app_with(EVENTS_TRACE);
        app.follow = false;
        key(&mut app, KeyCode::Char(':'));
        for c in "bogus".chars() {
            key(&mut app, KeyCode::Char(c));
        }
        key(&mut app, KeyCode::Enter);
        assert!(
            app.status.contains("unknown command :bogus"),
            "{}",
            app.status
        );
        assert!(app.input.is_none());
    }

    #[test]
    fn command_palette_q_and_quit() {
        let mut app = app_with(EVENTS_TRACE);
        app.follow = false;
        key(&mut app, KeyCode::Char(':'));
        key(&mut app, KeyCode::Char('q'));
        key(&mut app, KeyCode::Enter);
        assert!(app.quit);

        let mut app = app_with(EVENTS_TRACE);
        app.follow = false;
        key(&mut app, KeyCode::Char(':'));
        for c in "quit".chars() {
            key(&mut app, KeyCode::Char(c));
        }
        key(&mut app, KeyCode::Enter);
        assert!(app.quit);
    }

    #[test]
    fn command_palette_accepts_prefixes_and_reports_ambiguity() {
        let mut app = app_with(EVENTS_TRACE);
        app.follow = false;
        app.selected = 1;

        // Unique prefix :che resolves to :checks without Tab
        key(&mut app, KeyCode::Char(':'));
        for c in "che".chars() {
            key(&mut app, KeyCode::Char(c));
        }
        key(&mut app, KeyCode::Enter);
        assert_eq!(app.lens, Some(Lens::Checks));

        // Unique prefix :fun <arg> resolves to :function
        key(&mut app, KeyCode::Char(':'));
        for c in "fun ^process".chars() {
            key(&mut app, KeyCode::Char(c));
        }
        key(&mut app, KeyCode::Enter);
        assert!(app.sidebar_filter.is_some());

        // Ambiguous prefix :c reports ambiguity and does not execute
        key(&mut app, KeyCode::Char(':'));
        key(&mut app, KeyCode::Char('c'));
        key(&mut app, KeyCode::Enter);
        assert!(
            app.status
                .contains("ambiguous command :c (matches: checks, clear, copy)"),
            "{}",
            app.status
        );
    }

    #[test]
    fn timeline_lists_events_and_enter_jumps_to_the_offset() {
        let mut app = app_with(EVENTS_TRACE);
        app.follow = false;

        key(&mut app, KeyCode::Tab);
        assert!(app.timeline);
        let rows = app.rows();
        assert_eq!(rows.len(), 3, "marking, bailout, bailout end");

        // Move onto the deopt event; the viewport shows the bailout block
        // with the cursor on the event line (the 6.5 panel).
        key(&mut app, KeyCode::Char('j'));
        assert_eq!(app.rows()[app.selected], Row::Event(1));
        let vm = app.view_model();
        match &vm {
            ViewModel::Plain { range } => assert_eq!(
                range.clone(),
                6..11,
                "bailout begin through bailout end, verbose block included"
            ),
            _ => panic!("event views are plain"),
        }
        assert_eq!(vm.line_at(app.cursor), Some(6));

        // Enter: correlation puts us in f's Maglev graph at bytecode offset 2.
        key(&mut app, KeyCode::Enter);
        assert!(!app.timeline);
        assert_eq!(app.focus, Pane::Viewport);
        let vm = app.view_model();
        assert_eq!(vm.line_at(app.cursor), Some(4), "the `2 :` bytecode line");
        assert!(app.status.contains("offset 2"), "{}", app.status);

        // Ctrl+O returns to the timeline, on the event row.
        ctrl(&mut app, 'o');
        assert!(app.timeline);
        assert_eq!(app.rows()[app.selected], Row::Event(1));
    }

    #[test]
    fn deopts_command_filters_the_timeline() {
        let mut app = app_with(EVENTS_TRACE);
        app.follow = false;
        key(&mut app, KeyCode::Char(':'));
        for c in "deopts".chars() {
            key(&mut app, KeyCode::Char(c));
        }
        key(&mut app, KeyCode::Enter);
        assert!(app.timeline && app.timeline_deopts_only);
        assert_eq!(app.rows(), vec![Row::Event(1)]);
        assert!(app.status.contains("1 deopt event"), "{}", app.status);

        // Tab back out clears the narrowing.
        key(&mut app, KeyCode::Tab);
        key(&mut app, KeyCode::Tab);
        assert_eq!(app.rows().len(), 3);
    }

    #[test]
    fn function_command_filters_the_sidebar() {
        let mut app = app_with(TRACE);
        app.follow = false;
        key(&mut app, KeyCode::Char(':'));
        for c in "function g".chars() {
            key(&mut app, KeyCode::Char(c));
        }
        key(&mut app, KeyCode::Enter);
        assert_eq!(app.rows(), vec![Row::Compilation(1)]);
        key(&mut app, KeyCode::Char(':'));
        for c in "function".chars() {
            key(&mut app, KeyCode::Char(c));
        }
        key(&mut app, KeyCode::Enter);
        assert_eq!(app.rows().len(), 3, "no argument clears the filter");
    }

    #[test]
    fn marking_event_jump_binds_forward_to_the_dump() {
        // An *earlier* instance of the same (sfi, tier) exists — the stale
        // code the marking event is about to replace. The jump must land on
        // the dump the event triggers, not the dead one (review finding: the
        // backward search used to win whenever an earlier instance existed).
        let trace = "\
Compiling 0x1 <JSFunction f (sfi = 0x10)> with Maglev
----- Maglev graph building -----
   1: Old
[marking 0x09b8 <JSFunction f (sfi = 0x10)> for optimization to MAGLEV, ConcurrencyMode::kConcurrent, reason: hot and stable]
Compiling 0x1 <JSFunction f (sfi = 0x10)> with Maglev
----- Maglev graph building -----
   1: New
";
        let mut app = app_with(trace);
        app.follow = false;
        key(&mut app, KeyCode::Tab);
        assert_eq!(app.rows(), vec![Row::Event(0)]);
        key(&mut app, KeyCode::Enter);
        assert!(!app.timeline);
        assert_eq!(
            app.rows()[app.selected],
            Row::Compilation(1),
            "the dump the marking produced, not the stale instance"
        );

        // A deopt of the same key binds strictly backwards.
        let mut app = app_with(EVENTS_TRACE);
        app.follow = false;
        key(&mut app, KeyCode::Tab);
        key(&mut app, KeyCode::Char('j'));
        key(&mut app, KeyCode::Enter);
        assert_eq!(app.rows()[app.selected], Row::Compilation(0));
    }

    #[test]
    fn deopt_panel_has_its_full_extent_while_streaming() {
        // No Eof: the tail raw section is not in the index yet, which used
        // to shrink the panel to the single bailout line (review finding).
        let sources = vec![LogSource::Stdin];
        let keys = Keymap::build(&std::collections::HashMap::new()).unwrap();
        let mut app = App::new(&sources, None, keys);
        app.handle(Event::Source(SourceEvent::Chunk {
            source: 0,
            bytes: EVENTS_TRACE.as_bytes().to_vec(),
        }));
        app.follow = false;

        key(&mut app, KeyCode::Tab);
        key(&mut app, KeyCode::Char('j'));
        assert_eq!(app.rows()[app.selected], Row::Event(1));
        let vm = app.view_model();
        match &vm {
            ViewModel::Plain { range } => {
                assert_eq!(
                    range.clone(),
                    6..11,
                    "full bailout block, live tail included"
                );
            }
            _ => panic!("event views are plain"),
        }
    }

    #[test]
    fn history_round_trip_preserves_the_left_mode_selection() {
        let mut app = app_with(EVENTS_TRACE);
        app.follow = false;
        // Leave the compilation list parked on the compilation row.
        app.selected = 1;
        key(&mut app, KeyCode::Tab);
        key(&mut app, KeyCode::Char('j'));
        key(&mut app, KeyCode::Enter); // deopt jump into the compilation
        ctrl(&mut app, 'o'); // back onto the event row
        assert!(app.timeline);
        assert_eq!(app.rows()[app.selected], Row::Event(1));
        // Tab must return to a *compilation-list* selection, not interpret
        // an event index against it (review finding: the parked selection
        // was clobbered by the history restore).
        key(&mut app, KeyCode::Tab);
        assert!(!app.timeline);
        assert!(
            matches!(app.rows()[app.selected], Row::Compilation(_)),
            "left the timeline onto a compilation row"
        );
    }

    /// Two graph phases with a replacement, an addition, and a deletion —
    /// the Phase 7 material.
    const DIFF_TRACE: &str = "\
Compiling 0x1 <JSFunction f (sfi = 0x10)> with Maglev
----- Maglev graph building -----
 Block b0
   1: InitialValue(a0)
   5: Int32Add [n1, n1]
   6: CheckSmth [n5]
----- Phi untagging -----
 Block b0
   1: InitialValue(a0)
   5: Identity [n12]
  12: Float64Add [n1, n1]
";

    #[test]
    fn split_opens_switches_direction_and_closes() {
        let mut app = app_with(DIFF_TRACE);
        app.follow = false;
        key(&mut app, KeyCode::Char('v'));
        assert_eq!(app.split, Some(SplitDir::Vertical));
        assert_eq!(app.other_view.selected, app.selected, "clone on open");

        ctrl(&mut app, 'w');
        assert_eq!(app.active_pane, 1);
        assert_eq!(app.focus, Pane::Viewport);

        key(&mut app, KeyCode::Char('s'));
        assert_eq!(app.split, Some(SplitDir::Horizontal), "direction switch");
        key(&mut app, KeyCode::Char('s'));
        assert_eq!(app.split, None, "same key closes");
        assert_eq!(app.active_pane, 0);
    }

    #[test]
    fn d_on_a_phase_diffs_against_the_previous_graph_phase() {
        let mut app = app_with(DIFF_TRACE);
        app.follow = false;
        key(&mut app, KeyCode::Enter); // expand the compilation
        let rows = app.rows();
        assert_eq!(rows.len(), 3, "compilation + two phase rows");
        app.selected = 2; // Phi untagging

        key(&mut app, KeyCode::Char('d'));
        assert!(app.diff);
        assert_eq!(app.split, Some(SplitDir::Vertical));
        assert_eq!(app.active_pane, 1, "the newer phase is the active side");
        assert_eq!(app.pane_state(0).selected, 1, "left pane: graph building");
        assert_eq!(app.pane_state(1).selected, 2, "right pane: phi untagging");

        let model = app.diff_model().expect("model builds");
        assert_eq!(model.summary.nodes_replaced, 1, "n5 → Identity [n12]");
        assert_eq!(model.summary.nodes_added, 1, "n12");
        assert_eq!(model.summary.nodes_deleted, 1, "n6");
        assert!(app.status.starts_with("diff:"), "{}", app.status);

        // Movement ranges over the aligned rows, not either pane's own list.
        let len = model.rows.len();
        key(&mut app, KeyCode::Char('G'));
        assert_eq!(app.cursor, len - 1);

        // Node jumps refuse politely instead of corrupting the shared cursor.
        key(&mut app, KeyCode::Char('i'));
        assert!(app.status.contains("diff view"), "{}", app.status);

        key(&mut app, KeyCode::Char('d'));
        assert!(!app.diff, "d toggles off, split remains");
        assert_eq!(app.split, Some(SplitDir::Vertical));
    }

    #[test]
    fn d_on_a_compilation_diffs_first_vs_last_graph_phase() {
        let mut app = app_with(DIFF_TRACE);
        app.follow = false;
        assert_eq!(app.selected, 0);
        key(&mut app, KeyCode::Char('d'));
        assert!(app.diff);
        assert!(app.compilation_expanded(0), "auto-expanded to show phases");
        assert_eq!(app.pane_state(0).selected, 1);
        assert_eq!(app.pane_state(1).selected, 2);
    }

    #[test]
    fn d_without_a_diffable_selection_explains() {
        let mut app = app_with(TRACE);
        app.follow = false;
        app.selected = 0; // the raw warmup section
        key(&mut app, KeyCode::Char('d'));
        assert!(!app.diff);
        assert!(app.status.contains("diff works on"), "{}", app.status);
    }

    #[test]
    fn lens_cleared_when_a_jump_targets_a_hidden_line() {
        let mut app = app_with(TRACE);
        app.follow = false;
        app.selected = 1;
        app.focus = Pane::Viewport;
        // `:phi` hides everything in this graph except headers.
        app.lens = Some(Lens::Phi);
        let vm = app.view_model();
        assert!(vm.len() < 9);
        // A search for a hidden node line still lands: the jump wins.
        app.goto_line(5); // `2: Bar [n1]`
        assert_eq!(app.lens, None);
        let vm = app.view_model();
        assert_eq!(vm.line_at(app.cursor), Some(5));
    }
}
