// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

//! Generic string ops.
//!
//! `string_op_grow` is for ops which may grow but not shrink the target
//! string, and should have a `cap` method which will return the new
//! minimum required capacity.
//!
//! `string_op_shrink` is for ops which may shrinl but not grow the target
//! string. They don't need a `cap` method, and will try to demote the
//! string as appropriate after calling `op`.

use core::{
    marker::PhantomData,
    ops::{Bound, Deref, DerefMut, RangeBounds},
};

pub(crate) trait GenericString: Deref<Target = str> + DerefMut<Target = str> {
    fn set_size(&mut self, size: usize);
    fn as_mut_capacity_slice(&mut self) -> &mut [u8];
}

macro_rules! string_op_grow {
    ($action:ty, $target:ident, $($arg:expr),*) => {
        match $target.cast_mut() {
            StringCastMut::Boxed(this) => {
                this.ensure_capacity(<$action>::cap(this, $($arg),*));
                <$action>::op(this, $($arg),*)
            }
            StringCastMut::Inline(this) => {
                let new_size = <$action>::cap(this,$($arg),*);
                if new_size > MAX_INLINE {
                    let mut new_str = BoxedString::from_str(new_size, this);
                    let result = <$action>::op(&mut new_str, $($arg),*);
                    $target.promote_from(new_str);
                    result
                } else {
                    <$action>::op(this, $($arg),*)
                }
            }
        }
    };
}
pub(crate) use string_op_grow;

macro_rules! string_op_shrink {
    ($action:ty, $target:ident, $($arg:expr),*) => {{
        let result = match $target.cast_mut() {
            StringCastMut::Boxed(this) => {
                <$action>::op(this, $($arg),*)
            }
            StringCastMut::Inline(this) => {
                <$action>::op(this, $($arg),*)
            }
        };
        $target.try_demote();
        result
    }};

    ($action:ty, $target:ident) => {
        string_op_shrink!($action, $target,)
    }
}
pub(crate) use string_op_shrink;

use crate::{SmartString, SmartStringMode};

pub(crate) fn bounds_for<R>(range: &R, max_len: usize) -> (usize, usize)
where
    R: RangeBounds<usize>,
{
    let start = match range.start_bound() {
        Bound::Included(&n) => n,
        Bound::Excluded(&n) => n.checked_add(1).unwrap(),
        Bound::Unbounded => 0,
    };
    let end = match range.end_bound() {
        Bound::Included(&n) => n.checked_add(1).unwrap(),
        Bound::Excluded(&n) => n,
        Bound::Unbounded => max_len,
    };
    (start, end)
}

fn insert_bytes<S: GenericString>(this: &mut S, index: usize, src: &[u8]) {
    let len = this.len();
    let src_len = src.len();
    let tail_index = index + src_len;
    if src_len > 0 {
        let buf = this.as_mut_capacity_slice();
        buf.copy_within(index..len, tail_index);
        buf[index..tail_index].copy_from_slice(src);
        this.set_size(len + src_len);
    }
}

pub(crate) struct PushStr;
impl PushStr {
    pub(crate) fn cap<S: GenericString>(this: &S, string: &str) -> usize {
        this.len() + string.len()
    }

    pub(crate) fn op<S: GenericString>(this: &mut S, string: &str) {
        let len = this.len();
        let new_len = len + string.len();
        this.as_mut_capacity_slice()[len..new_len].copy_from_slice(string.as_bytes());
        this.set_size(new_len);
    }
}

pub(crate) struct Push;
impl Push {
    pub(crate) fn cap<S: GenericString>(this: &S, ch: char) -> usize {
        this.len() + ch.len_utf8()
    }

    pub(crate) fn op<S: GenericString>(this: &mut S, ch: char) {
        let len = this.len();
        let written = ch
            .encode_utf8(&mut this.as_mut_capacity_slice()[len..])
            .len();
        this.set_size(len + written);
    }
}

pub(crate) struct Truncate;
impl Truncate {
    pub(crate) fn op<S: GenericString>(this: &mut S, new_len: usize) {
        if new_len < this.len() {
            assert!(this.deref().is_char_boundary(new_len));
            this.set_size(new_len)
        }
    }
}

