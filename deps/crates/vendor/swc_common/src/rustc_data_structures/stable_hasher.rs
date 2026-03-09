use std::{
    hash::{BuildHasher, Hash, Hasher},
    mem,
};

use siphasher::sip128::{Hash128, Hasher128, SipHasher24};

/// When hashing something that ends up affecting properties like symbol names,
/// we want these symbol names to be calculated independently of other factors
/// like what architecture you're compiling *from*.
///
/// To that end we always convert integers to little-endian format before
/// hashing and the architecture dependent `isize` and `usize` types are
/// extended to 64 bits if needed.
pub struct StableHasher {
    state: SipHasher24,
}

impl ::std::fmt::Debug for StableHasher {
    fn fmt(&self, f: &mut ::std::fmt::Formatter<'_>) -> ::std::fmt::Result {
        write!(f, "{:?}", self.state)
    }
}

pub trait StableHasherResult: Sized {
    fn finish(hasher: StableHasher) -> Self;
}

impl StableHasher {
    #[inline]
    pub fn new() -> Self {
        StableHasher {
            state: SipHasher24::new_with_keys(0, 0),
        }
    }

    #[inline]
    pub fn finish<W: StableHasherResult>(self) -> W {
        W::finish(self)
    }
}

impl StableHasherResult for u128 {
    #[inline]
    fn finish(hasher: StableHasher) -> Self {
        hasher.finalize().as_u128()
    }
}

impl StableHasherResult for u64 {
    #[inline]
    fn finish(hasher: StableHasher) -> Self {
        hasher.finalize().h1
    }
}

impl StableHasher {
    #[inline]
    pub fn finalize(self) -> Hash128 {
        self.state.finish128()
    }
}

impl Hasher for StableHasher {
    fn finish(&self) -> u64 {
        panic!("use StableHasher::finalize instead");
    }

    #[inline]
    fn write(&mut self, bytes: &[u8]) {
        self.state.write(bytes);
    }

    #[inline]
    fn write_u8(&mut self, i: u8) {
        self.state.write_u8(i);
    }

    #[inline]
    fn write_u16(&mut self, i: u16) {
        self.state.write_u16(i.to_le());
    }

    #[inline]
    fn write_u32(&mut self, i: u32) {
        self.state.write_u32(i.to_le());
    }

    #[inline]
    fn write_u64(&mut self, i: u64) {
        self.state.write_u64(i.to_le());
    }

    #[inline]
    fn write_u128(&mut self, i: u128) {
        self.state.write_u128(i.to_le());
    }

    #[inline]
    fn write_usize(&mut self, i: usize) {
        // Always treat usize as u64 so we get the same results on 32 and 64 bit
        // platforms. This is important for symbol hashes when cross compiling,
        // for example.
        self.state.write_u64((i as u64).to_le());
    }

    #[inline]
    fn write_i8(&mut self, i: i8) {
        self.state.write_i8(i);
    }

    #[inline]
    fn write_i16(&mut self, i: i16) {
        self.state.write_i16(i.to_le());
    }

    #[inline]
    fn write_i32(&mut self, i: i32) {
        self.state.write_i32(i.to_le());
    }

    #[inline]
    fn write_i64(&mut self, i: i64) {
        self.state.write_i64(i.to_le());
    }

    #[inline]
    fn write_i128(&mut self, i: i128) {
        self.state.write_i128(i.to_le());
    }

    #[inline]
    fn write_isize(&mut self, i: isize) {
        // Always treat isize as a 64-bit number so we get the same results on 32 and 64
        // bit platforms. This is important for symbol hashes when cross
        // compiling, for example. Sign extending here is preferable as it means
        // that the same negative number hashes the same on both 32 and 64 bit
        // platforms.
        let value = i as u64;

        // Cold path
        #[cold]
        #[inline(never)]
        fn hash_value(state: &mut SipHasher24, value: u64) {
            state.write_u8(0xff);
            state.write_u64(value.to_le());
        }

        // `isize` values often seem to have a small (positive) numeric value in
        // practice. To exploit this, if the value is small, we will hash a
        // smaller amount of bytes. However, we cannot just skip the leading
        // zero bytes, as that would produce the same hash e.g. if you hash two
        // values that have the same bit pattern when they are swapped. See https://github.com/rust-lang/rust/pull/93014 for context.
        //
        // Therefore, we employ the following strategy:
        // 1) When we encounter a value that fits within a single byte (the most common
        // case), we hash just that byte. This is the most common case that is
        // being optimized. However, we do not do this for the value 0xFF, as
        // that is a reserved prefix (a bit like in UTF-8). 2) When we encounter
        // a larger value, we hash a "marker" 0xFF and then the corresponding
        // 8 bytes. Since this prefix cannot occur when we hash a single byte, when we
        // hash two `isize`s that fit within a different amount of bytes, they
        // should always produce a different byte stream for the hasher.
        if value < 0xff {
            self.state.write_u8(value as u8);
        } else {
            hash_value(&mut self.state, value);
        }
    }
}

