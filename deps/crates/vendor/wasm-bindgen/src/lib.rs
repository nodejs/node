//! Runtime support for the `wasm-bindgen` tool
//!
//! This crate contains the runtime support necessary for `wasm-bindgen` the
//! attribute and tool. Crates pull in the `#[wasm_bindgen]` attribute through
//! this crate and this crate also provides JS bindings through the `JsValue`
//! interface.
//!
//! ## Features
//!
//! ### `enable-interning`
//!
//! Enables the internal cache for [`wasm_bindgen::intern`].
//!
//! This feature currently enables the `std` feature, meaning that it is not
//! compatible with `no_std` environments.
//!
//! ### `std` (default)
//!
//! Enabling this feature will make the crate depend on the Rust standard library.
//!
//! Disable this feature to use this crate in `no_std` environments.
//!
//! ### `strict-macro`
//!
//! All warnings the `#[wasm_bindgen]` macro emits are turned into hard errors.
//! This mainly affects unused attribute options.
//!
//! ### Deprecated features
//!
//! #### `serde-serialize`
//!
//! **Deprecated:** Use the [`serde-wasm-bindgen`](https://docs.rs/serde-wasm-bindgen/latest/serde_wasm_bindgen/) crate instead.
//!
//! Enables the `JsValue::from_serde` and `JsValue::into_serde` methods for
//! serializing and deserializing Rust types to and from JavaScript.
//!
//! #### `spans`
//!
//! **Deprecated:** This feature became a no-op in wasm-bindgen v0.2.20 (Sep 7, 2018).

#![no_std]
#![cfg_attr(wasm_bindgen_unstable_test_coverage, feature(coverage_attribute))]
#![cfg_attr(target_feature = "atomics", feature(thread_local))]
#![cfg_attr(
    any(target_feature = "atomics", wasm_bindgen_unstable_test_coverage),
    feature(allow_internal_unstable),
    allow(internal_features)
)]
#![doc(html_root_url = "https://docs.rs/wasm-bindgen/0.2")]

extern crate alloc;
#[cfg(feature = "std")]
extern crate std;

use crate::convert::{TryFromJsValue, UpcastFrom, VectorIntoWasmAbi};
use crate::sys::Promising;
use alloc::boxed::Box;
use alloc::string::String;
use alloc::vec::Vec;
use core::convert::TryFrom;
use core::marker::PhantomData;
use core::ops::{
    Add, BitAnd, BitOr, BitXor, Deref, DerefMut, Div, Mul, Neg, Not, Rem, Shl, Shr, Sub,
};
use core::ptr::NonNull;

const _: () = {
    /// Dummy empty function provided in order to detect linker-injected functions like `__wasm_call_ctors` and others that should be skipped by the wasm-bindgen interpreter.
    ///
    /// ## About `__wasm_call_ctors`
    ///
    /// There are several ways `__wasm_call_ctors` is introduced by the linker:
    ///
    /// * Using `#[link_section = ".init_array"]`;
    /// * Linking with a C library that uses `__attribute__((constructor))`.
    ///
    /// The Wasm linker will insert a call to the `__wasm_call_ctors` function at the beginning of every
    /// function that your module exports if it regards a module as having "command-style linkage".
    /// Specifically, it regards a module as having "command-style linkage" if:
    ///
    /// * it is not relocatable;
    /// * it is not a position-independent executable;
    /// * and it does not call `__wasm_call_ctors`, directly or indirectly, from any
    ///   exported function.
    #[no_mangle]
    pub extern "C" fn __wbindgen_skip_interpret_calls() {}
};

