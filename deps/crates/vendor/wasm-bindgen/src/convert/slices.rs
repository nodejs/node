use alloc::boxed::Box;
use alloc::string::String;
use alloc::vec::Vec;
use core::mem::{self, MaybeUninit};
use core::ops::{Deref, DerefMut};
use core::str;

use crate::__rt::marker::ErasableGeneric;
use crate::__wbindgen_copy_to_typed_array;
use crate::convert::{
    js_value_vector_from_abi, js_value_vector_into_abi, FromWasmAbi, IntoWasmAbi,
    LongRefFromWasmAbi, OptionFromWasmAbi, OptionIntoWasmAbi, RefFromWasmAbi, RefMutFromWasmAbi,
    UpcastFrom, VectorFromWasmAbi, VectorIntoWasmAbi, WasmAbi,
};
use crate::describe::*;
use crate::JsValue;

use cfg_if::cfg_if;

/// # ⚠️ Unstable
///
/// This is part of the internal [`convert`](crate::convert) module, **no
/// stability guarantees** are provided. Use at your own risk. See its
/// documentation for more details.
// note: `WasmAbi` types do not need to be FFI-safe themselves, it's just more
// convenient to directly write `WasmSlice` in some of the manually-written FFI
// functions in `lib.rs` rather than `WasmRet<WasmSlice>`.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct WasmSlice {
    pub ptr: u32,
    pub len: u32,
}

impl WasmAbi for WasmSlice {
    /// `self.ptr`
    type Prim1 = u32;
    /// `self.len`
    type Prim2 = u32;
    type Prim3 = ();
    type Prim4 = ();

    #[inline]
    fn split(self) -> (u32, u32, (), ()) {
        (self.ptr, self.len, (), ())
    }

    #[inline]
    fn join(ptr: u32, len: u32, _: (), _: ()) -> Self {
        Self { ptr, len }
    }
}

#[inline]
fn null_slice() -> WasmSlice {
    WasmSlice { ptr: 0, len: 0 }
}

pub struct WasmMutSlice {
    pub slice: WasmSlice,
    pub idx: u32,
}

impl WasmAbi for WasmMutSlice {
    /// `self.slice.ptr`
    type Prim1 = u32;
    /// `self.slice.len`
    type Prim2 = u32;
    /// `self.idx`
    type Prim3 = u32;
    type Prim4 = ();

    #[inline]
    fn split(self) -> (u32, u32, u32, ()) {
        (self.slice.ptr, self.slice.len, self.idx, ())
    }

    #[inline]
    fn join(ptr: u32, len: u32, idx: u32, _: ()) -> Self {
        Self {
            slice: WasmSlice { ptr, len },
            idx,
        }
    }
}

/// The representation of a mutable slice passed from JS to Rust.
pub struct MutSlice<T> {
    /// A copy of the data in the JS typed array.
    contents: Box<[T]>,
    /// A reference to the original JS typed array.
    js: JsValue,
}

impl<T> Drop for MutSlice<T> {
    fn drop(&mut self) {
        let byte_slice = unsafe {
            core::slice::from_raw_parts(
                self.contents.as_ptr() as *const u8,
                self.contents.len() * mem::size_of::<T>(),
            )
        };
        __wbindgen_copy_to_typed_array(byte_slice, &self.js);
    }
}

impl<T> Deref for MutSlice<T> {
    type Target = [T];

    fn deref(&self) -> &[T] {
        &self.contents
    }
}

impl<T> DerefMut for MutSlice<T> {
    fn deref_mut(&mut self) -> &mut [T] {
        &mut self.contents
    }
}

macro_rules! vectors {
    ($($t:ty)*) => ($(
        vectors_internal!($t);
        vectors_internal!(MaybeUninit<$t>);
    )*)
}

