// Copyright 2012 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! The SourceMap tracks all the source code used within a single crate.
//!
//! The mapping from integer byte positions to the original source code location
//! is stored in `spans`.
//!
//! Each bit of source parsed during crate parsing (typically files, in-memory
//! strings, or various bits of macro expansion) cover a continuous range of
//! bytes in the SourceMap and are represented by SourceFiles. Byte positions
//! are stored in `spans` and used pervasively in the compiler. They are
//! absolute positions within the SourceMap, which upon request can be converted
//! to line and column information, source code snippets, etc.
use std::{
    cmp, env, fs,
    hash::Hash,
    io,
    path::{Path, PathBuf},
    sync::atomic::{AtomicUsize, Ordering::SeqCst},
};

use bytes_str::BytesStr;
use once_cell::sync::Lazy;
use rustc_hash::FxHashMap;
#[cfg(feature = "sourcemap")]
use swc_sourcemap::SourceMapBuilder;
use tracing::debug;

pub use crate::syntax_pos::*;
use crate::{
    errors::SourceMapper,
    rustc_data_structures::stable_hasher::StableHasher,
    sync::{Lock, LockGuard, Lrc, MappedLockGuard},
};

static CURRENT_DIR: Lazy<Option<PathBuf>> = Lazy::new(|| env::current_dir().ok());

// _____________________________________________________________________________
// SourceFile, MultiByteChar, FileName, FileLines
//

/// An abstraction over the fs operations used by the Parser.
pub trait FileLoader {
    /// Query the existence of a file.
    fn file_exists(&self, path: &Path) -> bool;

    /// Return an absolute path to a file, if possible.
    fn abs_path(&self, path: &Path) -> Option<PathBuf>;

    /// Read the contents of an UTF-8 file into memory.
    fn read_file(&self, path: &Path) -> io::Result<BytesStr>;
}

/// A FileLoader that uses std::fs to load real files.
pub struct RealFileLoader;

impl FileLoader for RealFileLoader {
    fn file_exists(&self, path: &Path) -> bool {
        fs::metadata(path).is_ok()
    }

    fn abs_path(&self, path: &Path) -> Option<PathBuf> {
        if path.is_absolute() {
            Some(path.to_path_buf())
        } else {
            CURRENT_DIR.as_ref().map(|cwd| cwd.join(path))
        }
    }

    fn read_file(&self, path: &Path) -> io::Result<BytesStr> {
        let bytes = fs::read(path)?;
        BytesStr::from_utf8(bytes.into()).map_err(|_| {
            io::Error::new(
                io::ErrorKind::InvalidData,
                "Failed to convert bytes to UTF-8",
            )
        })
    }
}

// This is a SourceFile identifier that is used to correlate SourceFiles between
// subsequent compilation sessions (which is something we need to do during
// incremental compilation).
#[derive(Copy, Clone, PartialEq, Eq, Hash, Debug)]
pub struct StableSourceFileId(u128);

impl StableSourceFileId {
    pub fn new(source_file: &SourceFile) -> StableSourceFileId {
        let mut hasher = StableHasher::new();

        source_file.name.hash(&mut hasher);
        source_file.name_was_remapped.hash(&mut hasher);
        source_file.unmapped_path.hash(&mut hasher);

        StableSourceFileId(hasher.finish())
    }
}

// _____________________________________________________________________________
// SourceMap
//

#[derive(Default)]
pub(super) struct SourceMapFiles {
    pub(super) source_files: Vec<Lrc<SourceFile>>,
    stable_id_to_source_file: FxHashMap<StableSourceFileId, Lrc<SourceFile>>,
}

/// The interner for spans.
///
/// As most spans are simply stored, we store them as interned form.
///
///  - Each ast node only stores pointer to actual data ([BytePos]).
///  - The pointers ([BytePos]) can be converted to file name, line and column
///    using this struct.
///
/// # Note
///
/// This struct should be shared. `swc_common` uses [crate::sync::Lrc], which is
/// [std::rc::Rc] or [std::sync::Arc], depending on the compile option, for this
/// purpose.
///
/// ## Note for bundler authors
///
/// If you are bundling modules, you should share this struct while parsing
/// modules. Otherwise, you have to implement a code generator which accepts
/// multiple [SourceMap].
pub struct SourceMap {
    pub(super) files: Lock<SourceMapFiles>,
    start_pos: AtomicUsize,
    file_loader: Box<dyn FileLoader + Sync + Send>,
    // This is used to apply the file path remapping as specified via
    // --remap-path-prefix to all SourceFiles allocated within this SourceMap.
    path_mapping: FilePathMapping,
    /// In case we are in a doctest, replace all file names with the PathBuf,
    /// and add the given offsets to the line info
    doctest_offset: Option<(FileName, isize)>,
}

impl Default for SourceMap {
    fn default() -> Self {
        Self::new(FilePathMapping::empty())
    }
}

impl SourceMap {
    pub fn new(path_mapping: FilePathMapping) -> SourceMap {
        SourceMap {
            files: Default::default(),
            start_pos: AtomicUsize::new(1),
            file_loader: Box::new(RealFileLoader),
            path_mapping,
            doctest_offset: None,
        }
    }

    pub fn with_file_loader(
        file_loader: Box<dyn FileLoader + Sync + Send>,
        path_mapping: FilePathMapping,
    ) -> SourceMap {
        SourceMap {
            files: Default::default(),
            start_pos: AtomicUsize::new(1),
            file_loader,
            path_mapping,
            doctest_offset: None,
        }
    }

    pub fn path_mapping(&self) -> &FilePathMapping {
        &self.path_mapping
    }

    pub fn file_exists(&self, path: &Path) -> bool {
        self.file_loader.file_exists(path)
    }

    pub fn load_file(&self, path: &Path) -> io::Result<Lrc<SourceFile>> {
        let src = self.file_loader.read_file(path)?;
        let filename = Lrc::new(path.to_path_buf().into());
        Ok(self.new_source_file(filename, src))
    }

