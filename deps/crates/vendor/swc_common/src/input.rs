use std::str;

use crate::syntax_pos::{BytePos, SourceFile};

pub type SourceFileInput<'a> = StringInput<'a>;

/// Implementation of [Input].
#[derive(Clone)]
pub struct StringInput<'a> {
    last_pos: BytePos,
    /// Remaining input as str - we slice this as we consume bytes
    remaining: &'a str,
    orig: &'a str,
    /// Original start position.
    orig_start: BytePos,
    orig_end: BytePos,
}

impl<'a> StringInput<'a> {
    /// `start` and `end` can be arbitrary value, but start should be less than
    /// or equal to end.
    ///
    ///
    /// `swc` get this value from [SourceMap] because code generator depends on
    /// some methods of [SourceMap].
    /// If you are not going to use methods from
    /// [SourceMap], you may use any value.
    pub fn new(src: &'a str, start: BytePos, end: BytePos) -> Self {
        assert!(start <= end);

        StringInput {
            last_pos: start,
            orig: src,
            remaining: src,
            orig_start: start,
            orig_end: end,
        }
    }

    #[inline(always)]
    pub fn as_str(&self) -> &str {
        self.remaining
    }

    #[inline(always)]
    /// Compared to [StringInput::slice], this function doesn't set
    /// `self.last_pos = end` because in most cases this property has been
    /// satisfied but the compiler cannot optimize it.
    ///
    /// Caution: This function should only be used internally and will be
    /// changed in the future.
    ///
    /// # Safety
    /// - start should be less than or equal to end.
    /// - start and end should be in the valid range of input.
    pub unsafe fn slice_str(&self, start: BytePos, end: BytePos) -> &'a str {
        debug_assert!(start <= end, "Cannot slice {start:?}..{end:?}");
        let s = self.orig;

        let start_idx = (start - self.orig_start).0 as usize;
        let end_idx = (end - self.orig_start).0 as usize;

        debug_assert!(end_idx <= s.len());

        let ret = unsafe { s.get_unchecked(start_idx..end_idx) };

        ret
    }

    pub fn start_pos(&self) -> BytePos {
        self.orig_start
    }

    #[inline(always)]
    pub fn end_pos(&self) -> BytePos {
        self.orig_end
    }
}

/// Creates an [Input] from [SourceFile]. This is an alias for
///
/// ```ignore
///    StringInput::new(&fm.src, fm.start_pos, fm.end_pos)
/// ```
impl<'a> From<&'a SourceFile> for StringInput<'a> {
    fn from(fm: &'a SourceFile) -> Self {
        StringInput::new(&fm.src, fm.start_pos, fm.end_pos)
    }
}

