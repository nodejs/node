use alloc::boxed::Box;
use alloc::vec::Vec;
use core::char;
use core::mem::{self, ManuallyDrop};
use core::ptr::NonNull;

use crate::__rt::marker::ErasableGeneric;
use crate::convert::traits::{WasmAbi, WasmPrimitive};
use crate::convert::{
    FromWasmAbi, IntoWasmAbi, LongRefFromWasmAbi, OptionFromWasmAbi, OptionIntoWasmAbi,
    RefFromWasmAbi, ReturnWasmAbi, TryFromJsValue, UpcastFrom,
};
use crate::sys::Promising;
use crate::sys::{JsOption, Undefined};
use crate::{Clamped, JsError, JsValue, UnwrapThrowExt};

// Primitive types can always be passed over the ABI.
impl<T: WasmPrimitive> WasmAbi for T {
    type Prim1 = Self;
    type Prim2 = ();
    type Prim3 = ();
    type Prim4 = ();

    #[inline]
    fn split(self) -> (Self, (), (), ()) {
        (self, (), (), ())
    }

    #[inline]
    fn join(prim: Self, _: (), _: (), _: ()) -> Self {
        prim
    }
}

impl WasmAbi for i128 {
    type Prim1 = u64;
    type Prim2 = u64;
    type Prim3 = ();
    type Prim4 = ();

    #[inline]
    fn split(self) -> (u64, u64, (), ()) {
        let low = self as u64;
        let high = (self >> 64) as u64;
        (low, high, (), ())
    }

    #[inline]
    fn join(low: u64, high: u64, _: (), _: ()) -> Self {
        ((high as u128) << 64 | low as u128) as i128
    }
}
impl WasmAbi for u128 {
    type Prim1 = u64;
    type Prim2 = u64;
    type Prim3 = ();
    type Prim4 = ();

    #[inline]
    fn split(self) -> (u64, u64, (), ()) {
        let low = self as u64;
        let high = (self >> 64) as u64;
        (low, high, (), ())
    }

    #[inline]
    fn join(low: u64, high: u64, _: (), _: ()) -> Self {
        (high as u128) << 64 | low as u128
    }
}

impl<T: WasmAbi<Prim4 = ()>> WasmAbi for Option<T> {
    /// Whether this `Option` is a `Some` value.
    type Prim1 = u32;
    type Prim2 = T::Prim1;
    type Prim3 = T::Prim2;
    type Prim4 = T::Prim3;

    #[inline]
    fn split(self) -> (u32, T::Prim1, T::Prim2, T::Prim3) {
        match self {
            None => (
                0,
                Default::default(),
                Default::default(),
                Default::default(),
            ),
            Some(value) => {
                let (prim1, prim2, prim3, ()) = value.split();
                (1, prim1, prim2, prim3)
            }
        }
    }

    #[inline]
    fn join(is_some: u32, prim1: T::Prim1, prim2: T::Prim2, prim3: T::Prim3) -> Self {
        if is_some == 0 {
            None
        } else {
            Some(T::join(prim1, prim2, prim3, ()))
        }
    }
}

macro_rules! type_wasm_native {
    ($($t:tt as $c:tt)*) => ($(
        impl IntoWasmAbi for $t {
            type Abi = $c;

            #[inline]
            fn into_abi(self) -> $c { self as $c }
        }

        impl FromWasmAbi for $t {
            type Abi = $c;

            #[inline]
            unsafe fn from_abi(js: $c) -> Self { js as $t }
        }

        impl IntoWasmAbi for Option<$t> {
            type Abi = Option<$c>;

            #[inline]
            fn into_abi(self) -> Self::Abi {
                self.map(|v| v as $c)
            }
        }

        impl FromWasmAbi for Option<$t> {
            type Abi = Option<$c>;

            #[inline]
            unsafe fn from_abi(js: Self::Abi) -> Self {
                js.map(|v: $c| v as $t)
            }
        }

        impl UpcastFrom<$t> for JsValue {}
        impl UpcastFrom<$t> for JsOption<JsValue> {}
        impl UpcastFrom<$t> for $t {}
    )*)
}