    pub fn files(&self) -> MappedLockGuard<'_, Vec<Lrc<SourceFile>>> {
        LockGuard::map(self.files.borrow(), |files| &mut files.source_files)
    }

    pub fn source_file_by_stable_id(
        &self,
        stable_id: StableSourceFileId,
    ) -> Option<Lrc<SourceFile>> {
        self.files
            .borrow()
            .stable_id_to_source_file
            .get(&stable_id)
            .cloned()
    }

    fn next_start_pos(&self, len: usize) -> usize {
        // Add one so there is some space between files. This lets us distinguish
        // positions in the source_map, even in the presence of zero-length files.
        self.start_pos.fetch_add(len + 1, SeqCst)
    }

    /// Creates a new source_file.
    /// This does not ensure that only one SourceFile exists per file name.
    ///
    /// - `src` should not have UTF8 BOM
    /// - `&'static str` and [String] implements `Into<BytesStr>`
    #[inline(always)]
    pub fn new_source_file(
        &self,
        filename: Lrc<FileName>,
        src: impl Into<BytesStr>,
    ) -> Lrc<SourceFile> {
        self.new_source_file_impl(filename, src.into())
    }

    fn new_source_file_impl(&self, filename: Lrc<FileName>, mut src: BytesStr) -> Lrc<SourceFile> {
        remove_bom(&mut src);

        // The path is used to determine the directory for loading submodules and
        // include files, so it must be before remapping.
        // Note that filename may not be a valid path, eg it may be `<anon>` etc,
        // but this is okay because the directory determined by `path.pop()` will
        // be empty, so the working directory will be used.
        let unmapped_path = filename.clone();

        let (filename, was_remapped) = match &*filename {
            FileName::Real(filename) => {
                let (filename, was_remapped) = self.path_mapping.map_prefix(filename);
                (Lrc::new(FileName::Real(filename)), was_remapped)
            }
            _ => (filename, false),
        };

        // We hold lock at here to prevent panic
        // If we don't do this, lookup_char_pos and its family **may** panic.
        let mut files = self.files.borrow_mut();

        let start_pos = self.next_start_pos(src.len());

        let source_file = Lrc::new(SourceFile::new(
            filename,
            was_remapped,
            unmapped_path,
            src,
            SmallPos::from_usize(start_pos),
        ));

        {
            files.source_files.push(source_file.clone());
            files
                .stable_id_to_source_file
                .insert(StableSourceFileId::new(&source_file), source_file.clone());
        }

        source_file
    }

    pub fn mk_substr_filename(&self, sp: Span) -> String {
        let pos = self.lookup_char_pos(sp.lo());
        format!(
            "<{}:{}:{}>",
            pos.file.name,
            pos.line,
            pos.col.to_usize() + 1
        )
    }

    // If there is a doctest_offset, apply it to the line
    pub fn doctest_offset_line(&self, mut orig: usize) -> usize {
        if let Some((_, line)) = self.doctest_offset {
            if line >= 0 {
                orig += line as usize;
            } else {
                orig -= (-line) as usize;
            }
        }
        orig
    }

    /// Lookup source information about a BytePos
    pub fn lookup_char_pos(&self, pos: BytePos) -> Loc {
        self.try_lookup_char_pos(pos).unwrap()
    }

    /// Lookup source information about a BytePos
    pub fn try_lookup_char_pos(&self, pos: BytePos) -> Result<Loc, SourceMapLookupError> {
        let fm = self.try_lookup_source_file(pos)?.unwrap();
        self.try_lookup_char_pos_with(fm, pos)
    }

    /// Lookup source information about a BytePos
    ///
    ///
    /// This method exists only for optimization and it's not part of public
    /// api.
    #[doc(hidden)]
    pub fn lookup_char_pos_with(&self, fm: Lrc<SourceFile>, pos: BytePos) -> Loc {
        self.try_lookup_char_pos_with(fm, pos).unwrap()
    }

    /// Lookup source information about a BytePos
    ///
    ///
    /// This method exists only for optimization and it's not part of public
    /// api.
    #[doc(hidden)]
    pub fn try_lookup_char_pos_with(
        &self,
        fm: Lrc<SourceFile>,
        pos: BytePos,
    ) -> Result<Loc, SourceMapLookupError> {
        let line_info = self.lookup_line_with(fm, pos);
        match line_info {
            Ok(SourceFileAndLine { sf: f, line: a }) => {
                let analysis = f.analyze();
                let chpos = self.bytepos_to_file_charpos_with(&f, pos);

                let line = a + 1; // Line numbers start at 1
                let linebpos = f.analyze().lines[a];
                assert!(
                    pos >= linebpos,
                    "{}: bpos = {:?}; linebpos = {:?};",
                    f.name,
                    pos,
                    linebpos,
                );

                let linechpos = self.bytepos_to_file_charpos_with(&f, linebpos);
                let col = chpos - linechpos;

                let col_display = {
                    let start_width_idx = analysis
                        .non_narrow_chars
                        .binary_search_by_key(&linebpos, |x| x.pos())
                        .unwrap_or_else(|x| x);
                    let end_width_idx = analysis
                        .non_narrow_chars
                        .binary_search_by_key(&pos, |x| x.pos())
                        .unwrap_or_else(|x| x);
                    let special_chars = end_width_idx - start_width_idx;
                    let non_narrow: usize = analysis.non_narrow_chars
                        [start_width_idx..end_width_idx]
                        .iter()
                        .map(|x| x.width())
                        .sum();
                    col.0 - special_chars + non_narrow
                };
                if cfg!(feature = "debug") {
                    debug!(
                        "byte pos {:?} is on the line at byte pos {:?}",
                        pos, linebpos
                    );
                    debug!(
                        "char pos {:?} is on the line at char pos {:?}",
                        chpos, linechpos
                    );
                    debug!("byte is on line: {}", line);
                }
                //                assert!(chpos >= linechpos);
                Ok(Loc {
                    file: f,
                    line,
                    col,
                    col_display,
                })
            }
            Err(f) => {
                let analysis = f.analyze();
                let chpos = self.bytepos_to_file_charpos(pos)?;

                let col_display = {
                    let end_width_idx = analysis
                        .non_narrow_chars
                        .binary_search_by_key(&pos, |x| x.pos())
                        .unwrap_or_else(|x| x);
                    let non_narrow: usize = analysis.non_narrow_chars[0..end_width_idx]
                        .iter()
                        .map(|x| x.width())
                        .sum();
                    chpos.0 - end_width_idx + non_narrow
                };
                Ok(Loc {
                    file: f,
                    line: 0,
                    col: chpos,
                    col_display,
                })
            }
        }
    }

    /// If the relevant source_file is empty, we don't return a line number.
    pub fn lookup_line(&self, pos: BytePos) -> Result<SourceFileAndLine, Lrc<SourceFile>> {
        let f = self.try_lookup_source_file(pos).unwrap().unwrap();

        self.lookup_line_with(f, pos)
    }

    /// If the relevant source_file is empty, we don't return a line number.
    ///
    /// This method exists only for optimization and it's not part of public
    /// api.
    #[doc(hidden)]
    pub fn lookup_line_with(
        &self,
        f: Lrc<SourceFile>,
        pos: BytePos,
    ) -> Result<SourceFileAndLine, Lrc<SourceFile>> {
        match f.lookup_line(pos) {
            Some(line) => Ok(SourceFileAndLine { sf: f, line }),
            None => Err(f),
        }
    }

    pub fn lookup_char_pos_adj(&self, pos: BytePos) -> LocWithOpt {
        let loc = self.lookup_char_pos(pos);
        LocWithOpt {
            filename: loc.file.name.clone(),
            line: loc.line,
            col: loc.col,
            file: Some(loc.file),
        }
    }

    /// Returns `Some(span)`, a union of the lhs and rhs span.  The lhs must
    /// precede the rhs. If there are gaps between lhs and rhs, the
    /// resulting union will cross these gaps. For this to work, the spans
    /// have to be:
    ///
    ///    * the ctxt of both spans much match
    ///    * the lhs span needs to end on the same line the rhs span begins
    ///    * the lhs span must start at or before the rhs span
    pub fn merge_spans(&self, sp_lhs: Span, sp_rhs: Span) -> Option<Span> {
        let lhs_end = match self.lookup_line(sp_lhs.hi()) {
            Ok(x) => x,
            Err(_) => return None,
        };
        let rhs_begin = match self.lookup_line(sp_rhs.lo()) {
            Ok(x) => x,
            Err(_) => return None,
        };

        // if we must cross lines to merge, don't merge
        if lhs_end.line != rhs_begin.line {
            return None;
        }

        // ensure these follow the expected order and we don't overlap
        if (sp_lhs.lo() <= sp_rhs.lo()) && (sp_lhs.hi() <= sp_rhs.lo()) {
            Some(sp_lhs.to(sp_rhs))
        } else {
            None
        }
    }

    pub fn span_to_string(&self, sp: Span) -> String {
        if self.files.borrow().source_files.is_empty() && sp.is_dummy() {
            return "no-location".to_string();
        }

        let lo = self.lookup_char_pos_adj(sp.lo());
        let hi = self.lookup_char_pos_adj(sp.hi());
        format!(
            "{}:{}:{}: {}:{}",
            lo.filename,
            lo.line,
            lo.col.to_usize() + 1,
            hi.line,
            hi.col.to_usize() + 1
        )
    }

    pub fn span_to_filename(&self, sp: Span) -> Lrc<FileName> {
        self.lookup_char_pos(sp.lo()).file.name.clone()
    }

    pub fn span_to_unmapped_path(&self, sp: Span) -> Lrc<FileName> {
        self.lookup_char_pos(sp.lo())
            .file
            .unmapped_path
            .clone()
            .expect("SourceMap::span_to_unmapped_path called for imported SourceFile?")
    }

    pub fn is_multiline(&self, sp: Span) -> bool {
        let lo = self.lookup_char_pos(sp.lo());
        let hi = self.lookup_char_pos(sp.hi());
        lo.line != hi.line
    }

    pub fn span_to_lines(&self, sp: Span) -> FileLinesResult {
        if cfg!(feature = "debug") {
            debug!("span_to_lines(sp={:?})", sp);
        }

        if sp.lo() > sp.hi() {
            return Err(Box::new(SpanLinesError::IllFormedSpan(sp)));
        }

        let lo = self.lookup_char_pos(sp.lo());
        if cfg!(feature = "debug") {
            debug!("span_to_lines: lo={:?}", lo);
        }
        let hi = self.lookup_char_pos(sp.hi());
        if cfg!(feature = "debug") {
            debug!("span_to_lines: hi={:?}", hi);
        }

        if lo.file.start_pos != hi.file.start_pos {
            return Err(Box::new(SpanLinesError::DistinctSources(DistinctSources {
                begin: FilePos(lo.file.name.clone(), lo.file.start_pos),
                end: FilePos(hi.file.name.clone(), hi.file.start_pos),
            })));
        }
        assert!(hi.line >= lo.line);

        // Empty file contains no lines
        if lo.file.src.is_empty() {
            return Ok(FileLines {
                file: lo.file,
                lines: Vec::new(),
            });
        }

        let mut lines = Vec::with_capacity(hi.line - lo.line + 1);

        // The span starts partway through the first line,
        // but after that it starts from offset 0.
        let mut start_col = lo.col;

        // For every line but the last, it extends from `start_col`
        // and to the end of the line. Be careful because the line
        // numbers in Loc are 1-based, so we subtract 1 to get 0-based
        // lines.
        for line_index in lo.line - 1..hi.line - 1 {
            let line_len = lo
                .file
                .get_line(line_index)
                .map(|s| s.chars().count())
                .unwrap_or(0);
            lines.push(LineInfo {
                line_index,
                start_col,
                end_col: CharPos::from_usize(line_len),
            });
            start_col = CharPos::from_usize(0);
        }

        // For the last line, it extends from `start_col` to `hi.col`:
        lines.push(LineInfo {
            line_index: hi.line - 1,
            start_col,
            end_col: hi.col,
        });

        Ok(FileLines {
            file: lo.file,
            lines,
        })
    }

    /// Extract the source surrounding the given `Span` using the
    /// `extract_source` function. The extract function takes three
    /// arguments: a string slice containing the source, an index in
    /// the slice for the beginning of the span and an index in the slice for
    /// the end of the span.
    fn span_to_source<F, Ret>(
        &self,
        sp: Span,
        extract_source: F,
    ) -> Result<Ret, Box<SpanSnippetError>>
    where
        F: FnOnce(&str, usize, usize) -> Ret,
    {
        if sp.lo() > sp.hi() {
            return Err(Box::new(SpanSnippetError::IllFormedSpan(sp)));
        }
        if sp.lo.is_dummy() || sp.hi.is_dummy() {
            return Err(Box::new(SpanSnippetError::DummyBytePos));
        }

        let local_begin = self.try_lookup_byte_offset(sp.lo())?;
        let local_end = self.try_lookup_byte_offset(sp.hi())?;

        if local_begin.sf.start_pos != local_end.sf.start_pos {
            Err(Box::new(SpanSnippetError::DistinctSources(
                DistinctSources {
                    begin: FilePos(local_begin.sf.name.clone(), local_begin.sf.start_pos),
                    end: FilePos(local_end.sf.name.clone(), local_end.sf.start_pos),
                },
            )))
        } else {
            let start_index = local_begin.pos.to_usize();
            let end_index = local_end.pos.to_usize();
            let source_len = (local_begin.sf.end_pos - local_begin.sf.start_pos).to_usize();

            if start_index > end_index || end_index > source_len {
                return Err(Box::new(SpanSnippetError::MalformedForSourcemap(
                    MalformedSourceMapPositions {
                        name: local_begin.sf.name.clone(),
                        source_len,
                        begin_pos: local_begin.pos,
                        end_pos: local_end.pos,
                    },
                )));
            }

            let src = &local_begin.sf.src;
            Ok(extract_source(src, start_index, end_index))
        }
    }

    /// Calls `op` with the source code located at `sp`.
    pub fn with_snippet_of_span<F, Ret>(
        &self,
        sp: Span,
        op: F,
    ) -> Result<Ret, Box<SpanSnippetError>>
    where
        F: FnOnce(&str) -> Ret,
    {
        self.span_to_source(sp, |src, start_index, end_index| {
            op(&src[start_index..end_index])
        })
    }

    pub fn span_to_margin(&self, sp: Span) -> Option<usize> {
        match self.span_to_prev_source(sp) {
            Err(_) => None,
            Ok(source) => source
                .split('\n')
                .next_back()
                .map(|last_line| last_line.len() - last_line.trim_start().len()),
        }
    }

    /// Calls the given closure with the source snippet before the given `Span`
    pub fn with_span_to_prev_source<F, Ret>(
        &self,
        sp: Span,
        op: F,
    ) -> Result<Ret, Box<SpanSnippetError>>
    where
        F: FnOnce(&str) -> Ret,
    {
        self.span_to_source(sp, |src, start_index, _| op(&src[..start_index]))
    }

    /// Return the source snippet as `String` before the given `Span`
    pub fn span_to_prev_source(&self, sp: Span) -> Result<String, Box<SpanSnippetError>> {
        self.with_span_to_prev_source(sp, |s| s.to_string())
    }

    /// Calls the given closure with the source snippet after the given `Span`
    pub fn with_span_to_next_source<F, Ret>(
        &self,
        sp: Span,
        op: F,
    ) -> Result<Ret, Box<SpanSnippetError>>
    where
        F: FnOnce(&str) -> Ret,
    {
        self.span_to_source(sp, |src, _, end_index| op(&src[end_index..]))
    }

    /// Return the source snippet as `String` after the given `Span`
    pub fn span_to_next_source(&self, sp: Span) -> Result<String, Box<SpanSnippetError>> {
        self.with_span_to_next_source(sp, |s| s.to_string())
    }

    /// Extend the given `Span` to just after the previous occurrence of `c`.
    /// Return the same span if no character could be found or if an error
    /// occurred while retrieving the code snippet.
    pub fn span_extend_to_prev_char(&self, sp: Span, c: char) -> Span {
        if let Ok(prev_source) = self.span_to_prev_source(sp) {
            let prev_source = prev_source.rsplit(c).next().unwrap_or("").trim_start();
            if !prev_source.is_empty() && !prev_source.contains('\n') {
                return sp.with_lo(BytePos(sp.lo().0 - prev_source.len() as u32));
            }
        }

        sp
    }

    /// Extend the given `Span` to just after the previous occurrence of `pat`
    /// when surrounded by whitespace. Return the same span if no character
    /// could be found or if an error occurred while retrieving the code
    /// snippet.
    pub fn span_extend_to_prev_str(&self, sp: Span, pat: &str, accept_newlines: bool) -> Span {
        // assure that the pattern is delimited, to avoid the following
        //     fn my_fn()
        //           ^^^^ returned span without the check
        //     ---------- correct span
        for ws in &[" ", "\t", "\n"] {
            let pat = pat.to_owned() + ws;
            if let Ok(prev_source) = self.span_to_prev_source(sp) {
                let prev_source = prev_source.rsplit(&pat).next().unwrap_or("").trim_start();
                if !prev_source.is_empty() && (!prev_source.contains('\n') || accept_newlines) {
                    return sp.with_lo(BytePos(sp.lo().0 - prev_source.len() as u32));
                }
            }
        }

        sp
    }

    /// Extend the given `Span` to just after the next occurrence of `c`.
    /// Return the same span if no character could be found or if an error
    /// occurred while retrieving the code snippet.
    pub fn span_extend_to_next_char(&self, sp: Span, c: char) -> Span {
        if let Ok(next_source) = self.span_to_next_source(sp) {
            let next_source = next_source.split(c).next().unwrap_or("").trim_end();
            if !next_source.is_empty() && !next_source.contains('\n') {
                return sp.with_hi(BytePos(sp.hi().0 + next_source.len() as u32));
            }
        }

        sp
    }

    /// Extend the given `Span` to just after the next occurrence of `pat`
    /// when surrounded by whitespace. Return the same span if no character
    /// could be found or if an error occurred while retrieving the code
    /// snippet.
    pub fn span_extend_to_next_str(&self, sp: Span, pat: &str, accept_newlines: bool) -> Span {
        for ws in &[" ", "\t", "\n"] {
            let pat = pat.to_owned() + ws;
            if let Ok(next_source) = self.span_to_next_source(sp) {
                let next_source = next_source.split(&pat).next().unwrap_or("").trim_end();
                if !next_source.is_empty() && (!next_source.contains('\n') || accept_newlines) {
                    return sp.with_hi(BytePos(sp.hi().0 + next_source.len() as u32));
                }
            }
        }

        sp
    }

    /// Given a `Span`, try to get a shorter span ending before the first
    /// occurrence of `c` `char`
    ///
    ///
    /// # Notes
    ///
    /// This method returns a dummy span for a dummy span.
    pub fn span_until_char(&self, sp: Span, c: char) -> Span {
        if sp.is_dummy() {
            return sp;
        }

        let v = self.span_to_source(sp, |src, start_index, end_index| {
            let snippet = &src[start_index..end_index];
            let snippet = snippet.split(c).next().unwrap_or("").trim_end();
            if !snippet.is_empty() && !snippet.contains('\n') {
                sp.with_hi(BytePos(sp.lo().0 + snippet.len() as u32))
            } else {
                sp
            }
        });
        match v {
            Ok(v) => v,
            Err(_) => sp,
        }
    }

    /// Given a `Span`, try to get a shorter span ending just after the first
    /// occurrence of `char` `c`.
    ///
    /// # Notes
    ///
    /// This method returns a dummy span for a dummy span.
    pub fn span_through_char(&self, sp: Span, c: char) -> Span {
        if sp.is_dummy() {
            return sp;
        }

        if let Ok(snippet) = self.span_to_snippet(sp) {
            if let Some(offset) = snippet.find(c) {
                return sp.with_hi(BytePos(sp.lo().0 + (offset + c.len_utf8()) as u32));
            }
        }
        sp
    }

    /// Given a `Span`, get a new `Span` covering the first token and all its
    /// trailing whitespace or the original `Span`.
    ///
    /// If `sp` points to `"let mut x"`, then a span pointing at `"let "` will
    /// be returned.
    pub fn span_until_non_whitespace(&self, sp: Span) -> Span {
        let mut whitespace_found = false;

        self.span_take_while(sp, |c| {
            if !whitespace_found && c.is_whitespace() {
                whitespace_found = true;
            }

            !whitespace_found || c.is_whitespace()
        })
    }

    /// Given a `Span`, get a new `Span` covering the first token without its
    /// trailing whitespace or the original `Span` in case of error.
    ///
    /// If `sp` points to `"let mut x"`, then a span pointing at `"let"` will be
    /// returned.
    pub fn span_until_whitespace(&self, sp: Span) -> Span {
        self.span_take_while(sp, |c| !c.is_whitespace())
    }

    /// Given a `Span`, get a shorter one until `predicate` yields false.
    pub fn span_take_while<P>(&self, sp: Span, mut predicate: P) -> Span
    where
        P: for<'r> FnMut(&'r char) -> bool,
    {
        self.span_to_source(sp, |src, start_index, end_index| {
            let snippet = &src[start_index..end_index];

            let offset = snippet
                .chars()
                .take_while(&mut predicate)
                .map(|c| c.len_utf8())
                .sum::<usize>();

            sp.with_hi(BytePos(sp.lo().0 + (offset as u32)))
        })
        .unwrap_or(sp)
    }

    pub fn def_span(&self, sp: Span) -> Span {
        self.span_until_char(sp, '{')
    }

    /// Returns a new span representing just the start-point of this span
    pub fn start_point(&self, sp: Span) -> Span {
        let pos = sp.lo().0;
        let width = self.find_width_of_character_at_span(sp, false);
        let corrected_start_position = pos.checked_add(width).unwrap_or(pos);
        let end_point = BytePos(cmp::max(corrected_start_position, sp.lo().0));
        sp.with_hi(end_point)
    }

    /// Returns a new span representing just the end-point of this span
    pub fn end_point(&self, sp: Span) -> Span {
        let pos = sp.hi().0;

        let width = self.find_width_of_character_at_span(sp, false);
        let corrected_end_position = pos.checked_sub(width).unwrap_or(pos);

        let end_point = BytePos(cmp::max(corrected_end_position, sp.lo().0));
        sp.with_lo(end_point)
    }

    /// Returns a new span representing the next character after the end-point
    /// of this span
    pub fn next_point(&self, sp: Span) -> Span {
        let start_of_next_point = sp.hi().0;

        let width = self.find_width_of_character_at_span(sp, true);
        // If the width is 1, then the next span should point to the same `lo` and `hi`.
        // However, in the case of a multibyte character, where the width != 1,
        // the next span should span multiple bytes to include the whole
        // character.
        let end_of_next_point = start_of_next_point
            .checked_add(width - 1)
            .unwrap_or(start_of_next_point);

        let end_of_next_point = BytePos(cmp::max(sp.lo().0 + 1, end_of_next_point));
        Span::new(BytePos(start_of_next_point), end_of_next_point)
    }

    /// Finds the width of a character, either before or after the provided
    /// span.
    fn find_width_of_character_at_span(&self, sp: Span, forwards: bool) -> u32 {
        // Disregard malformed spans and assume a one-byte wide character.
        if sp.lo() >= sp.hi() {
            debug!("find_width_of_character_at_span: early return malformed span");
            return 1;
        }

        let local_begin = self.lookup_byte_offset(sp.lo());
        let local_end = self.lookup_byte_offset(sp.hi());
        debug!(
            "find_width_of_character_at_span: local_begin=`{:?}`, local_end=`{:?}`",
            local_begin, local_end
        );

        let start_index = local_begin.pos.to_usize();
        let end_index = local_end.pos.to_usize();
        debug!(
            "find_width_of_character_at_span: start_index=`{:?}`, end_index=`{:?}`",
            start_index, end_index
        );

        // Disregard indexes that are at the start or end of their spans, they can't fit
        // bigger characters.
        if (!forwards && end_index == usize::MIN) || (forwards && start_index == usize::MAX) {
            debug!("find_width_of_character_at_span: start or end of span, cannot be multibyte");
            return 1;
        }

        let source_len = (local_begin.sf.end_pos - local_begin.sf.start_pos).to_usize();
        debug!(
            "find_width_of_character_at_span: source_len=`{:?}`",
            source_len
        );
        // Ensure indexes are also not malformed.
        if start_index > end_index || end_index > source_len {
            debug!("find_width_of_character_at_span: source indexes are malformed");
            return 1;
        }

        // We need to extend the snippet to the end of the src rather than to end_index
        // so when searching forwards for boundaries we've got somewhere to
        // search.
        let src = &local_begin.sf.src;
        let snippet = {
            let len = src.len();
            &src[start_index..len]
        };
        debug!("find_width_of_character_at_span: snippet=`{:?}`", snippet);

        let mut target = if forwards {
            end_index + 1
        } else {
            end_index - 1
        };
        debug!(
            "find_width_of_character_at_span: initial target=`{:?}`",
            target
        );

        while !snippet.is_char_boundary(target - start_index) && target < source_len {
            target = if forwards {
                target + 1
            } else {
                match target.checked_sub(1) {
                    Some(target) => target,
                    None => {
                        break;
                    }
                }
            };
            debug!("find_width_of_character_at_span: target=`{:?}`", target);
        }
        debug!(
            "find_width_of_character_at_span: final target=`{:?}`",
            target
        );

        if forwards {
            (target - end_index) as u32
        } else {
            (end_index - target) as u32
        }
    }

    pub fn get_source_file(&self, filename: &FileName) -> Option<Lrc<SourceFile>> {
        for sf in self.files.borrow().source_files.iter() {
            if *filename == *sf.name {
                return Some(sf.clone());
            }
        }
        None
    }

    /// For a global BytePos compute the local offset within the containing
    /// SourceFile
    pub fn lookup_byte_offset(&self, bpos: BytePos) -> SourceFileAndBytePos {
        self.try_lookup_byte_offset(bpos).unwrap()
    }

    /// For a global BytePos compute the local offset within the containing
    /// SourceFile
    pub fn try_lookup_byte_offset(
        &self,
        bpos: BytePos,
    ) -> Result<SourceFileAndBytePos, SourceMapLookupError> {
        let sf = self.try_lookup_source_file(bpos)?.unwrap();
        let offset = bpos - sf.start_pos;
        Ok(SourceFileAndBytePos { sf, pos: offset })
    }

    /// Converts an absolute BytePos to a CharPos relative to the source_file.
    fn bytepos_to_file_charpos(&self, bpos: BytePos) -> Result<CharPos, SourceMapLookupError> {
        let map = self.try_lookup_source_file(bpos)?.unwrap();

        Ok(self.bytepos_to_file_charpos_with(&map, bpos))
    }

    fn bytepos_to_file_charpos_with(&self, map: &SourceFile, bpos: BytePos) -> CharPos {
        let total_extra_bytes = calc_utf16_offset(map, bpos, &mut Default::default());
        assert!(
            map.start_pos.to_u32() + total_extra_bytes <= bpos.to_u32(),
            "map.start_pos = {:?}; total_extra_bytes = {}; bpos = {:?}",
            map.start_pos,
            total_extra_bytes,
            bpos,
        );
        CharPos(bpos.to_usize() - map.start_pos.to_usize() - total_extra_bytes as usize)
    }

    /// Converts a span of absolute BytePos to a CharPos relative to the
    /// source_file.
    pub fn span_to_char_offset(&self, file: &SourceFile, span: Span) -> (u32, u32) {
        // We rename this to feel more comfortable while doing math.
        let start_offset = file.start_pos;

        let mut state = ByteToCharPosState::default();
        let start =
            span.lo.to_u32() - start_offset.to_u32() - calc_utf16_offset(file, span.lo, &mut state);
        let end =
            span.hi.to_u32() - start_offset.to_u32() - calc_utf16_offset(file, span.hi, &mut state);

        (start, end)
    }

    /// Return the index of the source_file (in self.files) which contains pos.
    ///
    /// This method exists only for optimization and it's not part of public
    /// api.
    #[doc(hidden)]
    pub fn lookup_source_file_in(
        files: &[Lrc<SourceFile>],
        pos: BytePos,
    ) -> Option<Lrc<SourceFile>> {
        if pos.is_dummy() {
            return None;
        }

        let count = files.len();

        // Binary search for the source_file.
        let mut a = 0;
        let mut b = count;
        while b - a > 1 {
            let m = (a + b) / 2;
            if files[m].start_pos > pos {
                b = m;
            } else {
                a = m;
            }
        }

        if a >= count {
            return None;
        }

        Some(files[a].clone())
    }

    /// Return the index of the source_file (in self.files) which contains pos.
    ///
    /// This is not a public api.
    #[doc(hidden)]
    pub fn lookup_source_file(&self, pos: BytePos) -> Lrc<SourceFile> {
        self.try_lookup_source_file(pos).unwrap().unwrap()
    }

    /// Return the index of the source_file (in self.files) which contains pos.
    ///
    /// This is not a public api.
    #[doc(hidden)]
    pub fn try_lookup_source_file(
        &self,
        pos: BytePos,
    ) -> Result<Option<Lrc<SourceFile>>, SourceMapLookupError> {
        let files = self.files.borrow();
        let files = &files.source_files;
        let fm = Self::lookup_source_file_in(files, pos);
        match fm {
            Some(fm) => Ok(Some(fm)),
            None => Err(SourceMapLookupError::NoFileFor(pos)),
        }
    }

    pub fn count_lines(&self) -> usize {
        self.files().iter().fold(0, |a, f| a + f.count_lines())
    }

    pub fn generate_fn_name_span(&self, span: Span) -> Option<Span> {
        let prev_span = self.span_extend_to_prev_str(span, "fn", true);
        self.span_to_snippet(prev_span)
            .map(|snippet| {
                let len = snippet
                    .find(|c: char| !c.is_alphanumeric() && c != '_')
                    .expect("no label after fn");
                prev_span.with_hi(BytePos(prev_span.lo().0 + len as u32))
            })
            .ok()
    }

    /// Take the span of a type parameter in a function signature and try to
    /// generate a span for the function name (with generics) and a new
    /// snippet for this span with the pointed type parameter as a new local
    /// type parameter.
    ///
    /// For instance:
    /// ```rust,ignore (pseudo-Rust)
    /// // Given span
    /// fn my_function(param: T)
    /// //                    ^ Original span
    ///
    /// // Result
    /// fn my_function(param: T)
    /// // ^^^^^^^^^^^ Generated span with snippet `my_function<T>`
    /// ```
    ///
    /// Attention: The method used is very fragile since it essentially
    /// duplicates the work of the parser. If you need to use this function
    /// or something similar, please consider updating the source_map
    /// functions and this function to something more robust.
    pub fn generate_local_type_param_snippet(&self, span: Span) -> Option<(Span, String)> {
        // Try to extend the span to the previous "fn" keyword to retrieve the function
        // signature
        let sugg_span = self.span_extend_to_prev_str(span, "fn", false);
        if sugg_span != span {
            if let Ok(snippet) = self.span_to_snippet(sugg_span) {
                // Consume the function name
                let mut offset = snippet
                    .find(|c: char| !c.is_alphanumeric() && c != '_')
                    .expect("no label after fn");

                // Consume the generics part of the function signature
                let mut bracket_counter = 0;
                let mut last_char = None;
                for c in snippet[offset..].chars() {
                    match c {
                        '<' => bracket_counter += 1,
                        '>' => bracket_counter -= 1,
                        '(' => {
                            if bracket_counter == 0 {
                                break;
                            }
                        }
                        _ => {}
                    }
                    offset += c.len_utf8();
                    last_char = Some(c);
                }

                // Adjust the suggestion span to encompass the function name with its generics
                let sugg_span = sugg_span.with_hi(BytePos(sugg_span.lo().0 + offset as u32));

                // Prepare the new suggested snippet to append the type parameter that triggered
                // the error in the generics of the function signature
                let mut new_snippet = if last_char == Some('>') {
                    format!("{}, ", &snippet[..(offset - '>'.len_utf8())])
                } else {
                    format!("{}<", &snippet[..offset])
                };
                new_snippet.push_str(
                    &self
                        .span_to_snippet(span)
                        .unwrap_or_else(|_| "T".to_string()),
                );
                new_snippet.push('>');

                return Some((sugg_span, new_snippet));
            }
        }

        None
    }

    #[allow(clippy::ptr_arg)]
    #[cfg(feature = "sourcemap")]
    #[cfg_attr(docsrs, doc(cfg(feature = "sourcemap")))]
    pub fn build_source_map(
        &self,
        mappings: &[(BytePos, LineCol)],
        orig: Option<swc_sourcemap::SourceMap>,
        config: impl SourceMapGenConfig,
    ) -> swc_sourcemap::SourceMap {
        build_source_map(self, mappings, orig, &config)
    }
}

