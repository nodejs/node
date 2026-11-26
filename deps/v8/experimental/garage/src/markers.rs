// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! The version-keyed marker table.
//!
//! Phase-banner *names* are version-specific — every one changed between V8
//! 14.9 and 15.2 — while the banner *grammar* is stable. The names live in
//! `assets/markers.toml` (embedded at compile time) so that a routine V8
//! rename is a TOML edit, not a parser change (journey J5). The table is
//! version-keyed, not arch-keyed: architecture only affects register names
//! inside node lines (spike-findings.md §4).

use std::collections::HashMap;
use std::sync::OnceLock;

use crate::{toml_min as toml, tracing};

const MARKERS_TOML: &str = include_str!("../assets/markers.toml");

pub struct MarkerTable {
    /// phase name → versions that use it. One name can in principle appear in
    /// several versions; the detector counts votes.
    phases: HashMap<String, Vec<String>>,
}

impl MarkerTable {
    /// The embedded table. Parsing failure of compile-time data is a build
    /// defect, but the promise is "never panic at runtime on input" — and an
    /// empty table only downgrades phases to `known: false`, so degrade.
    pub fn get() -> &'static MarkerTable {
        static TABLE: OnceLock<MarkerTable> = OnceLock::new();
        TABLE.get_or_init(|| match Self::parse(MARKERS_TOML) {
            Ok(table) => table,
            Err(e) => {
                tracing::error!("embedded markers.toml is invalid: {e}");
                MarkerTable {
                    phases: HashMap::new(),
                }
            }
        })
    }

    fn parse(text: &str) -> Result<MarkerTable, toml::de::Error> {
        let value: toml::Table = toml::parse_table(text)?;
        let mut phases: HashMap<String, Vec<String>> = HashMap::new();

        if let Some(toml::Value::Table(versions)) = value.get("maglev") {
            for (version, entry) in versions {
                let Some(names) = entry.get("phases").and_then(|p| p.as_array()) else {
                    continue;
                };
                for name in names.iter().filter_map(|n| n.as_str()) {
                    phases
                        .entry(name.to_string())
                        .or_default()
                        .push(version.clone());
                }
            }
        }
        Ok(MarkerTable { phases })
    }

    /// Is this banner name a known phase, and for which versions?
    pub fn phase_versions(&self, name: &str) -> Option<&[String]> {
        self.phases.get(name).map(|v| v.as_slice())
    }
}

/// Counts marker-table votes per version and reports the winner — how the
/// indexer infers which V8 produced a trace (the trace itself never says).
#[derive(Debug, Default)]
pub struct VersionVotes {
    votes: HashMap<String, u32>,
}

impl VersionVotes {
    pub fn record(&mut self, versions: &[String]) {
        for v in versions {
            *self.votes.entry(v.clone()).or_default() += 1;
        }
    }

    pub fn detected(&self) -> Option<&str> {
        self.votes
            .iter()
            // Tie-break toward the newest version when phase vocabularies
            // overlap (e.g. 16.0 dropped LICM from 15.2's list).
            .max_by_key(|(version, count)| (**count, parse_version_key(version), version.as_str()))
            .map(|(version, _)| version.as_str())
    }
}

fn parse_version_key(v: &str) -> (u32, u32) {
    let mut parts = v.split('.').map(|p| p.parse::<u32>().unwrap_or(0));
    (parts.next().unwrap_or(0), parts.next().unwrap_or(0))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn embedded_table_parses_and_knows_versions() {
        let t = MarkerTable::get();
        let v = t.phase_versions("Maglev graph building").unwrap();
        assert!(v.contains(&"16.0".to_string()));
        assert!(v.contains(&"15.2".to_string()));
        assert_eq!(
            t.phase_versions("After graph building").unwrap(),
            ["14.9".to_string()]
        );
        assert!(t.phase_versions("Bytecode array").is_none());
        assert!(t.phase_versions("No such phase").is_none());
    }

    #[test]
    fn version_detection_takes_the_majority() {
        let t = MarkerTable::get();
        let mut votes = VersionVotes::default();
        votes.record(t.phase_versions("Maglev graph building").unwrap());
        votes.record(t.phase_versions("Phi untagging").unwrap());
        votes.record(t.phase_versions("After truncation").unwrap());
        assert_eq!(votes.detected(), Some("16.0"));
    }
}