type_wasm_native!(
    i64 as i64
    u64 as u64
    i128 as i128
    u128 as u128
    f64 as f64
);

impl UpcastFrom<u64> for u128 {}
impl UpcastFrom<u64> for JsOption<u128> {}
impl UpcastFrom<i64> for i128 {}
impl UpcastFrom<i64> for JsOption<i128> {}

/// The sentinel value is 2^32 + 1 for 32-bit primitive types.
///
/// 2^32 + 1 is used, because it's the smallest positive integer that cannot be
/// represented by any 32-bit primitive. While any value >= 2^32 works as a
/// sentinel value for 32-bit integers, it's a bit more tricky for `f32`. `f32`
/// can represent all powers of 2 up to 2^127 exactly. And between 2^32 and 2^33,
/// `f32` can represent all integers 2^32+512*k exactly.
const F64_ABI_OPTION_SENTINEL: f64 = 4294967297_f64;

macro_rules! type_wasm_native_f64_option {
    ($($t:tt as $c:tt)*) => ($(
        impl IntoWasmAbi for $t {
            type Abi = $c;

            #[inline]
            fn into_abi(self) -> $c { self as $c }
        }

        impl FromWasmAbi for $t {
            type Abi = $c;

            #[inline]
            unsafe fn from_abi(js: $c) -> Self { js as $t }
        }

        unsafe impl ErasableGeneric for $t {
            type Repr = $t;
        }

        impl Promising for $t {
            type Resolution = $t;
        }

        impl IntoWasmAbi for Option<$t> {
            type Abi = f64;

            #[inline]
            fn into_abi(self) -> Self::Abi {
                self.map(|v| v as $c as f64).unwrap_or(F64_ABI_OPTION_SENTINEL)
            }
        }

        impl FromWasmAbi for Option<$t> {
            type Abi = f64;

            #[inline]
            unsafe fn from_abi(js: Self::Abi) -> Self {
                if js == F64_ABI_OPTION_SENTINEL {
                    None
                } else {
                    Some(js as $c as $t)
                }
            }
        }

        impl UpcastFrom<$t> for JsValue {}
        impl UpcastFrom<$t> for JsOption<JsValue> {}
        impl UpcastFrom<$t> for $t {}
    )*)
}

type_wasm_native_f64_option!(
    i32 as i32
    isize as i32
    u32 as u32
    usize as u32
    f32 as f32
);

#[cfg(target_pointer_width = "32")]
impl UpcastFrom<isize> for i32 {}
#[cfg(target_pointer_width = "32")]
impl UpcastFrom<isize> for JsOption<i32> {}

impl UpcastFrom<isize> for i64 {}
impl UpcastFrom<isize> for JsOption<i64> {}
impl UpcastFrom<isize> for i128 {}
impl UpcastFrom<isize> for JsOption<i128> {}

impl UpcastFrom<i32> for isize {}
impl UpcastFrom<i32> for JsOption<isize> {}
impl UpcastFrom<i32> for i64 {}
impl UpcastFrom<i32> for JsOption<i64> {}
impl UpcastFrom<i32> for i128 {}
impl UpcastFrom<i32> for JsOption<i128> {}

impl UpcastFrom<u32> for usize {}
impl UpcastFrom<u32> for JsOption<usize> {}
impl UpcastFrom<u32> for u64 {}
impl UpcastFrom<u32> for JsOption<u64> {}
impl UpcastFrom<u32> for u128 {}
impl UpcastFrom<u32> for JsOption<u128> {}

#[cfg(target_pointer_width = "32")]
impl UpcastFrom<usize> for u32 {}
#[cfg(target_pointer_width = "32")]
impl UpcastFrom<usize> for JsOption<u32> {}
impl UpcastFrom<usize> for u64 {}
impl UpcastFrom<usize> for JsOption<u64> {}
impl UpcastFrom<usize> for u128 {}
impl UpcastFrom<usize> for JsOption<u128> {}

impl UpcastFrom<f32> for f64 {}
impl UpcastFrom<f32> for JsOption<f64> {}

