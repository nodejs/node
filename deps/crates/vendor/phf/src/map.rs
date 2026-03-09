//! An immutable map constructed at compile time.
use core::fmt;
use core::iter::FusedIterator;
use core::iter::IntoIterator;
use core::ops::Index;
use core::slice;
use phf_shared::{self, HashKey, PhfBorrow, PhfHash};
#[cfg(feature = "serde")]
use serde::ser::{Serialize, SerializeMap, Serializer};

/// An immutable map constructed at compile time.
///
/// ## Note
///
/// The fields of this struct are public so that they may be initialized by the
/// `phf_map!` macro and code generation. They are subject to change at any
/// time and should never be accessed directly.
pub struct Map<K: 'static, V: 'static> {
    #[doc(hidden)]
    pub key: HashKey,
    #[doc(hidden)]
    pub disps: &'static [(u32, u32)],
    #[doc(hidden)]
    pub entries: &'static [(K, V)],
}

impl<K, V> fmt::Debug for Map<K, V>
where
    K: fmt::Debug,
    V: fmt::Debug,
{
    fn fmt(&self, fmt: &mut fmt::Formatter<'_>) -> fmt::Result {
        fmt.debug_map().entries(self.entries()).finish()
    }
}

impl<'a, K, V, T: ?Sized> Index<&'a T> for Map<K, V>
where
    T: Eq + PhfHash,
    K: PhfBorrow<T>,
{
    type Output = V;

    fn index(&self, k: &'a T) -> &V {
        self.get(k).expect("invalid key")
    }
}

impl<K, V> Default for Map<K, V> {
    fn default() -> Self {
        Self::new()
    }
}

impl<K, V> PartialEq for Map<K, V>
where
    K: PartialEq,
    V: PartialEq,
{
    fn eq(&self, other: &Self) -> bool {
        self.key == other.key && self.disps == other.disps && self.entries == other.entries
    }
}

impl<K, V> Eq for Map<K, V>
where
    K: Eq,
    V: Eq,
{
}

impl<K, V> Map<K, V> {
    /// Create a new, empty, immutable map.
    #[inline]
    pub const fn new() -> Self {
        Self {
            key: 0,
            disps: &[],
            entries: &[],
        }
    }

    /// Returns the number of entries in the `Map`.
    #[inline]
    pub const fn len(&self) -> usize {
        self.entries.len()
    }

    /// Returns true if the `Map` is empty.
    #[inline]
    pub const fn is_empty(&self) -> bool {
        self.len() == 0
    }

    /// Determines if `key` is in the `Map`.
    pub fn contains_key<T: ?Sized>(&self, key: &T) -> bool
    where
        T: Eq + PhfHash,
        K: PhfBorrow<T>,
    {
        self.get(key).is_some()
    }

    /// Returns a reference to the value that `key` maps to.
    pub fn get<T: ?Sized>(&self, key: &T) -> Option<&V>
    where
        T: Eq + PhfHash,
        K: PhfBorrow<T>,
    {
        self.get_entry(key).map(|e| e.1)
    }

    /// Returns a reference to the map's internal static instance of the given
    /// key.
    ///
    /// This can be useful for interning schemes.
    pub fn get_key<T: ?Sized>(&self, key: &T) -> Option<&K>
    where
        T: Eq + PhfHash,
        K: PhfBorrow<T>,
    {
        self.get_entry(key).map(|e| e.0)
    }

    /// Like `get`, but returns both the key and the value.
    pub fn get_entry<T: ?Sized>(&self, key: &T) -> Option<(&K, &V)>
    where
        T: Eq + PhfHash,
        K: PhfBorrow<T>,
    {
        if self.disps.is_empty() {
            return None;
        } //Prevent panic on empty map
        let hashes = phf_shared::hash(key, &self.key);
        let index = phf_shared::get_index(&hashes, self.disps, self.entries.len());
        let entry = &self.entries[index as usize];
        let b: &T = entry.0.borrow();
        if b == key {
            Some((&entry.0, &entry.1))
        } else {
            None
        }
    }

    /// Returns an iterator over the key/value pairs in the map.
    ///
    /// Entries are returned in an arbitrary but fixed order.
    pub fn entries(&self) -> Entries<'_, K, V> {
        Entries {
            iter: self.entries.iter(),
        }
    }

    /// Returns an iterator over the keys in the map.
    ///
    /// Keys are returned in an arbitrary but fixed order.
    pub fn keys(&self) -> Keys<'_, K, V> {
        Keys {
            iter: self.entries(),
        }
    }

    /// Returns an iterator over the values in the map.
    ///
    /// Values are returned in an arbitrary but fixed order.
    pub fn values(&self) -> Values<'_, K, V> {
        Values {
            iter: self.entries(),
        }
    }
}

