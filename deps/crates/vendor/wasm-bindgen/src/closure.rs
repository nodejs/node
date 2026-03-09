//! Support for closures in `wasm-bindgen`
//!
//! This module defines the [`ScopedClosure`] type which is used to pass Rust closures
//! to JavaScript. All closures are unwind safe by default: panics are caught and converted to
//! JavaScript exceptions when built with `panic=unwind`.
//!
//! # Immutable by Default
//!
//! The closure API defaults to **immutable** (`Fn`) closures because they are more
//! likely to satisfy `UnwindSafe` automatically:
//!
//! - `&T` where `T: RefUnwindSafe` is `UnwindSafe`
//! - `&mut T` is **never** `UnwindSafe` regardless of `T`
//!
//! This means:
//! - [`ScopedClosure::borrow`] creates an immutable `Fn` closure
//! - [`ScopedClosure::borrow_mut`] creates a mutable `FnMut` closure
//!
//! Immutable closures can be upcasted to mutable closures using [`upcast_ref`](crate::Upcast::upcast_ref).
//!
//! # Type Aliases
//!
//! - [`ScopedClosure<'a, T>`] — The unified closure type with a lifetime parameter
//! - [`Closure<T>`] — Alias for `ScopedClosure<'static, T>` (for backwards compatibility)
//!
//! # Unwind Safety
//!
//! For immediate/synchronous callbacks, use `&dyn FnMut` / `&dyn Fn`, when you are
//! **absolutely sure** the code will support unwind safety.
//!
//! For [`ScopedClosure`], the default constructors (`borrow`, `borrow_mut`, `own`) catch
//! panics, while the `_aborting` variants (`borrow_aborting`, `borrow_mut_aborting`, etc.) do not.
//!
//! # Ownership Model
//!
//! `ScopedClosure` follows the same ownership model as other wasm-bindgen types:
//! the JavaScript reference remains valid until the Rust value is dropped. When
//! dropped, the closure is invalidated and any subsequent calls from JavaScript
//! will throw an exception.
//!
//! For borrowed closures created with `borrow`/`borrow_mut`, Rust's borrow checker
//! ensures the `ScopedClosure` cannot outlive the closure's captured data.
//!
//! See the [`ScopedClosure`] type documentation for detailed examples.

#![allow(clippy::fn_to_numeric_cast)]
use alloc::boxed::Box;
use alloc::string::String;
use core::fmt;
use core::mem::{self, ManuallyDrop};

use crate::__rt::marker::ErasableGeneric;
use crate::__rt::marker::MaybeUnwindSafe;
use crate::describe::*;
use crate::JsValue;
use crate::{convert::*, JsCast};
use core::marker::PhantomData;
use core::panic::AssertUnwindSafe;

#[wasm_bindgen_macro::wasm_bindgen(wasm_bindgen = crate)]
extern "C" {
    type JsClosure;

    #[wasm_bindgen(method)]
    fn _wbg_cb_unref(js: &JsClosure);
}

