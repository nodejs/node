//! Interface to QuRT (Qualcomm Real-Time OS) C library
//!
//! This module re-exports items from the new module structure.
//! QuRT was introduced after the `src/new/` module structure was established,
//! so all definitions live in `src/new/qurt/` and are re-exported here
//! for compatibility with the existing libc structure.

// Re-export everything from the new qurt module
pub use crate::new::qurt::*;

// Architecture-specific modules (if any are needed in the future)
cfg_if! {
    if #[cfg(target_arch = "hexagon")] {
        // Currently no hexagon-specific items needed beyond what's in new/qurt
    } else {
        // Add other architectures as needed
    }
}
