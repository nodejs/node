// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::Error;
use writeable::{TryWriteable, Writeable};

#[cfg(feature = "alloc")]
use alloc::{borrow::Cow, boxed::Box};

/// A borrowed item in a [`Pattern`]. Items are either string literals or placeholders.
///
/// [`Pattern`]: crate::Pattern
#[derive(Debug, Copy, Clone, PartialEq, Eq, PartialOrd, Ord)]
#[allow(clippy::exhaustive_enums)] // Part of core data model
pub enum PatternItem<'a, T> {
    /// A placeholder of the type specified on this [`PatternItem`].
    Placeholder(T),
    /// A string literal. This can occur in one of three places:
    ///
    /// 1. Between the start of the string and the first placeholder (prefix)
    /// 2. Between two placeholders (infix)
    /// 3. Between the final placeholder and the end of the string (suffix)
    Literal(&'a str),
}

/// A borrowed-or-owned item in a [`Pattern`]. Items are either string literals or placeholders.
///
/// ✨ *Enabled with the `alloc` Cargo feature.*
///
/// [`Pattern`]: crate::Pattern
#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord)]
#[allow(clippy::exhaustive_enums)] // Part of core data model
#[cfg(feature = "alloc")]
#[cfg_attr(feature = "serde", derive(serde::Deserialize, serde::Serialize))]
pub enum PatternItemCow<'a, T> {
    /// A placeholder of the type specified on this [`PatternItemCow`].
    Placeholder(T),
    /// A string literal. This can occur in one of three places:
    ///
    /// 1. Between the start of the string and the first placeholder (prefix)
    /// 2. Between two placeholders (infix)
    /// 3. Between the final placeholder and the end of the string (suffix)
    #[cfg_attr(feature = "serde", serde(borrow))]
    Literal(Cow<'a, str>),
}

#[cfg(feature = "alloc")]
impl<'a, T, U> From<PatternItem<'a, U>> for PatternItemCow<'a, T>
where
    T: From<U>,
{
    fn from(value: PatternItem<'a, U>) -> Self {
        match value {
            PatternItem::Placeholder(t) => Self::Placeholder(t.into()),
            PatternItem::Literal(s) => Self::Literal(Cow::Borrowed(s)),
        }
    }
}

/// Types that implement backing data models for [`Pattern`] implement this trait.
///
/// The trait has no public methods and is not implementable outside of this crate.
///
/// <div class="stab unstable">
/// 🚫 This trait is sealed; it cannot be implemented by user code. If an API requests an item that implements this
/// trait, please consider using a type from the implementors listed below.
/// </div>
///
/// [`Pattern`]: crate::Pattern
// Debug so that `#[derive(Debug)]` on types generic in `PatternBackend` works
pub trait PatternBackend: crate::private::Sealed + 'static + core::fmt::Debug {
    /// The type to be used as the placeholder key in code.
    type PlaceholderKey<'a>;

    /// Cowable version of the type to be used as the placeholder key in code.
    // Note: it is not good practice to feature-gate trait methods, but this trait is sealed
    #[cfg(feature = "alloc")]
    type PlaceholderKeyCow<'a>;

    /// The type of error that the [`TryWriteable`] for this backend can return.
    type Error<'a>;

    /// The unsized type of the store required for this backend, usually `str` or `[u8]`.
    // Note: it is not good practice to feature-gate trait types, but this trait is sealed
    #[doc(hidden)] // TODO(#4467): Should be internal
    #[cfg(not(feature = "zerovec"))]
    type Store: ?Sized + PartialEq + core::fmt::Debug;
    #[cfg(feature = "zerovec")]
    type Store: ?Sized + PartialEq + core::fmt::Debug + zerovec::ule::VarULE;

    /// The iterator type returned by [`Self::try_from_items`].
    #[doc(hidden)] // TODO(#4467): Should be internal
    type Iter<'a>: Iterator<Item = PatternItem<'a, Self::PlaceholderKey<'a>>>;

    /// Checks a store for validity, returning an error if invalid.
    #[doc(hidden)] // TODO(#4467): Should be internal
    fn validate_store(store: &Self::Store) -> Result<(), Error>;

    /// Constructs a store from pattern items.
    // Note: it is not good practice to feature-gate trait methods, but this trait is sealed
    #[doc(hidden)] // TODO(#4467): Should be internal
    #[cfg(feature = "alloc")]
    fn try_from_items<
        'cow,
        'ph,
        I: Iterator<Item = Result<PatternItemCow<'cow, Self::PlaceholderKeyCow<'ph>>, Error>>,
    >(
        items: I,
    ) -> Result<Box<Self::Store>, Error>;

    /// Iterates over the pattern items in a store.
    #[doc(hidden)] // TODO(#4467): Should be internal
    fn iter_items(store: &Self::Store) -> Self::Iter<'_>;

    /// The store for the empty pattern, used to implement `Default`
    #[doc(hidden)] // TODO(#4467): Should be internal
    fn empty() -> &'static Self::Store;
}

