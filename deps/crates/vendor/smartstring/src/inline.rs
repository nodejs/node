// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

use crate::{config::MAX_INLINE, marker_byte::Marker, ops::GenericString};
use core::{
    ops::{Deref, DerefMut},
    str::{from_utf8_unchecked, from_utf8_unchecked_mut},
};

#[cfg(target_endian = "little")]
#[repr(C)]
#[cfg_attr(target_pointer_width = "64", repr(align(8)))]
#[cfg_attr(target_pointer_width = "32", repr(align(4)))]
pub(crate) struct InlineString {
    pub(crate) marker: Marker,
    pub(crate) data: [u8; MAX_INLINE],
}

#[cfg(target_endian = "big")]
#[repr(C)]
#[cfg_attr(target_pointer_width = "64", repr(align(8)))]
#[cfg_attr(target_pointer_width = "32", repr(align(4)))]
pub(crate) struct InlineString {
    pub(crate) data: [u8; MAX_INLINE],
    pub(crate) marker: Marker,
}

impl Clone for InlineString {
    fn clone(&self) -> Self {
        unreachable!("InlineString should be copy!")
    }
}

impl Copy for InlineString {}

impl Deref for InlineString {
    type Target = str;

    fn deref(&self) -> &Self::Target {
        #[allow(unsafe_code)]
        unsafe {
            from_utf8_unchecked(&self.data[..self.len()])
        }
    }
}

impl DerefMut for InlineString {
    fn deref_mut(&mut self) -> &mut Self::Target {
        let len = self.len();
        #[allow(unsafe_code)]
        unsafe {
            from_utf8_unchecked_mut(&mut self.data[..len])
        }
    }
}

impl GenericString for InlineString {
    fn set_size(&mut self, size: usize) {
        self.marker.set_data(size as u8);
    }

    fn as_mut_capacity_slice(&mut self) -> &mut [u8] {
        self.data.as_mut()
    }
}

impl InlineString {
    pub(crate) const fn new() -> Self {
        Self {
            marker: Marker::empty(),
            data: [0; MAX_INLINE],
        }
    }

    pub(crate) fn len(&self) -> usize {
        let len = self.marker.data() as usize;
        debug_assert!(len <= MAX_INLINE);
        len
    }
}

impl From<&str> for InlineString {
    fn from(string: &str) -> Self {
        let len = string.len();
        debug_assert!(len <= MAX_INLINE);
        let mut out = Self::new();
        out.marker = Marker::new_inline(len as u8);
        out.data.as_mut()[..len].copy_from_slice(string.as_bytes());
        out
    }
}
