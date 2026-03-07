#![cfg_attr(feature = "atom_size_128", feature(integer_atomics))]
//! See [Atom] for more information.

use core::str;
use std::{
    borrow::Borrow,
    fmt::{Debug, Display},
    hash::Hash,
    mem::{self, forget, transmute, ManuallyDrop},
    num::NonZeroU8,
    ops::Deref,
    str::from_utf8_unchecked,
};

use debug_unreachable::debug_unreachable;
use once_cell::sync::Lazy;

pub use crate::dynamic::{global_atom_store_gc, AtomStore};
use crate::{
    macros::{get_hash, impl_from_alias, partial_eq},
    tagged_value::TaggedValue,
};

mod dynamic;
mod global_store;
mod macros;
mod tagged_value;
#[cfg(test)]
mod tests;
pub mod wtf8;
mod wtf8_atom;

pub use wtf8_atom::Wtf8Atom;

/// An immutable string which is cheap to clone, compare, hash, and has small
/// size.
///
/// # Usecase
///
/// This type is designed for the compilers or build tools like a bundler
/// written in Rust. String interning is much costly than simply allocating a
/// string, but in compilers, hashing and comparison of strings are very
/// frequent operations for some of strings, and the other strings are mostly
/// just passed around. According to DoD, we should optimize types
/// for operations that occur frequently, and this type is the result.
///
/// # Features
///
/// ## No mutex on creation and destruction.
///
/// This type also considers concurrent processing of AST nodes. Parsers
/// generates lots of [Atom]s, so the creation of [Atom] should never involve a
/// global mutex, because parsers are embarrassingly parallel. Also, the AST
/// nodes are typically dropped in parallel. So [Drop] implementation of [Atom]
/// should not involve a global mutex, too.
///
///
/// ## Small size (One `u64`)
///
/// The most of strings are simply passed around, so the size of [Atom] should
/// be small as possible.
///
/// ```rust
/// # use std::mem::size_of;
/// # if !cfg!(feature = "atom_size_128") {
/// use hstr::Atom;
/// assert!(size_of::<Atom>() == size_of::<u64>());
/// assert!(size_of::<Option<Atom>>() == size_of::<u64>());
/// # }
/// ````
///
///
/// ## Fast equality check (in most cases)
///
/// Equality comparison is O(1) in most cases. If two atoms are from the same
/// [AtomStore], or they are from different stores but they are
/// [`AtomStore::merge`]d, they are compared by numeric equality.
///
/// If two strings are created from different [AtomStore]s, they are compared
/// using `strcmp` by default. But `hstr` allows you to make `==` faster -
/// [`AtomStore::merge`].
///
///
/// ## Fast [Hash] implementation
///
/// [Atom] precompute the hash value of long strings when they are created, so
/// it is `O(1)` to compute hash.
///
///
/// ## Small strings as inline data
///
/// Small strings are stored in the [Atom] itself without any allocation.
///
///
/// ## Creating atoms
///
/// If you are working on a module which creates lots of [Atom]s, you are
/// recommended to use [AtomStore] API because it's faster. But if you are not,
/// you can use global APIs for convenience.
///
/// ## Dealloc
///
/// - Atoms stored in the local `AtomStore` have the same lifetime as the store
///   itself, and will be deallocated when the store is dropped.
/// - Atoms created via the `atom!` macro or `String::into` are stored in the
///   global atom store. By default, these atoms are never deallocated. To clean
///   up unused atoms, call [global_atom_store_gc].
#[repr(transparent)]
pub struct Atom {
    // If this Atom is a dynamic one, this is *const Entry
    unsafe_data: TaggedValue,
}

#[doc(hidden)]
pub type CachedAtom = Lazy<Atom>;

#[doc(hidden)]
pub const fn inline_atom(s: &str) -> Option<Atom> {
    dynamic::inline_atom(s)
}

/// Create an atom from a string literal. This atom is never dropped.
#[macro_export]
macro_rules! atom {
    ($s:expr) => {{
        const INLINE: ::core::option::Option<$crate::Atom> = $crate::inline_atom($s);
        // This condition can be evaluated at compile time to enable inlining as a
        // simple constant
        if INLINE.is_some() {
            INLINE.unwrap()
        } else {
            // Otherwise we use a
            #[inline(never)]
            fn get_atom() -> $crate::Atom {
                static CACHE: $crate::CachedAtom =
                    $crate::CachedAtom::new(|| $crate::Atom::from($s));

                (*CACHE).clone()
            }

            get_atom()
        }
    }};
}

impl Default for Atom {
    #[inline(never)]
    fn default() -> Self {
        atom!("")
    }
}

/// Immutable, so it's safe to be shared between threads
unsafe impl Send for Atom {}

/// Immutable, so it's safe to be shared between threads
unsafe impl Sync for Atom {}

impl Display for Atom {
    #[inline]
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        Display::fmt(self.as_str(), f)
    }
}