/// Trait implemented on collections that can produce [`TryWriteable`]s for interpolation.
///
/// This trait can add [`Part`]s for individual literals or placeholders. The implementations
/// of this trait on standard types do not add any [`Part`]s.
///
/// This trait has a blanket implementation and is therefore not implementable by user code.
///
/// # Examples
///
/// A custom implementation that adds parts:
///
/// ```
/// use core::str::FromStr;
/// use icu_pattern::Pattern;
/// use icu_pattern::DoublePlaceholder;
/// use icu_pattern::DoublePlaceholderKey;
/// use icu_pattern::PlaceholderValueProvider;
/// use writeable::adapters::WithPart;
/// use writeable::adapters::WriteableAsTryWriteableInfallible;
/// use writeable::assert_writeable_parts_eq;
/// use writeable::Part;
/// use writeable::Writeable;
///
/// let pattern = Pattern::<DoublePlaceholder>::try_from_str(
///     "Hello, {0} and {1}!",
///     Default::default(),
/// )
/// .unwrap();
///
/// struct ValuesWithParts<'a>(&'a str, &'a str);
///
/// const PART_PLACEHOLDER_0: Part = Part {
///     category: "custom",
///     value: "placeholder0",
/// };
/// const PART_PLACEHOLDER_1: Part = Part {
///     category: "custom",
///     value: "placeholder1",
/// };
/// const PART_LITERAL: Part = Part {
///     category: "custom",
///     value: "literal",
/// };
///
/// impl PlaceholderValueProvider<DoublePlaceholderKey> for ValuesWithParts<'_> {
///     type Error = core::convert::Infallible;
///
///     type W<'a> = WriteableAsTryWriteableInfallible<WithPart<&'a str>>
///     where
///         Self: 'a;
///
///     type L<'a, 'l> = WithPart<&'l str>
///     where
///         Self: 'a;
///
///     #[inline]
///     fn value_for(&self, key: DoublePlaceholderKey) -> Self::W<'_> {
///         let writeable = match key {
///             DoublePlaceholderKey::Place0 => WithPart {
///                 writeable: self.0,
///                 part: PART_PLACEHOLDER_0,
///             },
///             DoublePlaceholderKey::Place1 => WithPart {
///                 writeable: self.1,
///                 part: PART_PLACEHOLDER_1,
///             },
///         };
///         WriteableAsTryWriteableInfallible(writeable)
///     }
///
///     #[inline]
///     fn map_literal<'a, 'l>(&'a self, literal: &'l str) -> Self::L<'a, 'l> {
///         WithPart {
///             writeable: literal,
///             part: PART_LITERAL,
///         }
///     }
/// }
///
/// assert_writeable_parts_eq!(
///     pattern.interpolate(ValuesWithParts("Alice", "Bob")),
///     "Hello, Alice and Bob!",
///     [
///         (0, 7, PART_LITERAL),
///         (7, 12, PART_PLACEHOLDER_0),
///         (12, 17, PART_LITERAL),
///         (17, 20, PART_PLACEHOLDER_1),
///         (20, 21, PART_LITERAL),
///     ]
/// );
/// ```
///
/// [`Part`]: writeable::Part
pub trait PlaceholderValueProvider<K> {
    type Error;

    /// The type of [`TryWriteable`] returned by [`Self::value_for`].
    ///
    /// To return a [`Writeable`], wrap it with [`WriteableAsTryWriteableInfallible`].
    ///
    /// [`WriteableAsTryWriteableInfallible`]: writeable::adapters::WriteableAsTryWriteableInfallible
    type W<'a>: TryWriteable<Error = Self::Error>
    where
        Self: 'a;

    /// The type of [`Writeable`] returned by [`Self::map_literal`].
    ///
    /// If you are not adding parts, this can be `&'l str`.
    type L<'a, 'l>: Writeable
    where
        Self: 'a;

    /// Returns the [`TryWriteable`] to substitute for the given placeholder.
    ///
    /// See [`PatternItem::Placeholder`]
    fn value_for(&self, key: K) -> Self::W<'_>;

    /// Maps a literal string to a [`Writeable`] that could contain parts.
    ///
    /// See [`PatternItem::Literal`]
    fn map_literal<'a, 'l>(&'a self, literal: &'l str) -> Self::L<'a, 'l>;
}

impl<K, T> PlaceholderValueProvider<K> for &'_ T
where
    T: PlaceholderValueProvider<K> + ?Sized,
{
    type Error = T::Error;

    type W<'a>
        = T::W<'a>
    where
        Self: 'a;

    type L<'a, 'l>
        = T::L<'a, 'l>
    where
        Self: 'a;

    fn value_for(&self, key: K) -> Self::W<'_> {
        (*self).value_for(key)
    }
    fn map_literal<'a, 'l>(&'a self, literal: &'l str) -> Self::L<'a, 'l> {
        (*self).map_literal(literal)
    }
}