/// Remove utf-8 BOM if any.
fn remove_bom(src: &mut BytesStr) {
    if src.starts_with('\u{feff}') {
        src.advance(3);
    }
}

/// Calculates the number of excess chars seen in the UTF-8 encoding of a
/// file compared with the UTF-16 encoding.
fn calc_utf16_offset(file: &SourceFile, bpos: BytePos, state: &mut ByteToCharPosState) -> u32 {
    let mut total_extra_bytes = state.total_extra_bytes;
    let mut index = state.mbc_index;
    let analysis = file.analyze();
    if bpos >= state.pos {
        let range = index..analysis.multibyte_chars.len();
        for i in range {
            let mbc = &analysis.multibyte_chars[i];
            debug!("{}-byte char at {:?}", mbc.bytes, mbc.pos);
            if mbc.pos >= bpos {
                break;
            }
            total_extra_bytes += mbc.byte_to_char_diff() as u32;
            // We should never see a byte position in the middle of a
            // character
            debug_assert!(
                bpos.to_u32() >= mbc.pos.to_u32() + mbc.bytes as u32,
                "bpos = {:?}, mbc.pos = {:?}, mbc.bytes = {:?}",
                bpos,
                mbc.pos,
                mbc.bytes
            );
            index += 1;
        }
    } else {
        let range = 0..index;
        for i in range.rev() {
            let mbc = &analysis.multibyte_chars[i];
            debug!("{}-byte char at {:?}", mbc.bytes, mbc.pos);
            if mbc.pos < bpos {
                break;
            }
            total_extra_bytes -= mbc.byte_to_char_diff() as u32;
            // We should never see a byte position in the middle of a
            // character
            debug_assert!(
                bpos.to_u32() <= mbc.pos.to_u32(),
                "bpos = {:?}, mbc.pos = {:?}",
                bpos,
                mbc.pos,
            );
            index -= 1;
        }
    }

    state.pos = bpos;
    state.total_extra_bytes = total_extra_bytes;
    state.mbc_index = index;

    total_extra_bytes
}