/// A closure with a lifetime parameter that represents a Rust closure passed to JavaScript.
///
/// `ScopedClosure<'a, T>` is the unified closure type. The lifetime `'a` indicates
/// how long the closure is valid:
///
/// - **`ScopedClosure<'static, T>`** - An owned closure with heap-allocated data. Requires
///   `'static` captures. Use for long-lived closures like event listeners and timers.
///   Created with [`Closure::new`] or [`ScopedClosure::own`]. May transfer ownership to the
///   JS GC using finalizers.
///
/// - **`ScopedClosure<'a, T>`** (non-`'static`) - A borrowed closure referencing stack data.
///   Allows non-`'static` captures. Use for immediate/synchronous callbacks. Created with
///   [`ScopedClosure::borrow`] (for `FnMut`) or [`ScopedClosure::borrow_immutable`] (for `Fn`).
///   Cannot transfer ownership to JS GC.
///
/// [`Closure<T>`] is currently a type alias for `ScopedClosure<'static, T>`. In
/// a future release, a lifetime argument will be added to this alias.
///
/// # Ownership Model
///
/// `ScopedClosure` follows the same ownership model as other wasm-bindgen types:
/// the JavaScript reference remains valid until the Rust value is dropped. When
/// dropped, the closure is invalidated and any subsequent calls from JavaScript
/// will throw: "closure invoked recursively or after being dropped".
///
/// For `'static` closures, you can also:
/// - Pass by value to transfer ownership to JS (implements [`IntoWasmAbi`])
/// - Call [`forget()`](Self::forget) to leak the closure (JS can use it indefinitely)
/// - Call [`into_js_value()`](Self::into_js_value) to transfer to JS GC management
///
/// # Lifetime Safety
///
/// For borrowed closures, Rust's borrow checker ensures that `ScopedClosure` cannot
/// be held longer than the closure's captured data:
///
/// ```ignore
/// let mut sum = 0;
/// let mut f = |x: u32| { sum += x; };  // f borrows sum
/// let closure = ScopedClosure::borrow(&mut f);  // closure borrows f
/// // closure cannot outlive f, and f cannot outlive sum
/// ```
///
/// # Examples
///
/// ## Borrowed closures with `ScopedClosure::borrow`
///
/// Use for immediate/synchronous callbacks where JS calls the closure right away:
///
/// ```ignore
/// use wasm_bindgen::prelude::*;
///
/// #[wasm_bindgen]
/// extern "C" {
///     fn call_immediately(cb: &ScopedClosure<dyn FnMut(u32)>);
/// }
///
/// let mut sum = 0;
/// {
///     let mut f = |x: u32| { sum += x; };
///     let closure = ScopedClosure::borrow(&mut f);
///     call_immediately(&closure);
/// }  // closure dropped here, JS function invalidated
/// assert_eq!(sum, 42);
/// ```
///
/// ## Owned closures with `Closure::new`
///
/// Use for long-lived callbacks like event listeners and timers:
///
/// ```ignore
/// use wasm_bindgen::prelude::*;
///
/// #[wasm_bindgen]
/// extern "C" {
///     fn setInterval(closure: &Closure<dyn FnMut()>, time: u32) -> i32;
/// }
///
/// // Closure::new requires 'static, so use `move` to capture by value
/// let cb = Closure::new(move || {
///     // ...
/// });
/// setInterval(&cb, 1000);
/// // Must keep `cb` alive or call `cb.forget()` to transfer to JS
/// ```
///
/// ## Transferring ownership to JS
///
/// Pass a `ScopedClosure<'static, T>` by value to transfer ownership:
///
/// ```ignore
/// use wasm_bindgen::prelude::*;
///
/// #[wasm_bindgen]
/// extern "C" {
///     fn set_one_shot_callback(cb: ScopedClosure<dyn FnMut()>);
/// }
///
/// let cb = ScopedClosure::own(|| { /* ... */ });
/// set_one_shot_callback(cb);  // Ownership transferred, no need to store
/// ```
pub struct ScopedClosure<'a, T: ?Sized> {
    js: JsClosure,
    // careful: must be Box<T> not just T because unsized PhantomData
    // seems to have weird interaction with Pin<>
    _marker: PhantomData<Box<T>>,
    _lifetime: PhantomData<&'a ()>,
}

/// Alias for [`ScopedClosure<'static, T>`] for backwards compatibility.
///
/// In a future major version, `Closure` will become `ScopedClosure<'a, T>` with a
/// lifetime parameter.
pub type Closure<T> = ScopedClosure<'static, T>;

// ScopedClosure is Unpin because it only contains a JsValue (which is just a u32)
// and PhantomData markers. The closure data is either on the heap (owned) or
// referenced through a raw pointer (borrowed), neither of which is stored inline.
impl<T: ?Sized> Unpin for ScopedClosure<'_, T> {}

fn _assert_compiles<T>(pin: core::pin::Pin<&mut ScopedClosure<'static, T>>) {
    let _ = &mut *pin.get_mut();
}

impl<T: ?Sized> Drop for ScopedClosure<'_, T> {
    fn drop(&mut self) {
        // Invalidate the closure on the JS side.
        //
        // The JS bindings distinguish owned vs borrowed closures via the `dtor_idx`
        // encoded in `WasmDescribe`: owned closures pass a non-zero destructor
        // function pointer, borrowed closures pass `0`.
        //
        // For owned closures (`Closure::new`/`ScopedClosure::own`), this decreases
        // the refcount and frees the Rust heap data when the count reaches zero.
        //
        // For borrowed closures (`ScopedClosure::borrow`/`borrow_mut`), this sets
        // state.a = state.b = 0 to prevent any further calls to the closure.
        self.js._wbg_cb_unref();
    }
}

impl<'a, T> ScopedClosure<'a, T>
where
    T: ?Sized + WasmClosure,
{
    /// Obtain a `&JsValue` reference for this closure
    pub fn as_js_value(&self) -> &JsValue {
        self.js.unchecked_ref()
    }
}

