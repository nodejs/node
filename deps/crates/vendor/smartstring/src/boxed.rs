// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

use alloc::{alloc::Layout, string::String};
use core::{
    mem::align_of,
    ops::{Deref, DerefMut},
    ptr::NonNull,
};

use crate::{ops::GenericString, MAX_INLINE};

#[cfg(target_endian = "little")]
#[repr(C)]
pub(crate) struct BoxedString {
    ptr: NonNull<u8>,
    cap: usize,
    len: usize,
}

#[cfg(target_endian = "big")]
#[repr(C)]
pub(crate) struct BoxedString {
    len: usize,
    cap: usize,
    ptr: NonNull<u8>,
}

/// Checks if a pointer is aligned to an even address (good)
/// or an odd address (either actually an InlineString or very, very bad).
///
/// Returns `true` if aligned to an odd address, `false` if even. The sense of
/// the boolean is "does this look like an InlineString? true/false"
fn check_alignment(ptr: *const u8) -> bool {
    ptr.align_offset(2) > 0
}

impl GenericString for BoxedString {
    fn set_size(&mut self, size: usize) {
        self.len = size;
        debug_assert!(self.len <= self.cap);
    }

    fn as_mut_capacity_slice(&mut self) -> &mut [u8] {
        #[allow(unsafe_code)]
        unsafe {
            core::slice::from_raw_parts_mut(self.ptr.as_ptr(), self.capacity())
        }
    }
}

impl BoxedString {
    const MINIMAL_CAPACITY: usize = MAX_INLINE * 2;

    pub(crate) fn check_alignment(this: &Self) -> bool {
        check_alignment(this.ptr.as_ptr())
    }

    fn layout_for(cap: usize) -> Layout {
        // Always request memory that is specifically aligned to at least 2, so
        // the least significant bit is guaranteed to be 0.
        let layout = Layout::array::<u8>(cap)
            .and_then(|layout| layout.align_to(align_of::<u16>()))
            .unwrap();
        assert!(
            layout.size() <= isize::MAX as usize,
            "allocation too large!"
        );
        layout
    }

    fn alloc(cap: usize) -> NonNull<u8> {
        let layout = Self::layout_for(cap);
        #[allow(unsafe_code)]
        let ptr = match NonNull::new(unsafe { alloc::alloc::alloc(layout) }) {
            Some(ptr) => ptr,
            None => alloc::alloc::handle_alloc_error(layout),
        };
        debug_assert!(ptr.as_ptr().align_offset(2) == 0);
        ptr
    }

    fn realloc(&mut self, cap: usize) {
        let layout = Self::layout_for(cap);
        let old_layout = Self::layout_for(self.cap);
        let old_ptr = self.ptr.as_ptr();
        #[allow(unsafe_code)]
        let ptr = unsafe { alloc::alloc::realloc(old_ptr, old_layout, layout.size()) };
        self.ptr = match NonNull::new(ptr) {
            Some(ptr) => ptr,
            None => alloc::alloc::handle_alloc_error(layout),
        };
        self.cap = cap;
        debug_assert!(self.ptr.as_ptr().align_offset(2) == 0);
    }

    pub(crate) fn ensure_capacity(&mut self, target_cap: usize) {
        let mut cap = self.cap;
        while cap < target_cap {
            cap *= 2;
        }
        self.realloc(cap)
    }

    pub(crate) fn new(cap: usize) -> Self {
        let cap = cap.max(Self::MINIMAL_CAPACITY);
        Self {
            cap,
            len: 0,
            ptr: Self::alloc(cap),
        }
    }

    pub(crate) fn from_str(cap: usize, src: &str) -> Self {
        let mut out = Self::new(cap);
        out.len = src.len();
        out.as_mut_capacity_slice()[..src.len()].copy_from_slice(src.as_bytes());
        out
    }

    pub(crate) fn capacity(&self) -> usize {
        self.cap
    }

    pub(crate) fn shrink_to_fit(&mut self) {
        self.realloc(self.len);
    }
}

impl Drop for BoxedString {
    fn drop(&mut self) {
        #[allow(unsafe_code)]
        unsafe {
            alloc::alloc::dealloc(self.ptr.as_ptr(), Self::layout_for(self.cap))
        }
    }
}

impl Clone for BoxedString {
    fn clone(&self) -> Self {
        Self::from_str(self.capacity(), self.deref())
    }
}

impl Deref for BoxedString {
    type Target = str;

    fn deref(&self) -> &Self::Target {
        #[allow(unsafe_code)]
        unsafe {
            core::str::from_utf8_unchecked(core::slice::from_raw_parts(self.ptr.as_ptr(), self.len))
        }
    }
}

impl DerefMut for BoxedString {
    fn deref_mut(&mut self) -> &mut Self::Target {
        #[allow(unsafe_code)]
        unsafe {
            core::str::from_utf8_unchecked_mut(core::slice::from_raw_parts_mut(
                self.ptr.as_ptr(),
                self.len,
            ))
        }
    }
}

impl From<String> for BoxedString {
    #[allow(unsafe_code, unused_mut)]
    fn from(mut s: String) -> Self {
        if s.is_empty() {
            Self::new(s.capacity())
        } else {
            #[cfg(has_allocator)]
            {
                // TODO: Use String::into_raw_parts when stabilised, meanwhile let's get unsafe
                let len = s.len();
                let cap = s.capacity();
                #[allow(unsafe_code)]
                let ptr = unsafe { NonNull::new_unchecked(s.as_mut_ptr()) };
                let old_layout = Layout::array::<u8>(cap).unwrap();

                use alloc::alloc::Allocator;
                let allocator = alloc::alloc::Global;
                if let Ok(aligned_ptr) =
                    unsafe { allocator.grow(ptr, old_layout, Self::layout_for(cap)) }
                {
                    core::mem::forget(s);
                    Self {
                        cap,
                        len,
                        ptr: aligned_ptr.cast(),
                    }
                } else {
                    Self::from_str(cap, &s)
                }
            }
            #[cfg(not(has_allocator))]
            Self::from_str(s.capacity(), &s)
        }
    }
}

impl From<BoxedString> for String {
    #[allow(unsafe_code)]
    fn from(s: BoxedString) -> Self {
        #[cfg(has_allocator)]
        {
            let ptr = s.ptr;
            let cap = s.cap;
            let len = s.len;
            let new_layout = Layout::array::<u8>(cap).unwrap();

            use alloc::alloc::Allocator;
            let allocator = alloc::alloc::Global;
            if let Ok(aligned_ptr) =
                unsafe { allocator.grow(ptr, BoxedString::layout_for(cap), new_layout) }
            {
                core::mem::forget(s);
                unsafe { String::from_raw_parts(aligned_ptr.as_ptr().cast(), len, cap) }
            } else {
                String::from(s.deref())
            }
        }
        #[cfg(not(has_allocator))]
        String::from(s.deref())
    }
}
