// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Keymap and config-file handling.
//!
//! One coherent, remappable keymap (PLAN §3.5): the defaults are PLAN §8
//! verbatim, and `~/.config/garage/config.toml` (or `--config <path>`) can
//! rebind any action. Remapping an action *replaces* its default chords —
//! binding `quit = ["x"]` frees `q` — and two actions on one chord is a
//! startup error, because silently double-bound keys are how v1 of the plan
//! ended up with `h` meaning two things.
//!
//! Everything here fails before the alternate screen goes up: a config typo
//! is a normal error message, not a broken TUI.

use std::collections::HashMap;
use std::path::{Path, PathBuf};

use crate::toml_min as toml;
use crate::tui::term::event::{KeyCode, KeyEvent, KeyModifiers};
use anyhow::{Context, Result, bail};

/// Everything a key can do. Phase 4 actions are declared now so configs
/// written against the MVP keep working as features land.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum Action {
    Quit,
    Back,
    Help,
    Up,
    Down,
    HalfPageDown,
    HalfPageUp,
    PageDown,
    PageUp,
    Top,
    Bottom,
    FocusSidebar,
    FocusViewport,
    Select,
    NextSource,
    ToggleFollow,
    ToggleGrouping,
    ToggleWrap,
    ScrollLeft,
    ScrollRight,
    FoldBlock,
    JumpToInput,
    CycleConsumers,
    PrevBlock,
    NextBlock,
    ToggleSidebar,
    ToggleSourcePane,
    InliningPanel,
    DualDiff,
    JumpBack,
    JumpForward,
    Search,
    SearchNext,
    SearchPrev,
    Filter,
    ToggleAnnotations,
    Yank,
    YankSection,
    Export,
    CommandPalette,
    ToggleTimeline,
    SplitVertical,
    SplitHorizontal,
    OtherPane,
    Diff,
    FoldAllBlocks,
}