pub trait Files {
    /// This function is called to change the [BytePos] in AST into an unmapped,
    /// real value.
    ///
    /// By default, it returns the raw value because by default, the AST stores
    /// original values.
    fn map_raw_pos(&self, raw_pos: BytePos) -> BytePos {
        raw_pos
    }

    /// Check if the given byte position is within the given file. This has a
    /// good default implementation that will work for most cases.
    ///
    /// The passed `raw_pos` is the value passed to [Files::map_raw_pos].
    fn is_in_file(&self, f: &Lrc<SourceFile>, raw_pos: BytePos) -> bool {
        f.start_pos <= raw_pos && raw_pos < f.end_pos
    }

    /// `raw_pos` is the [BytePos] in the AST. It's the raw value passed to
    /// the source map generator.
    fn try_lookup_source_file(
        &self,
        raw_pos: BytePos,
    ) -> Result<Option<Lrc<SourceFile>>, SourceMapLookupError>;
}

impl Files for SourceMap {
    fn try_lookup_source_file(
        &self,
        pos: BytePos,
    ) -> Result<Option<Lrc<SourceFile>>, SourceMapLookupError> {
        self.try_lookup_source_file(pos)
    }
}

#[allow(clippy::ptr_arg)]
#[cfg(feature = "sourcemap")]
#[cfg_attr(docsrs, doc(cfg(feature = "sourcemap")))]
pub fn build_source_map(
    files: &impl Files,
    mappings: &[(BytePos, LineCol)],
    orig: Option<swc_sourcemap::SourceMap>,
    config: &impl SourceMapGenConfig,
) -> swc_sourcemap::SourceMap {
    let mut builder = SourceMapBuilder::new(None);

    let mut src_id = 0u32;

    // // This method is optimized based on the fact that mapping is sorted.
    // mappings.sort_by_key(|v| v.0);

    let mut cur_file: Option<Lrc<SourceFile>> = None;

    let mut prev_dst_line = u32::MAX;

    let mut ch_state = ByteToCharPosState::default();
    let mut line_state = ByteToCharPosState::default();

    for (raw_pos, lc) in mappings.iter() {
        let pos = files.map_raw_pos(*raw_pos);

        if pos.is_reserved_for_comments() {
            continue;
        }

        let lc = *lc;

        // If pos is same as a DUMMY_SP (eg BytePos(0)) or if line and col are 0,
        // ignore the mapping.
        if lc.line == 0 && lc.col == 0 && pos.is_dummy() {
            continue;
        }

        if pos == BytePos(u32::MAX) {
            builder.add_raw(lc.line, lc.col, 0, 0, Some(src_id), None, false);
            continue;
        }

        let f;
        let f = match cur_file {
            Some(ref f) if files.is_in_file(f, *raw_pos) => f,
            _ => {
                let source_file = files.try_lookup_source_file(*raw_pos).unwrap();
                if let Some(source_file) = source_file {
                    f = source_file;
                } else {
                    continue;
                }
                if config.skip(&f.name) {
                    continue;
                }
                src_id = builder.add_source(config.file_name_to_source(&f.name).into());
                // orig.adjust_mappings below will throw this out if orig is Some
                if orig.is_none() && config.ignore_list(&f.name) {
                    builder.add_to_ignore_list(src_id);
                }

                // orig.adjust_mappings below will throw this out if orig is Some
                let inline_sources_content =
                    orig.is_none() && config.inline_sources_content(&f.name);
                if inline_sources_content {
                    builder.set_source_contents(src_id, Some(f.src.clone()));
                }

                ch_state = ByteToCharPosState::default();
                line_state = ByteToCharPosState::default();

                cur_file = Some(f.clone());
                &f
            }
        };
        if config.skip(&f.name) {
            continue;
        }

        let emit_columns = config.emit_columns(&f.name);

        if !emit_columns && lc.line == prev_dst_line {
            continue;
        }

        let line = match f.lookup_line(pos) {
            Some(line) => line as u32,
            None => continue,
        };

        let analysis = f.analyze();
        let linebpos = analysis.lines[line as usize];
        debug_assert!(
            pos >= linebpos,
            "{}: bpos = {:?}; linebpos = {:?};",
            f.name,
            pos,
            linebpos,
        );

        let linechpos = linebpos.to_u32() - calc_utf16_offset(f, linebpos, &mut line_state);
        let chpos = pos.to_u32() - calc_utf16_offset(f, pos, &mut ch_state);

        debug_assert!(
            chpos >= linechpos,
            "{}: chpos = {:?}; linechpos = {:?};",
            f.name,
            chpos,
            linechpos,
        );

        let col = chpos - linechpos;
        let name = None;

        let name_idx = if orig.is_none() {
            name.or_else(|| config.name_for_bytepos(pos)).map(|name| {
                builder.add_name(unsafe {
                    // Safety: name is `&str`, which is valid UTF-8
                    BytesStr::from_utf8_slice_unchecked(name.as_bytes())
                })
            })
        } else {
            // orig.adjust_mappings below will throw this out
            None
        };

        builder.add_raw(lc.line, lc.col, line, col, Some(src_id), name_idx, false);
        prev_dst_line = lc.line;
    }

    let map = builder.into_sourcemap();

    if let Some(mut orig) = orig {
        orig.adjust_mappings(&map);
        return orig;
    }

    map
}

