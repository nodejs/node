//! JavaScript system types that are re-exported by `js-sys`.
//!
//! These types represent fundamental JavaScript values and are designed to be
//! used as generic type parameters in typed JavaScript collections and APIs.

use crate::convert::UpcastFrom;
use crate::JsCast;
use crate::JsGeneric;
use crate::JsValue;
use core::fmt;
use core::mem::ManuallyDrop;
use core::ops::Deref;
use wasm_bindgen_macro::wasm_bindgen;

/// Marker trait for types which represent `Resolution` or `Promise<Resolution>`.
///
/// For all types except for `Promise`, `Resolution` is equal to the type itself.
/// For `Promise` or any thenable or type extending Promise, `Resolution` is the
/// type of the promise resolution.
///
/// Manually implementing this trait is only required for custom thenables or
/// types which extend Promise. To disable automatic implementation, use the
/// `#[wasm_bindgen(no_promising)]` attribute.
pub trait Promising {
    /// The type that this value resolves to.
    type Resolution;
}

// Undefined
#[wasm_bindgen(wasm_bindgen = crate)]
extern "C" {
    /// The JavaScript `undefined` value.
    ///
    /// This type represents the JavaScript `undefined` primitive value and can be
    /// used as a generic type parameter to indicate that a value is `undefined`.
    #[wasm_bindgen(is_type_of = JsValue::is_undefined, typescript_type = "undefined", no_upcast)]
    #[derive(Clone, PartialEq)]
    pub type Undefined;
}

impl Undefined {
    /// The undefined constant.
    pub const UNDEFINED: Undefined = Self {
        obj: JsValue::UNDEFINED,
    };
}

impl Eq for Undefined {}

impl Default for Undefined {
    fn default() -> Self {
        Self::UNDEFINED
    }
}

impl fmt::Debug for Undefined {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.write_str("undefined")
    }
}

impl fmt::Display for Undefined {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.write_str("undefined")
    }
}

impl UpcastFrom<Undefined> for Undefined {}
impl UpcastFrom<()> for Undefined {}
impl UpcastFrom<Undefined> for () {}
impl UpcastFrom<Undefined> for JsValue {}

// Null
#[wasm_bindgen(wasm_bindgen = crate)]
extern "C" {
    /// The JavaScript `null` value.
    ///
    /// This type represents the JavaScript `null` primitive value and can be
    /// used as a generic type parameter to indicate that a value is `null`.
    #[wasm_bindgen(is_type_of = JsValue::is_null, typescript_type = "null", no_upcast)]
    #[derive(Clone, PartialEq)]
    pub type Null;
}

impl Null {
    /// The null constant.
    pub const NULL: Null = Self { obj: JsValue::NULL };
}

impl Eq for Null {}

impl Default for Null {
    fn default() -> Self {
        Self::NULL
    }
}

impl fmt::Debug for Null {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.write_str("null")
    }
}

impl fmt::Display for Null {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.write_str("null")
    }
}

impl UpcastFrom<Null> for Null {}
impl UpcastFrom<Null> for JsValue {}

// JsOption
#[wasm_bindgen(wasm_bindgen = crate)]
extern "C" {
    /// A nullable JS value of type `T`.
    ///
    /// Unlike `Option<T>`, which is a Rust-side construct, `JsOption<T>` represents
    /// a JS value that may be `T`, `null`, or `undefined`, where the null status is
    /// not yet known in Rust. The value remains in JS until inspected via methods
    /// like [`is_empty`](Self::is_empty), [`as_option`](Self::as_option), or
    /// [`into_option`](Self::into_option).
    ///
    /// `T` must implement [`JsGeneric`], meaning it is any type that can be
    /// represented as a `JsValue` (e.g., `JsString`, `Number`, `Object`, etc.).
    /// `JsOption<T>` itself implements `JsGeneric`, so it can be used in all
    /// generic positions that accept JS types.
    #[wasm_bindgen(typescript_type = "any", no_upcast)]
    #[derive(Clone, PartialEq)]
    pub type JsOption<T>;
}

impl<T> JsOption<T> {
    /// Creates an empty `JsOption<T>` representing `null`.
    #[inline]
    pub fn new() -> Self {
        Null::NULL.unchecked_into()
    }