impl<'a, K, V> IntoIterator for &'a Map<K, V> {
    type Item = (&'a K, &'a V);
    type IntoIter = Entries<'a, K, V>;

    fn into_iter(self) -> Entries<'a, K, V> {
        self.entries()
    }
}

/// An iterator over the key/value pairs in a `Map`.
pub struct Entries<'a, K, V> {
    iter: slice::Iter<'a, (K, V)>,
}

impl<'a, K, V> Clone for Entries<'a, K, V> {
    #[inline]
    fn clone(&self) -> Self {
        Self {
            iter: self.iter.clone(),
        }
    }
}

impl<'a, K, V> fmt::Debug for Entries<'a, K, V>
where
    K: fmt::Debug,
    V: fmt::Debug,
{
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.debug_list().entries(self.clone()).finish()
    }
}

impl<'a, K, V> Iterator for Entries<'a, K, V> {
    type Item = (&'a K, &'a V);

    fn next(&mut self) -> Option<(&'a K, &'a V)> {
        self.iter.next().map(|&(ref k, ref v)| (k, v))
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        self.iter.size_hint()
    }
}

impl<'a, K, V> DoubleEndedIterator for Entries<'a, K, V> {
    fn next_back(&mut self) -> Option<(&'a K, &'a V)> {
        self.iter.next_back().map(|e| (&e.0, &e.1))
    }
}

impl<'a, K, V> ExactSizeIterator for Entries<'a, K, V> {}

impl<'a, K, V> FusedIterator for Entries<'a, K, V> {}

/// An iterator over the keys in a `Map`.
pub struct Keys<'a, K, V> {
    iter: Entries<'a, K, V>,
}

impl<'a, K, V> Clone for Keys<'a, K, V> {
    #[inline]
    fn clone(&self) -> Self {
        Self {
            iter: self.iter.clone(),
        }
    }
}

impl<'a, K, V> fmt::Debug for Keys<'a, K, V>
where
    K: fmt::Debug,
{
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.debug_list().entries(self.clone()).finish()
    }
}

impl<'a, K, V> Iterator for Keys<'a, K, V> {
    type Item = &'a K;

    fn next(&mut self) -> Option<&'a K> {
        self.iter.next().map(|e| e.0)
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        self.iter.size_hint()
    }
}

impl<'a, K, V> DoubleEndedIterator for Keys<'a, K, V> {
    fn next_back(&mut self) -> Option<&'a K> {
        self.iter.next_back().map(|e| e.0)
    }
}

impl<'a, K, V> ExactSizeIterator for Keys<'a, K, V> {}

impl<'a, K, V> FusedIterator for Keys<'a, K, V> {}

/// An iterator over the values in a `Map`.
pub struct Values<'a, K, V> {
    iter: Entries<'a, K, V>,
}

impl<'a, K, V> Clone for Values<'a, K, V> {
    #[inline]
    fn clone(&self) -> Self {
        Self {
            iter: self.iter.clone(),
        }
    }
}

impl<'a, K, V> fmt::Debug for Values<'a, K, V>
where
    V: fmt::Debug,
{
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.debug_list().entries(self.clone()).finish()
    }
}

impl<'a, K, V> Iterator for Values<'a, K, V> {
    type Item = &'a V;

    fn next(&mut self) -> Option<&'a V> {
        self.iter.next().map(|e| e.1)
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        self.iter.size_hint()
    }
}

impl<'a, K, V> DoubleEndedIterator for Values<'a, K, V> {
    fn next_back(&mut self) -> Option<&'a V> {
        self.iter.next_back().map(|e| e.1)
    }
}

impl<'a, K, V> ExactSizeIterator for Values<'a, K, V> {}

impl<'a, K, V> FusedIterator for Values<'a, K, V> {}

#[cfg(feature = "serde")]
impl<K, V> Serialize for Map<K, V>
where
    K: Serialize,
    V: Serialize,
{
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        let mut map = serializer.serialize_map(Some(self.len()))?;
        for (k, v) in self.entries() {
            map.serialize_entry(k, v)?;
        }
        map.end()
    }
}