/// Something that implements `HashStable<CTX>` can be hashed in a way that is
/// stable across multiple compilation sessions.
pub trait HashStable<CTX> {
    fn hash_stable(&self, hcx: &mut CTX, hasher: &mut StableHasher);
}

/// Implement this for types that can be turned into stable keys like, for
/// example, for DefId that can be converted to a DefPathHash. This is used for
/// bringing maps into a predictable order before hashing them.
pub trait ToStableHashKey<HCX> {
    type KeyType: Ord + Clone + Sized + HashStable<HCX>;
    fn to_stable_hash_key(&self, hcx: &HCX) -> Self::KeyType;
}

// Implement HashStable by just calling `Hash::hash()`. This works fine for
// self-contained values that don't depend on the hashing context `CTX`.
#[macro_export]
macro_rules! impl_stable_hash_via_hash {
    ($t:ty) => {
        impl<CTX> $crate::rustc_data_structures::stable_hasher::HashStable<CTX> for $t {
            #[inline]
            fn hash_stable(
                &self,
                _: &mut CTX,
                hasher: &mut $crate::rustc_data_structures::stable_hasher::StableHasher,
            ) {
                ::std::hash::Hash::hash(self, hasher);
            }
        }
    };
}

impl_stable_hash_via_hash!(i8);
impl_stable_hash_via_hash!(i16);
impl_stable_hash_via_hash!(i32);
impl_stable_hash_via_hash!(i64);
impl_stable_hash_via_hash!(isize);

impl_stable_hash_via_hash!(u8);
impl_stable_hash_via_hash!(u16);
impl_stable_hash_via_hash!(u32);
impl_stable_hash_via_hash!(u64);
impl_stable_hash_via_hash!(usize);

impl_stable_hash_via_hash!(u128);
impl_stable_hash_via_hash!(i128);

impl_stable_hash_via_hash!(char);
impl_stable_hash_via_hash!(());

impl<CTX> HashStable<CTX> for ::std::num::NonZeroU32 {
    fn hash_stable(&self, ctx: &mut CTX, hasher: &mut StableHasher) {
        self.get().hash_stable(ctx, hasher)
    }
}

impl<CTX> HashStable<CTX> for f32 {
    fn hash_stable(&self, ctx: &mut CTX, hasher: &mut StableHasher) {
        let val: u32 = f32::to_bits(*self);
        val.hash_stable(ctx, hasher);
    }
}

impl<CTX> HashStable<CTX> for f64 {
    fn hash_stable(&self, ctx: &mut CTX, hasher: &mut StableHasher) {
        let val: u64 = f64::to_bits(*self);
        val.hash_stable(ctx, hasher);
    }
}

impl<CTX> HashStable<CTX> for ::std::cmp::Ordering {
    fn hash_stable(&self, ctx: &mut CTX, hasher: &mut StableHasher) {
        (*self as i8).hash_stable(ctx, hasher);
    }
}

impl<T1: HashStable<CTX>, CTX> HashStable<CTX> for (T1,) {
    fn hash_stable(&self, ctx: &mut CTX, hasher: &mut StableHasher) {
        let (ref _0,) = *self;
        _0.hash_stable(ctx, hasher);
    }
}

impl<T1: HashStable<CTX>, T2: HashStable<CTX>, CTX> HashStable<CTX> for (T1, T2) {
    fn hash_stable(&self, ctx: &mut CTX, hasher: &mut StableHasher) {
        let (ref _0, ref _1) = *self;
        _0.hash_stable(ctx, hasher);
        _1.hash_stable(ctx, hasher);
    }
}

