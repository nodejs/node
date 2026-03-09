//! Various flavors of allocator.

/// Re-export for convenience.
#[cfg(feature = "hashbrown")]
pub extern crate hashbrown;

/// Types for arena allocator
pub mod arena {
    #[allow(unused_imports)]
    use crate::allocators::Arena;

    /// See [`hashbrown::HashMap`].
    #[cfg(feature = "hashbrown")]
    pub type HashMap<'alloc, K, V, S = rustc_hash::FxBuildHasher> =
        hashbrown::HashMap<K, V, S, &'alloc Arena>;

    /// See [`hashbrown::HashSet`].
    #[cfg(feature = "hashbrown")]
    pub type HashSet<'alloc, T, S = rustc_hash::FxBuildHasher> =
        hashbrown::HashSet<T, S, &'alloc Arena>;

    // /// See [`crate::Box`].
    // pub type Box<T, A = $alloc> = crate::Box<T, A>;

    // /// See [`std::vec::Vec`].
    // pub type Vec<T, A = $alloc> = crate::Vec<T, A>;
}

/// Types for scoped allocator
pub mod scoped {

    #[allow(unused_imports)]
    use crate::allocators::Scoped;

    /// See [`hashbrown::HashMap`].
    #[cfg(feature = "hashbrown")]
    pub type HashMap<'alloc, K, V, S = rustc_hash::FxBuildHasher> =
        hashbrown::HashMap<K, V, S, Scoped>;

    /// See [`hashbrown::HashSet`].
    #[cfg(feature = "hashbrown")]
    pub type HashSet<'alloc, T, S = rustc_hash::FxBuildHasher> = hashbrown::HashSet<T, S, Scoped>;
}

/// Types for global allocator
pub mod global {

    /// See [`hashbrown::HashMap`].
    #[cfg(feature = "hashbrown")]
    pub type HashMap<K, V, S = rustc_hash::FxBuildHasher> = hashbrown::HashMap<K, V, S>;

    /// See [`hashbrown::HashSet`].
    #[cfg(feature = "hashbrown")]
    pub type HashSet<T, S = rustc_hash::FxBuildHasher> = hashbrown::HashSet<T, S>;
}
