#![allow(unused_imports)]

#[cfg(feature = "nightly")]
pub use std::alloc::Allocator;

#[cfg(not(feature = "nightly"))]
pub use allocator_api2::alloc::Allocator;