impl<T1, T2, T3, CTX> HashStable<CTX> for (T1, T2, T3)
where
    T1: HashStable<CTX>,
    T2: HashStable<CTX>,
    T3: HashStable<CTX>,
{
    fn hash_stable(&self, ctx: &mut CTX, hasher: &mut StableHasher) {
        let (ref _0, ref _1, ref _2) = *self;
        _0.hash_stable(ctx, hasher);
        _1.hash_stable(ctx, hasher);
        _2.hash_stable(ctx, hasher);
    }
}

impl<T1, T2, T3, T4, CTX> HashStable<CTX> for (T1, T2, T3, T4)
where
    T1: HashStable<CTX>,
    T2: HashStable<CTX>,
    T3: HashStable<CTX>,
    T4: HashStable<CTX>,
{
    fn hash_stable(&self, ctx: &mut CTX, hasher: &mut StableHasher) {
        let (ref _0, ref _1, ref _2, ref _3) = *self;
        _0.hash_stable(ctx, hasher);
        _1.hash_stable(ctx, hasher);
        _2.hash_stable(ctx, hasher);
        _3.hash_stable(ctx, hasher);
    }
}

impl<T: HashStable<CTX>, CTX> HashStable<CTX> for [T] {
    fn hash_stable(&self, ctx: &mut CTX, hasher: &mut StableHasher) {
        self.len().hash_stable(ctx, hasher);
        for item in self {
            item.hash_stable(ctx, hasher);
        }
    }
}

impl<T: HashStable<CTX>, CTX> HashStable<CTX> for Vec<T> {
    #[inline]
    fn hash_stable(&self, ctx: &mut CTX, hasher: &mut StableHasher) {
        (&self[..]).hash_stable(ctx, hasher);
    }
}

impl<T: ?Sized + HashStable<CTX>, CTX> HashStable<CTX> for Box<T> {
    #[inline]
    fn hash_stable(&self, ctx: &mut CTX, hasher: &mut StableHasher) {
        (**self).hash_stable(ctx, hasher);
    }
}

impl<T: ?Sized + HashStable<CTX>, CTX> HashStable<CTX> for ::std::rc::Rc<T> {
    #[inline]
    fn hash_stable(&self, ctx: &mut CTX, hasher: &mut StableHasher) {
        (**self).hash_stable(ctx, hasher);
    }
}

impl<T: ?Sized + HashStable<CTX>, CTX> HashStable<CTX> for ::std::sync::Arc<T> {
    #[inline]
    fn hash_stable(&self, ctx: &mut CTX, hasher: &mut StableHasher) {
        (**self).hash_stable(ctx, hasher);
    }
}

impl<CTX> HashStable<CTX> for str {
    #[inline]
    fn hash_stable(&self, _: &mut CTX, hasher: &mut StableHasher) {
        self.len().hash(hasher);
        self.as_bytes().hash(hasher);
    }
}

impl<CTX> HashStable<CTX> for String {
    #[inline]
    fn hash_stable(&self, hcx: &mut CTX, hasher: &mut StableHasher) {
        (&self[..]).hash_stable(hcx, hasher);
    }
}

impl<HCX> ToStableHashKey<HCX> for String {
    type KeyType = String;

    #[inline]
    fn to_stable_hash_key(&self, _: &HCX) -> Self::KeyType {
        self.clone()
    }
}

impl<CTX> HashStable<CTX> for bool {
    #[inline]
    fn hash_stable(&self, ctx: &mut CTX, hasher: &mut StableHasher) {
        (if *self { 1u8 } else { 0u8 }).hash_stable(ctx, hasher);
    }
}

impl<T, CTX> HashStable<CTX> for Option<T>
where
    T: HashStable<CTX>,
{
    #[inline]
    fn hash_stable(&self, ctx: &mut CTX, hasher: &mut StableHasher) {
        if let Some(ref value) = *self {
            1u8.hash_stable(ctx, hasher);
            value.hash_stable(ctx, hasher);
        } else {
            0u8.hash_stable(ctx, hasher);
        }
    }
}

impl<T1, T2, CTX> HashStable<CTX> for Result<T1, T2>
where
    T1: HashStable<CTX>,
    T2: HashStable<CTX>,
{
    #[inline]
    fn hash_stable(&self, ctx: &mut CTX, hasher: &mut StableHasher) {
        mem::discriminant(self).hash_stable(ctx, hasher);
        match *self {
            Ok(ref x) => x.hash_stable(ctx, hasher),
            Err(ref x) => x.hash_stable(ctx, hasher),
        }
    }
}