macro_rules! externs {
    ($(#[$attr:meta])* extern "C" { $(fn $name:ident($($args:tt)*) -> $ret:ty;)* }) => (
        #[cfg(all(target_arch = "wasm32", any(target_os = "unknown", target_os = "none")))]
        $(#[$attr])*
        extern "C" {
            $(fn $name($($args)*) -> $ret;)*
        }

        $(
            #[cfg(not(all(target_arch = "wasm32", any(target_os = "unknown", target_os = "none"))))]
            #[allow(unused_variables)]
            unsafe extern "C" fn $name($($args)*) -> $ret {
                panic!("function not implemented on non-wasm32 targets")
            }
        )*
    )
}

/// A module which is typically glob imported.
///
/// ```
/// use wasm_bindgen::prelude::*;
/// ```
pub mod prelude {
    pub use crate::closure::{Closure, ScopedClosure};
    pub use crate::convert::Upcast; // provides upcast() and upcast_ref()
    pub use crate::JsCast;
    pub use crate::JsValue;
    pub use crate::UnwrapThrowExt;
    #[doc(hidden)]
    pub use wasm_bindgen_macro::__wasm_bindgen_class_marker;
    pub use wasm_bindgen_macro::wasm_bindgen;

    pub use crate::JsError;
}

pub use wasm_bindgen_macro::link_to;

pub mod closure;
pub mod convert;
pub mod describe;
mod link;
pub mod sys;

#[cfg(wbg_reference_types)]
mod externref;
#[cfg(wbg_reference_types)]
use externref::__wbindgen_externref_heap_live_count;

pub use crate::__rt::marker::ErasableGeneric;
pub use crate::convert::JsGeneric;

mod cast;
pub use crate::cast::JsCast;

mod cache;
pub use cache::intern::{intern, unintern};

#[doc(hidden)]
#[path = "rt/mod.rs"]
pub mod __rt;
use __rt::wbg_cast;

/// Representation of an object owned by JS.
///
/// A `JsValue` doesn't actually live in Rust right now but actually in a table
/// owned by the `wasm-bindgen` generated JS glue code. Eventually the ownership
/// will transfer into Wasm directly and this will likely become more efficient,
/// but for now it may be slightly slow.
pub struct JsValue {
    idx: u32,
    _marker: PhantomData<*mut u8>, // not at all threadsafe
}

#[cfg(not(target_feature = "atomics"))]
unsafe impl Send for JsValue {}
#[cfg(not(target_feature = "atomics"))]
unsafe impl Sync for JsValue {}

unsafe impl ErasableGeneric for JsValue {
    type Repr = JsValue;
}

impl Promising for JsValue {
    type Resolution = JsValue;
}

impl JsValue {
    /// The `null` JS value constant.
    pub const NULL: JsValue = JsValue::_new(__rt::JSIDX_NULL);

    /// The `undefined` JS value constant.
    pub const UNDEFINED: JsValue = JsValue::_new(__rt::JSIDX_UNDEFINED);

    /// The `true` JS value constant.
    pub const TRUE: JsValue = JsValue::_new(__rt::JSIDX_TRUE);

    /// The `false` JS value constant.
    pub const FALSE: JsValue = JsValue::_new(__rt::JSIDX_FALSE);

    #[inline]
    const fn _new(idx: u32) -> JsValue {
        JsValue {
            idx,
            _marker: PhantomData,
        }
    }

    /// Creates a new JS value which is a string.
    ///
    /// The utf-8 string provided is copied to the JS heap and the string will
    /// be owned by the JS garbage collector.
    #[allow(clippy::should_implement_trait)] // cannot fix without breaking change
    #[inline]
    pub fn from_str(s: &str) -> JsValue {
        wbg_cast(s)
    }

    /// Creates a new JS value which is a number.
    ///
    /// This function creates a JS value representing a number (a heap
    /// allocated number) and returns a handle to the JS version of it.
    #[inline]
    pub fn from_f64(n: f64) -> JsValue {
        wbg_cast(n)
    }

    /// Creates a new JS value which is a bigint from a string representing a number.
    ///
    /// This function creates a JS value representing a bigint (a heap
    /// allocated large integer) and returns a handle to the JS version of it.
    #[inline]
    pub fn bigint_from_str(s: &str) -> JsValue {
        __wbindgen_bigint_from_str(s)
    }

    /// Creates a new JS value which is a boolean.
    ///
    /// This function creates a JS object representing a boolean (a heap
    /// allocated boolean) and returns a handle to the JS version of it.
    #[inline]
    pub const fn from_bool(b: bool) -> JsValue {
        if b {
            JsValue::TRUE
        } else {
            JsValue::FALSE
        }
    }

    /// Creates a new JS value representing `undefined`.
    #[inline]
    pub const fn undefined() -> JsValue {
        JsValue::UNDEFINED
    }

    /// Creates a new JS value representing `null`.
    #[inline]
    pub const fn null() -> JsValue {
        JsValue::NULL
    }

    /// Creates a new JS symbol with the optional description specified.
    ///
    /// This function will invoke the `Symbol` constructor in JS and return the
    /// JS object corresponding to the symbol created.
    pub fn symbol(description: Option<&str>) -> JsValue {
        __wbindgen_symbol_new(description)
    }

    /// Creates a new `JsValue` from the JSON serialization of the object `t`
    /// provided.
    ///
    /// **This function is deprecated**, due to [creating a dependency cycle in
    /// some circumstances][dep-cycle-issue]. Use [`serde-wasm-bindgen`] or
    /// [`gloo_utils::format::JsValueSerdeExt`] instead.
    ///
    /// [dep-cycle-issue]: https://github.com/wasm-bindgen/wasm-bindgen/issues/2770
    /// [`serde-wasm-bindgen`]: https://docs.rs/serde-wasm-bindgen
    /// [`gloo_utils::format::JsValueSerdeExt`]: https://docs.rs/gloo-utils/latest/gloo_utils/format/trait.JsValueSerdeExt.html
    ///
    /// This function will serialize the provided value `t` to a JSON string,
    /// send the JSON string to JS, parse it into a JS object, and then return
    /// a handle to the JS object. This is unlikely to be super speedy so it's
    /// not recommended for large payloads, but it's a nice to have in some
    /// situations!
    ///
    /// Usage of this API requires activating the `serde-serialize` feature of
    /// the `wasm-bindgen` crate.
    ///
    /// # Errors
    ///
    /// Returns any error encountered when serializing `T` into JSON.
    #[cfg(feature = "serde-serialize")]
    #[deprecated = "causes dependency cycles, use `serde-wasm-bindgen` or `gloo_utils::format::JsValueSerdeExt` instead"]
    pub fn from_serde<T>(t: &T) -> serde_json::Result<JsValue>
    where
        T: serde::ser::Serialize + ?Sized,
    {
        let s = serde_json::to_string(t)?;
        Ok(__wbindgen_json_parse(s))
    }

    /// Invokes `JSON.stringify` on this value and then parses the resulting
    /// JSON into an arbitrary Rust value.
    ///
    /// **This function is deprecated**, due to [creating a dependency cycle in
    /// some circumstances][dep-cycle-issue]. Use [`serde-wasm-bindgen`] or
    /// [`gloo_utils::format::JsValueSerdeExt`] instead.
    ///
    /// [dep-cycle-issue]: https://github.com/wasm-bindgen/wasm-bindgen/issues/2770
    /// [`serde-wasm-bindgen`]: https://docs.rs/serde-wasm-bindgen
    /// [`gloo_utils::format::JsValueSerdeExt`]: https://docs.rs/gloo-utils/latest/gloo_utils/format/trait.JsValueSerdeExt.html
    ///
    /// This function will first call `JSON.stringify` on the `JsValue` itself.
    /// The resulting string is then passed into Rust which then parses it as
    /// JSON into the resulting value.
    ///
    /// Usage of this API requires activating the `serde-serialize` feature of
    /// the `wasm-bindgen` crate.
    ///
    /// # Errors
    ///
    /// Returns any error encountered when parsing the JSON into a `T`.
    #[cfg(feature = "serde-serialize")]
    #[deprecated = "causes dependency cycles, use `serde-wasm-bindgen` or `gloo_utils::format::JsValueSerdeExt` instead"]
    pub fn into_serde<T>(&self) -> serde_json::Result<T>
    where
        T: for<'a> serde::de::Deserialize<'a>,
    {
        let s = __wbindgen_json_serialize(self);
        // Turns out `JSON.stringify(undefined) === undefined`, so if
        // we're passed `undefined` reinterpret it as `null` for JSON
        // purposes.
        serde_json::from_str(s.as_deref().unwrap_or("null"))
    }

    /// Returns the `f64` value of this JS value if it's an instance of a
    /// number.
    ///
    /// If this JS value is not an instance of a number then this returns
    /// `None`.
    #[inline]
    pub fn as_f64(&self) -> Option<f64> {
        __wbindgen_number_get(self)
    }

    /// Tests whether this JS value is a JS string.
    #[inline]
    pub fn is_string(&self) -> bool {
        __wbindgen_is_string(self)
    }

    /// If this JS value is a string value, this function copies the JS string
    /// value into Wasm linear memory, encoded as UTF-8, and returns it as a
    /// Rust `String`.
    ///
    /// To avoid the copying and re-encoding, consider the
    /// `JsString::try_from()` function from [js-sys](https://docs.rs/js-sys)
    /// instead.
    ///
    /// If this JS value is not an instance of a string or if it's not valid
    /// utf-8 then this returns `None`.
    ///
    /// # UTF-16 vs UTF-8
    ///
    /// JavaScript strings in general are encoded as UTF-16, but Rust strings
    /// are encoded as UTF-8. This can cause the Rust string to look a bit
    /// different than the JS string sometimes. For more details see the
    /// [documentation about the `str` type][caveats] which contains a few
    /// caveats about the encodings.
    ///
    /// [caveats]: https://wasm-bindgen.github.io/wasm-bindgen/reference/types/str.html
    #[inline]
    pub fn as_string(&self) -> Option<String> {
        __wbindgen_string_get(self)
    }

    /// Returns the `bool` value of this JS value if it's an instance of a
    /// boolean.
    ///
    /// If this JS value is not an instance of a boolean then this returns
    /// `None`.
    #[inline]
    pub fn as_bool(&self) -> Option<bool> {
        __wbindgen_boolean_get(self)
    }

    /// Tests whether this JS value is `null`
    #[inline]
    pub fn is_null(&self) -> bool {
        __wbindgen_is_null(self)
    }

    /// Tests whether this JS value is `undefined`
    #[inline]
    pub fn is_undefined(&self) -> bool {
        __wbindgen_is_undefined(self)
    }

    /// Tests whether this JS value is `null` or `undefined`
    #[inline]
    pub fn is_null_or_undefined(&self) -> bool {
        __wbindgen_is_null_or_undefined(self)
    }

    /// Tests whether the type of this JS value is `symbol`
    #[inline]
    pub fn is_symbol(&self) -> bool {
        __wbindgen_is_symbol(self)
    }

    /// Tests whether `typeof self == "object" && self !== null`.
    #[inline]
    pub fn is_object(&self) -> bool {
        __wbindgen_is_object(self)
    }

    /// Tests whether this JS value is an instance of Array.
    #[inline]
    pub fn is_array(&self) -> bool {
        __wbindgen_is_array(self)
    }

    /// Tests whether the type of this JS value is `function`.
    #[inline]
    pub fn is_function(&self) -> bool {
        __wbindgen_is_function(self)
    }

    /// Tests whether the type of this JS value is `bigint`.
    #[inline]
    pub fn is_bigint(&self) -> bool {
        __wbindgen_is_bigint(self)
    }

    /// Applies the unary `typeof` JS operator on a `JsValue`.
    ///
    /// [MDN documentation](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Operators/typeof)
    #[inline]
    pub fn js_typeof(&self) -> JsValue {
        __wbindgen_typeof(self)
    }

    /// Applies the binary `in` JS operator on the two `JsValue`s.
    ///
    /// [MDN documentation](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Operators/in)
    #[inline]
    pub fn js_in(&self, obj: &JsValue) -> bool {
        __wbindgen_in(self, obj)
    }

    /// Tests whether the value is ["truthy"].
    ///
    /// ["truthy"]: https://developer.mozilla.org/en-US/docs/Glossary/Truthy
    #[inline]
    pub fn is_truthy(&self) -> bool {
        !self.is_falsy()
    }

    /// Tests whether the value is ["falsy"].
    ///
    /// ["falsy"]: https://developer.mozilla.org/en-US/docs/Glossary/Falsy
    #[inline]
    pub fn is_falsy(&self) -> bool {
        __wbindgen_is_falsy(self)
    }

    /// Get a string representation of the JavaScript object for debugging.
    fn as_debug_string(&self) -> String {
        __wbindgen_debug_string(self)
    }

    /// Compare two `JsValue`s for equality, using the `==` operator in JS.
    ///
    /// [MDN documentation](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Operators/Equality)
    #[inline]
    pub fn loose_eq(&self, other: &Self) -> bool {
        __wbindgen_jsval_loose_eq(self, other)
    }

    /// Applies the unary `~` JS operator on a `JsValue`.
    ///
    /// [MDN documentation](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Operators/Bitwise_NOT)
    #[inline]
    pub fn bit_not(&self) -> JsValue {
        __wbindgen_bit_not(self)
    }

    /// Applies the binary `>>>` JS operator on the two `JsValue`s.
    ///
    /// [MDN documentation](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Operators/Unsigned_right_shift)
    #[inline]
    pub fn unsigned_shr(&self, rhs: &Self) -> u32 {
        __wbindgen_unsigned_shr(self, rhs)
    }

    /// Applies the binary `/` JS operator on two `JsValue`s, catching and returning any `RangeError` thrown.
    ///
    /// [MDN documentation](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Operators/Division)
    #[inline]
    pub fn checked_div(&self, rhs: &Self) -> Self {
        __wbindgen_checked_div(self, rhs)
    }

    /// Applies the binary `**` JS operator on the two `JsValue`s.
    ///
    /// [MDN documentation](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Operators/Exponentiation)
    #[inline]
    pub fn pow(&self, rhs: &Self) -> Self {
        __wbindgen_pow(self, rhs)
    }

    /// Applies the binary `<` JS operator on the two `JsValue`s.
    ///
    /// [MDN documentation](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Operators/Less_than)
    #[inline]
    pub fn lt(&self, other: &Self) -> bool {
        __wbindgen_lt(self, other)
    }

    /// Applies the binary `<=` JS operator on the two `JsValue`s.
    ///
    /// [MDN documentation](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Operators/Less_than_or_equal)
    #[inline]
    pub fn le(&self, other: &Self) -> bool {
        __wbindgen_le(self, other)
    }

    /// Applies the binary `>=` JS operator on the two `JsValue`s.
    ///
    /// [MDN documentation](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Operators/Greater_than_or_equal)
    #[inline]
    pub fn ge(&self, other: &Self) -> bool {
        __wbindgen_ge(self, other)
    }

    /// Applies the binary `>` JS operator on the two `JsValue`s.
    ///
    /// [MDN documentation](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Operators/Greater_than)
    #[inline]
    pub fn gt(&self, other: &Self) -> bool {
        __wbindgen_gt(self, other)
    }

    /// Applies the unary `+` JS operator on a `JsValue`. Can throw.
    ///
    /// [MDN documentation](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Operators/Unary_plus)
    #[inline]
    pub fn unchecked_into_f64(&self) -> f64 {
        // Can't use `wbg_cast` here because it expects that the value already has a correct type
        // and will fail with an assertion error in debug mode.
        __wbindgen_as_number(self)
    }
}

impl PartialEq for JsValue {
    /// Compares two `JsValue`s for equality, using the `===` operator in JS.
    ///
    /// [MDN documentation](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Operators/Strict_equality)
    #[inline]
    fn eq(&self, other: &Self) -> bool {
        __wbindgen_jsval_eq(self, other)
    }
}

impl PartialEq<bool> for JsValue {
    #[inline]
    fn eq(&self, other: &bool) -> bool {
        self.as_bool() == Some(*other)
    }
}

impl PartialEq<str> for JsValue {
    #[inline]
    fn eq(&self, other: &str) -> bool {
        *self == JsValue::from_str(other)
    }
}

impl<'a> PartialEq<&'a str> for JsValue {
    #[inline]
    fn eq(&self, other: &&'a str) -> bool {
        <JsValue as PartialEq<str>>::eq(self, other)
    }
}

impl PartialEq<String> for JsValue {
    #[inline]
    fn eq(&self, other: &String) -> bool {
        <JsValue as PartialEq<str>>::eq(self, other)
    }
}
impl<'a> PartialEq<&'a String> for JsValue {
    #[inline]
    fn eq(&self, other: &&'a String) -> bool {
        <JsValue as PartialEq<str>>::eq(self, other)
    }
}

macro_rules! forward_deref_unop {
    (impl $imp:ident, $method:ident for $t:ty) => {
        impl $imp for $t {
            type Output = <&'static $t as $imp>::Output;

            #[inline]
            fn $method(self) -> <&'static $t as $imp>::Output {
                $imp::$method(&self)
            }
        }
    };
}

macro_rules! forward_deref_binop {
    (impl $imp:ident, $method:ident for $t:ty) => {
        impl<'a> $imp<$t> for &'a $t {
            type Output = <&'static $t as $imp<&'static $t>>::Output;

            #[inline]
            fn $method(self, other: $t) -> <&'static $t as $imp<&'static $t>>::Output {
                $imp::$method(self, &other)
            }
        }

        impl $imp<&$t> for $t {
            type Output = <&'static $t as $imp<&'static $t>>::Output;

            #[inline]
            fn $method(self, other: &$t) -> <&'static $t as $imp<&'static $t>>::Output {
                $imp::$method(&self, other)
            }
        }

        impl $imp<$t> for $t {
            type Output = <&'static $t as $imp<&'static $t>>::Output;

            #[inline]
            fn $method(self, other: $t) -> <&'static $t as $imp<&'static $t>>::Output {
                $imp::$method(&self, &other)
            }
        }
    };
}

impl Not for &JsValue {
    type Output = bool;

    /// Applies the `!` JS operator on a `JsValue`.
    ///
    /// [MDN documentation](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Operators/Logical_NOT)
    #[inline]
    fn not(self) -> Self::Output {
        JsValue::is_falsy(self)
    }
}

forward_deref_unop!(impl Not, not for JsValue);

impl TryFrom<JsValue> for f64 {
    type Error = JsValue;

    /// Applies the unary `+` JS operator on a `JsValue`.
    /// Returns the numeric result on success, or the JS error value on error.
    ///
    /// [MDN documentation](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Operators/Unary_plus)
    #[inline]
    fn try_from(val: JsValue) -> Result<Self, Self::Error> {
        f64::try_from(&val)
    }
}

impl TryFrom<&JsValue> for f64 {
    type Error = JsValue;

    /// Applies the unary `+` JS operator on a `JsValue`.
    /// Returns the numeric result on success, or the JS error value on error.
    ///
    /// [MDN documentation](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Operators/Unary_plus)
    #[inline]
    fn try_from(val: &JsValue) -> Result<Self, Self::Error> {
        let jsval = __wbindgen_try_into_number(val);
        match jsval.as_f64() {
            Some(num) => Ok(num),
            None => Err(jsval),
        }
    }
}

impl Neg for &JsValue {
    type Output = JsValue;

    /// Applies the unary `-` JS operator on a `JsValue`.
    ///
    /// [MDN documentation](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Operators/Unary_negation)
    #[inline]
    fn neg(self) -> Self::Output {
        __wbindgen_neg(self)
    }
}

forward_deref_unop!(impl Neg, neg for JsValue);

impl BitAnd for &JsValue {
    type Output = JsValue;

    /// Applies the binary `&` JS operator on two `JsValue`s.
    ///
    /// [MDN documentation](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Operators/Bitwise_AND)
    #[inline]
    fn bitand(self, rhs: Self) -> Self::Output {
        __wbindgen_bit_and(self, rhs)
    }
}

forward_deref_binop!(impl BitAnd, bitand for JsValue);

impl BitOr for &JsValue {
    type Output = JsValue;

    /// Applies the binary `|` JS operator on two `JsValue`s.
    ///
    /// [MDN documentation](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Operators/Bitwise_OR)
    #[inline]
    fn bitor(self, rhs: Self) -> Self::Output {
        __wbindgen_bit_or(self, rhs)
    }
}

forward_deref_binop!(impl BitOr, bitor for JsValue);

impl BitXor for &JsValue {
    type Output = JsValue;

    /// Applies the binary `^` JS operator on two `JsValue`s.
    ///
    /// [MDN documentation](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Operators/Bitwise_XOR)
    #[inline]
    fn bitxor(self, rhs: Self) -> Self::Output {
        __wbindgen_bit_xor(self, rhs)
    }
}

forward_deref_binop!(impl BitXor, bitxor for JsValue);

impl Shl for &JsValue {
    type Output = JsValue;

    /// Applies the binary `<<` JS operator on two `JsValue`s.
    ///
    /// [MDN documentation](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Operators/Left_shift)
    #[inline]
    fn shl(self, rhs: Self) -> Self::Output {
        __wbindgen_shl(self, rhs)
    }
}

forward_deref_binop!(impl Shl, shl for JsValue);

impl Shr for &JsValue {
    type Output = JsValue;

    /// Applies the binary `>>` JS operator on two `JsValue`s.
    ///
    /// [MDN documentation](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Operators/Right_shift)
    #[inline]
    fn shr(self, rhs: Self) -> Self::Output {
        __wbindgen_shr(self, rhs)
    }
}

forward_deref_binop!(impl Shr, shr for JsValue);

impl Add for &JsValue {
    type Output = JsValue;

    /// Applies the binary `+` JS operator on two `JsValue`s.
    ///
    /// [MDN documentation](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Operators/Addition)
    #[inline]
    fn add(self, rhs: Self) -> Self::Output {
        __wbindgen_add(self, rhs)
    }
}

forward_deref_binop!(impl Add, add for JsValue);

impl Sub for &JsValue {
    type Output = JsValue;

    /// Applies the binary `-` JS operator on two `JsValue`s.
    ///
    /// [MDN documentation](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Operators/Subtraction)
    #[inline]
    fn sub(self, rhs: Self) -> Self::Output {
        __wbindgen_sub(self, rhs)
    }
}

forward_deref_binop!(impl Sub, sub for JsValue);

impl Div for &JsValue {
    type Output = JsValue;

    /// Applies the binary `/` JS operator on two `JsValue`s.
    ///
    /// [MDN documentation](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Operators/Division)
    #[inline]
    fn div(self, rhs: Self) -> Self::Output {
        __wbindgen_div(self, rhs)
    }
}

forward_deref_binop!(impl Div, div for JsValue);

impl Mul for &JsValue {
    type Output = JsValue;

    /// Applies the binary `*` JS operator on two `JsValue`s.
    ///
    /// [MDN documentation](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Operators/Multiplication)
    #[inline]
    fn mul(self, rhs: Self) -> Self::Output {
        __wbindgen_mul(self, rhs)
    }
}

forward_deref_binop!(impl Mul, mul for JsValue);

impl Rem for &JsValue {
    type Output = JsValue;

    /// Applies the binary `%` JS operator on two `JsValue`s.
    ///
    /// [MDN documentation](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Operators/Remainder)
    #[inline]
    fn rem(self, rhs: Self) -> Self::Output {
        __wbindgen_rem(self, rhs)
    }
}

forward_deref_binop!(impl Rem, rem for JsValue);

impl<'a> From<&'a str> for JsValue {
    #[inline]
    fn from(s: &'a str) -> JsValue {
        JsValue::from_str(s)
    }
}

impl<T> From<*mut T> for JsValue {
    #[inline]
    fn from(s: *mut T) -> JsValue {
        JsValue::from(s as usize)
    }
}

impl<T> From<*const T> for JsValue {
    #[inline]
    fn from(s: *const T) -> JsValue {
        JsValue::from(s as usize)
    }
}

impl<T> From<NonNull<T>> for JsValue {
    #[inline]
    fn from(s: NonNull<T>) -> JsValue {
        JsValue::from(s.as_ptr() as usize)
    }
}

impl<'a> From<&'a String> for JsValue {
    #[inline]
    fn from(s: &'a String) -> JsValue {
        JsValue::from_str(s)
    }
}

impl From<String> for JsValue {
    #[inline]
    fn from(s: String) -> JsValue {
        JsValue::from_str(&s)
    }
}

impl TryFrom<JsValue> for String {
    type Error = JsValue;

    fn try_from(value: JsValue) -> Result<Self, Self::Error> {
        match value.as_string() {
            Some(s) => Ok(s),
            None => Err(value),
        }
    }
}

impl TryFromJsValue for String {
    fn try_from_js_value_ref(value: &JsValue) -> Option<Self> {
        value.as_string()
    }
}

impl From<bool> for JsValue {
    #[inline]
    fn from(s: bool) -> JsValue {
        JsValue::from_bool(s)
    }
}

impl TryFromJsValue for bool {
    fn try_from_js_value_ref(value: &JsValue) -> Option<Self> {
        value.as_bool()
    }
}

impl TryFromJsValue for char {
    fn try_from_js_value_ref(value: &JsValue) -> Option<Self> {
        let s = value.as_string()?;
        if s.len() == 1 {
            Some(s.chars().nth(0).unwrap())
        } else {
            None
        }
    }
}

impl<'a, T> From<&'a T> for JsValue
where
    T: JsCast,
{
    #[inline]
    fn from(s: &'a T) -> JsValue {
        s.as_ref().clone()
    }
}

impl<T> From<Option<T>> for JsValue
where
    JsValue: From<T>,
{
    #[inline]
    fn from(s: Option<T>) -> JsValue {
        match s {
            Some(s) => s.into(),
            None => JsValue::undefined(),
        }
    }
}

// everything is a `JsValue`!
impl JsCast for JsValue {
    #[inline]
    fn instanceof(_val: &JsValue) -> bool {
        true
    }
    #[inline]
    fn unchecked_from_js(val: JsValue) -> Self {
        val
    }
    #[inline]
    fn unchecked_from_js_ref(val: &JsValue) -> &Self {
        val
    }
}

impl AsRef<JsValue> for JsValue {
    #[inline]
    fn as_ref(&self) -> &JsValue {
        self
    }
}

impl UpcastFrom<JsValue> for JsValue {}

// Loosely based on toInt32 in ecma-272 for abi semantics
// with restriction that it only applies for numbers
fn to_uint_32(v: &JsValue) -> Option<u32> {
    v.as_f64().map(|n| {
        if n.is_infinite() {
            0
        } else {
            (n as i64) as u32
        }
    })
}

macro_rules! integers {
    ($($n:ident)*) => ($(
        impl PartialEq<$n> for JsValue {
            #[inline]
            fn eq(&self, other: &$n) -> bool {
                self.as_f64() == Some(f64::from(*other))
            }
        }

        impl From<$n> for JsValue {
            #[inline]
            fn from(n: $n) -> JsValue {
                JsValue::from_f64(n.into())
            }
        }

        // Follows semantics of https://www.w3.org/TR/wasm-js-api-2/#towebassemblyvalue
        impl TryFromJsValue for $n {
            #[inline]
            fn try_from_js_value_ref(val: &JsValue) -> Option<$n> {
                to_uint_32(val).map(|n| n as $n)
            }
        }
    )*)
}

integers! { i8 u8 i16 u16 i32 u32 }

macro_rules! floats {
    ($($n:ident)*) => ($(
        impl PartialEq<$n> for JsValue {
            #[inline]
            fn eq(&self, other: &$n) -> bool {
                self.as_f64() == Some(f64::from(*other))
            }
        }

        impl From<$n> for JsValue {
            #[inline]
            fn from(n: $n) -> JsValue {
                JsValue::from_f64(n.into())
            }
        }

        impl TryFromJsValue for $n {
            #[inline]
            fn try_from_js_value_ref(val: &JsValue) -> Option<$n> {
                val.as_f64().map(|n| n as $n)
            }
        }
    )*)
}

floats! { f32 f64 }

macro_rules! big_integers {
    ($($n:ident)*) => ($(
        impl PartialEq<$n> for JsValue {
            #[inline]
            fn eq(&self, other: &$n) -> bool {
                self == &JsValue::from(*other)
            }
        }

        impl From<$n> for JsValue {
            #[inline]
            fn from(arg: $n) -> JsValue {
                wbg_cast(arg)
            }
        }

        impl TryFrom<JsValue> for $n {
            type Error = JsValue;

            #[inline]
            fn try_from(v: JsValue) -> Result<Self, JsValue> {
                Self::try_from_js_value(v)
            }
        }

        impl TryFromJsValue for $n {
            #[inline]
            fn try_from_js_value_ref(val: &JsValue) -> Option<$n> {
                let as_i64 = __wbindgen_bigint_get_as_i64(&val)?;
                // Reinterpret bits; ABI-wise this is safe to do and allows us to avoid
                // having separate intrinsics per signed/unsigned types.
                let as_self = as_i64 as $n;
                // Double-check that we didn't truncate the bigint to 64 bits.
                if val == &as_self {
                    Some(as_self)
                } else {
                    None
                }
            }
        }
    )*)
}

big_integers! { i64 u64 }

macro_rules! num128 {
    ($ty:ty, $hi_ty:ty) => {
        impl PartialEq<$ty> for JsValue {
            #[inline]
            fn eq(&self, other: &$ty) -> bool {
                self == &JsValue::from(*other)
            }
        }

        impl From<$ty> for JsValue {
            #[inline]
            fn from(arg: $ty) -> JsValue {
                wbg_cast(arg)
            }
        }

        impl TryFrom<JsValue> for $ty {
            type Error = JsValue;

            #[inline]
            fn try_from(v: JsValue) -> Result<Self, JsValue> {
                Self::try_from_js_value(v)
            }
        }

        impl TryFromJsValue for $ty {
            // This is a non-standard Wasm bindgen conversion, supported equally
            fn try_from_js_value_ref(v: &JsValue) -> Option<$ty> {
                // Truncate the bigint to 64 bits, this will give us the lower part.
                // The lower part must be interpreted as unsigned in both i128 and u128.
                let lo = __wbindgen_bigint_get_as_i64(&v)? as u64;
                // Now we know it's a bigint, so we can safely use `>> 64n` without
                // worrying about a JS exception on type mismatch.
                let hi = v >> JsValue::from(64_u64);
                // The high part is the one we want checked against a 64-bit range.
                // If it fits, then our original number is in the 128-bit range.
                <$hi_ty>::try_from_js_value_ref(&hi).map(|hi| Self::from(hi) << 64 | Self::from(lo))
            }
        }
    };
}

num128!(i128, i64);

num128!(u128, u64);

impl TryFromJsValue for () {
    fn try_from_js_value_ref(value: &JsValue) -> Option<Self> {
        if value.is_undefined() {
            Some(())
        } else {
            None
        }
    }
}

impl<T: TryFromJsValue> TryFromJsValue for Option<T> {
    fn try_from_js_value_ref(value: &JsValue) -> Option<Self> {
        if value.is_undefined() {
            Some(None)
        } else {
            T::try_from_js_value_ref(value).map(Some)
        }
    }
}

// `usize` and `isize` have to be treated a bit specially, because we know that
// they're 32-bit but the compiler conservatively assumes they might be bigger.
// So, we have to manually forward to the `u32`/`i32` versions.
impl PartialEq<usize> for JsValue {
    #[inline]
    fn eq(&self, other: &usize) -> bool {
        *self == (*other as u32)
    }
}

impl From<usize> for JsValue {
    #[inline]
    fn from(n: usize) -> Self {
        Self::from(n as u32)
    }
}

impl PartialEq<isize> for JsValue {
    #[inline]
    fn eq(&self, other: &isize) -> bool {
        *self == (*other as i32)
    }
}

impl From<isize> for JsValue {
    #[inline]
    fn from(n: isize) -> Self {
        Self::from(n as i32)
    }
}

// Follows semantics of https://www.w3.org/TR/wasm-js-api-2/#towebassemblyvalue
impl TryFromJsValue for isize {
    #[inline]
    fn try_from_js_value_ref(val: &JsValue) -> Option<isize> {
        val.as_f64().map(|n| n as isize)
    }
}

// Follows semantics of https://www.w3.org/TR/wasm-js-api-2/#towebassemblyvalue
impl TryFromJsValue for usize {
    #[inline]
    fn try_from_js_value_ref(val: &JsValue) -> Option<usize> {
        val.as_f64().map(|n| n as usize)
    }
}

// Intrinsics that are simply JS function bindings and can be self-hosted via the macro.
#[wasm_bindgen_macro::wasm_bindgen(wasm_bindgen = crate)]
extern "C" {
    #[wasm_bindgen(js_namespace = Array, js_name = isArray)]
    fn __wbindgen_is_array(v: &JsValue) -> bool;

    #[wasm_bindgen(js_name = BigInt)]
    fn __wbindgen_bigint_from_str(s: &str) -> JsValue;

    #[wasm_bindgen(js_name = Symbol)]
    fn __wbindgen_symbol_new(description: Option<&str>) -> JsValue;

    #[wasm_bindgen(js_name = Error)]
    fn __wbindgen_error_new(msg: &str) -> JsValue;

    #[wasm_bindgen(js_namespace = JSON, js_name = parse)]
    fn __wbindgen_json_parse(json: String) -> JsValue;

    #[wasm_bindgen(js_namespace = JSON, js_name = stringify)]
    fn __wbindgen_json_serialize(v: &JsValue) -> Option<String>;

    #[wasm_bindgen(js_name = Number)]
    fn __wbindgen_as_number(v: &JsValue) -> f64;
}

// Intrinsics which are handled by cli-support but for which we can use
// standard wasm-bindgen ABI conversions.
#[wasm_bindgen_macro::wasm_bindgen(wasm_bindgen = crate, raw_module = "__wbindgen_placeholder__")]
extern "C" {
    #[cfg(not(wbg_reference_types))]
    fn __wbindgen_externref_heap_live_count() -> u32;

    fn __wbindgen_is_null(js: &JsValue) -> bool;
    fn __wbindgen_is_undefined(js: &JsValue) -> bool;
    fn __wbindgen_is_null_or_undefined(js: &JsValue) -> bool;
    fn __wbindgen_is_symbol(js: &JsValue) -> bool;
    fn __wbindgen_is_object(js: &JsValue) -> bool;
    fn __wbindgen_is_function(js: &JsValue) -> bool;
    fn __wbindgen_is_string(js: &JsValue) -> bool;
    fn __wbindgen_is_bigint(js: &JsValue) -> bool;
    fn __wbindgen_typeof(js: &JsValue) -> JsValue;

    fn __wbindgen_in(prop: &JsValue, obj: &JsValue) -> bool;

    fn __wbindgen_is_falsy(js: &JsValue) -> bool;
    fn __wbindgen_try_into_number(js: &JsValue) -> JsValue;
    fn __wbindgen_neg(js: &JsValue) -> JsValue;
    fn __wbindgen_bit_and(a: &JsValue, b: &JsValue) -> JsValue;
    fn __wbindgen_bit_or(a: &JsValue, b: &JsValue) -> JsValue;
    fn __wbindgen_bit_xor(a: &JsValue, b: &JsValue) -> JsValue;
    fn __wbindgen_bit_not(js: &JsValue) -> JsValue;
    fn __wbindgen_shl(a: &JsValue, b: &JsValue) -> JsValue;
    fn __wbindgen_shr(a: &JsValue, b: &JsValue) -> JsValue;
    fn __wbindgen_unsigned_shr(a: &JsValue, b: &JsValue) -> u32;
    fn __wbindgen_add(a: &JsValue, b: &JsValue) -> JsValue;
    fn __wbindgen_sub(a: &JsValue, b: &JsValue) -> JsValue;
    fn __wbindgen_div(a: &JsValue, b: &JsValue) -> JsValue;
    fn __wbindgen_checked_div(a: &JsValue, b: &JsValue) -> JsValue;
    fn __wbindgen_mul(a: &JsValue, b: &JsValue) -> JsValue;
    fn __wbindgen_rem(a: &JsValue, b: &JsValue) -> JsValue;
    fn __wbindgen_pow(a: &JsValue, b: &JsValue) -> JsValue;
    fn __wbindgen_lt(a: &JsValue, b: &JsValue) -> bool;
    fn __wbindgen_le(a: &JsValue, b: &JsValue) -> bool;
    fn __wbindgen_ge(a: &JsValue, b: &JsValue) -> bool;
    fn __wbindgen_gt(a: &JsValue, b: &JsValue) -> bool;

    fn __wbindgen_number_get(js: &JsValue) -> Option<f64>;
    fn __wbindgen_boolean_get(js: &JsValue) -> Option<bool>;
    fn __wbindgen_string_get(js: &JsValue) -> Option<String>;
    fn __wbindgen_bigint_get_as_i64(js: &JsValue) -> Option<i64>;

    fn __wbindgen_debug_string(js: &JsValue) -> String;

    fn __wbindgen_throw(msg: &str) /* -> ! */;
    fn __wbindgen_rethrow(js: JsValue) /* -> ! */;

    fn __wbindgen_jsval_eq(a: &JsValue, b: &JsValue) -> bool;
    fn __wbindgen_jsval_loose_eq(a: &JsValue, b: &JsValue) -> bool;

    fn __wbindgen_copy_to_typed_array(data: &[u8], js: &JsValue);

    fn __wbindgen_init_externref_table();

    fn __wbindgen_exports() -> JsValue;
    fn __wbindgen_memory() -> JsValue;
    fn __wbindgen_module() -> JsValue;
    fn __wbindgen_function_table() -> JsValue;
}

// Intrinsics that have to use raw imports because they're matched by other
// parts of the transform codebase instead of just generating JS.
externs! {
    #[link(wasm_import_module = "__wbindgen_placeholder__")]
    extern "C" {
        fn __wbindgen_object_clone_ref(idx: u32) -> u32;
        fn __wbindgen_object_drop_ref(idx: u32) -> ();

        fn __wbindgen_describe(v: u32) -> ();
        fn __wbindgen_describe_cast(func: *const (), prims: *const ()) -> *const ();
    }
}

impl Clone for JsValue {
    #[inline]
    fn clone(&self) -> JsValue {
        JsValue::_new(unsafe { __wbindgen_object_clone_ref(self.idx) })
    }
}

impl core::fmt::Debug for JsValue {
    fn fmt(&self, f: &mut core::fmt::Formatter) -> core::fmt::Result {
        write!(f, "JsValue({})", self.as_debug_string())
    }
}

impl Drop for JsValue {
    #[inline]
    fn drop(&mut self) {
        unsafe {
            // We definitely should never drop anything in the stack area
            debug_assert!(
                self.idx >= __rt::JSIDX_OFFSET,
                "free of stack slot {}",
                self.idx
            );

            // Otherwise if we're not dropping one of our reserved values,
            // actually call the intrinsic. See #1054 for eventually removing
            // this branch.
            if self.idx >= __rt::JSIDX_RESERVED {
                __wbindgen_object_drop_ref(self.idx);
            }
        }
    }
}

impl Default for JsValue {
    fn default() -> Self {
        Self::UNDEFINED
    }
}

/// Wrapper type for imported statics.
///
/// This type is used whenever a `static` is imported from a JS module, for
/// example this import:
///
/// ```ignore
/// #[wasm_bindgen]
/// extern "C" {
///     static console: JsValue;
/// }
/// ```
///
/// will generate in Rust a value that looks like:
///
/// ```ignore
/// static console: JsStatic<JsValue> = ...;
/// ```
///
/// This type implements `Deref` to the inner type so it's typically used as if
/// it were `&T`.
#[cfg(feature = "std")]
#[deprecated = "use with `#[wasm_bindgen(thread_local_v2)]` instead"]
pub struct JsStatic<T: 'static> {
    #[doc(hidden)]
    pub __inner: &'static std::thread::LocalKey<T>,
}