impl<'a> Input<'a> for StringInput<'a> {
    #[inline]
    fn cur(&self) -> Option<u8> {
        self.remaining.as_bytes().first().copied()
    }

    #[inline]
    fn peek(&self) -> Option<u8> {
        self.remaining.as_bytes().get(1).copied()
    }

    #[inline]
    fn peek_ahead(&self) -> Option<u8> {
        self.remaining.as_bytes().get(2).copied()
    }

    #[inline]
    unsafe fn bump_bytes(&mut self, n: usize) {
        debug_assert!(n <= self.remaining.len());
        self.remaining = unsafe { self.remaining.get_unchecked(n..) };
        self.last_pos.0 += n as u32;
    }

    #[inline]
    fn cur_as_ascii(&self) -> Option<u8> {
        let first_byte = *self.remaining.as_bytes().first()?;
        if first_byte <= 0x7f {
            Some(first_byte)
        } else {
            None
        }
    }

    #[inline]
    fn cur_as_char(&self) -> Option<char> {
        self.remaining.chars().next()
    }

    #[inline]
    fn is_at_start(&self) -> bool {
        self.orig_start == self.last_pos
    }

    /// TODO(kdy1): Remove this?
    #[inline]
    fn cur_pos(&self) -> BytePos {
        self.last_pos
    }

    #[inline]
    fn last_pos(&self) -> BytePos {
        self.last_pos
    }

    #[inline]
    unsafe fn slice(&mut self, start: BytePos, end: BytePos) -> &'a str {
        debug_assert!(start <= end, "Cannot slice {start:?}..{end:?}");
        let s = self.orig;

        let start_idx = (start - self.orig_start).0 as usize;
        let end_idx = (end - self.orig_start).0 as usize;

        debug_assert!(end_idx <= s.len());

        let ret = unsafe { s.get_unchecked(start_idx..end_idx) };

        self.remaining = unsafe { s.get_unchecked(end_idx..) };
        self.last_pos = end;

        ret
    }

    #[inline]
    fn uncons_while<F>(&mut self, mut pred: F) -> &'a str
    where
        F: FnMut(char) -> bool,
    {
        let last = {
            let mut last = 0;
            for c in self.remaining.chars() {
                if pred(c) {
                    last += c.len_utf8();
                } else {
                    break;
                }
            }
            last
        };

        debug_assert!(last <= self.remaining.len());
        let ret = unsafe { self.remaining.get_unchecked(..last) };

        self.last_pos = self.last_pos + BytePos(last as _);
        self.remaining = unsafe { self.remaining.get_unchecked(last..) };

        ret
    }

    #[inline]
    unsafe fn reset_to(&mut self, to: BytePos) {
        if self.last_pos == to {
            // No need to reset.
            return;
        }

        let orig = self.orig;
        let idx = (to - self.orig_start).0 as usize;

        debug_assert!(idx <= orig.len());
        self.remaining = unsafe { orig.get_unchecked(idx..) };
        self.last_pos = to;
    }

    #[inline]
    fn is_byte(&self, c: u8) -> bool {
        self.remaining
            .as_bytes()
            .first()
            .map(|b| *b == c)
            .unwrap_or(false)
    }

    #[inline]
    fn is_str(&self, s: &str) -> bool {
        self.remaining.starts_with(s)
    }

    #[inline]
    fn eat_byte(&mut self, c: u8) -> bool {
        if self.is_byte(c) {
            self.remaining = unsafe { self.remaining.get_unchecked(1..) };
            self.last_pos = self.last_pos + BytePos(1_u32);
            true
        } else {
            false
        }
    }
}