/// Methods for creating and managing `'static` closures.
///
/// These methods are only available on `ScopedClosure<'static, T>`
/// not on borrowed `ScopedClosure<'a, T>` where `'a` is not `'static`.
impl<T> ScopedClosure<'static, T>
where
    T: ?Sized + WasmClosure,
{
    /// Alias for [`Closure::own`].
    ///
    /// Creates a new static owned `ScopedClosure<'static, T>` from the provided
    /// Rust function, with panic unwind support.
    ///
    /// The type parameter `T` determines whether the closure is `Fn` or `FnMut`.
    ///
    /// Supports unwind via its UnwindSafe bound when building with panic=unwind.
    /// Alternatively, pass `Closure::own(AssertUnwindSafe(...))` to assert unwind
    /// safety, or use [`own_assert_unwind_safe`](Self::own_assert_unwind_safe) or
    /// [`own_aborting`](Self::own_aborting).
    ///
    /// When provided to a JS function as an own value, to be managed by the JS GC.
    ///
    /// See [`borrow`](Self::borrow) for creating a borrowed `ScopedClosure` with
    /// an associated lifetime (defaults to immutable `Fn`).
    pub fn new<F>(t: F) -> Self
    where
        F: IntoWasmClosure<T> + MaybeUnwindSafe + 'static,
    {
        Self::own(t)
    }

    /// Creates a new static owned `ScopedClosure<'static, T>` from the provided
    /// Rust function, with panic unwind support.
    ///
    /// The type parameter `T` determines whether the closure is `Fn` or `FnMut`.
    ///
    /// Supports unwind via its UnwindSafe bound when building with panic=unwind.
    /// Alternatively, pass `Closure::own(AssertUnwindSafe(...))` to assert unwind
    /// safety, or use [`own_assert_unwind_safe`](Self::own_assert_unwind_safe) or
    /// [`own_aborting`](Self::own_aborting).
    ///
    /// When provided to a JS function as an own value, to be managed by the JS GC.
    ///
    /// See [`borrow`](Self::borrow) for creating a borrowed `ScopedClosure` with
    /// an associated lifetime (defaults to immutable `Fn`).
    pub fn own<F>(t: F) -> Self
    where
        F: IntoWasmClosure<T> + MaybeUnwindSafe + 'static,
    {
        Self::wrap_maybe_aborting::<true>(Box::new(t))
    }

    /// Creates a new owned `'static` closure that aborts on panic.
    ///
    /// Unlike [`own`](Self::own), this version does NOT catch panics and does NOT
    /// require `UnwindSafe`. If the closure panics, the process will abort.
    ///
    /// The type parameter `T` determines whether the closure is `Fn` or `FnMut`.
    ///
    /// When provided to a JS function as an own value, to be managed by the JS GC.
    ///
    /// See [`borrow_aborting`](Self::borrow_aborting) for creating a borrowed
    /// `ScopedClosure` with an associated lifetime.
    ///
    /// **Note: Not unwind safe. Prefer [`own`](Self::own) or `own` with
    /// [`AssertUnwindSafe`](core::panic::AssertUnwindSafe) when possible.**
    pub fn own_aborting<F>(t: F) -> Self
    where
        F: IntoWasmClosure<T> + 'static,
    {
        Self::wrap_maybe_aborting::<false>(Box::new(t))
    }

    /// Creates a new static owned `ScopedClosure<'static, T>` from the provided
    /// Rust function, with panic unwind support.
    ///
    /// The type parameter `T` determines whether the closure is `Fn` or `FnMut`.
    ///
    /// When provided to a JS function as an own value, to be managed by the JS GC.
    ///
    /// **Safety: Unwind safety is assumed when using this function, like using
    /// `AssertUnwindSafe(...)`, this must be verified explicitly.**
    ///
    /// See [`borrow_aborting`](Self::borrow_aborting) for creating a borrowed
    /// `ScopedClosure` with an associated lifetime.
    pub fn own_assert_unwind_safe<F>(t: F) -> Self
    where
        F: IntoWasmClosure<T> + 'static,
    {
        Self::wrap_maybe_aborting::<true>(Box::new(t))
    }

    /// A more direct version of `Closure::new` which creates a `Closure` from
    /// a `Box<dyn Fn>`/`Box<dyn FnMut>`, which is how it's kept internally.
    ///
    /// Supports unwind via its UnwindSafe bound when building with panic=unwind.
    /// Alternatively, use [`wrap_assert_unwind_safe`](Self::wrap_assert_unwind_safe)
    /// to assert unwind safety, or use [`wrap_aborting`](Self::wrap_aborting).
    ///
    pub fn wrap<F>(data: Box<F>) -> Self
    where
        F: IntoWasmClosure<T> + ?Sized + MaybeUnwindSafe,
    {
        Self::wrap_maybe_aborting::<true>(data)
    }

    /// A more direct version of `Closure::new` which creates a `Closure` from
    /// a `Box<dyn Fn>`/`Box<dyn FnMut>`, which is how it's kept internally.
    ///
    /// This version aborts on panics.
    pub fn wrap_aborting<F>(data: Box<F>) -> Self
    where
        F: IntoWasmClosure<T> + ?Sized,
    {
        Self::wrap_maybe_aborting::<false>(data)
    }

    /// A more direct version of `Closure::new` which creates a `Closure` from
    /// a `Box<dyn Fn>`/`Box<dyn FnMut>`, which is how it's kept internally.
    ///
    /// **Safety: Unwind safety is assumed when using this function, like using
    /// `AssertUnwindSafe(...)`, this must be verified explicitly.**
    ///
    /// This version catches panics when unwinding is available.
    pub fn wrap_assert_unwind_safe<F>(data: Box<F>) -> Self
    where
        F: IntoWasmClosure<T> + ?Sized,
    {
        Self::wrap_maybe_aborting::<true>(data)
    }

    fn wrap_maybe_aborting<const UNWIND_SAFE: bool>(
        data: Box<impl IntoWasmClosure<T> + ?Sized>,
    ) -> Self {
        Self {
            js: crate::__rt::wbg_cast(OwnedClosure::<T, UNWIND_SAFE>(data.unsize())),
            _marker: PhantomData,
            _lifetime: PhantomData,
        }
    }

    /// Creates a scoped closure by borrowing an immutable `Fn` closure with
    /// panic unwind support.
    ///
    /// This is the recommended way to pass closures to JavaScript for immediate/
    /// synchronous use. Unlike [`Closure::own`], this does not require the closure
    /// to be `'static`, allowing you to capture references to local variables.
    ///
    /// Use [`borrow_mut`](Self::borrow_mut) when you need to mutate captured state.
    ///
    /// The returned `ScopedClosure<'a, _>` has lifetime `'a` from the closure
    /// reference, which means it cannot outlive the closure or any data the
    /// closure captures.
    ///
    /// Supports unwind via its UnwindSafe bound when building with panic=unwind.
    /// Wrap with `AssertUnwindSafe` if necessary to achieve this bound, or
    /// use [`borrow_assert_unwind_safe`](Self::borrow_assert_unwind_safe) or
    /// [`borrow_aborting`](Self::borrow_aborting) for non-unwind-safe functions.
    ///
    /// The resulting closure can be upcasted to `FnMut` using [`upcast_ref`](crate::Upcast::upcast_ref).
    ///
    /// # When to use scoped closures
    ///
    /// Use `ScopedClosure::borrow` or `ScopedClosure::borrow_mut` when:
    /// - JavaScript will call the closure immediately and not retain it
    /// - You need to capture non-`'static` references
    /// - You want automatic cleanup when the `ScopedClosure` is dropped
    ///
    /// # Closure lifetime
    ///
    /// The JavaScript function is only valid while the `ScopedClosure` exists.
    /// Once dropped, the JavaScript function is invalidated. If JavaScript retains
    /// a reference and calls it later, it will throw: "closure invoked recursively
    /// or after being dropped".
    ///
    /// Rust's borrow checker ensures `ScopedClosure` cannot outlive the closure's
    /// captured data, preventing use-after-free bugs.
    ///
    /// # Example
    ///
    /// ```ignore
    /// use wasm_bindgen::prelude::*;
    ///
    /// #[wasm_bindgen]
    /// extern "C" {
    ///     fn call_with_value(cb: &ScopedClosure<dyn Fn(u32)>, value: u32);
    ///     fn call_fnmut(cb: &ScopedClosure<dyn FnMut(u32)>, value: u32);
    /// }
    ///
    /// let data = vec![1, 2, 3];
    /// let f = |x| {
    ///     println!("data len: {}, x: {}", data.len(), x);
    /// };
    /// let closure = ScopedClosure::borrow(&f);
    /// call_with_value(&closure, 42);
    /// // Can also upcast to FnMut
    /// call_fnmut(closure.upcast_ref(), 42);
    /// ```
    pub fn borrow<'a, F>(t: &'a F) -> ScopedClosure<'a, T::Static>
    where
        F: IntoWasmClosureRef<T> + MaybeUnwindSafe + ?Sized,
    {
        Self::borrow_assert_unwind_safe(t)
    }

    /// Like [`borrow`](Self::borrow), but does not catch panics.
    ///
    /// If the closure panics, the process will abort. This variant does not
    /// require `UnwindSafe`.
    ///
    /// **Note: Not unwind safe. Prefer [`borrow`](Self::borrow) or
    /// [`borrow_assert_unwind_safe`](Self::borrow_assert_unwind_safe) when possible.**
    pub fn borrow_aborting<'a, F>(t: &'a F) -> ScopedClosure<'a, T::Static>
    where
        F: IntoWasmClosureRef<T> + ?Sized,
    {
        Self::wrap_borrow::<_, false>(t)
    }

    /// Like [`borrow`](Self::borrow), but catches panics without requiring `MaybeUnwindSafe`.
    ///
    /// **Safety: Unwind safety is assumed when using this function, like using
    /// `AssertUnwindSafe(...)`, this must be verified explicitly.**
    pub fn borrow_assert_unwind_safe<'a, F>(t: &'a F) -> ScopedClosure<'a, T::Static>
    where
        F: IntoWasmClosureRef<T> + ?Sized,
    {
        Self::wrap_borrow::<_, true>(t)
    }

    fn wrap_borrow<'a, F, const UNWIND_SAFE: bool>(t: &'a F) -> ScopedClosure<'a, T::Static>
    where
        F: IntoWasmClosureRef<T> + ?Sized,
    {
        let t: &T = t.unsize_closure_ref();
        let (ptr, len): (u32, u32) = unsafe { mem::transmute_copy(&t) };
        ScopedClosure {
            js: crate::__rt::wbg_cast(BorrowedClosure::<T, UNWIND_SAFE> {
                data: WasmSlice { ptr, len },
                _marker: PhantomData,
            }),
            _marker: PhantomData,
            _lifetime: PhantomData,
        }
    }

    /// Creates a scoped closure by mutably borrowing a `FnMut` closure.
    ///
    /// Use this for closures that need to mutate captured state. For closures that
    /// don't need mutation, prefer [`borrow`](Self::borrow) which creates an immutable
    /// `Fn` closure that is more likely to satisfy `UnwindSafe` automatically.
    ///
    /// Supports unwind via its UnwindSafe bound when building with panic=unwind.
    /// Wrap with `AssertUnwindSafe` if necessary to achieve this bound, or
    /// use [`borrow_mut_assert_unwind_safe`](Self::borrow_mut_assert_unwind_safe) or
    /// [`borrow_mut_aborting`](Self::borrow_mut_aborting) for non-unwind-safe functions.
    ///
    /// See [`borrow`](Self::borrow) for full documentation on scoped closures.
    ///
    /// # Example
    ///
    /// ```ignore
    /// use wasm_bindgen::prelude::*;
    ///
    /// #[wasm_bindgen]
    /// extern "C" {
    ///     fn call_three_times(cb: &ScopedClosure<dyn FnMut(u32)>);
    /// }
    ///
    /// let mut sum = 0;
    /// let closure = ScopedClosure::borrow_mut(&mut |x: u32| {
    ///     sum += x;
    /// });
    /// call_three_times(&closure);
    /// // closure dropped, `sum` is accessible again
    /// assert_eq!(sum, 6); // 1 + 2 + 3
    /// ```
    pub fn borrow_mut<'a, F>(t: &'a mut F) -> ScopedClosure<'a, T::Static>
    where
        F: IntoWasmClosureRefMut<T> + MaybeUnwindSafe + ?Sized,
    {
        Self::borrow_mut_assert_unwind_safe(t)
    }

    /// Like [`borrow_mut`](Self::borrow_mut), but does not catch panics.
    ///
    /// If the closure panics, the process will abort. This variant does not
    /// require `UnwindSafe`.
    ///
    /// **Note: Not unwind safe. Prefer [`borrow_mut`](Self::borrow_mut) or
    /// [`borrow_mut_assert_unwind_safe`](Self::borrow_mut_assert_unwind_safe) when possible.**
    pub fn borrow_mut_aborting<'a, F>(t: &'a mut F) -> ScopedClosure<'a, T::Static>
    where
        F: IntoWasmClosureRefMut<T> + ?Sized,
    {
        Self::wrap_borrow_mut::<_, false>(t)
    }

    /// Like [`borrow_mut`](Self::borrow_mut), but catches panics without requiring `MaybeUnwindSafe`.
    ///
    /// **Safety: Unwind safety is assumed when using this function, like using
    /// `AssertUnwindSafe(...)`, this must be verified explicitly.**
    pub fn borrow_mut_assert_unwind_safe<'a, F>(t: &'a mut F) -> ScopedClosure<'a, T::Static>
    where
        F: IntoWasmClosureRefMut<T> + ?Sized,
    {
        Self::wrap_borrow_mut::<_, true>(t)
    }

    fn wrap_borrow_mut<'a, F, const UNWIND_SAFE: bool>(t: &'a mut F) -> ScopedClosure<'a, T::Static>
    where
        F: IntoWasmClosureRefMut<T> + ?Sized,
    {
        let t: &mut T = t.unsize_closure_ref();
        let (ptr, len): (u32, u32) = unsafe { mem::transmute_copy(&t) };
        ScopedClosure {
            js: crate::__rt::wbg_cast(BorrowedClosure::<T, UNWIND_SAFE> {
                data: WasmSlice { ptr, len },
                _marker: PhantomData,
            }),
            _marker: PhantomData,
            _lifetime: PhantomData,
        }
    }

    /// Release memory management of this closure from Rust to the JS GC.
    ///
    /// When a `Closure` is dropped it will release the Rust memory and
    /// invalidate the associated JS closure, but this isn't always desired.
    /// Some callbacks are alive for the entire duration of the program or for a
    /// lifetime dynamically managed by the JS GC. This function can be used
    /// to drop this `Closure` while keeping the associated JS function still
    /// valid.
    ///
    /// If the platform supports weak references, the Rust memory will be
    /// reclaimed when the JS closure is GC'd. If weak references is not
    /// supported, this can be dangerous if this function is called many times
    /// in an application because the memory leak will overwhelm the page
    /// quickly and crash the wasm.
    ///
    /// # Safety Note
    ///
    /// This method is only available on `'static` closures. Calling it on a
    /// borrowed `ScopedClosure` would be unsound because the closure data
    /// would become invalid when the borrow ends.
    pub fn into_js_value(self) -> JsValue {
        let idx = self.js.idx;
        mem::forget(self);
        JsValue::_new(idx)
    }

    /// Same as `mem::forget(self)`.
    ///
    /// This can be used to fully relinquish closure ownership to the JS.
    ///
    /// # Safety Note
    ///
    /// This method is only available on `'static` closures. Calling it on a borrowed
    /// `ScopedClosure` would be unsound because the closure data would become invalid
    /// when the borrow ends.
    pub fn forget(self) {
        mem::forget(self);
    }

    /// Create a `Closure` from a function that can only be called once.
    ///
    /// Since we have no way of enforcing that JS cannot attempt to call this
    /// `FnOne(A...) -> R` more than once, this produces a `Closure<dyn FnMut(A...)
    /// -> R>` that will dynamically throw a JavaScript error if called more
    /// than once.
    ///
    /// # Example
    ///
    /// ```rust,no_run
    /// use wasm_bindgen::prelude::*;
    ///
    /// // Create an non-`Copy`, owned `String`.
    /// let mut s = String::from("Hello");
    ///
    /// // Close over `s`. Since `f` returns `s`, it is `FnOnce` and can only be
    /// // called once. If it was called a second time, it wouldn't have any `s`
    /// // to work with anymore!
    /// let f = move || {
    ///     s += ", World!";
    ///     s
    /// };
    ///
    /// // Create a `Closure` from `f`. Note that the `Closure`'s type parameter
    /// // is `FnMut`, even though `f` is `FnOnce`.
    /// let closure: Closure<dyn FnMut() -> String> = Closure::once(f);
    /// ```
    ///
    /// Note: the `A` and `R` type parameters are here just for backward compat
    /// and will be removed in the future.
    pub fn once<F, A, R>(fn_once: F) -> Self
    where
        F: WasmClosureFnOnce<T, A, R> + MaybeUnwindSafe,
    {
        Closure::wrap_maybe_aborting::<true>(fn_once.into_fn_mut())
    }

    /// Create a `Closure` from a function that can only be called once.
    ///
    /// Unlike `once`, this version does NOT catch panics and does NOT require `UnwindSafe`.
    /// If the closure panics, the process will abort.
    ///
    /// Use this when:
    /// - Your closure captures types that aren't `UnwindSafe` (like `Rc<Cell<T>>`)
    /// - You don't need panic catching across the JS boundary
    /// - You prefer abort-on-panic behavior
    ///
    /// Since we have no way of enforcing that JS cannot attempt to call this
    /// `FnOnce(A...) -> R` more than once, this produces a `Closure<dyn FnMut(A...)
    /// -> R>` that will dynamically throw a JavaScript error if called more
    /// than once.
    ///
    /// **Note: Not unwind safe. Prefer [`once`](Self::once) or `once` with
    /// [`AssertUnwindSafe`](core::panic::AssertUnwindSafe) when possible.**
    ///
    /// Note: the `A` and `R` type parameters are here just for backward compat
    /// and will be removed in the future.
    pub fn once_aborting<F, A, R>(fn_once: F) -> Self
    where
        F: WasmClosureFnOnceAbort<T, A, R>,
    {
        Closure::wrap_maybe_aborting::<false>(fn_once.into_fn_mut())
    }

    /// Create a `Closure` from a function that can only be called once,
    /// with panic unwind support.
    ///
    /// **Safety: Unwind safety is assumed when using this function, like using
    /// `AssertUnwindSafe(...)`, this must be verified explicitly.**
    ///
    /// Use this when:
    /// - Your closure captures types that aren't `UnwindSafe` (like `Rc<Cell<T>>`)
    /// - You still want panics to be caught and converted to JS exceptions
    ///
    /// Since we have no way of enforcing that JS cannot attempt to call this
    /// `FnOnce(A...) -> R` more than once, this produces a `Closure<dyn FnMut(A...)
    /// -> R>` that will dynamically throw a JavaScript error if called more
    /// than once.
    ///
    /// Note: the `A` and `R` type parameters are here just for backward compat
    /// and will be removed in the future.
    pub fn once_assert_unwind_safe<F, A, R>(fn_once: F) -> Self
    where
        F: WasmClosureFnOnceAbort<T, A, R>,
    {
        Closure::wrap_maybe_aborting::<true>(fn_once.into_fn_mut())
    }

    // TODO: Update once closures to be generated on construction as once
    // closures instead of using wrap(). Then we can share the same into_js()
    // function between all closures, and deprecate this method.
    /// Convert a `FnOnce(A...) -> R` into a JavaScript `Function` object.
    ///
    /// If the JavaScript function is invoked more than once, it will throw an
    /// exception.
    ///
    /// Unlike `Closure::once`, this does *not* return a `Closure` that can be
    /// dropped before the function is invoked to deallocate the closure. The
    /// only way the `FnOnce` is deallocated is by calling the JavaScript
    /// function. If the JavaScript function is never called then the `FnOnce`
    /// and everything it closes over will leak.
    ///
    /// ```rust,ignore
    /// use wasm_bindgen::{prelude::*, JsCast};
    ///
    /// let f = Closure::once_into_js(move || {
    ///     // ...
    /// });
    ///
    /// assert!(f.is_instance_of::<js_sys::Function>());
    /// ```
    ///
    /// Note: the `A` and `R` type parameters are here just for backward compat
    /// and will be removed in the future.
    pub fn once_into_js<F, A, R>(fn_once: F) -> JsValue
    where
        F: WasmClosureFnOnce<T, A, R> + MaybeUnwindSafe,
    {
        fn_once.into_js_function()
    }
}