/// The sentinel value is 0xFF_FFFF for primitives with less than 32 bits.
///
/// This value is used, so all small primitive types (`bool`, `i8`, `u8`,
/// `i16`, `u16`, `char`) can use the same JS glue code. `char::MAX` is
/// 0x10_FFFF btw.
const U32_ABI_OPTION_SENTINEL: u32 = 0x00FF_FFFFu32;

macro_rules! type_abi_as_u32 {
    ($($t:tt)*) => ($(
        impl IntoWasmAbi for $t {
            type Abi = u32;

            #[inline]
            fn into_abi(self) -> u32 { self as u32 }
        }

        impl FromWasmAbi for $t {
            type Abi = u32;

            #[inline]
            unsafe fn from_abi(js: u32) -> Self { js as $t }
        }

        impl OptionIntoWasmAbi for $t {
            #[inline]
            fn none() -> u32 { U32_ABI_OPTION_SENTINEL }
        }

        impl OptionFromWasmAbi for $t {
            #[inline]
            fn is_none(js: &u32) -> bool { *js == U32_ABI_OPTION_SENTINEL }
        }

        unsafe impl ErasableGeneric for $t {
            type Repr = $t;
        }

        impl Promising for $t {
            type Resolution = $t;
        }

        impl UpcastFrom<$t> for JsValue {}
        impl UpcastFrom<$t> for JsOption<JsValue> {}
        impl UpcastFrom<$t> for $t {}
    )*)
}

type_abi_as_u32!(i8 u8 i16 u16);

impl UpcastFrom<i8> for i16 {}
impl UpcastFrom<i8> for JsOption<i16> {}
impl UpcastFrom<i8> for i32 {}
impl UpcastFrom<i8> for JsOption<i32> {}
impl UpcastFrom<i8> for i64 {}
impl UpcastFrom<i8> for JsOption<i64> {}
impl UpcastFrom<i8> for i128 {}
impl UpcastFrom<i8> for JsOption<i128> {}

impl UpcastFrom<u8> for u16 {}
impl UpcastFrom<u8> for JsOption<u16> {}
impl UpcastFrom<u8> for u32 {}
impl UpcastFrom<u8> for JsOption<u32> {}
impl UpcastFrom<u8> for u64 {}
impl UpcastFrom<u8> for JsOption<u64> {}
impl UpcastFrom<u8> for u128 {}
impl UpcastFrom<u8> for JsOption<u128> {}

impl UpcastFrom<i16> for i32 {}
impl UpcastFrom<i16> for JsOption<i32> {}
impl UpcastFrom<i16> for i64 {}
impl UpcastFrom<i16> for JsOption<i64> {}
impl UpcastFrom<i16> for i128 {}
impl UpcastFrom<i16> for JsOption<i128> {}

impl UpcastFrom<u16> for u32 {}
impl UpcastFrom<u16> for JsOption<u32> {}
impl UpcastFrom<u16> for u64 {}
impl UpcastFrom<u16> for JsOption<u64> {}
impl UpcastFrom<u16> for u128 {}
impl UpcastFrom<u16> for JsOption<u128> {}

impl IntoWasmAbi for bool {
    type Abi = u32;

    #[inline]
    fn into_abi(self) -> u32 {
        self as u32
    }
}

impl FromWasmAbi for bool {
    type Abi = u32;

    #[inline]
    unsafe fn from_abi(js: u32) -> bool {
        js != 0
    }
}

impl OptionIntoWasmAbi for bool {
    #[inline]
    fn none() -> u32 {
        U32_ABI_OPTION_SENTINEL
    }
}

impl OptionFromWasmAbi for bool {
    #[inline]
    fn is_none(js: &u32) -> bool {
        *js == U32_ABI_OPTION_SENTINEL
    }
}

unsafe impl ErasableGeneric for bool {
    type Repr = bool;
}

impl Promising for bool {
    type Resolution = bool;
}

impl UpcastFrom<bool> for JsValue {}
impl UpcastFrom<bool> for JsOption<JsValue> {}
impl UpcastFrom<bool> for bool {}

impl IntoWasmAbi for char {
    type Abi = u32;

    #[inline]
    fn into_abi(self) -> u32 {
        self as u32
    }
}

impl FromWasmAbi for char {
    type Abi = u32;