impl Debug for Atom {
    #[inline]
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        Debug::fmt(self.as_str(), f)
    }
}

#[cfg(feature = "serde")]
impl serde::ser::Serialize for Atom {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::ser::Serializer,
    {
        serializer.serialize_str(self)
    }
}

#[cfg(feature = "serde")]
impl<'de> serde::de::Deserialize<'de> for Atom {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        String::deserialize(deserializer).map(Self::new)
    }
}
const DYNAMIC_TAG: u8 = 0b_00;
const INLINE_TAG: u8 = 0b_01; // len in upper nybble
const INLINE_TAG_INIT: NonZeroU8 = unsafe { NonZeroU8::new_unchecked(INLINE_TAG) };
const TAG_MASK: u8 = 0b_11;
const LEN_OFFSET: usize = 4;
const LEN_MASK: u8 = 0xf0;

// const STATIC_SHIFT_BITS: usize = 32;

impl Atom {
    #[inline(always)]
    pub fn new<S>(s: S) -> Self
    where
        Self: From<S>,
    {
        Self::from(s)
    }

    #[inline(always)]
    fn tag(&self) -> u8 {
        self.unsafe_data.tag() & TAG_MASK
    }

    /// Return true if this is a dynamic Atom.
    #[inline(always)]
    fn is_dynamic(&self) -> bool {
        self.tag() == DYNAMIC_TAG
    }
}

impl Atom {
    fn from_mutated_str<F: FnOnce(&mut str)>(s: &str, f: F) -> Self {
        let mut buffer = mem::MaybeUninit::<[u8; 64]>::uninit();
        let buffer = unsafe { &mut *buffer.as_mut_ptr() };

        if let Some(buffer_prefix) = buffer.get_mut(..s.len()) {
            buffer_prefix.copy_from_slice(s.as_bytes());
            let as_str = unsafe { ::std::str::from_utf8_unchecked_mut(buffer_prefix) };
            f(as_str);
            Atom::from(&*as_str)
        } else {
            let mut string = s.to_owned();
            f(&mut string);
            Atom::from(string)
        }
    }

    /// Like [`to_ascii_uppercase`].
    ///
    /// [`to_ascii_uppercase`]: https://doc.rust-lang.org/std/ascii/trait.AsciiExt.html#tymethod.to_ascii_uppercase
    pub fn to_ascii_uppercase(&self) -> Self {
        for (i, b) in self.bytes().enumerate() {
            if let b'a'..=b'z' = b {
                return Atom::from_mutated_str(self, |s| s[i..].make_ascii_uppercase());
            }
        }
        self.clone()
    }

    /// Like [`to_ascii_lowercase`].
    ///
    /// [`to_ascii_lowercase`]: https://doc.rust-lang.org/std/ascii/trait.AsciiExt.html#tymethod.to_ascii_lowercase
    pub fn to_ascii_lowercase(&self) -> Self {
        for (i, b) in self.bytes().enumerate() {
            if let b'A'..=b'Z' = b {
                return Atom::from_mutated_str(self, |s| s[i..].make_ascii_lowercase());
            }
        }
        self.clone()
    }
}

impl Atom {
    fn get_hash(&self) -> u64 {
        get_hash!(self)
    }

    fn as_str(&self) -> &str {
        match self.tag() {
            DYNAMIC_TAG => unsafe {
                let item = crate::dynamic::deref_from(self.unsafe_data);
                from_utf8_unchecked(transmute::<&[u8], &'static [u8]>(&item.slice))
            },
            INLINE_TAG => {
                let len = (self.unsafe_data.tag() & LEN_MASK) >> LEN_OFFSET;
                let src = self.unsafe_data.data();
                unsafe { std::str::from_utf8_unchecked(&src[..(len as usize)]) }
            }
            _ => unsafe { debug_unreachable!() },
        }
    }
}

#[cfg(test)]
impl Atom {
    pub(crate) fn ref_count(&self) -> usize {
        match self.tag() {
            DYNAMIC_TAG => {
                let ptr = unsafe { crate::dynamic::deref_from(self.unsafe_data) };

                triomphe::ThinArc::strong_count(&ptr.0)
            }
            _ => 1,
        }
    }
}

impl PartialEq for Atom {
    #[inline(never)]
    fn eq(&self, other: &Self) -> bool {
        partial_eq!(self, other);

        // If the store is different, the string may be the same, even though the
        // `unsafe_data` is different
        self.as_str() == other.as_str()
    }
}

impl Eq for Atom {}

impl Hash for Atom {
    #[inline(always)]
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        state.write_u64(self.get_hash());
    }
}

impl Drop for Atom {
    #[inline(always)]
    fn drop(&mut self) {
        if self.is_dynamic() {
            unsafe { drop(crate::dynamic::restore_arc(self.unsafe_data)) }
        }
    }
}

impl Clone for Atom {
    #[inline(always)]
    fn clone(&self) -> Self {
        Self::from_alias(self.unsafe_data)
    }
}