pub trait Input<'a>: Clone {
    /// Returns the current byte. Returns [None] if at end of input.
    fn cur(&self) -> Option<u8>;

    /// Returns the next byte without consuming the current byte.
    fn peek(&self) -> Option<u8>;

    /// Returns the byte after the next byte without consuming anything.
    fn peek_ahead(&self) -> Option<u8>;

    /// Advances the input by exactly `n` bytes.
    /// Unlike `bump()`, this does not calculate UTF-8 character boundaries.
    ///
    /// # Safety
    ///
    /// - This should be called only when `cur()` returns `Some`. i.e. when the
    ///   Input is not empty.
    /// - `n` should be the number of bytes of the current character.
    unsafe fn bump_bytes(&mut self, n: usize);

    /// Returns the current byte as ASCII if it's valid ASCII (0x00-0x7F).
    /// Returns [None] if it's end of input or if the byte is not ASCII.
    #[inline]
    fn cur_as_ascii(&self) -> Option<u8> {
        self.cur()
            .and_then(|b| if b <= 0x7f { Some(b) } else { None })
    }

    /// Returns the current position as a UTF-8 char for cases where we need
    /// full character processing (identifiers, strings, etc).
    /// Returns [None] if at end of input or if the bytes don't form valid
    /// UTF-8.
    fn cur_as_char(&self) -> Option<char>;

    fn is_at_start(&self) -> bool;

    fn cur_pos(&self) -> BytePos;

    fn last_pos(&self) -> BytePos;

    /// # Safety
    ///
    /// - start should be less than or equal to end.
    /// - start and end should be in the valid range of input.
    unsafe fn slice(&mut self, start: BytePos, end: BytePos) -> &'a str;

    /// Takes items from stream, testing each one with predicate. returns the
    /// range of items which passed predicate.
    fn uncons_while<F>(&mut self, f: F) -> &'a str
    where
        F: FnMut(char) -> bool;

    /// # Safety
    ///
    /// - `to` be in the valid range of input.
    unsafe fn reset_to(&mut self, to: BytePos);

    /// Check if the current byte equals the given byte.
    /// `c` should typically be an ASCII byte for performance.
    #[inline]
    #[allow(clippy::wrong_self_convention)]
    fn is_byte(&self, c: u8) -> bool {
        self.cur() == Some(c)
    }

    /// Implementors can override the method to make it faster.
    ///
    /// `s` must be ASCII only.
    fn is_str(&self, s: &str) -> bool;

    /// Implementors can override the method to make it faster.
    ///
    /// `c` must be ASCII.
    #[inline]
    fn eat_byte(&mut self, c: u8) -> bool {
        if self.is_byte(c) {
            unsafe {
                // Safety: We are sure that the input is not empty
                self.bump_bytes(1);
            }
            true
        } else {
            false
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{sync::Lrc, FileName, FilePathMapping, SourceMap};

    fn with_test_sess<F>(src: &'static str, f: F)
    where
        F: FnOnce(StringInput<'_>),
    {
        let cm = Lrc::new(SourceMap::new(FilePathMapping::empty()));
        let fm = cm.new_source_file(FileName::Real("testing".into()).into(), src);

        f((&*fm).into())
    }

    #[test]
    fn src_input_slice_1() {
        with_test_sess("foo/d", |mut i| {
            assert_eq!(unsafe { i.slice(BytePos(1), BytePos(2)) }, "f");
            assert_eq!(i.last_pos, BytePos(2));
            assert_eq!(i.cur(), Some(b'o'));

            assert_eq!(unsafe { i.slice(BytePos(2), BytePos(4)) }, "oo");
            assert_eq!(unsafe { i.slice(BytePos(1), BytePos(4)) }, "foo");
            assert_eq!(i.last_pos, BytePos(4));
            assert_eq!(i.cur(), Some(b'/'));
        });
    }

    #[test]
    fn src_input_reset_to_1() {
        with_test_sess("load", |mut i| {
            assert_eq!(unsafe { i.slice(BytePos(1), BytePos(3)) }, "lo");
            assert_eq!(i.last_pos, BytePos(3));
            assert_eq!(i.cur(), Some(b'a'));
            unsafe { i.reset_to(BytePos(1)) };

            assert_eq!(i.cur(), Some(b'l'));
            assert_eq!(i.last_pos, BytePos(1));
        });
    }

    #[test]
    fn src_input_smoke_01() {
        with_test_sess("foo/d", |mut i| {
            assert_eq!(i.cur_pos(), BytePos(1));
            assert_eq!(i.last_pos, BytePos(1));
            assert_eq!(i.uncons_while(|c| c.is_alphabetic()), "foo");

            // assert_eq!(i.cur_pos(), BytePos(4));
            assert_eq!(i.last_pos, BytePos(4));
            assert_eq!(i.cur(), Some(b'/'));

            unsafe {
                i.bump_bytes(1);
            }
            assert_eq!(i.last_pos, BytePos(5));
            assert_eq!(i.cur(), Some(b'd'));

            unsafe {
                i.bump_bytes(1);
            }
            assert_eq!(i.last_pos, BytePos(6));
            assert_eq!(i.cur(), None);
        });
    }

    // #[test]
    // fn src_input_find_01() {
    //     with_test_sess("foo/d", |mut i| {
    //         assert_eq!(i.cur_pos(), BytePos(1));
    //         assert_eq!(i.last_pos, BytePos(1));

    //         assert_eq!(i.find(|c| c == '/'), Some(BytePos(5)));
    //         assert_eq!(i.last_pos, BytePos(5));
    //         assert_eq!(i.cur(), Some('d'));
    //     });
    // }

    //    #[test]
    //    fn src_input_smoke_02() {
    //        let _ = crate::with_test_sess("℘℘/℘℘", | mut i| {
    //            assert_eq!(i.iter.as_str(), "℘℘/℘℘");
    //            assert_eq!(i.cur_pos(), BytePos(0));
    //            assert_eq!(i.last_pos, BytePos(0));
    //            assert_eq!(i.start_pos, BytePos(0));
    //            assert_eq!(i.uncons_while(|c| c.is_ident_part()), "℘℘");
    //
    //            assert_eq!(i.iter.as_str(), "/℘℘");
    //            assert_eq!(i.last_pos, BytePos(6));
    //            assert_eq!(i.start_pos, BytePos(6));
    //            assert_eq!(i.cur(), Some('/'));
    //            i.bump();
    //            assert_eq!(i.last_pos, BytePos(7));
    //            assert_eq!(i.start_pos, BytePos(6));
    //
    //            assert_eq!(i.iter.as_str(), "℘℘");
    //            assert_eq!(i.uncons_while(|c| c.is_ident_part()), "℘℘");
    //            assert_eq!(i.last_pos, BytePos(13));
    //            assert_eq!(i.start_pos, BytePos(13));
    //
    //            Ok(())
    //        });
    //    }
}