impl Action {
    /// The config-file name of the action.
    pub fn name(self) -> &'static str {
        match self {
            Action::Quit => "quit",
            Action::Back => "back",
            Action::Help => "help",
            Action::Up => "up",
            Action::Down => "down",
            Action::HalfPageDown => "half-page-down",
            Action::HalfPageUp => "half-page-up",
            Action::PageDown => "page-down",
            Action::PageUp => "page-up",
            Action::Top => "top",
            Action::Bottom => "bottom",
            Action::FocusSidebar => "focus-sidebar",
            Action::FocusViewport => "focus-viewport",
            Action::Select => "select",
            Action::NextSource => "next-source",
            Action::ToggleFollow => "toggle-follow",
            Action::ToggleGrouping => "toggle-grouping",
            Action::ToggleWrap => "toggle-wrap",
            Action::ScrollLeft => "scroll-left",
            Action::ScrollRight => "scroll-right",
            Action::FoldBlock => "fold-block",
            Action::JumpToInput => "jump-to-input",
            Action::CycleConsumers => "cycle-consumers",
            Action::PrevBlock => "prev-block",
            Action::NextBlock => "next-block",
            Action::ToggleSidebar => "toggle-sidebar",
            Action::ToggleSourcePane => "source-pane",
            Action::InliningPanel => "inlining",
            Action::DualDiff => "dual-diff",
            Action::JumpBack => "jump-back",
            Action::JumpForward => "jump-forward",
            Action::Search => "search",
            Action::SearchNext => "search-next",
            Action::SearchPrev => "search-prev",
            Action::Filter => "filter",
            Action::ToggleAnnotations => "toggle-annotations",
            Action::Yank => "yank",
            Action::YankSection => "yank-section",
            Action::Export => "export",
            Action::CommandPalette => "command-palette",
            Action::ToggleTimeline => "toggle-timeline",
            Action::SplitVertical => "split-vertical",
            Action::SplitHorizontal => "split-horizontal",
            Action::OtherPane => "other-pane",
            Action::Diff => "diff",
            Action::FoldAllBlocks => "fold-all-blocks",
        }
    }

    /// One-line description for the help modal.
    pub fn describe(self) -> &'static str {
        match self {
            Action::Quit => "quit",
            Action::Back => "back / unfocus",
            Action::Help => "this help",
            Action::Up => "move up",
            Action::Down => "move down",
            Action::HalfPageDown => "half page down",
            Action::HalfPageUp => "half page up",
            Action::PageDown => "page down",
            Action::PageUp => "page up",
            Action::Top => "jump to top",
            Action::Bottom => "jump to bottom",
            Action::FocusSidebar => "focus sidebar",
            Action::FocusViewport => "focus viewport",
            Action::Select => "expand / collapse / focus",
            Action::NextSource => "next source",
            Action::ToggleFollow => "follow the stream end",
            Action::ToggleGrouping => "sidebar: chronological / by function",
            Action::ToggleWrap => "wrap long lines",
            Action::ScrollLeft => "scroll left",
            Action::ScrollRight => "scroll right",
            Action::FoldBlock => "fold / unfold basic block",
            Action::JumpToInput => "jump to input definition",
            Action::CycleConsumers => "cycle consumers",
            Action::PrevBlock => "previous block header",
            Action::NextBlock => "next block header",
            Action::ToggleSidebar => "show / hide the sidebar",
            Action::ToggleSourcePane => "JS source alignment pane",
            Action::InliningPanel => "inlining decisions panel",
            Action::DualDiff => "diff against the other run",
            Action::JumpBack => "jump history back",
            Action::JumpForward => "jump history forward",
            Action::Search => "regex search",
            Action::SearchNext => "next match",
            Action::SearchPrev => "previous match",
            Action::Filter => "filter sidebar",
            Action::ToggleAnnotations => "show trace annotations",
            Action::Yank => "copy the cursor line",
            Action::YankSection => "copy the whole section",
            Action::Export => "export section to a file",
            Action::CommandPalette => "command palette (:checks, :deopts, …)",
            Action::ToggleTimeline => "timeline ⇄ compilation list",
            Action::SplitVertical => "vertical split (again: close)",
            Action::SplitHorizontal => "horizontal split (again: close)",
            Action::OtherPane => "focus the other pane",
            Action::Diff => "phase diff mode",
            Action::FoldAllBlocks => "fold / unfold all blocks",
        }
    }

    fn all() -> &'static [Action] {
        use Action::*;
        &[
            Quit,
            Back,
            Help,
            Up,
            Down,
            HalfPageDown,
            HalfPageUp,
            PageDown,
            PageUp,
            Top,
            Bottom,
            FocusSidebar,
            FocusViewport,
            Select,
            NextSource,
            ToggleFollow,
            ToggleGrouping,
            ToggleWrap,
            ScrollLeft,
            ScrollRight,
            FoldBlock,
            JumpToInput,
            CycleConsumers,
            PrevBlock,
            NextBlock,
            ToggleSidebar,
            ToggleSourcePane,
            InliningPanel,
            DualDiff,
            JumpBack,
            JumpForward,
            Search,
            SearchNext,
            SearchPrev,
            Filter,
            ToggleAnnotations,
            Yank,
            YankSection,
            Export,
            CommandPalette,
            ToggleTimeline,
            SplitVertical,
            SplitHorizontal,
            OtherPane,
            Diff,
            FoldAllBlocks,
        ]
    }
}

/// A concrete key press. `Char` chords ignore `SHIFT` — the shifted character
/// itself carries the distinction (`N` vs `n`), and terminals disagree about
/// whether they also set the modifier.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct Chord {
    pub mods: KeyModifiers,
    pub code: KeyCode,
}

impl Chord {
    pub fn from_event(key: &KeyEvent) -> Self {
        let mut mods = key.modifiers;
        if matches!(key.code, KeyCode::Char(_)) {
            mods -= KeyModifiers::SHIFT;
        }
        Chord {
            mods,
            code: key.code,
        }
    }

    /// Parses `"q"`, `"Ctrl+d"`, `"Esc"`, `"PgDn"`, `"Space"`, `"?"`, …
    pub fn parse(spec: &str) -> Result<Self> {
        let mut mods = KeyModifiers::NONE;
        let mut rest = spec;
        loop {
            let lower = rest.to_ascii_lowercase();
            if let Some(r) = lower.starts_with("ctrl+").then(|| &rest[5..]) {
                mods |= KeyModifiers::CONTROL;
                rest = r;
            } else if let Some(r) = lower.starts_with("alt+").then(|| &rest[4..]) {
                mods |= KeyModifiers::ALT;
                rest = r;
            } else {
                break;
            }
        }

        let code = match rest {
            "" => bail!("empty key in {spec:?}"),
            "Esc" | "esc" => KeyCode::Esc,
            "Enter" | "enter" => KeyCode::Enter,
            "Tab" | "tab" => KeyCode::Tab,
            "Space" | "space" => KeyCode::Char(' '),
            "Up" | "up" => KeyCode::Up,
            "Down" | "down" => KeyCode::Down,
            "Left" | "left" => KeyCode::Left,
            "Right" | "right" => KeyCode::Right,
            "PgUp" | "pgup" | "PageUp" => KeyCode::PageUp,
            "PgDn" | "pgdn" | "PageDown" => KeyCode::PageDown,
            "Home" | "home" => KeyCode::Home,
            "End" | "end" => KeyCode::End,
            s => {
                let mut chars = s.chars();
                match (chars.next(), chars.next()) {
                    (Some(c), None) => KeyCode::Char(c),
                    _ => bail!("unknown key {spec:?}"),
                }
            }
        };
        Ok(Chord { mods, code })
    }

