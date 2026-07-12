// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[cfg(feature = "export")]
use zerovec::ule::EncodeAsVarULE;
use zerovec::ule::VarULE;

#[cfg(feature = "alloc")]
use zerovec::{maps::ZeroMapKV, ZeroMap, ZeroMap2d};

/// A trait that associates a [`VarULE`] type with a data struct.
///
/// Some data structs can be represented compactly as a single [`VarULE`],
/// such as `str` or a packed pattern. This trait allows for data providers
/// to use optimizations for such types.
///
/// ❗ Not all data structs benefit from this optimization. It works best when the
/// data struct is multiplied across a large number of data marker attributes.
///
/// Both [`MaybeAsVarULE`] and [`MaybeEncodeAsVarULE`] should be implemented
/// on all data structs. The [`data_struct!`](crate::data_struct) macro provides an impl.
pub trait MaybeAsVarULE {
    /// The [`VarULE`] type for this data struct, or `[()]`
    /// if it cannot be represented as [`VarULE`].
    type EncodedStruct: ?Sized + VarULE;
}

/// Export-only trait associated with [`MaybeAsVarULE`]. See that trait
/// for additional details.
///
/// ✨ *Enabled with the `export` Cargo feature.*
#[cfg(feature = "export")]
pub trait MaybeEncodeAsVarULE: MaybeAsVarULE {
    /// The type returned by [`Self::maybe_as_encodeable`].
    type EncodeableStruct<'a>: EncodeAsVarULE<Self::EncodedStruct>
    where
        Self: 'a;
    /// Returns something encodeable to the [`MaybeAsVarULE::EncodedStruct`] that represents
    /// this data struct, or `None` if the data struct does not support this representation.
    fn maybe_as_encodeable<'a>(&'a self) -> Option<Self::EncodeableStruct<'a>>;
}

/// Implements required traits on data structs, such as [`MaybeEncodeAsVarULE`].
#[macro_export] // canonical location is crate root
macro_rules! data_struct {
    (<$generic:ident: $bound:tt> $ty:path $(, $(#[$attr:meta])*)?) => {
        impl<$generic: $bound> $crate::ule::MaybeAsVarULE for $ty {
            type EncodedStruct = [()];
        }
        $($(#[$attr])*)?
        impl<$generic: $bound> $crate::ule::MaybeEncodeAsVarULE for $ty {
            type EncodeableStruct<'b> = &'b [()] where Self: 'b;
            fn maybe_as_encodeable<'b>(&'b self) -> Option<Self::EncodeableStruct<'b>> {
                None
            }
        }
    };
    ($ty:path $(, $(#[$attr:meta])*)?) => {
        impl $crate::ule::MaybeAsVarULE for $ty {
            type EncodedStruct = [()];
        }
        $($(#[$attr])*)?
        impl $crate::ule::MaybeEncodeAsVarULE for $ty {
            type EncodeableStruct<'b> = &'b [()] where Self: 'b;
            fn maybe_as_encodeable<'b>(&'b self) -> Option<Self::EncodeableStruct<'b>> {
                None
            }
        }
    };
    (
        $ty:ty,
        varule: $varule:ty,
        $(#[$attr:meta])*
        encode_as_varule: $encode_as_varule:expr
    ) => {
        impl<'data> $crate::ule::MaybeAsVarULE for $ty {
            type EncodedStruct = $varule;
        }
        $(#[$attr])*
        impl<'data> $crate::ule::MaybeEncodeAsVarULE for $ty {
            type EncodeableStruct<'b> = &'b $varule where Self: 'b;
            fn maybe_as_encodeable<'b>(&'b self) -> Option<Self::EncodeableStruct<'b>> {
                // Workaround for <https://rust-lang.github.io/rfcs/3216-closure-lifetime-binder.html>
                fn bind_lifetimes<F>(f: F) -> F where F: for<'data> Fn(&'data $ty) -> &'data $varule { f }
                Some(bind_lifetimes($encode_as_varule)(self))
            }
        }
    };
}

//=== Standard impls ===//

#[cfg(feature = "alloc")]
impl<'a, K0, V> MaybeAsVarULE for ZeroMap<'a, K0, V>
where
    K0: ZeroMapKV<'a>,
    V: ZeroMapKV<'a>,
    K0: ?Sized,
    V: ?Sized,
{
    type EncodedStruct = [()];
}

#[cfg(feature = "alloc")]
#[cfg(feature = "export")]
impl<'a, K0, V> MaybeEncodeAsVarULE for ZeroMap<'a, K0, V>
where
    K0: ZeroMapKV<'a>,
    V: ZeroMapKV<'a>,
    K0: ?Sized,
    V: ?Sized,
{
    type EncodeableStruct<'b>
        = &'b [()]
    where
        Self: 'b;
    fn maybe_as_encodeable<'b>(&'b self) -> Option<Self::EncodeableStruct<'b>> {
        None
    }
}

#[cfg(feature = "alloc")]
impl<'a, K0, K1, V> MaybeAsVarULE for ZeroMap2d<'a, K0, K1, V>
where
    K0: ZeroMapKV<'a>,
    K1: ZeroMapKV<'a>,
    V: ZeroMapKV<'a>,
    K0: ?Sized,
    K1: ?Sized,
    V: ?Sized,
{
    type EncodedStruct = [()];
}

#[cfg(feature = "alloc")]
#[cfg(feature = "export")]
impl<'a, K0, K1, V> MaybeEncodeAsVarULE for ZeroMap2d<'a, K0, K1, V>
where
    K0: ZeroMapKV<'a>,
    K1: ZeroMapKV<'a>,
    V: ZeroMapKV<'a>,
    K0: ?Sized,
    K1: ?Sized,
    V: ?Sized,
{
    type EncodeableStruct<'b>
        = &'b [()]
    where
        Self: 'b;
    fn maybe_as_encodeable<'b>(&'b self) -> Option<Self::EncodeableStruct<'b>> {
        None
    }
}

impl<T, const N: usize> MaybeAsVarULE for [T; N] {
    type EncodedStruct = [()];
}

#[cfg(feature = "export")]
impl<T, const N: usize> MaybeEncodeAsVarULE for [T; N] {
    type EncodeableStruct<'a>
        = &'a [()]
    where
        Self: 'a;
    fn maybe_as_encodeable<'a>(&'a self) -> Option<Self::EncodeableStruct<'a>> {
        None
    }
}

impl MaybeAsVarULE for u16 {
    type EncodedStruct = [()];
}

#[cfg(feature = "export")]
impl MaybeEncodeAsVarULE for u16 {
    type EncodeableStruct<'a>
        = &'a [()]
    where
        Self: 'a;
    fn maybe_as_encodeable<'a>(&'a self) -> Option<Self::EncodeableStruct<'a>> {
        None
    }
}

impl<'a, V: VarULE + ?Sized> MaybeAsVarULE for zerovec::VarZeroCow<'a, V> {
    type EncodedStruct = V;
}

#[cfg(feature = "export")]
impl<'a, V: VarULE + ?Sized> MaybeEncodeAsVarULE for zerovec::VarZeroCow<'a, V> {
    type EncodeableStruct<'b>
        = &'b V
    where
        Self: 'b;
    fn maybe_as_encodeable<'b>(&'b self) -> Option<Self::EncodeableStruct<'b>> {
        Some(&**self)
    }
}