    #[inline]
    unsafe fn from_abi(js: u32) -> char {
        // SAFETY: Checked in bindings.
        char::from_u32_unchecked(js)
    }
}

impl OptionIntoWasmAbi for char {
    #[inline]
    fn none() -> u32 {
        U32_ABI_OPTION_SENTINEL
    }
}

impl OptionFromWasmAbi for char {
    #[inline]
    fn is_none(js: &u32) -> bool {
        *js == U32_ABI_OPTION_SENTINEL
    }
}

unsafe impl ErasableGeneric for char {
    type Repr = char;
}

impl Promising for char {
    type Resolution = char;
}

impl UpcastFrom<char> for JsValue {}
impl UpcastFrom<char> for JsOption<JsValue> {}
impl UpcastFrom<char> for char {}

impl<T> IntoWasmAbi for *const T {
    type Abi = u32;

    #[inline]
    fn into_abi(self) -> u32 {
        self as u32
    }
}

impl<T> FromWasmAbi for *const T {
    type Abi = u32;

    #[inline]
    unsafe fn from_abi(js: u32) -> *const T {
        js as *const T
    }
}

unsafe impl<T: ErasableGeneric> ErasableGeneric for *const T {
    type Repr = *const T::Repr;
}

impl<T, Target> UpcastFrom<*const T> for *const Target where Target: UpcastFrom<T> {}
impl<T, Target> UpcastFrom<*const T> for JsOption<*const Target> where Target: UpcastFrom<T> {}

impl<T> IntoWasmAbi for Option<*const T> {
    type Abi = f64;

    #[inline]
    fn into_abi(self) -> f64 {
        self.map(|ptr| ptr as u32 as f64)
            .unwrap_or(F64_ABI_OPTION_SENTINEL)
    }
}

unsafe impl<T: ErasableGeneric> ErasableGeneric for Option<T> {
    type Repr = Option<<T as ErasableGeneric>::Repr>;
}

impl<T, Target> UpcastFrom<Option<T>> for Option<Target> where Target: UpcastFrom<T> {}
impl<T, Target> UpcastFrom<Option<T>> for JsOption<Option<Target>> where Target: UpcastFrom<T> {}

impl<T> FromWasmAbi for Option<*const T> {
    type Abi = f64;

    #[inline]
    unsafe fn from_abi(js: f64) -> Option<*const T> {
        if js == F64_ABI_OPTION_SENTINEL {
            None
        } else {
            Some(js as u32 as *const T)
        }
    }
}

impl<T> IntoWasmAbi for *mut T {
    type Abi = u32;

    #[inline]
    fn into_abi(self) -> u32 {
        self as u32
    }
}

impl<T> FromWasmAbi for *mut T {
    type Abi = u32;

    #[inline]
    unsafe fn from_abi(js: u32) -> *mut T {
        js as *mut T
    }
}

impl<T> IntoWasmAbi for Option<*mut T> {
    type Abi = f64;

    #[inline]
    fn into_abi(self) -> f64 {
        self.map(|ptr| ptr as u32 as f64)
            .unwrap_or(F64_ABI_OPTION_SENTINEL)
    }
}

impl<T> FromWasmAbi for Option<*mut T> {
    type Abi = f64;

    #[inline]
    unsafe fn from_abi(js: f64) -> Option<*mut T> {
        if js == F64_ABI_OPTION_SENTINEL {
            None
        } else {
            Some(js as u32 as *mut T)
        }
    }
}

impl<T> IntoWasmAbi for NonNull<T> {
    type Abi = u32;

    #[inline]
    fn into_abi(self) -> u32 {
        self.as_ptr() as u32
    }
}

impl<T> OptionIntoWasmAbi for NonNull<T> {
    #[inline]
    fn none() -> u32 {
        0
    }
}

impl<T> FromWasmAbi for NonNull<T> {
    type Abi = u32;

    #[inline]
    unsafe fn from_abi(js: Self::Abi) -> Self {
        // SAFETY: Checked in bindings.
        NonNull::new_unchecked(js as *mut T)
    }
}

impl<T> OptionFromWasmAbi for NonNull<T> {
    #[inline]
    fn is_none(js: &u32) -> bool {
        *js == 0
    }
}