#[cfg(feature = "std")]
#[allow(deprecated)]
#[cfg(not(target_feature = "atomics"))]
impl<T: crate::convert::FromWasmAbi + 'static> Deref for JsStatic<T> {
    type Target = T;
    fn deref(&self) -> &T {
        unsafe { self.__inner.with(|ptr| &*(ptr as *const T)) }
    }
}

/// Wrapper type for imported statics.
///
/// This type is used whenever a `static` is imported from a JS module, for
/// example this import:
///
/// ```ignore
/// #[wasm_bindgen]
/// extern "C" {
///     #[wasm_bindgen(thread_local_v2)]
///     static console: JsValue;
/// }
/// ```
///
/// will generate in Rust a value that looks like:
///
/// ```ignore
/// static console: JsThreadLocal<JsValue> = ...;
/// ```
pub struct JsThreadLocal<T: 'static> {
    #[doc(hidden)]
    #[cfg(not(target_feature = "atomics"))]
    pub __inner: &'static __rt::LazyCell<T>,
    #[doc(hidden)]
    #[cfg(target_feature = "atomics")]
    pub __inner: fn() -> *const T,
}

impl<T> JsThreadLocal<T> {
    pub fn with<F, R>(&'static self, f: F) -> R
    where
        F: FnOnce(&T) -> R,
    {
        #[cfg(not(target_feature = "atomics"))]
        return f(self.__inner);
        #[cfg(target_feature = "atomics")]
        f(unsafe { &*(self.__inner)() })
    }
}

#[cold]
#[inline(never)]
#[deprecated(note = "renamed to `throw_str`")]
#[doc(hidden)]
pub fn throw(s: &str) -> ! {
    throw_str(s)
}

/// Throws a JS exception.
///
/// This function will throw a JS exception with the message provided. The
/// function will not return as the Wasm stack will be popped when the exception
/// is thrown.
///
/// Note that it is very easy to leak memory with this function because this
/// function, unlike `panic!` on other platforms, **will not run destructors**.
/// It's recommended to return a `Result` where possible to avoid the worry of
/// leaks.
///
/// If you need destructors to run, consider using `panic!` when building with
/// `-Cpanic=unwind`. If the `std` feature is used panics will be caught at the
/// JavaScript boundary and converted to JavaScript exceptions.
#[cold]
#[inline(never)]
pub fn throw_str(s: &str) -> ! {
    __wbindgen_throw(s);
    unsafe { core::hint::unreachable_unchecked() }
}

/// Rethrow a JS exception
///
/// This function will throw a JS exception with the JS value provided. This
/// function will not return and the Wasm stack will be popped until the point
/// of entry of Wasm itself.
///
/// Note that it is very easy to leak memory with this function because this
/// function, unlike `panic!` on other platforms, **will not run destructors**.
/// It's recommended to return a `Result` where possible to avoid the worry of
/// leaks.
///
/// If you need destructors to run, consider using `panic!` when building with
/// `-Cpanic=unwind`. If the `std` feature is used panics will be caught at the
/// JavaScript boundary and converted to JavaScript exceptions.
#[cold]
#[inline(never)]
pub fn throw_val(s: JsValue) -> ! {
    __wbindgen_rethrow(s);
    unsafe { core::hint::unreachable_unchecked() }
}

/// Get the count of live `externref`s / `JsValue`s in `wasm-bindgen`'s heap.
///
/// ## Usage
///
/// This is intended for debugging and writing tests.
///
/// To write a test that asserts against unnecessarily keeping `anref`s /
/// `JsValue`s alive:
///
/// * get an initial live count,
///
/// * perform some series of operations or function calls that should clean up
///   after themselves, and should not keep holding onto `externref`s / `JsValue`s
///   after completion,
///
/// * get the final live count,
///
/// * and assert that the initial and final counts are the same.
///
/// ## What is Counted
///
/// Note that this only counts the *owned* `externref`s / `JsValue`s that end up in
/// `wasm-bindgen`'s heap. It does not count borrowed `externref`s / `JsValue`s
/// that are on its stack.
///
/// For example, these `JsValue`s are accounted for:
///
/// ```ignore
/// #[wasm_bindgen]
/// pub fn my_function(this_is_counted: JsValue) {
///     let also_counted = JsValue::from_str("hi");
///     assert!(wasm_bindgen::externref_heap_live_count() >= 2);
/// }
/// ```
///
/// While this borrowed `JsValue` ends up on the stack, not the heap, and
/// therefore is not accounted for:
///
/// ```ignore
/// #[wasm_bindgen]
/// pub fn my_other_function(this_is_not_counted: &JsValue) {
///     // ...
/// }
/// ```
pub fn externref_heap_live_count() -> u32 {
    __wbindgen_externref_heap_live_count()
}

#[doc(hidden)]
pub fn anyref_heap_live_count() -> u32 {
    externref_heap_live_count()
}

/// An extension trait for `Option<T>` and `Result<T, E>` for unwrapping the `T`
/// value, or throwing a JS error if it is not available.
///
/// These methods should have a smaller code size footprint than the normal
/// `Option::unwrap` and `Option::expect` methods, but they are specific to
/// working with Wasm and JS.
///
/// On non-wasm32 targets, defaults to the normal unwrap/expect calls.
///
/// # Example
///
/// ```
/// use wasm_bindgen::prelude::*;
///
/// // If the value is `Option::Some` or `Result::Ok`, then we just get the
/// // contained `T` value.
/// let x = Some(42);
/// assert_eq!(x.unwrap_throw(), 42);
///
/// let y: Option<i32> = None;
///
/// // This call would throw an error to JS!
/// //
/// //     y.unwrap_throw()
/// //
/// // And this call would throw an error to JS with a custom error message!
/// //
/// //     y.expect_throw("woopsie daisy!")
/// ```
pub trait UnwrapThrowExt<T>: Sized {
    /// Unwrap this `Option` or `Result`, but instead of panicking on failure,
    /// throw an exception to JavaScript.
    #[cfg_attr(
        any(
            debug_assertions,
            not(all(target_arch = "wasm32", any(target_os = "unknown", target_os = "none")))
        ),
        track_caller
    )]
    fn unwrap_throw(self) -> T {
        if cfg!(all(
            debug_assertions,
            all(
                target_arch = "wasm32",
                any(target_os = "unknown", target_os = "none")
            )
        )) {
            let loc = core::panic::Location::caller();
            let msg = alloc::format!(
                "called `{}::unwrap_throw()` ({}:{}:{})",
                core::any::type_name::<Self>(),
                loc.file(),
                loc.line(),
                loc.column()
            );
            self.expect_throw(&msg)
        } else {
            self.expect_throw("called `unwrap_throw()`")
        }
    }

    /// Unwrap this container's `T` value, or throw an error to JS with the
    /// given message if the `T` value is unavailable (e.g. an `Option<T>` is
    /// `None`).
    #[cfg_attr(
        any(
            debug_assertions,
            not(all(target_arch = "wasm32", any(target_os = "unknown", target_os = "none")))
        ),
        track_caller
    )]
    fn expect_throw(self, message: &str) -> T;
}