/// A trait for converting an `FnOnce(A...) -> R` into a `FnMut(A...) -> R` that
/// will throw if ever called more than once.
#[doc(hidden)]
pub trait WasmClosureFnOnce<FnMut: ?Sized, A, R>: 'static {
    fn into_fn_mut(self) -> Box<FnMut>;

    fn into_js_function(self) -> JsValue;
}

/// A trait for converting an `FnOnce(A...) -> R` into a `FnMut(A...) -> R` that
/// will throw if ever called more than once. This variant does not require UnwindSafe.
#[doc(hidden)]
pub trait WasmClosureFnOnceAbort<FnMut: ?Sized, A, R>: 'static {
    fn into_fn_mut(self) -> Box<FnMut>;

    fn into_js_function(self) -> JsValue;
}

impl<T: ?Sized> AsRef<JsValue> for ScopedClosure<'_, T> {
    fn as_ref(&self) -> &JsValue {
        &self.js
    }
}

/// Internal representation of an owned closure sent to JS.
/// `UNWIND_SAFE` selects the invoke shim: `true` catches panics, `false` does not.
#[repr(transparent)]
struct OwnedClosure<T: ?Sized, const UNWIND_SAFE: bool>(Box<T>);

/// Internal representation of a borrowed closure sent to JS.
/// `UNWIND_SAFE` selects the invoke shim: `true` catches panics, `false` does not.
struct BorrowedClosure<T: ?Sized, const UNWIND_SAFE: bool> {
    data: WasmSlice,
    _marker: PhantomData<T>,
}