    /// Render for the help modal.
    pub fn display(&self) -> String {
        let mut out = String::new();
        if self.mods.contains(KeyModifiers::CONTROL) {
            out.push_str("Ctrl+");
        }
        if self.mods.contains(KeyModifiers::ALT) {
            out.push_str("Alt+");
        }
        match self.code {
            KeyCode::Char(' ') => out.push_str("Space"),
            KeyCode::Char(c) => out.push(c),
            KeyCode::Esc => out.push_str("Esc"),
            KeyCode::Enter => out.push_str("Enter"),
            KeyCode::Tab => out.push_str("Tab"),
            KeyCode::Up => out.push('↑'),
            KeyCode::Down => out.push('↓'),
            KeyCode::Left => out.push('←'),
            KeyCode::Right => out.push('→'),
            KeyCode::PageUp => out.push_str("PgUp"),
            KeyCode::PageDown => out.push_str("PgDn"),
            KeyCode::Home => out.push_str("Home"),
            KeyCode::End => out.push_str("End"),
            other => out.push_str(&format!("{other:?}")),
        }
        out
    }
}

#[derive(Debug)]
pub struct Keymap {
    bindings: HashMap<Chord, Action>,
}

impl Keymap {
    /// PLAN §8, with the additions Phase 3 needed (grouping, wrap, horizontal
    /// scroll, source switching) — all documented in the help modal.
    pub fn defaults() -> Vec<(Action, Vec<&'static str>)> {
        vec![
            (Action::Quit, vec!["q", "Ctrl+c"]),
            (Action::Back, vec!["Esc"]),
            (Action::Help, vec!["?"]),
            (Action::Up, vec!["k", "Up"]),
            (Action::Down, vec!["j", "Down"]),
            (Action::HalfPageDown, vec!["Ctrl+d"]),
            (Action::HalfPageUp, vec!["Ctrl+u"]),
            (Action::PageDown, vec!["PgDn"]),
            (Action::PageUp, vec!["PgUp"]),
            (Action::Top, vec!["g", "Home"]),
            (Action::Bottom, vec!["G", "End"]),
            (Action::FocusSidebar, vec!["h", "Left"]),
            (Action::FocusViewport, vec!["l", "Right"]),
            (Action::Select, vec!["Enter"]),
            // Tab belongs to the timeline per PLAN §8; `]` moved on to block
            // navigation once that landed, pushing source switching to `}`.
            (Action::NextSource, vec!["}"]),
            (Action::ToggleFollow, vec!["F"]),
            (Action::ToggleGrouping, vec!["c"]),
            (Action::ToggleWrap, vec!["w"]),
            (Action::ScrollLeft, vec!["<"]),
            (Action::ScrollRight, vec![">"]),
            (Action::FoldBlock, vec!["Space"]),
            (Action::JumpToInput, vec!["i"]),
            (Action::CycleConsumers, vec!["u"]),
            (Action::PrevBlock, vec!["["]),
            (Action::NextBlock, vec!["]"]),
            (Action::ToggleSidebar, vec!["b"]),
            (Action::ToggleSourcePane, vec!["S"]),
            (Action::InliningPanel, vec!["I"]),
            (Action::DualDiff, vec!["D"]),
            (Action::JumpBack, vec!["Ctrl+o"]),
            (Action::JumpForward, vec!["Ctrl+i"]),
            (Action::Search, vec!["/"]),
            (Action::SearchNext, vec!["n"]),
            (Action::SearchPrev, vec!["N"]),
            (Action::Filter, vec!["f"]),
            (Action::ToggleAnnotations, vec!["t"]),
            (Action::Yank, vec!["y"]),
            (Action::YankSection, vec!["Y"]),
            (Action::Export, vec!["E"]),
            (Action::CommandPalette, vec![":"]),
            (Action::ToggleTimeline, vec!["Tab"]),
            (Action::SplitVertical, vec!["v"]),
            (Action::SplitHorizontal, vec!["s"]),
            (Action::OtherPane, vec!["Ctrl+w"]),
            (Action::Diff, vec!["d"]),
            (Action::FoldAllBlocks, vec!["z"]),
        ]
    }