impl<T> UnwrapThrowExt<T> for Option<T> {
    fn unwrap_throw(self) -> T {
        const MSG: &str = "called `Option::unwrap_throw()` on a `None` value";

        if cfg!(all(
            target_arch = "wasm32",
            any(target_os = "unknown", target_os = "none")
        )) {
            if let Some(val) = self {
                val
            } else if cfg!(debug_assertions) {
                let loc = core::panic::Location::caller();
                let msg = alloc::format!("{MSG} ({}:{}:{})", loc.file(), loc.line(), loc.column(),);

                throw_str(&msg)
            } else {
                throw_str(MSG)
            }
        } else {
            self.expect(MSG)
        }
    }

    fn expect_throw(self, message: &str) -> T {
        if cfg!(all(
            target_arch = "wasm32",
            any(target_os = "unknown", target_os = "none")
        )) {
            if let Some(val) = self {
                val
            } else if cfg!(debug_assertions) {
                let loc = core::panic::Location::caller();
                let msg =
                    alloc::format!("{message} ({}:{}:{})", loc.file(), loc.line(), loc.column(),);

                throw_str(&msg)
            } else {
                throw_str(message)
            }
        } else {
            self.expect(message)
        }
    }
}

impl<T, E> UnwrapThrowExt<T> for Result<T, E>
where
    E: core::fmt::Debug,
{
    fn unwrap_throw(self) -> T {
        const MSG: &str = "called `Result::unwrap_throw()` on an `Err` value";

        if cfg!(all(
            target_arch = "wasm32",
            any(target_os = "unknown", target_os = "none")
        )) {
            match self {
                Ok(val) => val,
                Err(err) => {
                    if cfg!(debug_assertions) {
                        let loc = core::panic::Location::caller();
                        let msg = alloc::format!(
                            "{MSG} ({}:{}:{}): {err:?}",
                            loc.file(),
                            loc.line(),
                            loc.column(),
                        );

                        throw_str(&msg)
                    } else {
                        throw_str(MSG)
                    }
                }
            }
        } else {
            self.expect(MSG)
        }
    }

    fn expect_throw(self, message: &str) -> T {
        if cfg!(all(
            target_arch = "wasm32",
            any(target_os = "unknown", target_os = "none")
        )) {
            match self {
                Ok(val) => val,
                Err(err) => {
                    if cfg!(debug_assertions) {
                        let loc = core::panic::Location::caller();
                        let msg = alloc::format!(
                            "{message} ({}:{}:{}): {err:?}",
                            loc.file(),
                            loc.line(),
                            loc.column(),
                        );

                        throw_str(&msg)
                    } else {
                        throw_str(message)
                    }
                }
            }
        } else {
            self.expect(message)
        }
    }
}