    /// Wraps a value in a `JsOption<T>`.
    #[inline]
    pub fn wrap(val: T) -> Self {
        unsafe { core::mem::transmute_copy(&ManuallyDrop::new(val)) }
    }

    /// Creates a `JsOption<T>` from an `Option<T>`.
    ///
    /// Returns `JsOption::wrap(val)` if `Some(val)`, otherwise `JsOption::new()`.
    #[inline]
    pub fn from_option(opt: Option<T>) -> Self {
        match opt {
            Some(val) => Self::wrap(val),
            None => Self::new(),
        }
    }

    /// Tests whether this `JsOption<T>` is empty (`null` or `undefined`).
    #[inline]
    pub fn is_empty(&self) -> bool {
        JsValue::is_null_or_undefined(self)
    }

    /// Converts this `JsOption<T>` to an `Option<T>` by cloning the inner value.
    ///
    /// Returns `None` if the value is `null` or `undefined`, otherwise returns
    /// `Some(T)` with a clone of the contained value.
    #[inline]
    pub fn as_option(&self) -> Option<T> {
        if JsValue::is_null_or_undefined(self) {
            None
        } else {
            let cloned = self.deref().clone();
            Some(unsafe { core::mem::transmute_copy(&ManuallyDrop::new(cloned)) })
        }
    }

    /// Converts this `JsOption<T>` into an `Option<T>`, consuming `self`.
    ///
    /// Returns `None` if the value is `null` or `undefined`, otherwise returns
    /// `Some(T)` with the contained value.
    #[inline]
    pub fn into_option(self) -> Option<T> {
        if JsValue::is_null_or_undefined(&self) {
            None
        } else {
            Some(unsafe { core::mem::transmute_copy(&ManuallyDrop::new(self)) })
        }
    }

    /// Returns the contained value, consuming `self`.
    ///
    /// # Panics
    ///
    /// Panics if the value is `null` or `undefined`.
    #[inline]
    pub fn unwrap(self) -> T {
        self.expect("called `JsOption::unwrap()` on an empty value")
    }

    /// Returns the contained value, consuming `self`.
    ///
    /// # Panics
    ///
    /// Panics if the value is `null` or `undefined`, with a panic message
    /// including the passed message.
    #[inline]
    pub fn expect(self, msg: &str) -> T {
        match self.into_option() {
            Some(val) => val,
            None => panic!("{}", msg),
        }
    }

    /// Returns the contained value or a default.
    ///
    /// Returns the contained value if not `null` or `undefined`, otherwise
    /// returns the default value of `T`.
    #[inline]
    pub fn unwrap_or_default(self) -> T
    where
        T: Default,
    {
        self.into_option().unwrap_or_default()
    }

    /// Returns the contained value or computes it from a closure.
    ///
    /// Returns the contained value if not `null` or `undefined`, otherwise
    /// calls `f` and returns the result.
    #[inline]
    pub fn unwrap_or_else<F>(self, f: F) -> T
    where
        F: FnOnce() -> T,
    {
        self.into_option().unwrap_or_else(f)
    }
}

impl<T: JsGeneric> Default for JsOption<T> {
    fn default() -> Self {
        Self::new()
    }
}

impl<T: JsGeneric + fmt::Debug> fmt::Debug for JsOption<T> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{}?(", core::any::type_name::<T>())?;
        match self.as_option() {
            Some(v) => write!(f, "{v:?}")?,
            None => f.write_str("null")?,
        }
        f.write_str(")")
    }
}

impl<T: JsGeneric + fmt::Display> fmt::Display for JsOption<T> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{}?(", core::any::type_name::<T>())?;
        match self.as_option() {
            Some(v) => write!(f, "{v}")?,
            None => f.write_str("null")?,
        }
        f.write_str(")")
    }
}

impl UpcastFrom<JsValue> for JsOption<JsValue> {}
impl<T> UpcastFrom<Undefined> for JsOption<T> {}
impl<T> UpcastFrom<Null> for JsOption<T> {}
impl<T> UpcastFrom<()> for JsOption<T> {}
impl<T> UpcastFrom<JsOption<T>> for JsValue {}
impl<T, U> UpcastFrom<JsOption<U>> for JsOption<T> where T: UpcastFrom<U> {}