macro_rules! vectors_internal {
    ($t:ty) => {
        impl WasmDescribeVector for $t {
            #[cfg_attr(wasm_bindgen_unstable_test_coverage, coverage(off))]
            fn describe_vector() {
                inform(VECTOR);
                <$t>::describe();
            }
        }

        impl VectorIntoWasmAbi for $t {
            type Abi = WasmSlice;

            #[inline]
            fn vector_into_abi(vector: Box<[$t]>) -> WasmSlice {
                let ptr = vector.as_ptr();
                let len = vector.len();
                mem::forget(vector);
                WasmSlice {
                    ptr: ptr.into_abi(),
                    len: len as u32,
                }
            }
        }

        impl VectorFromWasmAbi for $t {
            type Abi = WasmSlice;

            #[inline]
            unsafe fn vector_from_abi(js: WasmSlice) -> Box<[$t]> {
                let ptr = <*mut $t>::from_abi(js.ptr);
                let len = js.len as usize;
                Vec::from_raw_parts(ptr, len, len).into_boxed_slice()
            }
        }

        impl<'a> IntoWasmAbi for &'a [$t] {
            type Abi = WasmSlice;

            #[inline]
            fn into_abi(self) -> WasmSlice {
                WasmSlice {
                    ptr: self.as_ptr().into_abi(),
                    len: self.len() as u32,
                }
            }
        }

        impl<'a> OptionIntoWasmAbi for &'a [$t] {
            #[inline]
            fn none() -> WasmSlice {
                null_slice()
            }
        }

        impl<'a> IntoWasmAbi for &'a mut [$t] {
            type Abi = WasmSlice;

            #[inline]
            fn into_abi(self) -> WasmSlice {
                (&*self).into_abi()
            }
        }

        impl<'a> OptionIntoWasmAbi for &'a mut [$t] {
            #[inline]
            fn none() -> WasmSlice {
                null_slice()
            }
        }

        impl RefFromWasmAbi for [$t] {
            type Abi = WasmSlice;
            type Anchor = Box<[$t]>;

            #[inline]
            unsafe fn ref_from_abi(js: WasmSlice) -> Box<[$t]> {
                <Box<[$t]>>::from_abi(js)
            }
        }

        impl RefMutFromWasmAbi for [$t] {
            type Abi = WasmMutSlice;
            type Anchor = MutSlice<$t>;

            #[inline]
            unsafe fn ref_mut_from_abi(js: WasmMutSlice) -> MutSlice<$t> {
                let contents = <Box<[$t]>>::from_abi(js.slice);
                let js = JsValue::from_abi(js.idx);
                MutSlice { contents, js }
            }
        }

        impl LongRefFromWasmAbi for [$t] {
            type Abi = WasmSlice;
            type Anchor = Box<[$t]>;

            #[inline]
            unsafe fn long_ref_from_abi(js: WasmSlice) -> Box<[$t]> {
                Self::ref_from_abi(js)
            }
        }
    };
}

vectors! {
    u8 i8 u16 i16 u32 i32 u64 i64 usize isize f32 f64
}

impl WasmDescribeVector for String {
    #[cfg_attr(wasm_bindgen_unstable_test_coverage, coverage(off))]
    fn describe_vector() {
        inform(VECTOR);
        inform(NAMED_EXTERNREF);
        // Trying to use an actual loop for this breaks the Wasm interpreter.
        inform(6);
        inform('s' as u32);
        inform('t' as u32);
        inform('r' as u32);
        inform('i' as u32);
        inform('n' as u32);
        inform('g' as u32);
    }
}

impl VectorIntoWasmAbi for String {
    type Abi = <Box<[JsValue]> as IntoWasmAbi>::Abi;

    fn vector_into_abi(vector: Box<[Self]>) -> Self::Abi {
        js_value_vector_into_abi(vector)
    }
}

impl VectorFromWasmAbi for String {
    type Abi = <Box<[JsValue]> as FromWasmAbi>::Abi;

    unsafe fn vector_from_abi(js: Self::Abi) -> Box<[Self]> {
        js_value_vector_from_abi(js)
    }
}

cfg_if! {
    if #[cfg(feature = "enable-interning")] {
        #[inline]
        fn unsafe_get_cached_str(x: &str) -> Option<WasmSlice> {
            // This uses 0 for the ptr as an indication that it is a JsValue and not a str.
            crate::cache::intern::unsafe_get_str(x).map(|x| WasmSlice { ptr: 0, len: x })
        }

    } else {
        #[inline]
        fn unsafe_get_cached_str(_x: &str) -> Option<WasmSlice> {
            None
        }
    }
}

impl<T> IntoWasmAbi for Vec<T>
where
    Box<[T]>: IntoWasmAbi<Abi = WasmSlice>,
{
    type Abi = <Box<[T]> as IntoWasmAbi>::Abi;

    #[inline]
    fn into_abi(self) -> Self::Abi {
        self.into_boxed_slice().into_abi()
    }
}

impl<T> OptionIntoWasmAbi for Vec<T>
where
    Box<[T]>: IntoWasmAbi<Abi = WasmSlice>,
{
    #[inline]
    fn none() -> WasmSlice {
        null_slice()
    }
}

impl<T> FromWasmAbi for Vec<T>
where
    Box<[T]>: FromWasmAbi<Abi = WasmSlice>,
{
    type Abi = <Box<[T]> as FromWasmAbi>::Abi;

    #[inline]
    unsafe fn from_abi(js: Self::Abi) -> Self {
        <Box<[T]>>::from_abi(js).into()
    }
}

impl<T> OptionFromWasmAbi for Vec<T>
where
    Box<[T]>: FromWasmAbi<Abi = WasmSlice>,
{
    #[inline]
    fn is_none(abi: &WasmSlice) -> bool {
        abi.ptr == 0
    }
}

impl IntoWasmAbi for String {
    type Abi = <Vec<u8> as IntoWasmAbi>::Abi;

    #[inline]
    fn into_abi(self) -> Self::Abi {
        // This is safe because the JsValue is immediately looked up in the heap and
        // then returned, so use-after-free cannot occur.
        unsafe_get_cached_str(&self).unwrap_or_else(|| self.into_bytes().into_abi())
    }
}

impl OptionIntoWasmAbi for String {
    #[inline]
    fn none() -> Self::Abi {
        null_slice()
    }
}

impl FromWasmAbi for String {
    type Abi = <Vec<u8> as FromWasmAbi>::Abi;