/// Returns a handle to this Wasm instance's `WebAssembly.Module`.
/// This is only available when the final Wasm app is built with
/// `--target no-modules`, `--target web`, `--target deno` or `--target nodejs`.
/// It is unavailable for `--target bundler`.
pub fn module() -> JsValue {
    __wbindgen_module()
}

/// Returns a handle to this Wasm instance's `WebAssembly.Instance.prototype.exports`
pub fn exports() -> JsValue {
    __wbindgen_exports()
}

/// Returns a handle to this Wasm instance's `WebAssembly.Memory`
pub fn memory() -> JsValue {
    __wbindgen_memory()
}

/// Returns a handle to this Wasm instance's `WebAssembly.Table` which is the
/// indirect function table used by Rust
pub fn function_table() -> JsValue {
    __wbindgen_function_table()
}

/// A wrapper type around slices and vectors for binding the `Uint8ClampedArray`
/// array in JS.
///
/// If you need to invoke a JS API which must take `Uint8ClampedArray` array,
/// then you can define it as taking one of these types:
///
/// * `Clamped<&[u8]>`
/// * `Clamped<&mut [u8]>`
/// * `Clamped<Vec<u8>>`
///
/// All of these types will show up as `Uint8ClampedArray` in JS and will have
/// different forms of ownership in Rust.
#[derive(Copy, Clone, PartialEq, Debug, Eq)]
pub struct Clamped<T>(pub T);

