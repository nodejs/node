//! Platform-agnostic support types.

#[cfg(feature = "extra_traits")]
use core::hash::Hash;
use core::mem::MaybeUninit;

use crate::prelude::*;

/// A transparent wrapper over `MaybeUninit<T>` to represent uninitialized padding
/// while providing `Default`.
// This is restricted to `Copy` types since that's a loose indicator that zeros is actually
// a valid bitpattern. There is no technical reason this is required, though, so it could be
// lifted in the future if it becomes a problem.
#[allow(unused)]
#[repr(transparent)]
#[derive(Clone, Copy)]
pub(crate) struct Padding<T: Copy>(MaybeUninit<T>);

impl<T: Copy> Default for Padding<T> {
    fn default() -> Self {
        Self(MaybeUninit::zeroed())
    }
}

impl<T: Copy> Padding<T> {
    /// Const constructor for uninitialized padding in const contexts.
    // FIXME: Change this into zeroed() and use MaybeUninit::zeroed()
    // when we depend on rustc 1.75.0.
    #[allow(unused)]
    pub(crate) const fn uninit() -> Self {
        // We can still safely use uninit here, since padding is something
        // that is not meant to be read or written anyways.
        Self(MaybeUninit::uninit())
    }
}

impl<T: Copy> fmt::Debug for Padding<T> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        // Taken frmo `MaybeUninit`'s debug implementation
        // NB: there is no `.pad_fmt` so we can't use a simpler `format_args!("Padding<{..}>").
        let full_name = core::any::type_name::<Self>();
        let prefix_len = full_name.find("Padding").unwrap();
        f.pad(&full_name[prefix_len..])
    }
}

/// Do nothing when hashing to ignore the existence of padding fields.
#[cfg(feature = "extra_traits")]
impl<T: Copy> Hash for Padding<T> {
    fn hash<H: hash::Hasher>(&self, _state: &mut H) {}
}

/// Padding fields are all equal, regardless of what is inside them, so they do not affect anything.
#[cfg(feature = "extra_traits")]
impl<T: Copy> PartialEq for Padding<T> {
    fn eq(&self, _other: &Self) -> bool {
        true
    }
}

/// Mark that `Padding` implements `Eq` so that it can be used in types that implement it.
#[cfg(feature = "extra_traits")]
impl<T: Copy> Eq for Padding<T> {}

/// The default repr type used for C style enums in Rust.
#[cfg(target_env = "msvc")]
#[allow(unused)]
pub(crate) type CEnumRepr = c_int;
#[cfg(not(target_env = "msvc"))]
#[allow(unused)]
pub(crate) type CEnumRepr = c_uint;