impl IntoWasmAbi for JsValue {
    type Abi = u32;

    #[inline]
    fn into_abi(self) -> u32 {
        let ret = self.idx;
        mem::forget(self);
        ret
    }
}

impl FromWasmAbi for JsValue {
    type Abi = u32;

    #[inline]
    unsafe fn from_abi(js: u32) -> JsValue {
        JsValue::_new(js)
    }
}

impl IntoWasmAbi for &JsValue {
    type Abi = u32;

    #[inline]
    fn into_abi(self) -> u32 {
        self.idx
    }
}

impl RefFromWasmAbi for JsValue {
    type Abi = u32;
    type Anchor = ManuallyDrop<JsValue>;

    #[inline]
    unsafe fn ref_from_abi(js: u32) -> Self::Anchor {
        ManuallyDrop::new(JsValue::_new(js))
    }
}

impl LongRefFromWasmAbi for JsValue {
    type Abi = u32;
    type Anchor = JsValue;

    #[inline]
    unsafe fn long_ref_from_abi(js: u32) -> Self::Anchor {
        Self::from_abi(js)
    }
}

impl OptionIntoWasmAbi for JsValue {
    #[inline]
    fn none() -> u32 {
        crate::__rt::JSIDX_UNDEFINED
    }
}

impl OptionIntoWasmAbi for &JsValue {
    #[inline]
    fn none() -> u32 {
        crate::__rt::JSIDX_UNDEFINED
    }
}

impl OptionFromWasmAbi for JsValue {
    #[inline]
    fn is_none(js: &u32) -> bool {
        unsafe { Self::ref_from_abi(*js) }.is_undefined()
    }
}

impl<T: OptionIntoWasmAbi> IntoWasmAbi for Option<T> {
    type Abi = T::Abi;

    #[inline]
    fn into_abi(self) -> T::Abi {
        match self {
            None => T::none(),
            Some(me) => me.into_abi(),
        }
    }
}

impl<T: OptionFromWasmAbi> FromWasmAbi for Option<T> {
    type Abi = T::Abi;

    #[inline]
    unsafe fn from_abi(js: T::Abi) -> Self {
        if T::is_none(&js) {
            None
        } else {
            Some(T::from_abi(js))
        }
    }
}

impl<T: OptionIntoWasmAbi + ErasableGeneric<Repr = JsValue> + Promising> Promising for Option<T> {
    type Resolution = Option<<T as Promising>::Resolution>;
}

impl<T: IntoWasmAbi> IntoWasmAbi for Clamped<T> {
    type Abi = T::Abi;

    #[inline]
    fn into_abi(self) -> Self::Abi {
        self.0.into_abi()
    }
}

impl<T: FromWasmAbi> FromWasmAbi for Clamped<T> {
    type Abi = T::Abi;

    #[inline]
    unsafe fn from_abi(js: T::Abi) -> Self {
        Clamped(T::from_abi(js))
    }
}

impl IntoWasmAbi for () {
    type Abi = ();

    #[inline]
    fn into_abi(self) {
        self
    }
}

impl FromWasmAbi for () {
    type Abi = ();

    #[inline]
    unsafe fn from_abi(_js: ()) {}
}

impl Promising for () {
    type Resolution = Undefined;
}

impl UpcastFrom<()> for JsValue {}
impl UpcastFrom<()> for () {}

unsafe impl ErasableGeneric for () {
    type Repr = ();
}

impl<T: WasmAbi<Prim3 = (), Prim4 = ()>> WasmAbi for Result<T, u32> {
    type Prim1 = T::Prim1;
    type Prim2 = T::Prim2;
    // The order of primitives here is such that we can pop() the possible error
    // first, deal with it and move on. Later primitives are popped off the
    // stack first.
    /// If this `Result` is an `Err`, the error value.
    type Prim3 = u32;
    /// Whether this `Result` is an `Err`.
    type Prim4 = u32;

    #[inline]
    fn split(self) -> (T::Prim1, T::Prim2, u32, u32) {
        match self {
            Ok(value) => {
                let (prim1, prim2, (), ()) = value.split();
                (prim1, prim2, 0, 0)
            }
            Err(err) => (Default::default(), Default::default(), err, 1),
        }
    }