impl<T> Deref for Clamped<T> {
    type Target = T;

    fn deref(&self) -> &T {
        &self.0
    }
}

impl<T> DerefMut for Clamped<T> {
    fn deref_mut(&mut self) -> &mut T {
        &mut self.0
    }
}

/// Convenience type for use on exported `fn() -> Result<T, JsError>` functions, where you wish to
/// throw a JavaScript `Error` object.
///
/// You can get wasm_bindgen to throw basic errors by simply returning
/// `Err(JsError::new("message"))` from such a function.
///
/// For more complex error handling, `JsError` implements `From<T> where T: std::error::Error` by
/// converting it to a string, so you can use it with `?`. Many Rust error types already do this,
/// and you can use [`thiserror`](https://crates.io/crates/thiserror) to derive Display
/// implementations easily or use any number of boxed error types that implement it already.
///
///
/// To allow JavaScript code to catch only your errors, you may wish to add a subclass of `Error`
/// in a JS module, and then implement `Into<JsValue>` directly on a type and instantiate that
/// subclass. In that case, you would not need `JsError` at all.
///
/// ### Basic example
///
/// ```rust,no_run
/// use wasm_bindgen::prelude::*;
///
/// #[wasm_bindgen]
/// pub fn throwing_function() -> Result<(), JsError> {
///     Err(JsError::new("message"))
/// }
/// ```
///
/// ### Complex Example
///
/// ```rust,no_run
/// use wasm_bindgen::prelude::*;
///
/// #[derive(Debug, Clone)]
/// enum MyErrorType {
///     SomeError,
/// }
///
/// use core::fmt;
/// impl std::error::Error for MyErrorType {}
/// impl fmt::Display for MyErrorType {
///     fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
///         write!(f, "display implementation becomes the error message")
///     }
/// }
///
/// fn internal_api() -> Result<(), MyErrorType> {
///     Err(MyErrorType::SomeError)
/// }
///
/// #[wasm_bindgen]
/// pub fn throwing_function() -> Result<(), JsError> {
///     internal_api()?;
///     Ok(())
/// }
///
/// ```
#[derive(Clone, Debug)]
pub struct JsError {
    value: JsValue,
}