impl<'a, T, CTX> HashStable<CTX> for &'a T
where
    T: HashStable<CTX> + ?Sized,
{
    #[inline]
    fn hash_stable(&self, ctx: &mut CTX, hasher: &mut StableHasher) {
        (**self).hash_stable(ctx, hasher);
    }
}

impl<T, CTX> HashStable<CTX> for ::std::mem::Discriminant<T> {
    #[inline]
    fn hash_stable(&self, _: &mut CTX, hasher: &mut StableHasher) {
        ::std::hash::Hash::hash(self, hasher);
    }
}

// impl<I: ::indexed_vec::Idx, T, CTX> HashStable<CTX> for
// ::indexed_vec::IndexVec<I, T> where
//     T: HashStable<CTX>,
// {
//     fn hash_stable<W: StableHasherResult>(&self, ctx: &mut CTX, hasher: &mut
// StableHasher<W>) {         self.len().hash_stable(ctx, hasher);
//         for v in &self.raw {
//             v.hash_stable(ctx, hasher);
//         }
//     }
// }

// impl<I: ::indexed_vec::Idx, CTX> HashStable<CTX> for ::bit_set::BitSet<I> {
//     fn hash_stable<W: StableHasherResult>(&self, ctx: &mut CTX, hasher: &mut
// StableHasher<W>) {         self.words().hash_stable(ctx, hasher);
//     }
// }

impl_stable_hash_via_hash!(::std::path::Path);
impl_stable_hash_via_hash!(::std::path::PathBuf);

impl<K, V, R, HCX> HashStable<HCX> for ::std::collections::HashMap<K, V, R>
where
    K: ToStableHashKey<HCX> + Eq,
    V: HashStable<HCX>,
    R: BuildHasher,
{
    #[inline]
    fn hash_stable(&self, hcx: &mut HCX, hasher: &mut StableHasher) {
        stable_hash_reduce(
            hcx,
            hasher,
            self.iter(),
            self.len(),
            |hasher, hcx, (key, value)| {
                let key = key.to_stable_hash_key(hcx);
                key.hash_stable(hcx, hasher);
                value.hash_stable(hcx, hasher);
            },
        );
    }
}

impl<K, R, HCX> HashStable<HCX> for ::std::collections::HashSet<K, R>
where
    K: ToStableHashKey<HCX> + Eq,
    R: BuildHasher,
{
    fn hash_stable(&self, hcx: &mut HCX, hasher: &mut StableHasher) {
        stable_hash_reduce(hcx, hasher, self.iter(), self.len(), |hasher, hcx, key| {
            let key = key.to_stable_hash_key(hcx);
            key.hash_stable(hcx, hasher);
        });
    }
}

impl<K, V, HCX> HashStable<HCX> for ::std::collections::BTreeMap<K, V>
where
    K: ToStableHashKey<HCX>,
    V: HashStable<HCX>,
{
    fn hash_stable(&self, hcx: &mut HCX, hasher: &mut StableHasher) {
        stable_hash_reduce(
            hcx,
            hasher,
            self.iter(),
            self.len(),
            |hasher, hcx, (key, value)| {
                let key = key.to_stable_hash_key(hcx);
                key.hash_stable(hcx, hasher);
                value.hash_stable(hcx, hasher);
            },
        );
    }
}

impl<K, HCX> HashStable<HCX> for ::std::collections::BTreeSet<K>
where
    K: ToStableHashKey<HCX>,
{
    fn hash_stable(&self, hcx: &mut HCX, hasher: &mut StableHasher) {
        stable_hash_reduce(hcx, hasher, self.iter(), self.len(), |hasher, hcx, key| {
            let key = key.to_stable_hash_key(hcx);
            key.hash_stable(hcx, hasher);
        });
    }
}

fn stable_hash_reduce<HCX, I, C, F>(
    hcx: &mut HCX,
    hasher: &mut StableHasher,
    mut collection: C,
    length: usize,
    hash_function: F,
) where
    C: Iterator<Item = I>,
    F: Fn(&mut StableHasher, &mut HCX, I),
{
    length.hash_stable(hcx, hasher);

    match length {
        1 => {
            hash_function(hasher, hcx, collection.next().unwrap());
        }
        _ => {
            let hash = collection
                .map(|value| {
                    let mut hasher = StableHasher::new();
                    hash_function(&mut hasher, hcx, value);
                    hasher.finish::<u128>()
                })
                .reduce(|accum, value| accum.wrapping_add(value));
            hash.hash_stable(hcx, hasher);
        }
    }
}
