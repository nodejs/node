//! An immutable set constructed at compile time.
use core::fmt;
use core::iter::FusedIterator;
use core::iter::IntoIterator;

use phf_shared::{PhfBorrow, PhfHash};

use crate::{map, Map};

/// An immutable set constructed at compile time.
///
/// ## Note
///
/// The fields of this struct are public so that they may be initialized by the
/// `phf_set!` macro and code generation. They are subject to change at any
/// time and should never be accessed directly.
pub struct Set<T: 'static> {
    #[doc(hidden)]
    pub map: Map<T, ()>,
}

impl<T> fmt::Debug for Set<T>
where
    T: fmt::Debug,
{
    fn fmt(&self, fmt: &mut fmt::Formatter<'_>) -> fmt::Result {
        fmt.debug_set().entries(self).finish()
    }
}

impl<T> PartialEq for Set<T>
where
    T: PartialEq,
{
    fn eq(&self, other: &Self) -> bool {
        self.map == other.map
    }
}

impl<T> Eq for Set<T> where T: Eq {}

impl<T> Set<T> {
    /// Returns the number of elements in the `Set`.
    #[inline]
    pub const fn len(&self) -> usize {
        self.map.len()
    }

    /// Returns true if the `Set` contains no elements.
    #[inline]
    pub const fn is_empty(&self) -> bool {
        self.len() == 0
    }

    /// Returns a reference to the set's internal static instance of the given
    /// key.
    ///
    /// This can be useful for interning schemes.
    pub fn get_key<U: ?Sized>(&self, key: &U) -> Option<&T>
    where
        U: Eq + PhfHash,
        T: PhfBorrow<U>,
    {
        self.map.get_key(key)
    }

    /// Returns true if `value` is in the `Set`.
    pub fn contains<U: ?Sized>(&self, value: &U) -> bool
    where
        U: Eq + PhfHash,
        T: PhfBorrow<U>,
    {
        self.map.contains_key(value)
    }

    /// Returns an iterator over the values in the set.
    ///
    /// Values are returned in an arbitrary but fixed order.
    pub fn iter(&self) -> Iter<'_, T> {
        Iter {
            iter: self.map.keys(),
        }
    }
}

impl<T> Set<T>
where
    T: Eq + PhfHash + PhfBorrow<T>,
{
    /// Returns true if `other` shares no elements with `self`.
    pub fn is_disjoint(&self, other: &Set<T>) -> bool {
        !self.iter().any(|value| other.contains(value))
    }

    /// Returns true if `other` contains all values in `self`.
    pub fn is_subset(&self, other: &Set<T>) -> bool {
        self.iter().all(|value| other.contains(value))
    }

    /// Returns true if `self` contains all values in `other`.
    pub fn is_superset(&self, other: &Set<T>) -> bool {
        other.is_subset(self)
    }
}

impl<'a, T> IntoIterator for &'a Set<T> {
    type Item = &'a T;
    type IntoIter = Iter<'a, T>;

    fn into_iter(self) -> Iter<'a, T> {
        self.iter()
    }
}

/// An iterator over the values in a `Set`.
pub struct Iter<'a, T: 'static> {
    iter: map::Keys<'a, T, ()>,
}

impl<'a, T> Clone for Iter<'a, T> {
    #[inline]
    fn clone(&self) -> Self {
        Self {
            iter: self.iter.clone(),
        }
    }
}

impl<'a, T> fmt::Debug for Iter<'a, T>
where
    T: fmt::Debug,
{
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.debug_list().entries(self.clone()).finish()
    }
}

impl<'a, T> Iterator for Iter<'a, T> {
    type Item = &'a T;

    fn next(&mut self) -> Option<&'a T> {
        self.iter.next()
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        self.iter.size_hint()
    }
}

impl<'a, T> DoubleEndedIterator for Iter<'a, T> {
    fn next_back(&mut self) -> Option<&'a T> {
        self.iter.next_back()
    }
}

impl<'a, T> ExactSizeIterator for Iter<'a, T> {}

impl<'a, T> FusedIterator for Iter<'a, T> {}
