// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

use crate::{ops::bounds_for, SmartString, SmartStringMode};
use core::{
    fmt::{Debug, Error, Formatter},
    iter::FusedIterator,
    ops::RangeBounds,
    str::Chars,
};

/// A draining iterator for a [`SmartString`].
pub struct Drain<'a, Mode: SmartStringMode> {
    string: *mut SmartString<Mode>,
    start: usize,
    end: usize,
    iter: Chars<'a>,
}

impl<'a, Mode: SmartStringMode> Drain<'a, Mode> {
    pub(crate) fn new<R>(string: &'a mut SmartString<Mode>, range: R) -> Self
    where
        R: RangeBounds<usize>,
    {
        let string_ptr: *mut _ = string;
        let len = string.len();
        let (start, end) = bounds_for(&range, len);
        assert!(start <= end);
        assert!(end <= len);
        assert!(string.as_str().is_char_boundary(start));
        assert!(string.as_str().is_char_boundary(end));

        let iter = string.as_str()[start..end].chars();
        Drain {
            string: string_ptr,
            start,
            end,
            iter,
        }
    }
}

impl<'a, Mode: SmartStringMode> Drop for Drain<'a, Mode> {
    fn drop(&mut self) {
        #[allow(unsafe_code)]
        let string = unsafe { &mut *self.string };
        debug_assert!(string.as_str().is_char_boundary(self.start));
        debug_assert!(string.as_str().is_char_boundary(self.end));
        string.replace_range(self.start..self.end, "");
    }
}

impl<'a, Mode: SmartStringMode> Iterator for Drain<'a, Mode> {
    type Item = char;

    #[inline]
    fn next(&mut self) -> Option<Self::Item> {
        self.iter.next()
    }

    #[inline]
    fn size_hint(&self) -> (usize, Option<usize>) {
        self.iter.size_hint()
    }
}

impl<'a, Mode: SmartStringMode> DoubleEndedIterator for Drain<'a, Mode> {
    #[inline]
    fn next_back(&mut self) -> Option<Self::Item> {
        self.iter.next_back()
    }
}

impl<'a, Mode: SmartStringMode> FusedIterator for Drain<'a, Mode> {}

impl<'a, Mode: SmartStringMode> Debug for Drain<'a, Mode> {
    fn fmt(&self, f: &mut Formatter<'_>) -> Result<(), Error> {
        f.pad("Drain { ... }")
    }
}
