// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Lazy per-compilation parsing.
//!
//! The indexer records section boundaries; nothing inside a compilation is
//! parsed until someone actually views it. Parsed results are cached, keyed by
//! the section's current end line so that a still-streaming compilation is
//! re-parsed when it grows and served from cache once it stops.

pub mod listing;
pub mod maglev;
pub mod turbofan;

use std::collections::HashMap;
use std::sync::Arc;

use crate::model::{CompilationSection, ParsedCompilation};
use crate::source::LogBuffer;
use crate::tracing;

#[derive(Default)]
pub struct ParseCache {
    entries: HashMap<usize, Entry>,
}

struct Entry {
    /// The section's end line at parse time; a grown section re-parses.
    end_line: usize,
    parsed: Arc<ParsedCompilation>,
}

impl ParseCache {
    pub fn get_or_parse(
        &mut self,
        buffer: &LogBuffer,
        section: &CompilationSection,
        index: usize,
    ) -> Arc<ParsedCompilation> {
        if let Some(entry) = self.entries.get(&index)
            && entry.end_line == section.lines.end
        {
            return Arc::clone(&entry.parsed);
        }

        let started = std::time::Instant::now();
        let parsed = Arc::new(maglev::parse_compilation(buffer, section));
        tracing::debug!(
            compilation = index,
            lines = section.lines.len(),
            micros = started.elapsed().as_micros(),
            "parsed compilation"
        );
        self.entries.insert(
            index,
            Entry {
                end_line: section.lines.end,
                parsed: Arc::clone(&parsed),
            },
        );
        parsed
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::index::TraceIndex;

    const TRACE: &str = "\
Compiling 0x1 <JSFunction f (sfi = 0x10)> with Maglev
----- Maglev graph building -----
   1: Foo
";

    #[test]
    fn second_view_hits_the_cache() {
        let mut buffer = LogBuffer::new();
        buffer.append(TRACE.as_bytes());
        buffer.finish();
        let mut idx = TraceIndex::new(None);
        idx.ingest(&buffer, true);

        let mut cache = ParseCache::default();
        let a = cache.get_or_parse(&buffer, &idx.compilations[0], 0);
        let b = cache.get_or_parse(&buffer, &idx.compilations[0], 0);
        assert!(Arc::ptr_eq(&a, &b), "same section must not re-parse");
    }

    #[test]
    fn a_grown_section_reparses() {
        let mut buffer = LogBuffer::new();
        buffer.append(TRACE.as_bytes());
        let mut idx = TraceIndex::new(None);
        idx.ingest(&buffer, false);

        let mut cache = ParseCache::default();
        let before = cache.get_or_parse(&buffer, &idx.compilations[0], 0);

        buffer.append(b"   2: Bar\n");
        buffer.finish();
        idx.ingest(&buffer, true);
        let after = cache.get_or_parse(&buffer, &idx.compilations[0], 0);
        assert!(!Arc::ptr_eq(&before, &after), "grown section must re-parse");
        assert!(after.phases[0].node_count > before.phases[0].node_count);
    }
}