pub(crate) struct Pop;
impl Pop {
    pub(crate) fn op<S: GenericString>(this: &mut S) -> Option<char> {
        let ch = this.deref().chars().rev().next()?;
        this.set_size(this.len() - ch.len_utf8());
        Some(ch)
    }
}

pub(crate) struct Remove;
impl Remove {
    pub(crate) fn op<S: GenericString>(this: &mut S, index: usize) -> char {
        let ch = match this.deref()[index..].chars().next() {
            Some(ch) => ch,
            None => panic!("cannot remove a char from the end of a string"),
        };
        let next = index + ch.len_utf8();
        let len = this.len();
        let tail_len = len - next;
        if tail_len > 0 {
            this.as_mut_capacity_slice().copy_within(next..len, index);
        }
        this.set_size(len - (next - index));
        ch
    }
}

pub(crate) struct Insert;
impl Insert {
    pub(crate) fn cap<S: GenericString>(this: &S, index: usize, ch: char) -> usize {
        assert!(this.deref().is_char_boundary(index));
        this.len() + ch.len_utf8()
    }

    pub(crate) fn op<S: GenericString>(this: &mut S, index: usize, ch: char) {
        let mut buffer = [0; 4];
        let buffer = ch.encode_utf8(&mut buffer).as_bytes();
        insert_bytes(this, index, buffer);
    }
}

pub(crate) struct InsertStr;
impl InsertStr {
    pub(crate) fn cap<S: GenericString>(this: &S, index: usize, string: &str) -> usize {
        assert!(this.deref().is_char_boundary(index));
        this.len() + string.len()
    }

    pub(crate) fn op<S: GenericString>(this: &mut S, index: usize, string: &str) {
        insert_bytes(this, index, string.as_bytes());
    }
}

pub(crate) struct SplitOff<Mode: SmartStringMode>(PhantomData<Mode>);
impl<Mode: SmartStringMode> SplitOff<Mode> {
    pub(crate) fn op<S: GenericString>(this: &mut S, index: usize) -> SmartString<Mode> {
        assert!(this.deref().is_char_boundary(index));
        let result = this.deref()[index..].into();
        this.set_size(index);
        result
    }
}

pub(crate) struct Retain;
impl Retain {
    pub(crate) fn op<F, S>(this: &mut S, mut f: F)
    where
        F: FnMut(char) -> bool,
        S: GenericString,
    {
        let len = this.len();
        let mut del_bytes = 0;
        let mut index = 0;

        while index < len {
            let ch = this
                .deref_mut()
                .get(index..len)
                .unwrap()
                .chars()
                .next()
                .unwrap();
            let ch_len = ch.len_utf8();

            if !f(ch) {
                del_bytes += ch_len;
            } else if del_bytes > 0 {
                this.as_mut_capacity_slice()
                    .copy_within(index..index + ch_len, index - del_bytes);
            }
            index += ch_len;
        }

        if del_bytes > 0 {
            this.set_size(len - del_bytes);
        }
    }
}

pub(crate) struct ReplaceRange;
impl ReplaceRange {
    pub(crate) fn cap<R, S>(this: &S, range: &R, replace_with: &str) -> usize
    where
        R: RangeBounds<usize>,
        S: GenericString,
    {
        let len = this.len();
        let (start, end) = bounds_for(range, len);
        assert!(end >= start);
        assert!(end <= len);
        assert!(this.deref().is_char_boundary(start));
        assert!(this.deref().is_char_boundary(end));
        let replace_len = replace_with.len();
        let end_size = len - end;
        start + replace_len + end_size
    }

    pub(crate) fn op<R, S>(this: &mut S, range: &R, replace_with: &str)
    where
        R: RangeBounds<usize>,
        S: GenericString,
    {
        let len = this.len();
        let (start, end) = bounds_for(range, len);
        let replace_len = replace_with.len();
        let new_end = start + replace_len;
        let end_size = len - end;
        this.as_mut_capacity_slice().copy_within(end..len, new_end);
        if replace_len > 0 {
            this.as_mut_capacity_slice()[start..new_end].copy_from_slice(replace_with.as_bytes());
        }
        this.set_size(start + replace_len + end_size);
    }
}