unsafe extern "C" fn destroy<T: ?Sized>(a: usize, b: usize) {
    if a == 0 {
        return;
    }
    drop(mem::transmute_copy::<_, Box<T>>(&(a, b)));
}

impl<T, const UNWIND_SAFE: bool> WasmDescribe for OwnedClosure<T, UNWIND_SAFE>
where
    T: WasmClosure + ?Sized,
{
    #[cfg_attr(wasm_bindgen_unstable_test_coverage, coverage(off))]
    fn describe() {
        inform(CLOSURE);
        inform(destroy::<T> as *const () as usize as u32);
        inform(T::IS_MUT as u32);
        T::describe_invoke::<UNWIND_SAFE>();
    }
}

impl<T, const UNWIND_SAFE: bool> WasmDescribe for BorrowedClosure<T, UNWIND_SAFE>
where
    T: WasmClosure + ?Sized,
{
    #[cfg_attr(wasm_bindgen_unstable_test_coverage, coverage(off))]
    fn describe() {
        inform(CLOSURE);
        inform(0);
        inform(T::IS_MUT as u32);
        T::describe_invoke::<UNWIND_SAFE>();
    }
}

impl<T, const UNWIND_SAFE: bool> IntoWasmAbi for OwnedClosure<T, UNWIND_SAFE>
where
    T: WasmClosure + ?Sized,
{
    type Abi = WasmSlice;

    fn into_abi(self) -> WasmSlice {
        let (a, b): (usize, usize) = unsafe { mem::transmute_copy(&ManuallyDrop::new(self)) };
        WasmSlice {
            ptr: a as u32,
            len: b as u32,
        }
    }
}

