#![cfg_attr(not(any(unix, windows, target_os = "redox")), allow(unused_imports))]

use std::collections::BTreeSet;
use std::env;
use std::fs;
use std::io::Read;
use std::path::{Path, PathBuf};

use bytes_str::BytesStr;
use debugid::DebugId;
use rustc_hash::FxHashMap;
use url::Url;

use crate::errors::Result;
use crate::types::{RawToken, SourceMap, Token};

/// Helper for sourcemap generation
///
/// This helper exists because generating and modifying `SourceMap`
/// objects is generally not very comfortable.  As a general aid this
/// type can help.
pub struct SourceMapBuilder {
    file: Option<BytesStr>,
    name_map: FxHashMap<BytesStr, u32>,
    names: Vec<BytesStr>,
    tokens: Vec<RawToken>,
    source_map: FxHashMap<BytesStr, u32>,
    source_root: Option<BytesStr>,
    sources: Vec<BytesStr>,
    source_contents: Vec<Option<BytesStr>>,
    sources_mapping: Vec<u32>,
    ignore_list: BTreeSet<u32>,
    debug_id: Option<DebugId>,
}

#[cfg(any(unix, windows, target_os = "redox"))]
fn resolve_local_reference(base: &Url, reference: &str) -> Option<PathBuf> {
    let url = match base.join(reference) {
        Ok(url) => {
            if url.scheme() != "file" {
                return None;
            }
            url
        }
        Err(_) => {
            return None;
        }
    };

    url.to_file_path().ok()
}

impl SourceMapBuilder {
    /// Creates a new source map builder and sets the file.
    pub fn new(file: Option<BytesStr>) -> SourceMapBuilder {
        SourceMapBuilder {
            file,
            name_map: FxHashMap::default(),
            names: vec![],
            tokens: vec![],
            source_map: FxHashMap::default(),
            source_root: None,
            sources: vec![],
            source_contents: vec![],
            sources_mapping: vec![],
            ignore_list: BTreeSet::default(),
            debug_id: None,
        }
    }

    /// Sets the debug id for the sourcemap (optional)
    pub fn set_debug_id(&mut self, debug_id: Option<DebugId>) {
        self.debug_id = debug_id;
    }

    /// Sets the file for the sourcemap (optional)
    pub fn set_file<T: Into<BytesStr>>(&mut self, value: Option<T>) {
        self.file = value.map(Into::into);
    }

    /// Returns the currently set file.
    pub fn get_file(&self) -> Option<&str> {
        self.file.as_deref()
    }

    /// Sets a new value for the source_root.
    pub fn set_source_root<T: Into<BytesStr>>(&mut self, value: Option<T>) {
        self.source_root = value.map(Into::into);
    }

    /// Returns the embedded source_root in case there is one.
    pub fn get_source_root(&self) -> Option<&str> {
        self.source_root.as_deref()
    }

    /// Registers a new source with the builder and returns the source ID.
    pub fn add_source(&mut self, src: BytesStr) -> u32 {
        self.add_source_with_id(src, !0)
    }

    fn add_source_with_id(&mut self, src: BytesStr, old_id: u32) -> u32 {
        let count = self.sources.len() as u32;
        let id = *self.source_map.entry(src.clone()).or_insert(count);
        if id == count {
            self.sources.push(src);
            self.sources_mapping.push(old_id);
        }
        id
    }

    /// Changes the source name for an already set source.
    pub fn set_source(&mut self, src_id: u32, src: BytesStr) {
        assert!(src_id != !0, "Cannot set sources for tombstone source id");
        self.sources[src_id as usize] = src;
    }

    /// Looks up a source name for an ID.
    pub fn get_source(&self, src_id: u32) -> Option<&str> {
        self.sources.get(src_id as usize).map(|x| &x[..])
    }

    pub fn add_to_ignore_list(&mut self, src_id: u32) {
        self.ignore_list.insert(src_id);
    }

    /// Sets the source contents for an already existing source.
    pub fn set_source_contents(&mut self, src_id: u32, contents: Option<BytesStr>) {
        assert!(src_id != !0, "Cannot set sources for tombstone source id");
        if self.sources.len() > self.source_contents.len() {
            self.source_contents.resize(self.sources.len(), None);
        }
        self.source_contents[src_id as usize] = contents;
    }

    /// Returns the current source contents for a source.
    pub fn get_source_contents(&self, src_id: u32) -> Option<&str> {
        self.source_contents
            .get(src_id as usize)
            .and_then(|x| x.as_ref().map(|x| &x[..]))
    }

    /// Checks if a given source ID has source contents available.
    pub fn has_source_contents(&self, src_id: u32) -> bool {
        self.get_source_contents(src_id).is_some()
    }