impl SourceMapper for SourceMap {
    fn lookup_char_pos(&self, pos: BytePos) -> Loc {
        self.lookup_char_pos(pos)
    }

    fn span_to_lines(&self, sp: Span) -> FileLinesResult {
        self.span_to_lines(sp)
    }

    fn span_to_string(&self, sp: Span) -> String {
        self.span_to_string(sp)
    }

    fn span_to_filename(&self, sp: Span) -> Lrc<FileName> {
        self.span_to_filename(sp)
    }

    /// Return the source snippet as `String` corresponding to the given `Span`
    fn span_to_snippet(&self, sp: Span) -> Result<String, Box<SpanSnippetError>> {
        self.span_to_source(sp, |src, start_index, end_index| {
            src[start_index..end_index].to_string()
        })
    }

    fn merge_spans(&self, sp_lhs: Span, sp_rhs: Span) -> Option<Span> {
        self.merge_spans(sp_lhs, sp_rhs)
    }

    fn call_span_if_macro(&self, sp: Span) -> Span {
        sp
    }

    fn doctest_offset_line(&self, line: usize) -> usize {
        self.doctest_offset_line(line)
    }
}

#[derive(Clone, Default)]
pub struct FilePathMapping {
    mapping: Vec<(PathBuf, PathBuf)>,
}

