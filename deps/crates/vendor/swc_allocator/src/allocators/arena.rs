use std::ops::{Deref, DerefMut};

use bumpalo::Bump;

/// Arena allocator
#[derive(Default)]
pub struct Arena {
    inner: Bump,
}

impl Deref for Arena {
    type Target = Bump;

    fn deref(&self) -> &Self::Target {
        &self.inner
    }
}

impl DerefMut for Arena {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.inner
    }
}

impl From<Bump> for Arena {
    fn from(inner: Bump) -> Self {
        Self { inner }
    }
}

#[cfg(feature = "nightly")]
unsafe impl std::alloc::Allocator for &'_ Arena {
    #[inline]
    fn allocate(
        &self,
        layout: std::alloc::Layout,
    ) -> Result<std::ptr::NonNull<[u8]>, std::alloc::AllocError> {
        std::alloc::Allocator::allocate(&&self.inner, layout)
    }

    #[inline]
    unsafe fn deallocate(&self, ptr: std::ptr::NonNull<u8>, layout: std::alloc::Layout) {
        std::alloc::Allocator::deallocate(&&self.inner, ptr, layout)
    }

    #[inline]
    fn allocate_zeroed(
        &self,
        layout: std::alloc::Layout,
    ) -> Result<std::ptr::NonNull<[u8]>, std::alloc::AllocError> {
        std::alloc::Allocator::allocate_zeroed(&&self.inner, layout)
    }

    #[inline]
    unsafe fn grow(
        &self,
        ptr: std::ptr::NonNull<u8>,
        old_layout: std::alloc::Layout,
        new_layout: std::alloc::Layout,
    ) -> Result<std::ptr::NonNull<[u8]>, std::alloc::AllocError> {
        std::alloc::Allocator::grow(&&self.inner, ptr, old_layout, new_layout)
    }

    #[inline]
    unsafe fn grow_zeroed(
        &self,
        ptr: std::ptr::NonNull<u8>,
        old_layout: std::alloc::Layout,
        new_layout: std::alloc::Layout,
    ) -> Result<std::ptr::NonNull<[u8]>, std::alloc::AllocError> {
        std::alloc::Allocator::grow_zeroed(&&self.inner, ptr, old_layout, new_layout)
    }

    #[inline]
    unsafe fn shrink(
        &self,
        ptr: std::ptr::NonNull<u8>,
        old_layout: std::alloc::Layout,
        new_layout: std::alloc::Layout,
    ) -> Result<std::ptr::NonNull<[u8]>, std::alloc::AllocError> {
        std::alloc::Allocator::shrink(&&self.inner, ptr, old_layout, new_layout)
    }
}

#[cfg(not(feature = "nightly"))]
unsafe impl allocator_api2::alloc::Allocator for &'_ Arena {
    #[inline]
    fn allocate(
        &self,
        layout: std::alloc::Layout,
    ) -> Result<std::ptr::NonNull<[u8]>, allocator_api2::alloc::AllocError> {
        allocator_api2::alloc::Allocator::allocate(&&self.inner, layout)
    }

    #[inline]
    unsafe fn deallocate(&self, ptr: std::ptr::NonNull<u8>, layout: std::alloc::Layout) {
        allocator_api2::alloc::Allocator::deallocate(&&self.inner, ptr, layout)
    }

    #[inline]
    fn allocate_zeroed(
        &self,
        layout: std::alloc::Layout,
    ) -> Result<std::ptr::NonNull<[u8]>, allocator_api2::alloc::AllocError> {
        allocator_api2::alloc::Allocator::allocate_zeroed(&&self.inner, layout)
    }

    #[inline]
    unsafe fn grow(
        &self,
        ptr: std::ptr::NonNull<u8>,
        old_layout: std::alloc::Layout,
        new_layout: std::alloc::Layout,
    ) -> Result<std::ptr::NonNull<[u8]>, allocator_api2::alloc::AllocError> {
        allocator_api2::alloc::Allocator::grow(&&self.inner, ptr, old_layout, new_layout)
    }

    #[inline]
    unsafe fn grow_zeroed(
        &self,
        ptr: std::ptr::NonNull<u8>,
        old_layout: std::alloc::Layout,
        new_layout: std::alloc::Layout,
    ) -> Result<std::ptr::NonNull<[u8]>, allocator_api2::alloc::AllocError> {
        allocator_api2::alloc::Allocator::grow_zeroed(&&self.inner, ptr, old_layout, new_layout)
    }

    #[inline]
    unsafe fn shrink(
        &self,
        ptr: std::ptr::NonNull<u8>,
        old_layout: std::alloc::Layout,
        new_layout: std::alloc::Layout,
    ) -> Result<std::ptr::NonNull<[u8]>, allocator_api2::alloc::AllocError> {
        allocator_api2::alloc::Allocator::shrink(&&self.inner, ptr, old_layout, new_layout)
    }
}