impl_from_alias!(Atom);

impl Deref for Atom {
    type Target = str;

    #[inline(always)]
    fn deref(&self) -> &Self::Target {
        self.as_str()
    }
}

impl AsRef<str> for Atom {
    #[inline(always)]
    fn as_ref(&self) -> &str {
        self.as_str()
    }
}

impl PartialEq<str> for Atom {
    #[inline]
    fn eq(&self, other: &str) -> bool {
        self.as_str() == other
    }
}

impl PartialEq<&'_ str> for Atom {
    #[inline]
    fn eq(&self, other: &&str) -> bool {
        self.as_str() == *other
    }
}

impl PartialEq<Atom> for str {
    #[inline]
    fn eq(&self, other: &Atom) -> bool {
        self == other.as_str()
    }
}

impl Borrow<Wtf8Atom> for Atom {
    #[inline(always)]
    fn borrow(&self) -> &Wtf8Atom {
        // SAFETY:
        // 1. Wtf8Atom is #[repr(transparent)] over TaggedValue
        // 2. Atom is #[repr(transparent)] over TaggedValue
        // 3. hstr::Atom and hstr::Wtf8Atom share the same TaggedValue
        const _: () = assert!(std::mem::size_of::<Atom>() == std::mem::size_of::<Wtf8Atom>());
        const _: () = assert!(std::mem::align_of::<Atom>() == std::mem::align_of::<Wtf8Atom>());
        unsafe { transmute::<&Atom, &Wtf8Atom>(self) }
    }
}

/// NOT A PUBLIC API
#[cfg(feature = "rkyv")]
impl rkyv::Archive for Atom {
    type Archived = rkyv::string::ArchivedString;
    type Resolver = rkyv::string::StringResolver;

    #[allow(clippy::unit_arg)]
    unsafe fn resolve(&self, pos: usize, resolver: Self::Resolver, out: *mut Self::Archived) {
        rkyv::string::ArchivedString::resolve_from_str(self, pos, resolver, out)
    }
}

/// NOT A PUBLIC API
#[cfg(feature = "rkyv")]
impl<S: rkyv::ser::Serializer + ?Sized> rkyv::Serialize<S> for Atom {
    fn serialize(&self, serializer: &mut S) -> Result<Self::Resolver, S::Error> {
        String::serialize(&self.to_string(), serializer)
    }
}

/// NOT A PUBLIC API
#[cfg(feature = "rkyv")]
impl<D> rkyv::Deserialize<Atom, D> for rkyv::string::ArchivedString
where
    D: ?Sized + rkyv::Fallible,
{
    fn deserialize(&self, deserializer: &mut D) -> Result<Atom, <D as rkyv::Fallible>::Error> {
        let s: String = self.deserialize(deserializer)?;

        Ok(Atom::new(s))
    }
}

impl Atom {
    /// Converts a WTF-8 encoded [Wtf8Atom] to a regular UTF-8 [Atom] without
    /// validation.
    ///
    /// # Safety
    ///
    /// The caller must ensure that the WTF-8 atom contains only valid UTF-8
    /// data (no unpaired surrogates). This function performs no validation
    /// and will create an invalid `Atom` if the input contains unpaired
    /// surrogates.
    ///
    /// This is a zero-cost conversion that preserves all internal optimizations
    /// (inline storage, precomputed hashes, etc.) since both types have
    /// identical internal representation.
    pub unsafe fn from_wtf8_unchecked(s: Wtf8Atom) -> Self {
        let s = ManuallyDrop::new(s);
        Atom {
            unsafe_data: s.unsafe_data,
        }
    }
}

#[cfg(test)]
mod macro_tests {

    use super::*;
    #[test]
    fn test_atom() {
        // Test enough to exceed the small string optimization
        assert_eq!(atom!(""), Atom::default());
        assert_eq!(atom!(""), Atom::from(""));
        assert_eq!(atom!("a"), Atom::from("a"));
        assert_eq!(atom!("ab"), Atom::from("ab"));
        assert_eq!(atom!("abc"), Atom::from("abc"));
        assert_eq!(atom!("abcd"), Atom::from("abcd"));
        assert_eq!(atom!("abcde"), Atom::from("abcde"));
        assert_eq!(atom!("abcdef"), Atom::from("abcdef"));
        assert_eq!(atom!("abcdefg"), Atom::from("abcdefg"));
        assert_eq!(atom!("abcdefgh"), Atom::from("abcdefgh"));
        assert_eq!(atom!("abcdefghi"), Atom::from("abcdefghi"));
    }

    #[test]
    fn test_inline_atom() {
        // This is a silly test, just asserts that rustc can evaluate the pattern from
        // our macro in a constant context.
        const STR: Atom = {
            let inline = inline_atom("hello");
            if inline.is_some() {
                inline.unwrap()
            } else {
                unreachable!();
            }
        };
        assert_eq!(STR, Atom::from("hello"));
    }
}