impl FilePathMapping {
    pub fn empty() -> FilePathMapping {
        FilePathMapping {
            mapping: Vec::new(),
        }
    }

    pub fn new(mapping: Vec<(PathBuf, PathBuf)>) -> FilePathMapping {
        FilePathMapping { mapping }
    }

    /// Applies any path prefix substitution as defined by the mapping.
    /// The return value is the remapped path and a boolean indicating whether
    /// the path was affected by the mapping.
    pub fn map_prefix(&self, path: &Path) -> (PathBuf, bool) {
        // NOTE: We are iterating over the mapping entries from last to first
        //       because entries specified later on the command line should
        //       take precedence.
        for (from, to) in self.mapping.iter().rev() {
            if let Ok(rest) = path.strip_prefix(from) {
                return (to.join(rest), true);
            }
        }

        (path.to_path_buf(), false)
    }
}

pub trait SourceMapGenConfig {
    /// # Returns
    ///
    /// File path to used in `SourceMap.sources`.
    ///
    /// This should **not** return content of the file.
    fn file_name_to_source(&self, f: &FileName) -> String;

    /// # Returns identifier starting at `bpos`.
    fn name_for_bytepos(&self, _bpos: BytePos) -> Option<&str> {
        None
    }

    /// You can override this to control `sourceContents`.
    fn inline_sources_content(&self, f: &FileName) -> bool {
        !matches!(
            f,
            FileName::Real(..) | FileName::Custom(..) | FileName::Url(..)
        )
    }