    /// Loads source contents from locally accessible files if referenced
    /// accordingly.  Returns the number of loaded source contents
    #[cfg(any(unix, windows, target_os = "redox"))]
    pub fn load_local_source_contents(&mut self, base_path: Option<&Path>) -> Result<usize> {
        let mut abs_path = env::current_dir()?;
        if let Some(path) = base_path {
            abs_path.push(path);
        }
        let base_url = Url::from_directory_path(&abs_path).unwrap();

        let mut to_read = vec![];
        for (source, &src_id) in self.source_map.iter() {
            if self.has_source_contents(src_id) {
                continue;
            }
            if let Some(path) = resolve_local_reference(&base_url, source) {
                to_read.push((src_id, path));
            }
        }

        let rv = to_read.len();
        for (src_id, path) in to_read {
            if let Ok(mut f) = fs::File::open(path) {
                let mut contents = String::new();
                if f.read_to_string(&mut contents).is_ok() {
                    self.set_source_contents(src_id, Some(contents.into()));
                }
            }
        }

        Ok(rv)
    }

    /// Registers a name with the builder and returns the name ID.
    pub fn add_name(&mut self, name: BytesStr) -> u32 {
        let count = self.names.len() as u32;
        let id = *self.name_map.entry(name.clone()).or_insert(count);
        if id == count {
            self.names.push(name);
        }
        id
    }

    /// Adds a new mapping to the builder.
    #[allow(clippy::too_many_arguments)]
    pub fn add(
        &mut self,
        dst_line: u32,
        dst_col: u32,
        src_line: u32,
        src_col: u32,
        source: Option<BytesStr>,
        name: Option<BytesStr>,
        is_range: bool,
    ) -> RawToken {
        self.add_with_id(
            dst_line, dst_col, src_line, src_col, source, !0, name, is_range,
        )
    }

    #[allow(clippy::too_many_arguments)]
    fn add_with_id(
        &mut self,
        dst_line: u32,
        dst_col: u32,
        src_line: u32,
        src_col: u32,
        source: Option<BytesStr>,
        source_id: u32,
        name: Option<BytesStr>,
        is_range: bool,
    ) -> RawToken {
        let src_id = match source {
            Some(source) => self.add_source_with_id(source, source_id),
            None => !0,
        };
        let name_id = match name {
            Some(name) => self.add_name(name),
            None => !0,
        };
        let raw = RawToken {
            dst_line,
            dst_col,
            src_line,
            src_col,
            src_id,
            name_id,
            is_range,
        };
        self.tokens.push(raw);
        raw
    }

    /// Adds a new mapping to the builder.
    #[allow(clippy::too_many_arguments)]
    pub fn add_raw(
        &mut self,
        dst_line: u32,
        dst_col: u32,
        src_line: u32,
        src_col: u32,
        source: Option<u32>,
        name: Option<u32>,
        is_range: bool,
    ) -> RawToken {
        let src_id = source.unwrap_or(!0);
        let name_id = name.unwrap_or(!0);
        let raw = RawToken {
            dst_line,
            dst_col,
            src_line,
            src_col,
            src_id,
            name_id,
            is_range,
        };
        self.tokens.push(raw);
        raw
    }

    /// Shortcut for adding a new mapping based of an already existing token,
    /// optionally removing the name.
    pub fn add_token(&mut self, token: &Token<'_>, with_name: bool) -> RawToken {
        let name = if with_name { token.get_name() } else { None };
        self.add_with_id(
            token.get_dst_line(),
            token.get_dst_col(),
            token.get_src_line(),
            token.get_src_col(),
            token.get_source().cloned(),
            token.get_src_id(),
            name.cloned(),
            token.is_range(),
        )
    }

    /// Strips common prefixes from the sources in the builder
    pub fn strip_prefixes<S: AsRef<str>>(&mut self, prefixes: &[S]) {
        for source in self.sources.iter_mut() {
            for prefix in prefixes {
                let mut prefix = prefix.as_ref().to_string();
                if !prefix.ends_with('/') {
                    prefix.push('/');
                }
                if source.starts_with(&prefix) {
                    source.advance(prefix.len());
                    break;
                }
            }
        }
    }

    pub(crate) fn take_mapping(&mut self) -> Vec<u32> {
        std::mem::take(&mut self.sources_mapping)
    }

    /// Converts the builder into a sourcemap.
    pub fn into_sourcemap(self) -> SourceMap {
        let contents = if !self.source_contents.is_empty() {
            Some(self.source_contents)
        } else {
            None
        };

        let mut sm = SourceMap::new(self.file, self.tokens, self.names, self.sources, contents);
        sm.set_source_root(self.source_root);
        sm.set_debug_id(self.debug_id);
        for ignored_src_id in self.ignore_list {
            sm.add_to_ignore_list(ignored_src_id);
        }

        sm
    }
}