impl<T, const UNWIND_SAFE: bool> IntoWasmAbi for BorrowedClosure<T, UNWIND_SAFE>
where
    T: WasmClosure + ?Sized,
{
    type Abi = WasmSlice;
    fn into_abi(self) -> WasmSlice {
        self.data
    }
}

impl<T> WasmDescribe for ScopedClosure<'_, T>
where
    T: WasmClosure + ?Sized,
{
    #[cfg_attr(wasm_bindgen_unstable_test_coverage, coverage(off))]
    fn describe() {
        inform(EXTERNREF);
    }
}

// `ScopedClosure` can be passed by reference to imports (for any lifetime).
impl<T> IntoWasmAbi for &ScopedClosure<'_, T>
where
    T: WasmClosure + ?Sized,
{
    type Abi = u32;

    fn into_abi(self) -> u32 {
        (&self.js).into_abi()
    }
}

impl<T> OptionIntoWasmAbi for &ScopedClosure<'_, T>
where
    T: WasmClosure + ?Sized,
{
    fn none() -> Self::Abi {
        0
    }
}

/// `'static` closures can be passed by value to JS, transferring ownership.
///
/// This is useful for one-shot callbacks where you want JS to own the closure.
/// The closure will be cleaned up by JS GC (if weak references are supported)
/// or will leak (if weak references are not supported).
///
/// # Example
///
/// ```ignore
/// #[wasm_bindgen]
/// extern "C" {
///     fn set_one_shot_callback(cb: Closure<dyn FnMut()>);
/// }
///
/// let cb = Closure::new(|| { /* ... */ });
/// set_one_shot_callback(cb);  // Ownership transferred to JS
/// // No need to store or forget the closure
/// ```
impl<T> IntoWasmAbi for ScopedClosure<'static, T>
where
    T: WasmClosure + ?Sized,
{
    type Abi = u32;

    fn into_abi(self) -> u32 {
        let idx = self.js.idx;
        mem::forget(self);
        idx
    }
}