    /// You can define whether to emit sourcemap with columns or not
    fn emit_columns(&self, _f: &FileName) -> bool {
        true
    }

    /// By default, we skip internal files.
    fn skip(&self, f: &FileName) -> bool {
        matches!(f, FileName::Internal(..))
    }

    /// If true, the file will be in the `ignoreList` of `SourceMap`.
    ///
    /// Specification for ignoreList: https://tc39.es/ecma426/#json-ignoreList
    ///
    /// > The ignoreList field is an optional list of indices of files that
    /// > should be considered third party code, such as framework code or
    /// > bundler-generated code. This allows developer tools to avoid code that
    /// > developers likely don't want to see or step through, without requiring
    /// > developers to configure this beforehand. It refers to the sources
    /// > field and lists the indices of all the known third-party sources in
    /// > the source map. Some browsers may also use the deprecated
    /// > x_google_ignoreList field if ignoreList is not present.
    ///
    ///
    /// By default, we ignore anonymous files and internal files.
    fn ignore_list(&self, f: &FileName) -> bool {
        matches!(f, FileName::Anon | FileName::Internal(..))
    }
}

#[derive(Debug, Clone)]
pub struct DefaultSourceMapGenConfig;

macro_rules! impl_ref {
    ($TP:ident, $T:ty) => {
        impl<$TP> SourceMapGenConfig for $T
        where
            $TP: SourceMapGenConfig,
        {
            fn file_name_to_source(&self, f: &FileName) -> String {
                (**self).file_name_to_source(f)
            }
        }
    };
}