    /// Builds the map from defaults plus user overrides. An action named in
    /// the config loses its default chords first, so remapping frees the old
    /// key. A chord bound to two actions is an error, not a precedence rule.
    pub fn build(overrides: &HashMap<String, Vec<String>>) -> Result<Keymap> {
        let mut by_action: HashMap<Action, Vec<Chord>> = HashMap::new();
        for (action, specs) in Self::defaults() {
            let chords = specs
                .iter()
                .map(|s| Chord::parse(s).expect("default keymap parses"))
                .collect();
            by_action.insert(action, chords);
        }

        let known: HashMap<&'static str, Action> =
            Action::all().iter().map(|a| (a.name(), *a)).collect();
        for (name, specs) in overrides {
            let Some(&action) = known.get(name.as_str()) else {
                bail!(
                    "unknown action {name:?} in [keys] (known: {})",
                    known.keys().copied().collect::<Vec<_>>().join(", ")
                );
            };
            let mut chords = Vec::new();
            for spec in specs {
                chords.push(
                    Chord::parse(spec)
                        .with_context(|| format!("in [keys] {name} = [... {spec:?} ...]"))?,
                );
            }
            by_action.insert(action, chords);
        }

        let mut bindings = HashMap::new();
        for (&action, chords) in &by_action {
            for &chord in chords {
                // A chord listed twice for the *same* action is harmless
                // redundancy, not a conflict.
                if let Some(previous) = bindings.insert(chord, action)
                    && previous != action
                {
                    bail!(
                        "key {:?} is bound to both {} and {}",
                        chord.display(),
                        previous.name(),
                        action.name()
                    );
                }
            }
        }
        Ok(Keymap { bindings })
    }

    pub fn lookup(&self, key: &KeyEvent) -> Option<Action> {
        self.bindings.get(&Chord::from_event(key)).copied()
    }

    /// One displayable chord for an action, for inline hints ("[1/2 …]").
    /// Hints must come from the live keymap: a hard-coded key name goes
    /// stale the moment the action is rebound (found in review — the
    /// telemetry bar still said Tab after Tab became the timeline).
    pub fn chord_hint(&self, action: Action) -> Option<String> {
        let mut chords: Vec<String> = self
            .bindings
            .iter()
            .filter(|(_, a)| **a == action)
            .map(|(c, _)| c.display())
            .collect();
        chords.sort();
        chords.into_iter().next()
    }

    /// `(chords, action)` pairs for the help modal, in the stable order of
    /// [`Keymap::defaults`].
    pub fn help_rows(&self) -> Vec<(String, Action)> {
        let mut by_action: HashMap<Action, Vec<Chord>> = HashMap::new();
        for (&chord, &action) in &self.bindings {
            by_action.entry(action).or_default().push(chord);
        }
        Self::defaults()
            .iter()
            .filter_map(|(action, _)| {
                let mut chords = by_action.remove(action)?;
                chords.sort_by_key(|c| c.display());
                let keys = chords
                    .iter()
                    .map(|c| c.display())
                    .collect::<Vec<_>>()
                    .join(" / ");
                Some((keys, *action))
            })
            .collect()
    }
}

pub struct Config {
    pub keys: Keymap,
}

impl Config {
    /// Loads `--config <path>` (must exist), else the default location (may
    /// be absent), else pure defaults. Only a genuinely *missing* default
    /// config falls back silently — an unreadable or non-UTF-8 one is an
    /// error, not a silent shrug that makes the user's remaps vanish.
    pub fn load(explicit: Option<&Path>) -> Result<Config> {
        let text = match explicit {
            Some(path) => Some(
                std::fs::read_to_string(path)
                    .with_context(|| format!("cannot read config {}", path.display()))?,
            ),
            None => match default_path() {
                Some(path) => match std::fs::read_to_string(&path) {
                    Ok(text) => Some(text),
                    Err(e) if e.kind() == std::io::ErrorKind::NotFound => None,
                    Err(e) => {
                        return Err(e)
                            .with_context(|| format!("cannot read config {}", path.display()));
                    }
                },
                None => None,
            },
        };
        match text {
            Some(text) => Self::parse(&text),
            None => Ok(Config {
                keys: Keymap::build(&HashMap::new()).expect("defaults are consistent"),
            }),
        }
    }

