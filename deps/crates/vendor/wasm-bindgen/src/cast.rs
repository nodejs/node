use crate::{convert::TryFromJsValue, JsValue};

/// A trait for dynamic checked and unchecked casting between JS types.
///
/// Unlike [`crate::Upcast`], which provides type-safe zero-cost type
/// conversions for generic type wrappers, this trait can be used to
/// perform arbitrary casts with JS instance checking.
///
/// Specified [in an RFC][rfc] this trait is intended to provide support for
/// casting JS values between different types of one another. In JS there aren't
/// many static types but we've ascribed JS values with static types in Rust,
/// yet they often need to be switched to other types temporarily! This trait
/// provides both checked and unchecked casting into various kinds of values.
///
/// This trait is automatically implemented for any type imported in a
/// `#[wasm_bindgen]` `extern` block.
///
/// [rfc]: https://github.com/rustwasm/rfcs/blob/master/text/002-wasm-bindgen-inheritance-casting.md
pub trait JsCast
where
    Self: AsRef<JsValue> + Into<JsValue>,
{
    /// Test whether this JS value has a type `T`.
    ///
    /// This method will dynamically check to see if this JS object can be
    /// casted to the JS object of type `T`. Usually this uses the `instanceof`
    /// operator. This also works with primitive types like
    /// booleans/strings/numbers as well as cross-realm object like `Array`
    /// which can originate from other iframes.
    ///
    /// In general this is intended to be a more robust version of
    /// `is_instance_of`, but if you want strictly the `instanceof` operator
    /// it's recommended to use that instead.
    fn has_type<T>(&self) -> bool
    where
        T: JsCast,
    {
        T::is_type_of(self.as_ref())
    }

    /// Performs a dynamic cast (checked at runtime) of this value into the
    /// target type `T`.
    ///
    /// This method will return `Err(self)` if `self.has_type::<T>()`
    /// returns `false`, and otherwise it will return `Ok(T)` manufactured with
    /// an unchecked cast (verified correct via the `has_type` operation).
    fn dyn_into<T>(self) -> Result<T, Self>
    where
        T: JsCast,
    {
        if self.has_type::<T>() {
            Ok(self.unchecked_into())
        } else {
            Err(self)
        }
    }

    /// Performs a dynamic cast (checked at runtime) of this value into the
    /// target type `T`.
    ///
    /// This method will return `None` if `self.has_type::<T>()`
    /// returns `false`, and otherwise it will return `Some(&T)` manufactured
    /// with an unchecked cast (verified correct via the `has_type` operation).
    fn dyn_ref<T>(&self) -> Option<&T>
    where
        T: JsCast,
    {
        if self.has_type::<T>() {
            Some(self.unchecked_ref())
        } else {
            None
        }
    }

    /// Performs a zero-cost unchecked cast into the specified type.
    ///
    /// This method will convert the `self` value to the type `T`, where both
    /// `self` and `T` are simple wrappers around `JsValue`. This method **does
    /// not check whether `self` is an instance of `T`**. If used incorrectly
    /// then this method may cause runtime exceptions in both Rust and JS, this
    /// should be used with caution.
    fn unchecked_into<T>(self) -> T
    where
        T: JsCast,
    {
        T::unchecked_from_js(self.into())
    }

    /// Performs a zero-cost unchecked cast into a reference to the specified
    /// type.
    ///
    /// This method will convert the `self` value to the type `T`, where both
    /// `self` and `T` are simple wrappers around `JsValue`. This method **does
    /// not check whether `self` is an instance of `T`**. If used incorrectly
    /// then this method may cause runtime exceptions in both Rust and JS, this
    /// should be used with caution.
    ///
    /// This method, unlike `unchecked_into`, does not consume ownership of
    /// `self` and instead works over a shared reference.
    fn unchecked_ref<T>(&self) -> &T
    where
        T: JsCast,
    {
        T::unchecked_from_js_ref(self.as_ref())
    }

    /// Test whether this JS value is an instance of the type `T`.
    ///
    /// This method performs a dynamic check (at runtime) using the JS
    /// `instanceof` operator. This method returns `self instanceof T`.
    ///
    /// Note that `instanceof` does not always work with primitive values or
    /// across different realms (e.g. iframes). If you're not sure whether you
    /// specifically need only `instanceof` it's recommended to use `has_type`
    /// instead.
    fn is_instance_of<T>(&self) -> bool
    where
        T: JsCast,
    {
        T::instanceof(self.as_ref())
    }

    /// Performs a dynamic `instanceof` check to see whether the `JsValue`
    /// provided is an instance of this type.
    ///
    /// This is intended to be an internal implementation detail, you likely
    /// won't need to call this. It's generally called through the
    /// `is_instance_of` method instead.
    fn instanceof(val: &JsValue) -> bool;

    /// Performs a dynamic check to see whether the `JsValue` provided
    /// is a value of this type.
    ///
    /// Unlike `instanceof`, this can be specialised to use a custom check by
    /// adding a `#[wasm_bindgen(is_type_of = callback)]` attribute to the
    /// type import declaration.
    ///
    /// Other than that, this is intended to be an internal implementation
    /// detail of `has_type` and you likely won't need to call this.
    fn is_type_of(val: &JsValue) -> bool {
        Self::instanceof(val)
    }

    /// Performs a zero-cost unchecked conversion from a `JsValue` into an
    /// instance of `Self`
    ///
    /// This is intended to be an internal implementation detail, you likely
    /// won't need to call this.
    fn unchecked_from_js(val: JsValue) -> Self;

    /// Performs a zero-cost unchecked conversion from a `&JsValue` into an
    /// instance of `&Self`.
    ///
    /// Note the safety of this method, which basically means that `Self` must
    /// be a newtype wrapper around `JsValue`.
    ///
    /// This is intended to be an internal implementation detail, you likely
    /// won't need to call this.
    fn unchecked_from_js_ref(val: &JsValue) -> &Self;
}

impl<T: JsCast> TryFromJsValue for T {
    #[inline]
    fn try_from_js_value(val: JsValue) -> Result<Self, JsValue> {
        val.dyn_into()
    }

    #[inline]
    fn try_from_js_value_ref(val: &JsValue) -> Option<Self> {
        val.clone().dyn_into().ok()
    }
}