impl_ref!(T, &'_ T);
impl_ref!(T, Box<T>);
impl_ref!(T, std::rc::Rc<T>);
impl_ref!(T, std::sync::Arc<T>);

impl SourceMapGenConfig for DefaultSourceMapGenConfig {
    fn file_name_to_source(&self, f: &FileName) -> String {
        f.to_string()
    }
}

/// Stores the state of the last conversion between BytePos and CharPos.
#[derive(Debug, Clone, Default)]
pub struct ByteToCharPosState {
    /// The last BytePos to convert.
    pos: BytePos,

    /// The total number of extra chars in the UTF-8 encoding.
    total_extra_bytes: u32,

    /// The index of the last MultiByteChar read to compute the extra bytes of
    /// the last conversion.
    mbc_index: usize,
}

// _____________________________________________________________________________
// Tests
//

#[cfg(test)]
mod tests {
    use super::*;

    fn init_source_map() -> SourceMap {
        let sm = SourceMap::new(FilePathMapping::empty());
        sm.new_source_file(
            Lrc::new(PathBuf::from("blork.rs").into()),
            "first line.\nsecond line",
        );
        sm.new_source_file(Lrc::new(PathBuf::from("empty.rs").into()), BytesStr::new());
        sm.new_source_file(
            Lrc::new(PathBuf::from("blork2.rs").into()),
            "first line.\nsecond line",
        );
        sm
    }

    #[test]
    fn t3() {
        // Test lookup_byte_offset
        let sm = init_source_map();

        let srcfbp1 = sm.lookup_byte_offset(BytePos(24));
        assert_eq!(*srcfbp1.sf.name, PathBuf::from("blork.rs").into());
        assert_eq!(srcfbp1.pos, BytePos(23));

        let srcfbp1 = sm.lookup_byte_offset(BytePos(25));
        assert_eq!(*srcfbp1.sf.name, PathBuf::from("empty.rs").into());
        assert_eq!(srcfbp1.pos, BytePos(0));

        let srcfbp2 = sm.lookup_byte_offset(BytePos(26));
        assert_eq!(*srcfbp2.sf.name, PathBuf::from("blork2.rs").into());
        assert_eq!(srcfbp2.pos, BytePos(0));
    }

    #[test]
    fn t4() {
        // Test bytepos_to_file_charpos
        let sm = init_source_map();

        let cp1 = sm.bytepos_to_file_charpos(BytePos(23)).unwrap();
        assert_eq!(cp1, CharPos(22));

        let cp2 = sm.bytepos_to_file_charpos(BytePos(26)).unwrap();
        assert_eq!(cp2, CharPos(0));
    }

    #[test]
    fn t5() {
        // Test zero-length source_files.
        let sm = init_source_map();

        let loc1 = sm.lookup_char_pos(BytePos(23));
        assert_eq!(*loc1.file.name, PathBuf::from("blork.rs").into());
        assert_eq!(loc1.line, 2);
        assert_eq!(loc1.col, CharPos(10));

        let loc2 = sm.lookup_char_pos(BytePos(26));
        assert_eq!(*loc2.file.name, PathBuf::from("blork2.rs").into());
        assert_eq!(loc2.line, 1);
        assert_eq!(loc2.col, CharPos(0));
    }

    fn init_source_map_mbc() -> SourceMap {
        let sm = SourceMap::new(FilePathMapping::empty());
        // € is a three byte utf8 char.
        sm.new_source_file(
            Lrc::new(PathBuf::from("blork.rs").into()),
            "fir€st €€€€ line.\nsecond line",
        );
        sm.new_source_file(
            Lrc::new(PathBuf::from("blork2.rs").into()),
            "first line€€.\n€ second line",
        );
        sm
    }

    #[test]
    fn t6() {
        // Test bytepos_to_file_charpos in the presence of multi-byte chars
        let sm = init_source_map_mbc();

        let cp1 = sm.bytepos_to_file_charpos(BytePos(4)).unwrap();
        assert_eq!(cp1, CharPos(3));

        let cp2 = sm.bytepos_to_file_charpos(BytePos(7)).unwrap();
        assert_eq!(cp2, CharPos(4));

        let cp3 = sm.bytepos_to_file_charpos(BytePos(57)).unwrap();
        assert_eq!(cp3, CharPos(12));

        let cp4 = sm.bytepos_to_file_charpos(BytePos(62)).unwrap();
        assert_eq!(cp4, CharPos(15));
    }

    #[test]
    fn t7() {
        // Test span_to_lines for a span ending at the end of source_file
        let sm = init_source_map();
        let span = Span::new(BytePos(13), BytePos(24));
        let file_lines = sm.span_to_lines(span).unwrap();

        assert_eq!(*file_lines.file.name, PathBuf::from("blork.rs").into());
        assert_eq!(file_lines.lines.len(), 1);
        assert_eq!(file_lines.lines[0].line_index, 1);
    }

    /// Given a string like " ~~~~~~~~~~~~ ", produces a span
    /// converting that range. The idea is that the string has the same
    /// length as the input, and we uncover the byte positions.  Note
    /// that this can span lines and so on.
    fn span_from_selection(input: &str, selection: &str) -> Span {
        assert_eq!(input.len(), selection.len());
        // +1 as BytePos starts at 1
        let left_index = (selection.find('~').unwrap() + 1) as u32;
        let right_index = selection
            .rfind('~')
            .map(|x| {
                // +1 as BytePos starts at 1
                (x + 1) as u32
            })
            .unwrap_or(left_index);
        Span::new(BytePos(left_index), BytePos(right_index + 1))
    }

    /// Test span_to_snippet and span_to_lines for a span converting 3
    /// lines in the middle of a file.
    #[test]
    fn span_to_snippet_and_lines_spanning_multiple_lines() {
        let sm = SourceMap::new(FilePathMapping::empty());
        let inputtext = "aaaaa\nbbbbBB\nCCC\nDDDDDddddd\neee\n";
        let selection = "     \n    ~~\n~~~\n~~~~~     \n   \n";
        sm.new_source_file(
            Lrc::new(Path::new("blork.rs").to_path_buf().into()),
            inputtext,
        );
        let span = span_from_selection(inputtext, selection);

        // check that we are extracting the text we thought we were extracting
        assert_eq!(&sm.span_to_snippet(span).unwrap(), "BB\nCCC\nDDDDD");

        // check that span_to_lines gives us the complete result with the lines/cols we
        // expected
        let lines = sm.span_to_lines(span).unwrap();
        let expected = vec![
            LineInfo {
                line_index: 1,
                start_col: CharPos(4),
                end_col: CharPos(6),
            },
            LineInfo {
                line_index: 2,
                start_col: CharPos(0),
                end_col: CharPos(3),
            },
            LineInfo {
                line_index: 3,
                start_col: CharPos(0),
                end_col: CharPos(5),
            },
        ];
        assert_eq!(lines.lines, expected);
    }

    #[test]
    fn t8() {
        // Test span_to_snippet for a span ending at the end of source_file
        let sm = init_source_map();
        let span = Span::new(BytePos(13), BytePos(24));
        let snippet = sm.span_to_snippet(span);

        assert_eq!(snippet, Ok("second line".to_string()));
    }

    #[test]
    fn t9() {
        // Test span_to_str for a span ending at the end of source_file
        let sm = init_source_map();
        let span = Span::new(BytePos(13), BytePos(24));
        let sstr = sm.span_to_string(span);

        assert_eq!(sstr, "blork.rs:2:1: 2:12");
    }

    #[test]
    fn t10() {
        // Test span_to_lines for a span of empty file
        let sm = SourceMap::new(FilePathMapping::empty());
        sm.new_source_file(Lrc::new(PathBuf::from("blork.rs").into()), "");
        let span = Span::new(BytePos(1), BytePos(1));
        let file_lines = sm.span_to_lines(span).unwrap();

        assert_eq!(*file_lines.file.name, PathBuf::from("blork.rs").into());
        assert_eq!(file_lines.lines.len(), 0);
    }

    /// Test failing to merge two spans on different lines
    #[test]
    fn span_merging_fail() {
        let sm = SourceMap::new(FilePathMapping::empty());
        let inputtext = "bbbb BB\ncc CCC\n";
        let selection1 = "     ~~\n      \n";
        let selection2 = "       \n   ~~~\n";
        sm.new_source_file(Lrc::new(Path::new("blork.rs").to_owned().into()), inputtext);
        let span1 = span_from_selection(inputtext, selection1);
        let span2 = span_from_selection(inputtext, selection2);

        assert!(sm.merge_spans(span1, span2).is_none());
    }

    #[test]
    fn test_calc_utf16_offset() {
        let input = "t¢e∆s💩t";
        let sm = SourceMap::new(FilePathMapping::empty());
        let file = sm.new_source_file(Lrc::new(PathBuf::from("blork.rs").into()), input);

        let mut state = ByteToCharPosState::default();
        let mut bpos = file.start_pos;
        let mut cpos = CharPos(bpos.to_usize());
        for c in input.chars() {
            let actual = bpos.to_u32() - calc_utf16_offset(&file, bpos, &mut state);

            assert_eq!(actual, cpos.to_u32());

            bpos = bpos + BytePos(c.len_utf8() as u32);
            cpos = cpos + CharPos(c.len_utf16());
        }

        for c in input.chars().rev() {
            bpos = bpos - BytePos(c.len_utf8() as u32);
            cpos = cpos - CharPos(c.len_utf16());

            let actual = bpos.to_u32() - calc_utf16_offset(&file, bpos, &mut state);

            assert_eq!(actual, cpos.to_u32());
        }
    }

    #[test]
    fn bytepos_to_charpos() {
        let input = "t¢e∆s💩t";
        let sm = SourceMap::new(FilePathMapping::empty());
        let file = sm.new_source_file(Lrc::new(PathBuf::from("blork.rs").into()), input);

        let mut bpos = file.start_pos;
        let mut cpos = CharPos(0);
        for c in input.chars() {
            let actual = sm.bytepos_to_file_charpos_with(&file, bpos);

            assert_eq!(actual, cpos);

            bpos = bpos + BytePos(c.len_utf8() as u32);
            cpos = cpos + CharPos(c.len_utf16());
        }
    }
}
