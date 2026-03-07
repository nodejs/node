use std::{
    borrow::Cow,
    cell::RefCell,
    ffi::c_void,
    hash::{BuildHasherDefault, Hash, Hasher},
    mem::ManuallyDrop,
    num::NonZeroU8,
    ops::Deref,
    ptr::NonNull,
};

use rustc_hash::FxHasher;
use triomphe::ThinArc;

use crate::{
    tagged_value::{TaggedValue, MAX_INLINE_LEN},
    wtf8::Wtf8,
    Atom, Wtf8Atom, INLINE_TAG, INLINE_TAG_INIT, LEN_OFFSET, TAG_MASK,
};

#[derive(PartialEq, Eq)]
pub(crate) struct Metadata {
    pub hash: u64,
}

#[derive(Clone, PartialEq, Eq)]
pub(crate) struct Item(pub ThinArc<Metadata, u8>);

impl Deref for Item {
    type Target = <ThinArc<Metadata, u8> as Deref>::Target;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl Hash for Item {
    fn hash<H: Hasher>(&self, state: &mut H) {
        state.write_u64(self.0.header.header.hash);
    }
}

pub(crate) unsafe fn deref_from(ptr: TaggedValue) -> ManuallyDrop<Item> {
    let item = restore_arc(ptr);

    ManuallyDrop::new(item)
}

pub(crate) unsafe fn restore_arc(v: TaggedValue) -> Item {
    let ptr = v.get_ptr();
    Item(ThinArc::from_raw(ptr))
}

/// A store that stores [Atom]s. Can be merged with other [AtomStore]s for
/// better performance.
///
///
/// # Merging [AtomStore]
pub struct AtomStore {
    pub(crate) data: hashbrown::HashMap<Item, (), BuildEntryHasher>,
}

impl Default for AtomStore {
    fn default() -> Self {
        Self {
            data: hashbrown::HashMap::with_capacity_and_hasher(64, BuildEntryHasher::default()),
        }
    }
}

impl AtomStore {
    #[inline(always)]
    pub fn atom<'a>(&mut self, text: impl Into<Cow<'a, str>>) -> Atom {
        atom_in(self, &text.into())
    }

    #[inline(always)]
    pub fn wtf8_atom<'a>(&mut self, text: impl Into<Cow<'a, Wtf8>>) -> Wtf8Atom {
        wtf8_atom_in(self, text.into().as_bytes())
    }

    fn gc(&mut self) {
        self.data.retain(|item, _| {
            let count = ThinArc::strong_count(&item.0);
            debug_assert!(count > 0);
            count > 1
        });
    }
}

thread_local! {
    static GLOBAL_DATA: RefCell<AtomStore> = Default::default();
}

/// Cleans up atoms in the global store that are no longer referenced.
pub fn global_atom_store_gc() {
    GLOBAL_DATA.with(|global| {
        let mut store = global.borrow_mut();
        store.gc();
    });
}

pub(crate) fn global_wtf8_atom(text: &[u8]) -> Wtf8Atom {
    GLOBAL_DATA.with(|global| {
        let mut store = global.borrow_mut();

        wtf8_atom_in(&mut *store, text)
    })
}

pub(crate) fn global_atom(text: &str) -> Atom {
    GLOBAL_DATA.with(|global| {
        let mut store = global.borrow_mut();

        atom_in(&mut *store, text)
    })
}

fn wtf8_atom_in<S>(storage: S, text: &[u8]) -> Wtf8Atom
where
    S: Storage,
{
    let len = text.len();

    if len <= MAX_INLINE_LEN {
        // INLINE_TAG ensures this is never zero
        let tag = INLINE_TAG_INIT | ((len as u8) << LEN_OFFSET);
        let mut unsafe_data = TaggedValue::new_tag(tag);
        unsafe {
            unsafe_data.data_mut()[..len].copy_from_slice(text);
        }
        return Wtf8Atom { unsafe_data };
    }

    let hash = calc_hash(text);
    let entry = storage.insert_entry(text, hash);
    let entry = ThinArc::into_raw(entry.0) as *mut c_void;

    let ptr: NonNull<c_void> = unsafe {
        // Safety: Arc::into_raw returns a non-null pointer
        NonNull::new_unchecked(entry)
    };
    debug_assert!(0 == ptr.as_ptr() as u8 & TAG_MASK);
    Wtf8Atom {
        unsafe_data: TaggedValue::new_ptr(ptr),
    }
}

/// This can create any kind of [Atom], although this lives in the `dynamic`
/// module.
fn atom_in<S>(storage: S, text: &str) -> Atom
where
    S: Storage,
{
    // SAFETY: `text` is valid UTF-8
    unsafe { Atom::from_wtf8_unchecked(wtf8_atom_in(storage, text.as_bytes())) }
}

