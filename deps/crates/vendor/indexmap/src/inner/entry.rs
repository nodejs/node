use super::{equivalent, get_hash, Bucket, Core};
use crate::map::{Entry, IndexedEntry};
use crate::HashValue;
use core::cmp::Ordering;
use core::mem;

impl<'a, K, V> Entry<'a, K, V> {
    pub(crate) fn new(map: &'a mut Core<K, V>, hash: HashValue, key: K) -> Self
    where
        K: Eq,
    {
        let eq = equivalent(&key, &map.entries);
        match map.indices.find_entry(hash.get(), eq) {
            Ok(entry) => Entry::Occupied(OccupiedEntry {
                bucket: entry.bucket_index(),
                index: *entry.get(),
                map,
            }),
            Err(_) => Entry::Vacant(VacantEntry { map, hash, key }),
        }
    }
}

/// A view into an occupied entry in an [`IndexMap`][crate::IndexMap].
/// It is part of the [`Entry`] enum.
pub struct OccupiedEntry<'a, K, V> {
    map: &'a mut Core<K, V>,
    // We have a mutable reference to the map, which keeps these two
    // indices valid and pointing to the correct entry.
    index: usize,
    bucket: usize,
}

impl<'a, K, V> OccupiedEntry<'a, K, V> {
    /// Constructor for `RawEntryMut::from_hash`
    pub(crate) fn from_hash<F>(
        map: &'a mut Core<K, V>,
        hash: HashValue,
        mut is_match: F,
    ) -> Result<Self, &'a mut Core<K, V>>
    where
        F: FnMut(&K) -> bool,
    {
        let entries = &*map.entries;
        let eq = move |&i: &usize| is_match(&entries[i].key);
        match map.indices.find_entry(hash.get(), eq) {
            Ok(entry) => Ok(OccupiedEntry {
                bucket: entry.bucket_index(),
                index: *entry.get(),
                map,
            }),
            Err(_) => Err(map),
        }
    }