impl JsError {
    /// Construct a JavaScript `Error` object with a string message
    #[inline]
    pub fn new(s: &str) -> JsError {
        Self {
            value: __wbindgen_error_new(s),
        }
    }
}

#[cfg(feature = "std")]
impl<E> From<E> for JsError
where
    E: std::error::Error,
{
    fn from(error: E) -> Self {
        use std::string::ToString;

        JsError::new(&error.to_string())
    }
}

impl From<JsError> for JsValue {
    fn from(error: JsError) -> Self {
        error.value
    }
}

impl<T: VectorIntoWasmAbi> From<Box<[T]>> for JsValue {
    fn from(vector: Box<[T]>) -> Self {
        wbg_cast(vector)
    }
}

impl<T: VectorIntoWasmAbi> From<Clamped<Box<[T]>>> for JsValue {
    fn from(vector: Clamped<Box<[T]>>) -> Self {
        wbg_cast(vector)
    }
}

impl<T: VectorIntoWasmAbi> From<Vec<T>> for JsValue {
    fn from(vector: Vec<T>) -> Self {
        JsValue::from(vector.into_boxed_slice())
    }
}

impl<T: VectorIntoWasmAbi> From<Clamped<Vec<T>>> for JsValue {
    fn from(vector: Clamped<Vec<T>>) -> Self {
        JsValue::from(Clamped(vector.0.into_boxed_slice()))
    }
}