/// Attempts to construct an [Atom] but only if it can be constructed inline.
/// This is primarily useful in constant contexts.
pub(crate) const fn inline_atom(text: &str) -> Option<Atom> {
    let len = text.len();
    if len <= MAX_INLINE_LEN {
        // INLINE_TAG ensures this is never zero
        let tag = INLINE_TAG | ((len as u8) << LEN_OFFSET);
        let mut unsafe_data = TaggedValue::new_tag(NonZeroU8::new(tag).unwrap());
        // This odd pattern is needed because we cannot create slices from ranges or
        // ranges at all in constant context.  So we write our own copying loop.
        unsafe {
            let data = unsafe_data.data_mut();
            let bytes = text.as_bytes();
            let mut i = 0;
            while i < len {
                data[i] = bytes[i];
                i += 1;
            }
        }
        return Some(Atom { unsafe_data });
    }
    None
}

trait Storage {
    fn insert_entry(self, text: &[u8], hash: u64) -> Item;
}

impl Storage for &'_ mut AtomStore {
    fn insert_entry(self, text: &[u8], hash: u64) -> Item {
        // If the text is too long, interning is not worth it.
        if text.len() > 512 {
            return Item(ThinArc::from_header_and_slice(Metadata { hash }, text));
        }

        let (entry, _) = self
            .data
            .raw_entry_mut()
            .from_hash(hash, |key| {
                key.header.header.hash == hash && key.slice.eq(text)
            })
            .or_insert_with(move || {
                (
                    Item(ThinArc::from_header_and_slice(Metadata { hash }, text)),
                    (),
                )
            });
        entry.clone()
    }
}

#[inline(always)]
fn calc_hash(text: &[u8]) -> u64 {
    let mut hasher = FxHasher::default();
    text.hash(&mut hasher);
    hasher.finish()
}

type BuildEntryHasher = BuildHasherDefault<EntryHasher>;

/// A "no-op" hasher for [Entry] that returns [Entry::hash]. The design is
/// inspired by the `nohash-hasher` crate.
///
/// Assumption: [Arc]'s implementation of [Hash] is a simple pass-through.
#[derive(Default)]
pub(crate) struct EntryHasher {
    hash: u64,
    #[cfg(debug_assertions)]
    write_called: bool,
}

impl Hasher for EntryHasher {
    fn finish(&self) -> u64 {
        #[cfg(debug_assertions)]
        debug_assert!(
            self.write_called,
            "EntryHasher expects write_u64 to have been called",
        );
        self.hash
    }

    fn write(&mut self, _bytes: &[u8]) {
        panic!("EntryHasher expects to be called with write_u64");
    }

    fn write_u64(&mut self, val: u64) {
        #[cfg(debug_assertions)]
        {
            debug_assert!(
                !self.write_called,
                "EntryHasher expects write_u64 to be called only once",
            );
            self.write_called = true;
        }

        self.hash = val;
    }
}

#[cfg(test)]
mod tests {
    use crate::{dynamic::GLOBAL_DATA, global_atom_store_gc, Atom};

    fn expect_size(expected: usize) {
        // This is a helper function to count the number of bytes in the global store.
        GLOBAL_DATA.with(|global| {
            let store = global.borrow();
            assert_eq!(store.data.len(), expected);
        })
    }

    #[test]
    fn global_ref_count_dynamic_0() {
        expect_size(0);

        // The strings should be long enough so that they are not inline even under
        // feature `atom_size_128`
        let atom1 = Atom::new("Hello, beautiful world!");

        expect_size(1);

        let atom2 = Atom::new("Hello, beautiful world!");

        expect_size(1);

        // 2 for the two atoms, 1 for the global store
        assert_eq!(atom1.ref_count(), 3);
        assert_eq!(atom2.ref_count(), 3);

        drop(atom1);

        expect_size(1);

        // 1 for the atom2, 1 for the global store
        assert_eq!(atom2.ref_count(), 2);

        drop(atom2);

        expect_size(1);
        global_atom_store_gc();
        expect_size(0);
    }

    #[test]
    fn global_ref_count_dynamic_1() {
        expect_size(0);

        {
            expect_size(0);
            let atom = Atom::new("Hello, beautiful world!");
            assert_eq!(atom.ref_count(), 2);
            expect_size(1);
        }

        expect_size(1);
        global_atom_store_gc();
        expect_size(0);

        {
            let atom = Atom::new("Hello, beautiful world!");
            assert_eq!(atom.ref_count(), 2);
            expect_size(1);
        }
        let atom = Atom::new("Hello, beautiful world!");
        assert_eq!(atom.ref_count(), 2);

        expect_size(1);
        global_atom_store_gc();
        expect_size(1);

        drop(atom);
        expect_size(1);
        global_atom_store_gc();
        expect_size(0);
    }
}