    pub(crate) fn into_core(self) -> &'a mut Core<K, V> {
        self.map
    }

    pub(crate) fn get_bucket(&self) -> &Bucket<K, V> {
        &self.map.entries[self.index]
    }

    pub(crate) fn get_bucket_mut(&mut self) -> &mut Bucket<K, V> {
        &mut self.map.entries[self.index]
    }

    pub(crate) fn into_bucket(self) -> &'a mut Bucket<K, V> {
        &mut self.map.entries[self.index]
    }

    /// Return the index of the key-value pair
    #[inline]
    pub fn index(&self) -> usize {
        self.index
    }

    /// Gets a reference to the entry's key in the map.
    ///
    /// Note that this is not the key that was used to find the entry. There may be an observable
    /// difference if the key type has any distinguishing features outside of `Hash` and `Eq`, like
    /// extra fields or the memory address of an allocation.
    pub fn key(&self) -> &K {
        &self.get_bucket().key
    }

    /// Gets a reference to the entry's value in the map.
    pub fn get(&self) -> &V {
        &self.get_bucket().value
    }

    /// Gets a mutable reference to the entry's value in the map.
    ///
    /// If you need a reference which may outlive the destruction of the
    /// [`Entry`] value, see [`into_mut`][Self::into_mut].
    pub fn get_mut(&mut self) -> &mut V {
        &mut self.get_bucket_mut().value
    }

    /// Converts into a mutable reference to the entry's value in the map,
    /// with a lifetime bound to the map itself.
    pub fn into_mut(self) -> &'a mut V {
        &mut self.into_bucket().value
    }

    /// Sets the value of the entry to `value`, and returns the entry's old value.
    pub fn insert(&mut self, value: V) -> V {
        mem::replace(self.get_mut(), value)
    }

    /// Remove the key, value pair stored in the map for this entry, and return the value.
    ///
    /// **NOTE:** This is equivalent to [`.swap_remove()`][Self::swap_remove], replacing this
    /// entry's position with the last element, and it is deprecated in favor of calling that
    /// explicitly. If you need to preserve the relative order of the keys in the map, use
    /// [`.shift_remove()`][Self::shift_remove] instead.
    #[deprecated(note = "`remove` disrupts the map order -- \
        use `swap_remove` or `shift_remove` for explicit behavior.")]
    pub fn remove(self) -> V {
        self.swap_remove()
    }

    /// Remove the key, value pair stored in the map for this entry, and return the value.
    ///
    /// Like [`Vec::swap_remove`][alloc::vec::Vec::swap_remove], the pair is removed by swapping it
    /// with the last element of the map and popping it off.
    /// **This perturbs the position of what used to be the last element!**
    ///
    /// Computes in **O(1)** time (average).
    pub fn swap_remove(self) -> V {
        self.swap_remove_entry().1
    }

    /// Remove the key, value pair stored in the map for this entry, and return the value.
    ///
    /// Like [`Vec::remove`][alloc::vec::Vec::remove], the pair is removed by shifting all of the
    /// elements that follow it, preserving their relative order.
    /// **This perturbs the index of all of those elements!**
    ///
    /// Computes in **O(n)** time (average).
    pub fn shift_remove(self) -> V {
        self.shift_remove_entry().1
    }

    /// Remove and return the key, value pair stored in the map for this entry
    ///
    /// **NOTE:** This is equivalent to [`.swap_remove_entry()`][Self::swap_remove_entry],
    /// replacing this entry's position with the last element, and it is deprecated in favor of
    /// calling that explicitly. If you need to preserve the relative order of the keys in the map,
    /// use [`.shift_remove_entry()`][Self::shift_remove_entry] instead.
    #[deprecated(note = "`remove_entry` disrupts the map order -- \
        use `swap_remove_entry` or `shift_remove_entry` for explicit behavior.")]
    pub fn remove_entry(self) -> (K, V) {
        self.swap_remove_entry()
    }

    /// Remove and return the key, value pair stored in the map for this entry
    ///
    /// Like [`Vec::swap_remove`][alloc::vec::Vec::swap_remove], the pair is removed by swapping it
    /// with the last element of the map and popping it off.
    /// **This perturbs the position of what used to be the last element!**
    ///
    /// Computes in **O(1)** time (average).
    pub fn swap_remove_entry(mut self) -> (K, V) {
        self.remove_index();
        self.map.swap_remove_finish(self.index)
    }

    /// Remove and return the key, value pair stored in the map for this entry
    ///
    /// Like [`Vec::remove`][alloc::vec::Vec::remove], the pair is removed by shifting all of the
    /// elements that follow it, preserving their relative order.
    /// **This perturbs the index of all of those elements!**
    ///
    /// Computes in **O(n)** time (average).
    pub fn shift_remove_entry(mut self) -> (K, V) {
        self.remove_index();
        self.map.shift_remove_finish(self.index)
    }

    fn remove_index(&mut self) {
        let entry = self.map.indices.get_bucket_entry(self.bucket).unwrap();
        debug_assert_eq!(*entry.get(), self.index);
        entry.remove();
    }

    /// Moves the position of the entry to a new index
    /// by shifting all other entries in-between.
    ///
    /// This is equivalent to [`IndexMap::move_index`][`crate::IndexMap::move_index`]
    /// coming `from` the current [`.index()`][Self::index].
    ///
    /// * If `self.index() < to`, the other pairs will shift down while the targeted pair moves up.
    /// * If `self.index() > to`, the other pairs will shift up while the targeted pair moves down.
    ///
    /// ***Panics*** if `to` is out of bounds.
    ///
    /// Computes in **O(n)** time (average).
    #[track_caller]
    pub fn move_index(self, to: usize) {
        if self.index != to {
            let _ = self.map.entries[to]; // explicit bounds check
            self.map.move_index_inner(self.index, to);
            self.update_index(to);
        }
    }

    /// Swaps the position of entry with another.
    ///
    /// This is equivalent to [`IndexMap::swap_indices`][`crate::IndexMap::swap_indices`]
    /// with the current [`.index()`][Self::index] as one of the two being swapped.
    ///
    /// ***Panics*** if the `other` index is out of bounds.
    ///
    /// Computes in **O(1)** time (average).
    #[track_caller]
    pub fn swap_indices(self, other: usize) {
        if self.index != other {
            // Since we already know where our bucket is, we only need to find the other.
            let hash = self.map.entries[other].hash;
            let other_mut = self.map.indices.find_mut(hash.get(), move |&i| i == other);
            *other_mut.expect("index not found") = self.index;

            self.map.entries.swap(self.index, other);
            self.update_index(other);
        }
    }

    fn update_index(self, to: usize) {
        let index = self.map.indices.get_bucket_mut(self.bucket).unwrap();
        debug_assert_eq!(*index, self.index);
        *index = to;
    }
}

impl<'a, K, V> From<IndexedEntry<'a, K, V>> for OccupiedEntry<'a, K, V> {
    fn from(other: IndexedEntry<'a, K, V>) -> Self {
        let index = other.index();
        let map = other.into_core();
        let hash = map.entries[index].hash;
        let bucket = map
            .indices
            .find_bucket_index(hash.get(), move |&i| i == index)
            .expect("index not found");
        Self { map, index, bucket }
    }
}

/// A view into a vacant entry in an [`IndexMap`][crate::IndexMap].
/// It is part of the [`Entry`] enum.
pub struct VacantEntry<'a, K, V> {
    map: &'a mut Core<K, V>,
    hash: HashValue,
    key: K,
}

impl<'a, K, V> VacantEntry<'a, K, V> {
    /// Return the index where a key-value pair may be inserted.
    pub fn index(&self) -> usize {
        self.map.indices.len()
    }

    /// Gets a reference to the key that was used to find the entry.
    pub fn key(&self) -> &K {
        &self.key
    }

    pub(crate) fn key_mut(&mut self) -> &mut K {
        &mut self.key
    }

