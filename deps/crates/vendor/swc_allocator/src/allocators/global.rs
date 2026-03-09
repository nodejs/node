#[cfg(feature = "nightly")]
pub use std::alloc::Global;

#[cfg(not(feature = "nightly"))]
pub use allocator_api2::alloc::Global;