    #[inline]
    unsafe fn from_abi(js: Self::Abi) -> Self {
        String::from_utf8_unchecked(<Vec<u8>>::from_abi(js))
    }
}

impl OptionFromWasmAbi for String {
    #[inline]
    fn is_none(slice: &WasmSlice) -> bool {
        slice.ptr == 0
    }
}

impl<'a> IntoWasmAbi for &'a str {
    type Abi = <&'a [u8] as IntoWasmAbi>::Abi;

    #[inline]
    fn into_abi(self) -> Self::Abi {
        // This is safe because the JsValue is immediately looked up in the heap and
        // then returned, so use-after-free cannot occur.
        unsafe_get_cached_str(self).unwrap_or_else(|| self.as_bytes().into_abi())
    }
}

impl OptionIntoWasmAbi for &str {
    #[inline]
    fn none() -> Self::Abi {
        null_slice()
    }
}

impl RefFromWasmAbi for str {
    type Abi = <[u8] as RefFromWasmAbi>::Abi;
    type Anchor = Box<str>;

    #[inline]
    unsafe fn ref_from_abi(js: Self::Abi) -> Self::Anchor {
        mem::transmute::<Box<[u8]>, Box<str>>(<Box<[u8]>>::from_abi(js))
    }
}

impl LongRefFromWasmAbi for str {
    type Abi = <[u8] as RefFromWasmAbi>::Abi;
    type Anchor = Box<str>;

    #[inline]
    unsafe fn long_ref_from_abi(js: Self::Abi) -> Self::Anchor {
        Self::ref_from_abi(js)
    }
}

unsafe impl ErasableGeneric for &str {
    type Repr = &'static str;
}

unsafe impl<T: ErasableGeneric> ErasableGeneric for Box<[T]> {
    type Repr = Box<[T::Repr]>;
}

impl UpcastFrom<&str> for &str {}

impl<T, Target> UpcastFrom<Box<[T]>> for Box<[Target]> where Target: UpcastFrom<T> {}

unsafe impl<T: ErasableGeneric> ErasableGeneric for Vec<T> {
    type Repr = Vec<T::Repr>;
}

impl<T, Target> UpcastFrom<Vec<T>> for Vec<Target> where Target: UpcastFrom<T> {}

impl<T: VectorIntoWasmAbi> IntoWasmAbi for Box<[T]> {
    type Abi = <T as VectorIntoWasmAbi>::Abi;

    fn into_abi(self) -> Self::Abi {
        T::vector_into_abi(self)
    }
}

impl<T> OptionIntoWasmAbi for Box<[T]>
where
    Self: IntoWasmAbi<Abi = WasmSlice>,
{
    fn none() -> WasmSlice {
        null_slice()
    }
}

impl<T: VectorFromWasmAbi> FromWasmAbi for Box<[T]> {
    type Abi = <T as VectorFromWasmAbi>::Abi;

    unsafe fn from_abi(js: Self::Abi) -> Self {
        T::vector_from_abi(js)
    }
}

impl<T> OptionFromWasmAbi for Box<[T]>
where
    Self: FromWasmAbi<Abi = WasmSlice>,
{
    fn is_none(slice: &WasmSlice) -> bool {
        slice.ptr == 0
    }
}

impl<T: ErasableGeneric<Repr = JsValue> + WasmDescribe> VectorFromWasmAbi for T {
    type Abi = WasmSlice;

    #[inline]
    unsafe fn vector_from_abi(js: WasmSlice) -> Box<[Self]> {
        let ptr = <*mut T>::from_abi(js.ptr);
        let len = js.len as usize;
        Vec::from_raw_parts(ptr, len, len).into_boxed_slice()
    }
}

impl<T: ErasableGeneric<Repr = JsValue> + WasmDescribe> VectorIntoWasmAbi for T {
    type Abi = WasmSlice;

    #[inline]
    fn vector_into_abi(vector: Box<[T]>) -> WasmSlice {
        let ptr = vector.as_ptr();
        let len = vector.len();
        mem::forget(vector);
        WasmSlice {
            ptr: ptr.into_abi(),
            len: len as u32,
        }
    }
}

// JsValue-like slice support (Rust-to-JS only)
// JsValue-like are repr(transparent) over u32, so &[JsValue] is a contiguous array of heap indices

unsafe impl<T: ErasableGeneric> ErasableGeneric for &[T] {
    type Repr = &'static [T::Repr];
}

impl<'a, T, Target> UpcastFrom<&'a [T]> for &'a [Target] where Target: UpcastFrom<T> {}

impl<T: ErasableGeneric<Repr = JsValue> + WasmDescribe> IntoWasmAbi for &[T] {
    type Abi = WasmSlice;

    #[inline]
    fn into_abi(self) -> WasmSlice {
        WasmSlice {
            ptr: self.as_ptr() as u32,
            len: self.len() as u32,
        }
    }
}

impl<T: ErasableGeneric<Repr = JsValue> + WasmDescribe> OptionIntoWasmAbi for &[T] {
    #[inline]
    fn none() -> WasmSlice {
        null_slice()
    }
}
