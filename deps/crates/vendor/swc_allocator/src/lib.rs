//! Allocator for swc.
//!
//! # Features
//!
//! - `scoped`: Enable `scoped` mode.
//!
//! # Modes
//!
//! ## Default mode
//!
//! In default mode, [crate::boxed::Box] and [crate::vec::Vec] are identical to
//! the original types in [std].
//!
//! ## Scoped mode
//!
//! - You need to enable `scoped` feature to use this mode.
//!
//! In `scoped` mode you can use [FastAlloc] to make [crate::boxed::Box] and
//! [crate::vec::Vec] very fast.
//!
//! In this mode, you need to be careful while using [crate::boxed::Box] and
//! [crate::vec::Vec]. You should ensure that [Allocator] outlives all
//! [crate::boxed::Box] and [crate::vec::Vec] created in the scope.
//!
//! Recommened way to use this mode is to wrap the whole operations in
//! a call to [Allocator::scope].

#![allow(clippy::needless_doctest_main)]
#![cfg_attr(docsrs, feature(doc_cfg))]
#![cfg_attr(
    feature = "nightly",
    feature(allocator_api, fundamental, with_negative_coherence, box_into_inner)
)]
#![deny(missing_docs)]
#![allow(clippy::derivable_impls)]

// TODO: Add types back
// pub use crate::types::*;

pub mod allocators;
pub mod api;
mod types;
mod util;

/// This expands to the given tokens if the `nightly` feature is enabled.
#[cfg(feature = "nightly")]
#[macro_export]
macro_rules! nightly_only {
    (
        $($item:item)*
    ) => {
        $(
            #[cfg_attr(docsrs, doc(cfg(feature = "nightly")))]
            $item
        )*
    };
}

/// This expands to the given tokens if the `nightly` feature is enabled.
#[cfg(not(feature = "nightly"))]
#[macro_export]
macro_rules! nightly_only {
    (
        $($item:item)*
    ) => {};
}