    #[inline]
    fn join(prim1: T::Prim1, prim2: T::Prim2, err: u32, is_err: u32) -> Self {
        if is_err == 0 {
            Ok(T::join(prim1, prim2, (), ()))
        } else {
            Err(err)
        }
    }
}

impl<T, E> ReturnWasmAbi for Result<T, E>
where
    T: IntoWasmAbi,
    E: Into<JsValue>,
    T::Abi: WasmAbi<Prim3 = (), Prim4 = ()>,
{
    type Abi = Result<T::Abi, u32>;

    #[inline]
    fn return_abi(self) -> Self::Abi {
        match self {
            Ok(v) => Ok(v.into_abi()),
            Err(e) => {
                let jsval = e.into();
                Err(jsval.into_abi())
            }
        }
    }
}

unsafe impl<T: ErasableGeneric, E: ErasableGeneric> ErasableGeneric for Result<T, E> {
    type Repr = Result<<T as ErasableGeneric>::Repr, <E as ErasableGeneric>::Repr>;
}

impl<T: ErasableGeneric + Promising, E: ErasableGeneric> Promising for Result<T, E> {
    type Resolution = Result<<T as Promising>::Resolution, E>;
}

impl<T, E, TargetT, TargetE> UpcastFrom<Result<T, E>> for Result<TargetT, TargetE>
where
    TargetT: UpcastFrom<T>,
    TargetE: UpcastFrom<E>,
{
}
impl<T, E, TargetT, TargetE> UpcastFrom<Result<T, E>> for JsOption<Result<TargetT, TargetE>>
where
    TargetT: UpcastFrom<T>,
    TargetE: UpcastFrom<E>,
{
}

unsafe impl ErasableGeneric for JsError {
    type Repr = JsValue;
}

impl IntoWasmAbi for JsError {
    type Abi = <JsValue as IntoWasmAbi>::Abi;

    fn into_abi(self) -> Self::Abi {
        self.value.into_abi()
    }
}

impl Promising for JsError {
    type Resolution = JsError;
}

impl UpcastFrom<JsError> for JsValue {}
impl UpcastFrom<JsError> for JsOption<JsValue> {}
impl UpcastFrom<JsError> for JsError {}

/// # ⚠️ Unstable
///
/// This is part of the internal [`convert`](crate::convert) module, **no
/// stability guarantees** are provided. Use at your own risk. See its
/// documentation for more details.
// Note: this can't take `&[T]` because the `Into<JsValue>` impl needs
// ownership of `T`.
pub fn js_value_vector_into_abi<T: Into<JsValue>>(
    vector: Box<[T]>,
) -> <Box<[JsValue]> as IntoWasmAbi>::Abi {
    let js_vals: Box<[JsValue]> = vector.into_vec().into_iter().map(|x| x.into()).collect();

    js_vals.into_abi()
}

/// # ⚠️ Unstable
///
/// This is part of the internal [`convert`](crate::convert) module, **no
/// stability guarantees** are provided. Use at your own risk. See its
/// documentation for more details.
pub unsafe fn js_value_vector_from_abi<T: TryFromJsValue>(
    js: <Box<[JsValue]> as FromWasmAbi>::Abi,
) -> Box<[T]> {
    let js_vals = <Vec<JsValue> as FromWasmAbi>::from_abi(js);

    let mut result = Vec::with_capacity(js_vals.len());
    for value in js_vals {
        // We push elements one-by-one instead of using `collect` in order to improve
        // error messages. When using `collect`, this `expect_throw` is buried in a
        // giant chain of internal iterator functions, which results in the actual
        // function that takes this `Vec` falling off the end of the call stack.
        // So instead, make sure to call it directly within this function.
        //
        // This is only a problem in debug mode. Since this is the browser's error stack
        // we're talking about, it can only see functions that actually make it to the
        // final Wasm binary (i.e., not inlined functions). All of those internal
        // iterator functions get inlined in release mode, and so they don't show up.
        result.push(
            T::try_from_js_value(value).expect_throw("array contains a value of the wrong type"),
        );
    }
    result.into_boxed_slice()
}
