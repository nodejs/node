use std::fmt;
use std::slice;
use std::str;
use std::sync::atomic::AtomicUsize;
use std::sync::atomic::Ordering;
use std::sync::Mutex;

use bytes_str::BytesStr;
use if_chain::if_chain;

use crate::detector::{locate_sourcemap_reference_slice, SourceMapRef};
use crate::errors::Result;
use crate::js_identifiers::{get_javascript_token, is_valid_javascript_identifier};
use crate::types::Token;

/// An iterator that iterates over tokens in reverse.
pub struct RevTokenIter<'view, 'map> {
    sv: &'view SourceView,
    token: Option<Token<'map>>,
    source_line: Option<(&'view str, usize, usize, usize)>,
}

impl<'view, 'map> Iterator for RevTokenIter<'view, 'map> {
    type Item = (Token<'map>, Option<&'view str>);

    fn next(&mut self) -> Option<(Token<'map>, Option<&'view str>)> {
        let token = self.token.take()?;
        let idx = token.idx;

        if idx > 0 {
            self.token = token.sm.get_token(idx - 1);
        }

        // if we are going to the same line as we did last iteration, we don't have to scan
        // up to it again.  For normal sourcemaps this should mean we only ever go to the
        // line once.
        let (source_line, last_char_offset, last_byte_offset) = if_chain! {
            if let Some((source_line, dst_line, last_char_offset,
                         last_byte_offset)) = self.source_line;

            if dst_line == token.get_dst_line() as usize;
            then {
                (source_line, last_char_offset, last_byte_offset)
            } else {
                if let Some(source_line) = self.sv.get_line(token.get_dst_line()) {
                    (source_line, !0, !0)
                } else {
                    // if we can't find the line, return am empty one
                    ("", !0, !0)
                }
            }
        };

        // find the byte offset where our token starts
        let byte_offset = if last_byte_offset == !0 {
            let mut off = 0;
            let mut idx = 0;
            for c in source_line.chars() {
                if idx >= token.get_dst_col() as usize {
                    break;
                }
                off += c.len_utf8();
                idx += c.len_utf16();
            }
            off
        } else {
            let chars_to_move = last_char_offset - token.get_dst_col() as usize;
            let mut new_offset = last_byte_offset;
            let mut idx = 0;
            for c in source_line
                .get(..last_byte_offset)
                .unwrap_or("")
                .chars()
                .rev()
            {
                if idx >= chars_to_move {
                    break;
                }
                new_offset -= c.len_utf8();
                idx += c.len_utf16();
            }
            new_offset
        };

        // remember where we were
        self.source_line = Some((
            source_line,
            token.get_dst_line() as usize,
            token.get_dst_col() as usize,
            byte_offset,
        ));

        // in case we run out of bounds here we reset the cache
        if byte_offset >= source_line.len() {
            self.source_line = None;
            Some((token, None))
        } else {
            Some((
                token,
                source_line
                    .get(byte_offset..)
                    .and_then(get_javascript_token),
            ))
        }
    }
}

pub struct Lines<'a> {
    sv: &'a SourceView,
    idx: u32,
}

impl<'a> Iterator for Lines<'a> {
    type Item = &'a str;

    fn next(&mut self) -> Option<&'a str> {
        if let Some(line) = self.sv.get_line(self.idx) {
            self.idx += 1;
            Some(line)
        } else {
            None
        }
    }
}

/// Provides efficient access to minified sources.
///
/// This type is used to implement fairly efficient source mapping
/// operations.
pub struct SourceView {
    pub(crate) source: BytesStr,
    processed_until: AtomicUsize,
    lines: Mutex<Vec<&'static str>>,
}

impl Clone for SourceView {
    fn clone(&self) -> SourceView {
        SourceView {
            source: self.source.clone(),
            processed_until: AtomicUsize::new(0),
            lines: Mutex::new(vec![]),
        }
    }
}

impl fmt::Debug for SourceView {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.debug_struct("SourceView")
            .field("source", &self.source())
            .finish()
    }
}

impl PartialEq for SourceView {
    fn eq(&self, other: &Self) -> bool {
        self.source == other.source
    }
}

impl SourceView {
    /// Creates an optimized view of a given source.
    pub fn new(source: BytesStr) -> SourceView {
        SourceView {
            source,
            processed_until: AtomicUsize::new(0),
            lines: Mutex::new(vec![]),
        }
    }

    /// Creates an optimized view from a given source string
    pub fn from_string(source: BytesStr) -> SourceView {
        SourceView {
            source,
            processed_until: AtomicUsize::new(0),
            lines: Mutex::new(vec![]),
        }
    }

    /// Returns a requested minified line.
    pub fn get_line(&self, idx: u32) -> Option<&str> {
        let idx = idx as usize;
        {
            let lines = self.lines.lock().unwrap();
            if idx < lines.len() {
                return Some(lines[idx]);
            }
        }

        // fetched everything
        if self.processed_until.load(Ordering::Relaxed) > self.source.len() {
            return None;
        }

        let mut lines = self.lines.lock().unwrap();
        let mut done = false;

        while !done {
            let rest = &self.source.as_bytes()[self.processed_until.load(Ordering::Relaxed)..];

            let rv = if let Some(mut idx) = rest.iter().position(|&x| x == b'\n' || x == b'\r') {
                let rv = &rest[..idx];
                if rest[idx] == b'\r' && rest.get(idx + 1) == Some(&b'\n') {
                    idx += 1;
                }
                self.processed_until.fetch_add(idx + 1, Ordering::Relaxed);
                rv
            } else {
                self.processed_until
                    .fetch_add(rest.len() + 1, Ordering::Relaxed);
                done = true;
                rest
            };

            lines.push(unsafe {
                str::from_utf8_unchecked(slice::from_raw_parts(rv.as_ptr(), rv.len()))
            });
            if let Some(&line) = lines.get(idx) {
                return Some(line);
            }
        }

        None
    }