impl<T> OptionIntoWasmAbi for ScopedClosure<'static, T>
where
    T: WasmClosure + ?Sized,
{
    fn none() -> Self::Abi {
        0
    }
}

fn _check() {
    fn _assert<T: IntoWasmAbi>() {}
    // ScopedClosure by reference (any lifetime)
    _assert::<&ScopedClosure<dyn Fn()>>();
    _assert::<&ScopedClosure<dyn Fn(String)>>();
    _assert::<&ScopedClosure<dyn Fn() -> String>>();
    _assert::<&ScopedClosure<dyn FnMut()>>();
    _assert::<&ScopedClosure<dyn FnMut(String)>>();
    _assert::<&ScopedClosure<dyn FnMut() -> String>>();
    // ScopedClosure by value (only 'static)
    _assert::<ScopedClosure<'static, dyn Fn()>>();
    _assert::<ScopedClosure<'static, dyn FnMut()>>();
    _assert::<Closure<dyn Fn()>>();
    _assert::<Closure<dyn FnMut()>>();
}

impl<T> fmt::Debug for ScopedClosure<'_, T>
where
    T: ?Sized,
{
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "Closure {{ ... }}")
    }
}

/// An internal trait for the `Closure` type.
///
/// This trait is not stable and it's not recommended to use this in bounds or
/// implement yourself.
#[doc(hidden)]
pub unsafe trait WasmClosure: WasmDescribe {
    const IS_MUT: bool;
    /// The `'static` version of `Self`. For example, if `Self` is `dyn Fn() + 'a`,
    /// then `Static` is `dyn Fn()` (implicitly `'static`).
    type Static: ?Sized + WasmClosure;
    /// The mutable version of this closure type.
    /// For `dyn Fn(...) -> R` this is `dyn FnMut(...) -> R`.
    /// For `dyn FnMut(...) -> R` this is itself.
    type AsMut: ?Sized;
    /// Emit the FUNCTION descriptor with the invoke shim selected by
    /// `UNWIND_SAFE`: `true` picks the panic-catching shim, `false`
    /// picks the non-catching shim.
    fn describe_invoke<const UNWIND_SAFE: bool>();
}