    fn parse(text: &str) -> Result<Config> {
        let value: toml::Table = toml::parse_table(text).context("config is not valid TOML")?;
        let mut overrides: HashMap<String, Vec<String>> = HashMap::new();
        if let Some(toml::Value::Table(keys)) = value.get("keys") {
            for (name, v) in keys {
                let specs = match v {
                    toml::Value::String(s) => vec![s.clone()],
                    toml::Value::Array(a) => a
                        .iter()
                        .map(|s| {
                            s.as_str().map(str::to_string).ok_or_else(|| {
                                anyhow::anyhow!("[keys] {name}: entries must be strings")
                            })
                        })
                        .collect::<Result<_>>()?,
                    _ => bail!("[keys] {name}: expected a string or array of strings"),
                };
                overrides.insert(name.clone(), specs);
            }
        }
        Ok(Config {
            keys: Keymap::build(&overrides)?,
        })
    }
}

fn default_path() -> Option<PathBuf> {
    let base = std::env::var_os("XDG_CONFIG_HOME")
        .map(PathBuf::from)
        .or_else(|| std::env::var_os("HOME").map(|h| PathBuf::from(h).join(".config")))?;
    Some(base.join("garage/config.toml"))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn defaults_have_no_conflicts() {
        let keymap = Keymap::build(&HashMap::new()).unwrap();
        let key = |code, mods| KeyEvent::new(code, mods);
        assert_eq!(
            keymap.lookup(&key(KeyCode::Char('q'), KeyModifiers::NONE)),
            Some(Action::Quit)
        );
        // Shifted characters match regardless of the reported SHIFT modifier.
        assert_eq!(
            keymap.lookup(&key(KeyCode::Char('N'), KeyModifiers::SHIFT)),
            Some(Action::SearchPrev)
        );
        assert_eq!(
            keymap.lookup(&key(KeyCode::Char('d'), KeyModifiers::CONTROL)),
            Some(Action::HalfPageDown)
        );
    }

    #[test]
    fn chord_parsing() {
        assert_eq!(
            Chord::parse("Ctrl+d").unwrap(),
            Chord {
                mods: KeyModifiers::CONTROL,
                code: KeyCode::Char('d')
            }
        );
        assert_eq!(Chord::parse("Space").unwrap().code, KeyCode::Char(' '));
        assert_eq!(Chord::parse("?").unwrap().code, KeyCode::Char('?'));
        assert!(Chord::parse("Hyper+x").is_err());
        assert!(Chord::parse("").is_err());
    }

    #[test]
    fn remapping_frees_the_default_key() {
        let mut overrides = HashMap::new();
        overrides.insert("quit".to_string(), vec!["x".to_string()]);
        let keymap = Keymap::build(&overrides).unwrap();
        let key = |c| KeyEvent::new(KeyCode::Char(c), KeyModifiers::NONE);
        assert_eq!(keymap.lookup(&key('x')), Some(Action::Quit));
        assert_eq!(keymap.lookup(&key('q')), None, "q was freed by the remap");
    }

    #[test]
    fn conflicting_bindings_are_an_error() {
        let mut overrides = HashMap::new();
        overrides.insert("help".to_string(), vec!["q".to_string()]);
        let err = Keymap::build(&overrides).unwrap_err().to_string();
        assert!(err.contains("bound to both"), "{err}");
    }

    #[test]
    fn config_toml_round_trip() {
        let config = Config::parse("[keys]\nquit = \"x\"\ndown = [\"j\", \"Down\"]\n").unwrap();
        let key = KeyEvent::new(KeyCode::Char('x'), KeyModifiers::NONE);
        assert_eq!(config.keys.lookup(&key), Some(Action::Quit));

        assert!(Config::parse("[keys]\nnope = \"x\"\n").is_err());
        assert!(Config::parse("[keys]\nquit = 3\n").is_err());
        assert!(Config::parse("not toml [").is_err());
    }
}
