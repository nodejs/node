/// Scoped allocator
pub struct Scoped;

#[cfg(feature = "nightly")]
unsafe impl std::alloc::Allocator for Scoped {
    fn allocate(
        &self,
        _layout: std::alloc::Layout,
    ) -> Result<std::ptr::NonNull<[u8]>, std::alloc::AllocError> {
        todo!()
    }

    unsafe fn deallocate(&self, _ptr: std::ptr::NonNull<u8>, _layout: std::alloc::Layout) {
        todo!()
    }
}

#[cfg(not(feature = "nightly"))]
unsafe impl allocator_api2::alloc::Allocator for Scoped {
    fn allocate(
        &self,
        _layout: std::alloc::Layout,
    ) -> Result<std::ptr::NonNull<[u8]>, allocator_api2::alloc::AllocError> {
        todo!()
    }

    unsafe fn deallocate(&self, _ptr: std::ptr::NonNull<u8>, _layout: std::alloc::Layout) {
        todo!()
    }
}
