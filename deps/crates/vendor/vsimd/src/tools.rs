#[cfg(feature = "alloc")]
item_group! {
    use core::mem::MaybeUninit;
    use alloc::boxed::Box;
}

/// Allocates uninit bytes
///
/// # Safety
/// This function requires:
///
/// + `len > 0`
/// + `len <= isize::MAX`
///
#[cfg(feature = "alloc")]
#[inline]
#[must_use]
pub unsafe fn alloc_uninit_bytes(len: usize) -> Box<[MaybeUninit<u8>]> {
    #[allow(clippy::checked_conversions)]
    #[cfg(any(debug_assertions, miri))]
    {
        assert!(len > 0 && len <= (isize::MAX as usize));
    }
    use alloc::alloc::{alloc, handle_alloc_error, Layout};
    let layout = Layout::from_size_align_unchecked(len, 1);
    let p = alloc(layout);
    if p.is_null() {
        handle_alloc_error(layout)
    }
    let ptr = p.cast();
    Box::from_raw(core::ptr::slice_from_raw_parts_mut(ptr, len))
}

#[cfg(feature = "alloc")]
#[inline]
#[must_use]
pub unsafe fn assume_init(b: Box<[MaybeUninit<u8>]>) -> Box<[u8]> {
    let len = b.len();
    let ptr = Box::into_raw(b).cast::<u8>();
    Box::from_raw(core::ptr::slice_from_raw_parts_mut(ptr, len))
}

#[inline(always)]
pub unsafe fn read<T>(base: *const T, offset: usize) -> T {
    base.add(offset).read()
}

#[inline(always)]
pub unsafe fn write<T>(base: *mut T, offset: usize, value: T) {
    base.add(offset).write(value);
}

#[inline(always)]
pub unsafe fn slice<'a, T>(data: *const T, len: usize) -> &'a [T] {
    core::slice::from_raw_parts(data, len)
}

#[inline(always)]
pub unsafe fn slice_mut<'a, T>(data: *mut T, len: usize) -> &'a mut [T] {
    core::slice::from_raw_parts_mut(data, len)
}

#[inline(always)]
pub fn unroll<T>(slice: &[T], chunk_size: usize, mut f: impl FnMut(&T)) {
    let mut iter = slice.chunks_exact(chunk_size);
    for chunk in &mut iter {
        chunk.iter().for_each(&mut f);
    }
    iter.remainder().iter().for_each(&mut f);
}

#[inline(always)]
#[must_use]
pub fn is_same_type<A, B>() -> bool
where
    A: 'static,
    B: 'static,
{
    use core::any::TypeId;
    TypeId::of::<A>() == TypeId::of::<B>()
}

#[inline(always)]
pub fn slice_parts<T>(slice: &[T]) -> (*const T, usize) {
    let len = slice.len();
    let ptr = slice.as_ptr();
    (ptr, len)
}

#[cfg(feature = "alloc")]
#[inline(always)]
#[must_use]
pub unsafe fn boxed_str(b: Box<[u8]>) -> Box<str> {
    let ptr = Box::into_raw(b);
    Box::from_raw(core::str::from_utf8_unchecked_mut(&mut *ptr))
}

#[allow(clippy::ptr_as_ptr)]
#[inline(always)]
#[cfg_attr(debug_assertions, track_caller)]
pub unsafe fn transmute_copy<A: Copy, B: Copy>(a: &A) -> B {
    debug_assert!(core::mem::size_of::<A>() == core::mem::size_of::<B>());
    *(a as *const A as *const B)
}