    /// Takes ownership of the key, leaving the entry vacant.
    pub fn into_key(self) -> K {
        self.key
    }

    /// Inserts the entry's key and the given value into the map, and returns a mutable reference
    /// to the value.
    ///
    /// Computes in **O(1)** time (amortized average).
    pub fn insert(self, value: V) -> &'a mut V {
        let Self { map, hash, key } = self;
        map.insert_unique(hash, key, value).value_mut()
    }

    /// Inserts the entry's key and the given value into the map, and returns an `OccupiedEntry`.
    ///
    /// Computes in **O(1)** time (amortized average).
    pub fn insert_entry(self, value: V) -> OccupiedEntry<'a, K, V> {
        let Self { map, hash, key } = self;
        let index = map.indices.len();
        debug_assert_eq!(index, map.entries.len());
        let bucket = map
            .indices
            .insert_unique(hash.get(), index, get_hash(&map.entries))
            .bucket_index();
        map.push_entry(hash, key, value);
        OccupiedEntry { map, index, bucket }
    }

    /// Inserts the entry's key and the given value into the map at its ordered
    /// position among sorted keys, and returns the new index and a mutable
    /// reference to the value.
    ///
    /// If the existing keys are **not** already sorted, then the insertion
    /// index is unspecified (like [`slice::binary_search`]), but the key-value
    /// pair is inserted at that position regardless.
    ///
    /// Computes in **O(n)** time (average).
    pub fn insert_sorted(self, value: V) -> (usize, &'a mut V)
    where
        K: Ord,
    {
        let slice = crate::map::Slice::from_slice(&self.map.entries);
        let i = slice.binary_search_keys(&self.key).unwrap_err();
        (i, self.shift_insert(i, value))
    }

    /// Inserts the entry's key and the given value into the map at its ordered
    /// position among keys sorted by `cmp`, and returns the new index and a
    /// mutable reference to the value.
    ///
    /// If the existing keys are **not** already sorted, then the insertion
    /// index is unspecified (like [`slice::binary_search`]), but the key-value
    /// pair is inserted at that position regardless.
    ///
    /// Computes in **O(n)** time (average).
    pub fn insert_sorted_by<F>(self, value: V, mut cmp: F) -> (usize, &'a mut V)
    where
        F: FnMut(&K, &V, &K, &V) -> Ordering,
    {
        let slice = crate::map::Slice::from_slice(&self.map.entries);
        let (Ok(i) | Err(i)) = slice.binary_search_by(|k, v| cmp(k, v, &self.key, &value));
        (i, self.shift_insert(i, value))
    }

    /// Inserts the entry's key and the given value into the map at its ordered
    /// position using a sort-key extraction function, and returns the new index
    /// and a mutable reference to the value.
    ///
    /// If the existing keys are **not** already sorted, then the insertion
    /// index is unspecified (like [`slice::binary_search`]), but the key-value
    /// pair is inserted at that position regardless.
    ///
    /// Computes in **O(n)** time (average).
    pub fn insert_sorted_by_key<B, F>(self, value: V, mut sort_key: F) -> (usize, &'a mut V)
    where
        B: Ord,
        F: FnMut(&K, &V) -> B,
    {
        let search_key = sort_key(&self.key, &value);
        let slice = crate::map::Slice::from_slice(&self.map.entries);
        let (Ok(i) | Err(i)) = slice.binary_search_by_key(&search_key, sort_key);
        (i, self.shift_insert(i, value))
    }

    /// Inserts the entry's key and the given value into the map at the given index,
    /// shifting others to the right, and returns a mutable reference to the value.
    ///
    /// ***Panics*** if `index` is out of bounds.
    ///
    /// Computes in **O(n)** time (average).
    #[track_caller]
    pub fn shift_insert(self, index: usize, value: V) -> &'a mut V {
        self.map
            .shift_insert_unique(index, self.hash, self.key, value)
            .value_mut()
    }

    /// Replaces the key at the given index with this entry's key, returning the
    /// old key and an `OccupiedEntry` for that index.
    ///
    /// ***Panics*** if `index` is out of bounds.
    ///
    /// Computes in **O(1)** time (average).
    #[track_caller]
    pub fn replace_index(self, index: usize) -> (K, OccupiedEntry<'a, K, V>) {
        let Self { map, hash, key } = self;

        // NB: This removal and insertion isn't "no grow" (with unreachable hasher)
        // because hashbrown's tombstones might force a resize anyway.
        let old_hash = map.entries[index].hash;
        map.indices
            .find_entry(old_hash.get(), move |&i| i == index)
            .expect("index not found")
            .remove();
        let bucket = map
            .indices
            .insert_unique(hash.get(), index, get_hash(&map.entries))
            .bucket_index();

        let entry = &mut map.entries[index];
        entry.hash = hash;
        let old_key = mem::replace(&mut entry.key, key);

        (old_key, OccupiedEntry { map, index, bucket })
    }
}
