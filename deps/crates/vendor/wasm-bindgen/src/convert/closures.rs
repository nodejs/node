use alloc::boxed::Box;
use core::mem;
use core::panic::AssertUnwindSafe;

use crate::__rt::marker::ErasableGeneric;
use crate::__rt::maybe_catch_unwind;
use crate::closure::{
    Closure, IntoWasmClosure, IntoWasmClosureRef, IntoWasmClosureRefMut, ScopedClosure,
    WasmClosure, WasmClosureFnOnce, WasmClosureFnOnceAbort,
};
use crate::convert::slices::WasmSlice;
use crate::convert::traits::UpcastFrom;
use crate::convert::RefFromWasmAbi;
use crate::convert::{FromWasmAbi, IntoWasmAbi, ReturnWasmAbi, WasmAbi, WasmRet};
use crate::describe::{inform, WasmDescribe, FUNCTION};
use crate::sys::Undefined;
use crate::throw_str;
use crate::JsValue;
use crate::UnwrapThrowExt;

macro_rules! closures {
    // Unwind safe passing
    ([$($maybe_unwind_safe:tt)*] $($rest:tt)*) => {
        closures!(@process [$($maybe_unwind_safe)*] $($rest)*);
    };

    // One-arity recurse
    (@process [$($unwind_safe:tt)*] ($($var:ident $arg1:ident $arg2:ident $arg3:ident $arg4:ident)*) $($rest:tt)*) => {
        closures!(@impl_for_args ($($var),*) FromWasmAbi [$($unwind_safe)*] $($var::from_abi($var) => $var $arg1 $arg2 $arg3 $arg4)*);
        closures!(@process [$($unwind_safe)*] $($rest)*);
    };

    // Base case
    (@process [$($unwind_safe:tt)*]) => {};

    // A counter helper to count number of arguments.
    (@count_one $ty:ty) => (1);

    (@describe ( $($ty:ty),* )) => {
        // Needs to be a constant so that interpreter doesn't crash on
        // unsupported operations in debug mode.
        const ARG_COUNT: u32 = 0 $(+ closures!(@count_one $ty))*;
        inform(ARG_COUNT);
        $(<$ty>::describe();)*
    };

    // This silly helper is because by default Rust infers `|var_with_ref_type| ...` closure
    // as `impl Fn(&'outer_lifetime A)` instead of `impl for<'temp_lifetime> Fn(&'temp_lifetime A)`
    // while `|var_with_ref_type: &A|` makes it use the higher-order generic as expected.
    (@closure ($($ty:ty),*) $($var:ident)* $body:block) => (move |$($var: $ty),*| $body);

    (@impl_for_fn $is_mut:literal [$($mut:ident)?] $Fn:ident $FnArgs:tt $FromWasmAbi:ident $($var_expr:expr => $var:ident $arg1:ident $arg2:ident $arg3:ident $arg4:ident)*) => (const _: () = {
        impl<$($var,)* R> IntoWasmAbi for &'_ $($mut)? (dyn $Fn $FnArgs -> R + '_)
        where
            Self: WasmDescribe,
        {
            type Abi = WasmSlice;

            fn into_abi(self) -> WasmSlice {
                unsafe {
                    let (a, b): (usize, usize) = mem::transmute(self);
                    WasmSlice { ptr: a as u32, len: b as u32 }
                }
            }
        }

        unsafe impl<'a, $($var,)* R> ErasableGeneric for &'a $($mut)? (dyn $Fn $FnArgs -> R + 'a)
        where
            $($var: ErasableGeneric,)*
            R: ErasableGeneric
        {
            type Repr = &'static (dyn $Fn ($(<$var as ErasableGeneric>::Repr,)*) -> <R as ErasableGeneric>::Repr + 'static);
        }

        // Invoke shim for closures. The const generic `UNWIND_SAFE` controls
        // whether panics are caught and converted to JS exceptions (`true`) or
        // left to unwind/abort (`false`). When `panic=unwind` is not available,
        // `UNWIND_SAFE` has no effect — panics always abort.
        #[allow(non_snake_case)]
        unsafe extern "C-unwind" fn invoke<$($var: $FromWasmAbi,)* R: ReturnWasmAbi, const UNWIND_SAFE: bool>(
            a: usize,
            b: usize,
            $(
            $arg1: <$var::Abi as WasmAbi>::Prim1,
            $arg2: <$var::Abi as WasmAbi>::Prim2,
            $arg3: <$var::Abi as WasmAbi>::Prim3,
            $arg4: <$var::Abi as WasmAbi>::Prim4,
            )*
        ) -> WasmRet<R::Abi> {
            if a == 0 {
                throw_str("closure invoked recursively or after being dropped");
            }
            let ret = {
                let f: & $($mut)? dyn $Fn $FnArgs -> R = mem::transmute((a, b));
                $(
                    let $var = $var::Abi::join($arg1, $arg2, $arg3, $arg4);
                )*
                if UNWIND_SAFE {
                    maybe_catch_unwind(AssertUnwindSafe(|| f($($var_expr),*)))
                } else {
                    f($($var_expr),*)
                }
            };
            ret.return_abi().into()
        }

        #[allow(clippy::fn_to_numeric_cast)]
        impl<$($var,)* R> WasmDescribe for dyn $Fn $FnArgs -> R + '_
        where
            $($var: $FromWasmAbi,)*
            R: ReturnWasmAbi,
        {
            #[cfg_attr(wasm_bindgen_unstable_test_coverage, coverage(off))]
            fn describe() {
                // Raw &dyn Fn/&dyn FnMut passed as arguments use the catching
                // invoke shim by default, matching the previous runtime behavior.
                <Self as WasmClosure>::describe_invoke::<true>();
            }
        }

        unsafe impl<'__closure, $($var,)* R> WasmClosure for dyn $Fn $FnArgs -> R + '__closure
        where
            $($var: $FromWasmAbi,)*
            R: ReturnWasmAbi,
        {
            const IS_MUT: bool = $is_mut;
            type Static = dyn $Fn $FnArgs -> R;
            type AsMut = dyn FnMut $FnArgs -> R + '__closure;
            #[cfg_attr(wasm_bindgen_unstable_test_coverage, coverage(off))]
            fn describe_invoke<const UNWIND_SAFE: bool>() {
                inform(FUNCTION);
                inform(invoke::<$($var,)* R, UNWIND_SAFE> as *const () as usize as u32);
                closures!(@describe $FnArgs);
                R::describe();
                R::describe();
            }
        }

        impl<T, $($var,)* R> IntoWasmClosure<dyn $Fn $FnArgs -> R> for T
        where
            T: 'static + $Fn $FnArgs -> R,
        {
            fn unsize(self: Box<Self>) -> Box<dyn $Fn $FnArgs -> R> { self }
        }
    };);

    // IntoWasmClosureRef is only implemented for Fn, not FnMut.
    // IntoWasmClosureRefMut is implemented for FnMut.
    // Since Fn: FnMut, any Fn closure can be used as FnMut, so this covers all cases.
    (@impl_unsize_closure_ref $FnArgs:tt $FromWasmAbi:ident $($var_expr:expr => $var:ident $arg1:ident $arg2:ident $arg3:ident $arg4:ident)*) => (
        impl<'a, T: 'a, $($var: 'a + $FromWasmAbi,)* R: 'a + ReturnWasmAbi> IntoWasmClosureRef<dyn Fn $FnArgs -> R + 'a> for T
        where
            T: Fn $FnArgs -> R,
        {
            fn unsize_closure_ref(&self) -> &(dyn Fn $FnArgs -> R + 'a) { self }
        }

        impl<'a, T: 'a, $($var: 'a + $FromWasmAbi,)* R: 'a + ReturnWasmAbi> IntoWasmClosureRefMut<dyn FnMut $FnArgs -> R + 'a> for T
        where
            T: FnMut $FnArgs -> R,
        {
            fn unsize_closure_ref(&mut self) -> &mut (dyn FnMut $FnArgs -> R + 'a) { self }
        }
    );

    (@impl_for_args $FnArgs:tt $FromWasmAbi:ident [$($maybe_unwind_safe:tt)*] $($var_expr:expr => $var:ident $arg1:ident $arg2:ident $arg3:ident $arg4:ident)*) => {
        closures!(@impl_for_fn false [] Fn $FnArgs $FromWasmAbi $($var_expr => $var $arg1 $arg2 $arg3 $arg4)*);
        closures!(@impl_for_fn true [mut] FnMut $FnArgs $FromWasmAbi $($var_expr => $var $arg1 $arg2 $arg3 $arg4)*);
        closures!(@impl_unsize_closure_ref $FnArgs $FromWasmAbi $($var_expr => $var $arg1 $arg2 $arg3 $arg4)*);

        // The memory safety here in these implementations below is a bit tricky. We
        // want to be able to drop the `Closure` object from within the invocation of a
        // `Closure` for cases like promises. That means that while it's running we
        // might drop the `Closure`, but that shouldn't invalidate the environment yet.
        //
        // Instead what we do is to wrap closures in `Rc` variables. The main `Closure`
        // has a strong reference count which keeps the trait object alive. Each
        // invocation of a closure then *also* clones this and gets a new reference
        // count. When the closure returns it will release the reference count.
        //
        // This means that if the main `Closure` is dropped while it's being invoked
        // then destruction is deferred until execution returns. Otherwise it'll
        // deallocate data immediately.

        #[allow(non_snake_case, unused_parens)]
        impl<T, $($var,)* R> WasmClosureFnOnce<dyn FnMut $FnArgs -> R, $FnArgs, R> for T
        where
            T: 'static + (FnOnce $FnArgs -> R),
            $($var: $FromWasmAbi + 'static,)*
            R: ReturnWasmAbi + 'static,
            $($maybe_unwind_safe)*
        {
            fn into_fn_mut(self) -> Box<dyn FnMut $FnArgs -> R> {
                let mut me = Some(self);
                Box::new(move |$($var),*| {
                    let me = me.take().expect_throw("FnOnce called more than once");
                    me($($var),*)
                })
            }

            fn into_js_function(self) -> JsValue {
                use alloc::rc::Rc;
                use crate::__rt::WasmRefCell;

                let rc1 = Rc::new(WasmRefCell::new(None));
                let rc2 = rc1.clone();

                let closure = Closure::once(closures!(@closure $FnArgs $($var)* {
                    let result = self($($var),*);

                    // And then drop the `Rc` holding this function's `Closure`
                    // alive.
                    debug_assert_eq!(Rc::strong_count(&rc2), 1);
                    let option_closure = rc2.borrow_mut().take();
                    debug_assert!(option_closure.is_some());
                    drop(option_closure);

                    result
                }));

                let js_val = closure.as_ref().clone();

                *rc1.borrow_mut() = Some(closure);
                debug_assert_eq!(Rc::strong_count(&rc1), 2);
                drop(rc1);

                js_val
            }
        }

        #[allow(non_snake_case, unused_parens)]
        impl<T, $($var,)* R> WasmClosureFnOnceAbort<dyn FnMut $FnArgs -> R, $FnArgs, R> for T
        where
            T: 'static + (FnOnce $FnArgs -> R),
            $($var: $FromWasmAbi + 'static,)*
            R: ReturnWasmAbi + 'static,
        {
            fn into_fn_mut(self) -> Box<dyn FnMut $FnArgs -> R> {
                let mut me = Some(self);
                Box::new(move |$($var),*| {
                    let me = me.take().expect_throw("FnOnce called more than once");
                    me($($var),*)
                })
            }

            fn into_js_function(self) -> JsValue {
                use alloc::rc::Rc;
                use crate::__rt::WasmRefCell;

                let rc1 = Rc::new(WasmRefCell::new(None));
                let rc2 = rc1.clone();

                // TODO: Unwind safety for FnOnce
                let closure = Closure::once_aborting(closures!(@closure $FnArgs $($var)* {
                    let result = self($($var),*);

                    // And then drop the `Rc` holding this function's `Closure`
                    // alive.
                    debug_assert_eq!(Rc::strong_count(&rc2), 1);
                    let option_closure = rc2.borrow_mut().take();
                    debug_assert!(option_closure.is_some());
                    drop(option_closure);

                    result
                }));

                let js_val = closure.as_ref().clone();

                *rc1.borrow_mut() = Some(closure);
                debug_assert_eq!(Rc::strong_count(&rc1), 2);
                drop(rc1);

                js_val
            }
        }
    };

    ([$($unwind_safe:tt)*] $( ($($var:ident $arg1:ident $arg2:ident $arg3:ident $arg4:ident)*) )*) => ($(
        closures!(@impl_for_args ($($var),*) FromWasmAbi [$($maybe_unwind_safe)*] $($var::from_abi($var) => $var $arg1 $arg2 $arg3 $arg4)*);
    )*);
}

#[cfg(all(feature = "std", target_arch = "wasm32", panic = "unwind"))]
closures! {
    [T: core::panic::UnwindSafe,]
    ()
    (A a1 a2 a3 a4)
    (A a1 a2 a3 a4 B b1 b2 b3 b4)
    (A a1 a2 a3 a4 B b1 b2 b3 b4 C c1 c2 c3 c4)
    (A a1 a2 a3 a4 B b1 b2 b3 b4 C c1 c2 c3 c4 D d1 d2 d3 d4)
    (A a1 a2 a3 a4 B b1 b2 b3 b4 C c1 c2 c3 c4 D d1 d2 d3 d4 E e1 e2 e3 e4)
    (A a1 a2 a3 a4 B b1 b2 b3 b4 C c1 c2 c3 c4 D d1 d2 d3 d4 E e1 e2 e3 e4 F f1 f2 f3 f4)
    (A a1 a2 a3 a4 B b1 b2 b3 b4 C c1 c2 c3 c4 D d1 d2 d3 d4 E e1 e2 e3 e4 F f1 f2 f3 f4 G g1 g2 g3 g4)
    (A a1 a2 a3 a4 B b1 b2 b3 b4 C c1 c2 c3 c4 D d1 d2 d3 d4 E e1 e2 e3 e4 F f1 f2 f3 f4 G g1 g2 g3 g4 H h1 h2 h3 h4)
}

#[cfg(not(all(feature = "std", target_arch = "wasm32", panic = "unwind")))]
closures! {
    []
    ()
    (A a1 a2 a3 a4)
    (A a1 a2 a3 a4 B b1 b2 b3 b4)
    (A a1 a2 a3 a4 B b1 b2 b3 b4 C c1 c2 c3 c4)
    (A a1 a2 a3 a4 B b1 b2 b3 b4 C c1 c2 c3 c4 D d1 d2 d3 d4)
    (A a1 a2 a3 a4 B b1 b2 b3 b4 C c1 c2 c3 c4 D d1 d2 d3 d4 E e1 e2 e3 e4)
    (A a1 a2 a3 a4 B b1 b2 b3 b4 C c1 c2 c3 c4 D d1 d2 d3 d4 E e1 e2 e3 e4 F f1 f2 f3 f4)
    (A a1 a2 a3 a4 B b1 b2 b3 b4 C c1 c2 c3 c4 D d1 d2 d3 d4 E e1 e2 e3 e4 F f1 f2 f3 f4 G g1 g2 g3 g4)
    (A a1 a2 a3 a4 B b1 b2 b3 b4 C c1 c2 c3 c4 D d1 d2 d3 d4 E e1 e2 e3 e4 F f1 f2 f3 f4 G g1 g2 g3 g4 H h1 h2 h3 h4)
}

// Comprehensive type-safe cross-function covariant and contravariant casting rules
macro_rules! impl_fn_upcasts {
    () => {
        impl_fn_upcasts!(@arities
            [0 []]
            [1 [A1 B1] O1]
            [2 [A1 B1 A2 B2] O2]
            [3 [A1 B1 A2 B2 A3 B3] O3]
            [4 [A1 B1 A2 B2 A3 B3 A4 B4] O4]
            [5 [A1 B1 A2 B2 A3 B3 A4 B4 A5 B5] O5]
            [6 [A1 B1 A2 B2 A3 B3 A4 B4 A5 B5 A6 B6] O6]
            [7 [A1 B1 A2 B2 A3 B3 A4 B4 A5 B5 A6 B6 A7 B7] O7]
            [8 [A1 B1 A2 B2 A3 B3 A4 B4 A5 B5 A6 B6 A7 B7 A8 B8] O8]
        );
    };

    (@arities) => {};

    (@arities [$n:tt $args:tt $($opt:ident)?] $([$rest_n:tt $rest_args:tt $($rest_opt:ident)?])*) => {
        impl_fn_upcasts!(@same $args);
        impl_fn_upcasts!(@cross_all $args [] $([$rest_n $rest_args $($rest_opt)?])*);
        impl_fn_upcasts!(@arities $([$rest_n $rest_args $($rest_opt)?])*);
    };

    (@same []) => {
        impl<R1, R2> UpcastFrom<fn() -> R1> for fn() -> R2
        where
            R2: UpcastFrom<R1>
        {}

        impl<'a, R1, R2> UpcastFrom<dyn Fn() -> R1 + 'a> for dyn Fn() -> R2 + 'a
        where
            R2: UpcastFrom<R1>
        {}

        impl<'a, R1, R2> UpcastFrom<dyn FnMut() -> R1 + 'a> for dyn FnMut() -> R2 + 'a
        where
            R2: UpcastFrom<R1>
        {}
    };

    // Arguments implemented with contravariance
    (@same [$($A1:ident $A2:ident)+]) => {
        impl<R1, R2, $($A1, $A2),+> UpcastFrom<fn($($A1),+) -> R1> for fn($($A2),+) -> R2
        where
            R2: UpcastFrom<R1>,
            $($A1: UpcastFrom<$A2>,)+
        {}

        impl<'a, R1, R2, $($A1, $A2),+> UpcastFrom<dyn Fn($($A1),+) -> R1 + 'a> for dyn Fn($($A2),+) -> R2 + 'a
        where
            R2: UpcastFrom<R1>,
            $($A1: UpcastFrom<$A2>,)+
        {}

        impl<'a, R1, R2, $($A1, $A2),+> UpcastFrom<dyn FnMut($($A1),+) -> R1 + 'a> for dyn FnMut($($A2),+) -> R2 + 'a
        where
            R2: UpcastFrom<R1>,
            $($A1: UpcastFrom<$A2>,)+
        {}
    };

    // Cross-all: done
    (@cross_all $args:tt $opts:tt) => {};

    // Cross-all: process next
    (@cross_all $args:tt [$($opts:ident)*] [$next_n:tt $next_args:tt $next_opt:ident] $([$rest_n:tt $rest_args:tt $($rest_opt:ident)?])*) => {
        impl_fn_upcasts!(@extend $args [$($opts)* $next_opt]);
        impl_fn_upcasts!(@shrink $args [$($opts)* $next_opt]);
        impl_fn_upcasts!(@cross_all $args [$($opts)* $next_opt] $([$rest_n $rest_args $($rest_opt)?])*);
    };

    // Extend: 0 -> N
    (@extend [] [$($O:ident)+]) => {
        impl<R1, R2, $($O),+> UpcastFrom<fn() -> R1> for fn($($O),+) -> R2
        where
            R2: UpcastFrom<R1>,
            $($O: UpcastFrom<Undefined>,)+
        {}

        impl<'a, R1, R2, $($O),+> UpcastFrom<dyn Fn() -> R1 + 'a> for dyn Fn($($O),+) -> R2 + 'a
        where
            R2: UpcastFrom<R1>,
            $($O: UpcastFrom<Undefined>,)+
        {}

        impl<'a, R1, R2, $($O),+> UpcastFrom<dyn FnMut() -> R1 + 'a> for dyn FnMut($($O),+) -> R2 + 'a
        where
            R2: UpcastFrom<R1>,
            $($O: UpcastFrom<Undefined>,)+
        {}
    };

    // Extend: N -> M
    (@extend [$($A1:ident $A2:ident)+] [$($O:ident)+]) => {
        impl<R1, R2, $($A1, $A2,)+ $($O),+> UpcastFrom<fn($($A1),+) -> R1> for fn($($A2,)+ $($O),+) -> R2
        where
            R2: UpcastFrom<R1>,
            $($A1: UpcastFrom<$A2>,)+  // Contravariant
            $($O: UpcastFrom<Undefined>,)+
        {}

        impl<'a, R1, R2, $($A1, $A2,)+ $($O),+> UpcastFrom<dyn Fn($($A1),+) -> R1 + 'a> for dyn Fn($($A2,)+ $($O),+) -> R2 + 'a
        where
            R2: UpcastFrom<R1>,
            $($A1: UpcastFrom<$A2>,)+  // Contravariant
            $($O: UpcastFrom<Undefined>,)+
        {}

        impl<'a, R1, R2, $($A1, $A2,)+ $($O),+> UpcastFrom<dyn FnMut($($A1),+) -> R1 + 'a> for dyn FnMut($($A2,)+ $($O),+) -> R2 + 'a
        where
            R2: UpcastFrom<R1>,
            $($A1: UpcastFrom<$A2>,)+  // Contravariant
            $($O: UpcastFrom<Undefined>,)+
        {}
    };

    // Shrink: N -> 0
    (@shrink [] [$($O:ident)+]) => {
        impl<R1, R2, $($O),+> UpcastFrom<fn($($O),+) -> R1> for fn() -> R2
        where
            R2: UpcastFrom<R1>,
            $($O: UpcastFrom<Undefined>,)+
        {}

        impl<'a, R1, R2, $($O),+> UpcastFrom<dyn Fn($($O),+) -> R1 + 'a> for dyn Fn() -> R2 + 'a
        where
            R2: UpcastFrom<R1>,
            $($O: UpcastFrom<Undefined>,)+
        {}

        impl<'a, R1, R2, $($O),+> UpcastFrom<dyn FnMut($($O),+) -> R1 + 'a> for dyn FnMut() -> R2 + 'a
        where
            R2: UpcastFrom<R1>,
            $($O: UpcastFrom<Undefined>,)+
        {}
    };

    // Shrink: M -> N
    (@shrink [$($A1:ident $A2:ident)+] [$($O:ident)+]) => {
        impl<R1, R2, $($A1, $A2,)+ $($O),+> UpcastFrom<fn($($A1,)+ $($O),+) -> R1> for fn($($A2),+) -> R2
        where
            R2: UpcastFrom<R1>,
            $($A1: UpcastFrom<$A2>,)+  // Contravariant
            $($O: UpcastFrom<Undefined>,)+
        {}

        impl<'a, R1, R2, $($A1, $A2,)+ $($O),+> UpcastFrom<dyn Fn($($A1,)+ $($O),+) -> R1 + 'a> for dyn Fn($($A2),+) -> R2 + 'a
        where
            R2: UpcastFrom<R1>,
            $($A1: UpcastFrom<$A2>,)+  // Contravariant
            $($O: UpcastFrom<Undefined>,)+
        {}

        impl<'a, R1, R2, $($A1, $A2,)+ $($O),+> UpcastFrom<dyn FnMut($($A1,)+ $($O),+) -> R1 + 'a> for dyn FnMut($($A2),+) -> R2 + 'a
        where
            R2: UpcastFrom<R1>,
            $($A1: UpcastFrom<$A2>,)+  // Contravariant
            $($O: UpcastFrom<Undefined>,)+
        {}
    };
}

impl_fn_upcasts!();

// Copy the above impls down here for where there's only one argument and it's a
// reference. We could add more impls for more kinds of references, but it
// becomes a combinatorial explosion quickly. Let's see how far we can get with
// just this one! Maybe someone else can figure out voodoo so we don't have to
// duplicate.

// We need to allow coherence leak check just for these traits because we're providing separate implementation for `Fn(&A)` variants when `Fn(A)` one already exists.
#[allow(coherence_leak_check)]
const _: () = {
    #[cfg(all(feature = "std", target_arch = "wasm32", panic = "unwind"))]
    closures!(@impl_for_args (&A) RefFromWasmAbi [T: core::panic::UnwindSafe,] &*A::ref_from_abi(A) => A a1 a2 a3 a4);

    #[cfg(not(all(feature = "std", target_arch = "wasm32", panic = "unwind")))]
    closures!(@impl_for_args (&A) RefFromWasmAbi [] &*A::ref_from_abi(A) => A a1 a2 a3 a4);
};

// UpcastFrom impl for ScopedClosure.
// ScopedClosure<T1> upcasts to ScopedClosure<T2> when the underlying closure type T1 upcasts to T2.
// The dyn Fn/FnMut UpcastFrom impls above encode correct variance (covariant return, contravariant args).
//
// The 'a: 'b bound is critical for soundness: it ensures the target lifetime 'b does not
// exceed the source lifetime 'a. Without it, upcast_into could fabricate a
// ScopedClosure<'static, _> from a short-lived ScopedClosure, enabling use-after-free.
impl<'a: 'b, 'b, T1, T2> UpcastFrom<ScopedClosure<'a, T1>> for ScopedClosure<'b, T2>
where
    T1: ?Sized + WasmClosure,
    T2: ?Sized + WasmClosure + UpcastFrom<T1>,
{
}