unsafe impl<T: WasmClosure> WasmClosure for AssertUnwindSafe<T> {
    type Static = T::Static;
    const IS_MUT: bool = T::IS_MUT;
    type AsMut = T::AsMut;
    fn describe_invoke<const UNWIND_SAFE: bool>() {
        T::describe_invoke::<UNWIND_SAFE>();
    }
}

/// An internal trait for the `Closure` type.
///
/// This trait is not stable and it's not recommended to use this in bounds or
/// implement yourself.
#[doc(hidden)]
pub trait IntoWasmClosure<T: ?Sized> {
    fn unsize(self: Box<Self>) -> Box<T>;
}

impl<T: ?Sized + WasmClosure> IntoWasmClosure<T> for T {
    fn unsize(self: Box<Self>) -> Box<T> {
        self
    }
}

/// Trait for converting a reference to a closure into a trait object reference.
///
/// This trait is not stable and it's not recommended to use this in bounds or
/// implement yourself.
#[doc(hidden)]
pub trait IntoWasmClosureRef<T: ?Sized> {
    fn unsize_closure_ref(&self) -> &T;
}

/// Trait for converting a mutable reference to a closure into a trait object reference.
///
/// This trait is not stable and it's not recommended to use this in bounds or
/// implement yourself.
#[doc(hidden)]
pub trait IntoWasmClosureRefMut<T: ?Sized> {
    fn unsize_closure_ref(&mut self) -> &mut T;
}

// Blanket impl for AssertUnwindSafe - delegates to inner type
impl<T: ?Sized, F> IntoWasmClosureRef<T> for AssertUnwindSafe<F>
where
    F: IntoWasmClosureRef<T>,
{
    fn unsize_closure_ref(&self) -> &T {
        self.0.unsize_closure_ref()
    }
}

impl<T: ?Sized, F> IntoWasmClosureRefMut<T> for AssertUnwindSafe<F>
where
    F: IntoWasmClosureRefMut<T>,
{
    fn unsize_closure_ref(&mut self) -> &mut T {
        self.0.unsize_closure_ref()
    }
}

// In ScopedClosure, the Rust closure type is the phantom type that erases.
unsafe impl<T: ?Sized + WasmClosure> ErasableGeneric for ScopedClosure<'_, T> {
    type Repr = ScopedClosure<'static, dyn FnMut()>;
}