    /// Returns a line slice.
    ///
    /// Note that columns are indexed as JavaScript WTF-16 columns.
    pub fn get_line_slice(&self, line: u32, col: u32, span: u32) -> Option<&str> {
        self.get_line(line).and_then(|line| {
            let mut off = 0;
            let mut idx = 0;
            let mut char_iter = line.chars().peekable();

            while let Some(&c) = char_iter.peek() {
                if idx >= col as usize {
                    break;
                }
                char_iter.next();
                off += c.len_utf8();
                idx += c.len_utf16();
            }

            let mut off_end = off;
            for c in char_iter {
                if idx >= (col + span) as usize {
                    break;
                }
                off_end += c.len_utf8();
                idx += c.len_utf16();
            }

            if idx < ((col + span) as usize) {
                None
            } else {
                line.get(off..off_end)
            }
        })
    }

    /// Returns an iterator over all lines.
    pub fn lines(&self) -> Lines<'_> {
        Lines { sv: self, idx: 0 }
    }

    /// Returns the source.
    pub fn source(&self) -> &BytesStr {
        &self.source
    }

    fn rev_token_iter<'this, 'map>(&'this self, token: Token<'map>) -> RevTokenIter<'this, 'map> {
        RevTokenIter {
            sv: self,
            token: Some(token),
            source_line: None,
        }
    }

    /// Given a token and minified function name this attemps to resolve the
    /// name to an original function name.
    ///
    /// This invokes some guesswork and requires access to the original minified
    /// source.  This will not yield proper results for anonymous functions or
    /// functions that do not have clear function names.  (For instance it's
    /// recommended that dotted function names are not passed to this
    /// function).
    pub fn get_original_function_name<'map>(
        &self,
        token: Token<'map>,
        minified_name: &str,
    ) -> Option<&'map BytesStr> {
        if !is_valid_javascript_identifier(minified_name) {
            return None;
        }

        let mut iter = self.rev_token_iter(token).take(128).peekable();

        while let Some((token, original_identifier)) = iter.next() {
            if_chain! {
                if original_identifier == Some(minified_name);
                if let Some(item) = iter.peek();
                if item.1 == Some("function");
                then {
                    return token.get_name();
                }
            }
        }

        None
    }

    /// Returns the number of lines.
    pub fn line_count(&self) -> usize {
        self.get_line(!0);
        self.lines.lock().unwrap().len()
    }

    /// Returns the source map reference in the source view.
    pub fn sourcemap_reference(&self) -> Result<Option<SourceMapRef>> {
        locate_sourcemap_reference_slice(self.source.as_bytes())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    #[allow(clippy::cognitive_complexity)]
    fn test_minified_source_view() {
        let view = SourceView::new("a\nb\nc".into());
        assert_eq!(view.get_line(0), Some("a"));
        assert_eq!(view.get_line(0), Some("a"));
        assert_eq!(view.get_line(2), Some("c"));
        assert_eq!(view.get_line(1), Some("b"));
        assert_eq!(view.get_line(3), None);

        assert_eq!(view.line_count(), 3);

        let view = SourceView::new("a\r\nb\r\nc".into());
        assert_eq!(view.get_line(0), Some("a"));
        assert_eq!(view.get_line(0), Some("a"));
        assert_eq!(view.get_line(2), Some("c"));
        assert_eq!(view.get_line(1), Some("b"));
        assert_eq!(view.get_line(3), None);

        assert_eq!(view.line_count(), 3);

        let view = SourceView::new("abc👌def\nblah".into());
        assert_eq!(view.get_line_slice(0, 0, 3), Some("abc"));
        assert_eq!(view.get_line_slice(0, 3, 1), Some("👌"));
        assert_eq!(view.get_line_slice(0, 3, 2), Some("👌"));
        assert_eq!(view.get_line_slice(0, 3, 3), Some("👌d"));
        assert_eq!(view.get_line_slice(0, 0, 4), Some("abc👌"));
        assert_eq!(view.get_line_slice(0, 0, 5), Some("abc👌"));
        assert_eq!(view.get_line_slice(0, 0, 6), Some("abc👌d"));
        assert_eq!(view.get_line_slice(1, 0, 4), Some("blah"));
        assert_eq!(view.get_line_slice(1, 0, 5), None);
        assert_eq!(view.get_line_slice(1, 0, 12), None);

        let view = SourceView::new("a\nb\nc\n".into());
        assert_eq!(view.get_line(0), Some("a"));
        assert_eq!(view.get_line(1), Some("b"));
        assert_eq!(view.get_line(2), Some("c"));
        assert_eq!(view.get_line(3), Some(""));
        assert_eq!(view.get_line(4), None);

        fn is_send<T: Send>() {}
        fn is_sync<T: Sync>() {}
        is_send::<SourceView>();
        is_sync::<SourceView>();
    }
}
