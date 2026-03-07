/// A macro for defining #[cfg] if-else statements.
///
/// This is similar to the `if/elif` C preprocessor macro by allowing definition
/// of a cascade of `#[cfg]` cases, emitting the implementation which matches
/// first.
///
/// This allows you to conveniently provide a long list #[cfg]'d blocks of code
/// without having to rewrite each clause multiple times.
macro_rules! cfg_if {
    // match if/else chains with a final `else`
    ($(
        if #[cfg($($meta:meta),*)] { $($it:item)* }
    ) else * else {
        $($it2:item)*
    }) => {
        cfg_if! {
            @__items
            () ;
            $( ( ($($meta),*) ($($it)*) ), )*
            ( () ($($it2)*) ),
        }
    };

    // match if/else chains lacking a final `else`
    (
        if #[cfg($($i_met:meta),*)] { $($i_it:item)* }
        $(
            else if #[cfg($($e_met:meta),*)] { $($e_it:item)* }
        )*
    ) => {
        cfg_if! {
            @__items
            () ;
            ( ($($i_met),*) ($($i_it)*) ),
            $( ( ($($e_met),*) ($($e_it)*) ), )*
            ( () () ),
        }
    };

    // Internal and recursive macro to emit all the items
    //
    // Collects all the negated `cfg`s in a list at the beginning and after the
    // semicolon is all the remaining items
    (@__items ($($not:meta,)*) ; ) => {};
    (@__items ($($not:meta,)*) ; ( ($($m:meta),*) ($($it:item)*) ),
     $($rest:tt)*) => {
        // Emit all items within one block, applying an appropriate #[cfg]. The
        // #[cfg] will require all `$m` matchers specified and must also negate
        // all previous matchers.
        cfg_if! { @__apply cfg(all($($m,)* not(any($($not),*)))), $($it)* }

        // Recurse to emit all other items in `$rest`, and when we do so add all
        // our `$m` matchers to the list of `$not` matchers as future emissions
        // will have to negate everything we just matched as well.
        cfg_if! { @__items ($($not,)* $($m,)*) ; $($rest)* }
    };

    // Internal macro to Apply a cfg attribute to a list of items
    (@__apply $m:meta, $($it:item)*) => {
        $(#[$m] $it)*
    };
}

/// Create an internal crate prelude with `core` reexports and common types.
macro_rules! prelude {
    () => {
        mod types;

        /// Frequently-used types that are available on all platforms
        ///
        /// We need to reexport the core types so this works with `rust-dep-of-std`.
        mod prelude {
            // Exports from `core`
            #[allow(unused_imports)]
            pub(crate) use core::clone::Clone;
            #[allow(unused_imports)]
            pub(crate) use core::default::Default;
            #[allow(unused_imports)]
            pub(crate) use core::marker::{
                Copy,
                Send,
                Sync,
            };
            #[allow(unused_imports)]
            pub(crate) use core::option::Option;
            #[allow(unused_imports)]
            pub(crate) use core::prelude::v1::derive;
            #[allow(unused_imports)]
            pub(crate) use core::{
                cfg,
                fmt,
                hash,
                iter,
                mem,
                ptr,
            };

            #[allow(unused_imports)]
            pub(crate) use fmt::Debug;
            #[allow(unused_imports)]
            pub(crate) use mem::{
                align_of,
                align_of_val,
                size_of,
                size_of_val,
            };

            #[allow(unused_imports)]
            pub(crate) use crate::types::{
                CEnumRepr,
                Padding,
            };
            // Commonly used types defined in this crate
            #[allow(unused_imports)]
            pub(crate) use crate::{
                c_char,
                c_double,
                c_float,
                c_int,
                c_long,
                c_longlong,
                c_short,
                c_uchar,
                c_uint,
                c_ulong,
                c_ulonglong,
                c_ushort,
                c_void,
                intptr_t,
                size_t,
                ssize_t,
                uintptr_t,
            };
        }
    };
}

/// Implement `Clone`, `Copy`, and `Debug` for one or more structs, as well as `PartialEq`, `Eq`,
/// and `Hash` if the `extra_traits` feature is enabled.
///
/// Also mark the type with `repr(C)`.
///
/// Use [`s_no_extra_traits`] for structs where the `extra_traits` feature does not
/// make sense, and for unions.
macro_rules! s {
    ($(
        $(#[$attr:meta])*
        $pub:vis $t:ident $i:ident { $($field:tt)* }
    )*) => ($(
        s!(it: $(#[$attr])* $pub $t $i { $($field)* });
    )*);

    (it: $(#[$attr:meta])* $pub:vis union $i:ident { $($field:tt)* }) => (
        compile_error!("unions cannot derive extra traits, use s_no_extra_traits instead");
    );

    (it: $(#[$attr:meta])* $pub:vis struct $i:ident { $($field:tt)* }) => (
        #[repr(C)]
        #[::core::prelude::v1::derive(
            ::core::clone::Clone,
            ::core::marker::Copy,
            ::core::fmt::Debug,
        )]
        #[cfg_attr(
            feature = "extra_traits",
            ::core::prelude::v1::derive(PartialEq, Eq, Hash)
        )]
        #[allow(deprecated)]
        $(#[$attr])*
        $pub struct $i { $($field)* }
    );
}

/// Implement `Clone`, `Copy`, and `Debug` for a tuple struct, as well as `PartialEq`, `Eq`,
/// and `Hash` if the `extra_traits` feature is enabled.
///
/// Unlike `s!`, this does *not* mark the type with `repr(C)`. Users should provide their own
/// `repr` attribute via `$attr` as necessary.
macro_rules! s_paren {
    ($(
        $(#[$attr:meta])*
        $pub:vis struct $i:ident ( $($field:tt)* );
    )*) => ($(
        #[::core::prelude::v1::derive(
            ::core::clone::Clone,
            ::core::marker::Copy,
            ::core::fmt::Debug,
        )]
        #[cfg_attr(
            feature = "extra_traits",
            ::core::prelude::v1::derive(PartialEq, Eq, Hash)
        )]
        $(#[$attr])*
        $pub struct $i ( $($field)* );
    )*);
}

/// Implement `Clone`, `Copy`, and `Debug` for one or more structs/unions, but exclude `PartialEq`,
/// `Eq`, and `Hash`.
///
/// Also mark the type with `repr(C)`.
///
/// Most structs will prefer to use [`s`].
macro_rules! s_no_extra_traits {
    ($(
        $(#[$attr:meta])*
        $pub:vis $t:ident $i:ident { $($field:tt)* }
    )*) => ($(
        s_no_extra_traits!(it: $(#[$attr])* $pub $t $i { $($field)* });
    )*);

    (it: $(#[$attr:meta])* $pub:vis union $i:ident { $($field:tt)* }) => (
        #[repr(C)]
        #[::core::prelude::v1::derive(
            ::core::clone::Clone,
            ::core::marker::Copy,
        )]
        $(#[$attr])*
        $pub union $i { $($field)* }

        impl ::core::fmt::Debug for $i {
            fn fmt(&self, f: &mut ::core::fmt::Formatter<'_>) -> ::core::fmt::Result {
                f.debug_struct(::core::stringify!($i)).finish_non_exhaustive()
            }
        }
    );

    (it: $(#[$attr:meta])* $pub:vis struct $i:ident { $($field:tt)* }) => (
        #[repr(C)]
        #[::core::prelude::v1::derive(
            ::core::clone::Clone,
            ::core::marker::Copy,
            ::core::fmt::Debug,
        )]
        $(#[$attr])*
        $pub struct $i { $($field)* }
    );
}

/// Create an uninhabited type that can't be constructed. It implements `Debug`, `Clone`,
/// and `Copy`, but these aren't meaningful for extern types so they should eventually
/// be removed.
///
/// Really what we want here is something that also can't be named without indirection (in
/// ADTs or function signatures), but this doesn't exist.
macro_rules! extern_ty {
    ($(
        $(#[$attr:meta])*
        pub enum $i:ident {}
    )*) => ($(
        $(#[$attr])*
        // FIXME(1.0): the type is uninhabited so these traits are unreachable and could be
        // removed.
        #[::core::prelude::v1::derive(
            ::core::clone::Clone,
            ::core::marker::Copy,
            ::core::fmt::Debug,
        )]
        pub enum $i { }
    )*);
}

/// Implement `Clone` and `Copy` for an enum, as well as `Debug`, `Eq`, `Hash`, and
/// `PartialEq` if the `extra_traits` feature is enabled.
// FIXME(#4419): Replace all uses of `e!` with `c_enum!`
macro_rules! e {
    ($(
        $(#[$attr:meta])*
        pub enum $i:ident { $($field:tt)* }
    )*) => ($(
        #[cfg_attr(
            feature = "extra_traits",
            ::core::prelude::v1::derive(Eq, Hash, PartialEq)
        )]
        #[::core::prelude::v1::derive(
            ::core::clone::Clone,
            ::core::marker::Copy,
            ::core::fmt::Debug,
        )]
        $(#[$attr])*
        pub enum $i { $($field)* }
    )*);
}

/// Represent a C enum as Rust constants and a type.
///
/// C enums can't soundly be mapped to Rust enums since C enums are allowed to have duplicates or
/// unlisted values, but this is UB in Rust. This enum doesn't implement any traits, its main
/// purpose is to calculate the correct enum values.
///
/// Use the magic name `#anon` if the C enum doesn't create a type.
///
/// See <https://github.com/rust-lang/libc/issues/4419> for more.
macro_rules! c_enum {
    // Matcher for multiple enums
    ($(
        $(#[repr($repr:ty)])?
        pub enum $($ty_name:ident)? $(#$anon:ident)? {
            $($vis:vis $variant:ident $(= $value:expr)?,)+
        }
    )+) => {
        $(c_enum!(@single;
            $(#[repr($repr)])?
            pub enum $($ty_name)? $(#$anon)? {
                $($vis $variant $(= $value)?,)+
            }
        );)+
    };

    // Matcher for a single enum
    (@single;
        $(#[repr($repr:ty)])?
        pub enum $ty_name:ident {
            $($vis:vis $variant:ident $(= $value:expr)?,)+
        }
    ) => {
        pub type $ty_name = c_enum!(@ty $($repr)?);
        c_enum! {
            @variant;
            ty: $ty_name;
            default: 0;
            variants: [$($vis $variant $(= $value)?,)+]
        }
    };

    // Matcher for a single anonymous enum
    (@single;
        $(#[repr($repr:ty)])?
        pub enum #anon {
            $($vis:vis $variant:ident $(= $value:expr)?,)+
        }
    ) => {
        c_enum! {
            @variant;
            ty: c_enum!(@ty $($repr)?);
            default: 0;
            variants: [$($vis $variant $(= $value)?,)+]
        }
    };

    // Matcher for variants: eats a single variant then recurses with the rest
    (@variant; ty: $_ty_name:ty; default: $_idx:expr; variants: []) => { /* end of the chain */ };
    (
        @variant;
        ty: $ty_name:ty;
        default: $default_val:expr;
        variants: [
            $vis:vis $variant:ident $(= $value:expr)?,
            $($tail:tt)*
        ]
    ) => {
        $vis const $variant: $ty_name = {
            #[allow(unused_variables)]
            let r = $default_val;
            $(let r = $value;)?
            r
        };

        // The next value is always one more than the previous value, unless
        // set explicitly.
        c_enum! {
            @variant;
            ty: $ty_name;
            default: $variant + 1;
            variants: [$($tail)*]
        }
    };

    // Use a specific type if provided, otherwise default to `CEnumRepr`
    (@ty $repr:ty) => { $repr };
    (@ty) => { $crate::prelude::CEnumRepr };
}

/// Define a `unsafe` function.
macro_rules! f {
    ($(
        $(#[$attr:meta])*
        // Less than ideal hack to match either `fn` or `const fn`.
        pub $(fn $i:ident)? $(const fn $const_i:ident)?
        ($($arg:ident: $argty:ty),* $(,)*) -> $ret:ty
            $body:block
    )+) => {$(
        #[inline]
        $(#[$attr])*
        pub $(unsafe extern "C" fn $i)? $(const unsafe extern "C" fn $const_i)?
        ($($arg: $argty),*) -> $ret
            $body
    )+};
}

/// Define a safe function.
macro_rules! safe_f {
    ($(
        $(#[$attr:meta])*
        // Less than ideal hack to match either `fn` or `const fn`.
        pub $(fn $i:ident)? $(const fn $const_i:ident)?
        ($($arg:ident: $argty:ty),* $(,)*) -> $ret:ty
            $body:block
    )+) => {$(
        #[inline]
        $(#[$attr])*
        pub $(extern "C" fn $i)? $(const extern "C" fn $const_i)?
        ($($arg: $argty),*) -> $ret
            $body
    )+};
}

// This macro is used to deprecate items that should be accessed via the mach2 crate
macro_rules! deprecated_mach {
    (pub const $id:ident: $ty:ty = $expr:expr;) => {
        #[deprecated(
            since = "0.2.55",
            note = "Use the `mach2` crate instead",
        )]
        #[allow(deprecated)]
        pub const $id: $ty = $expr;
    };
    ($(pub const $id:ident: $ty:ty = $expr:expr;)*) => {
        $(
            deprecated_mach!(
                pub const $id: $ty = $expr;
            );
        )*
    };
    (pub type $id:ident = $ty:ty;) => {
        #[deprecated(
            since = "0.2.55",
            note = "Use the `mach2` crate instead",
        )]
        #[allow(deprecated)]
        pub type $id = $ty;
    };
    ($(pub type $id:ident = $ty:ty;)*) => {
        $(
            deprecated_mach!(
                pub type $id = $ty;
            );
        )*
    }
}

/// Polyfill for std's `offset_of`.
// FIXME(msrv): stabilized in std in 1.77
macro_rules! offset_of {
    ($Ty:path, $field:ident) => {{
        // Taken from bytemuck, avoids accidentally calling on deref
        #[allow(clippy::unneeded_field_pattern)]
        let $Ty { $field: _, .. };
        let data = core::mem::MaybeUninit::<$Ty>::uninit();
        let ptr = data.as_ptr();
        // nested unsafe, see f!
        #[allow(unused_unsafe)]
        // SAFETY: computed address is inbounds since we have a stack alloc for T
        let fptr = unsafe { core::ptr::addr_of!((*ptr).$field) };
        let off = (fptr as usize).checked_sub(ptr as usize).unwrap();
        core::assert!(off <= core::mem::size_of::<$Ty>());
        off
    }};
}

#[cfg(test)]
mod tests {
    use core::any::TypeId;

    use crate::types::CEnumRepr;

    #[test]
    fn c_enum_basic() {
        // By default, variants get sequential values.
        c_enum! {
            pub enum e {
                VAR0,
                VAR1,
                VAR2,
            }

            // Also check enums that don't create a type.
            pub enum #anon {
                ANON0,
                ANON1,
                ANON2,
            }
        }

        assert_eq!(TypeId::of::<e>(), TypeId::of::<CEnumRepr>());
        assert_eq!(VAR0, 0 as CEnumRepr);
        assert_eq!(VAR1, 1 as CEnumRepr);
        assert_eq!(VAR2, 2 as CEnumRepr);

        assert_eq!(type_id_of_val(&ANON0), TypeId::of::<CEnumRepr>());
        assert_eq!(ANON0, 0 as CEnumRepr);
        assert_eq!(ANON1, 1 as CEnumRepr);
        assert_eq!(ANON2, 2 as CEnumRepr);
    }

    #[test]
    fn c_enum_repr() {
        // Check specifying the integer representation
        c_enum! {
            #[repr(u16)]
            pub enum e {
                VAR0,
            }

            #[repr(u16)]
            pub enum #anon {
                ANON0,
            }
        }

        assert_eq!(TypeId::of::<e>(), TypeId::of::<u16>());
        assert_eq!(VAR0, 0_u16);

        assert_eq!(type_id_of_val(&ANON0), TypeId::of::<u16>());
        assert_eq!(ANON0, 0_u16);
    }

    #[test]
    fn c_enum_set_value() {
        // Setting an explicit value resets the count.
        c_enum! {
            pub enum e {
                VAR2 = 2,
                VAR3,
                VAR4,
            }
        }

        assert_eq!(VAR2, 2 as CEnumRepr);
        assert_eq!(VAR3, 3 as CEnumRepr);
        assert_eq!(VAR4, 4 as CEnumRepr);
    }

    #[test]
    fn c_enum_multiple_set_value() {
        // C enums always take one more than the previous value, unless set to a specific
        // value. Duplicates are allowed.
        c_enum! {
            pub enum e {
                VAR0,
                VAR2_0 = 2,
                VAR3_0,
                VAR4_0,
                VAR2_1 = 2,
                VAR3_1,
                VAR4_1,
            }
        }

        assert_eq!(VAR0, 0 as CEnumRepr);
        assert_eq!(VAR2_0, 2 as CEnumRepr);
        assert_eq!(VAR3_0, 3 as CEnumRepr);
        assert_eq!(VAR4_0, 4 as CEnumRepr);
        assert_eq!(VAR2_1, 2 as CEnumRepr);
        assert_eq!(VAR3_1, 3 as CEnumRepr);
        assert_eq!(VAR4_1, 4 as CEnumRepr);
    }

    #[test]
    fn c_enum_vis() {
        mod priv1 {
            c_enum! {
                #[repr(u8)]
                pub enum e1 {
                    PRIV_ON_1 = 10,
                    // Variant should still be usable within its visibility
                    pub PUB1 = PRIV_ON_1 * 2,
                }
            }
        }
        mod priv2 {
            c_enum! {
                #[repr(u16)]
                pub enum e2 {
                    pub PRIV_ON_1 = 42,
                    pub PUB2 = PRIV_ON_1 * 2,
                }
            }
        }

        use priv1::*;
        use priv2::*;

        assert_eq!(TypeId::of::<e1>(), TypeId::of::<u8>());
        assert_eq!(TypeId::of::<e2>(), TypeId::of::<u16>());
        assert_eq!(PUB1, 10u8 * 2);
        assert_eq!(PUB2, 42u16 * 2);
        // Verify that the default is private. If `PRIV_ON_1` was actually public in `priv1`, this
        // would be an ambiguous import and/or type mismatch error.
        assert_eq!(PRIV_ON_1, 42u16);
    }

    fn type_id_of_val<T: 'static>(_: &T) -> TypeId {
        TypeId::of::<T>()
    }

    #[test]
    fn test_offset_of() {
        #[repr(C)]
        struct Off1 {
            a: u8,
            b: u32,
            c: Off2,
            d: u64,
        }

        #[repr(C)]
        #[repr(align(128))]
        struct Off2 {}

        assert_eq!(core::mem::offset_of!(Off1, a), offset_of!(Off1, a));
        assert_eq!(core::mem::offset_of!(Off1, b), offset_of!(Off1, b));
        assert_eq!(core::mem::offset_of!(Off1, c), offset_of!(Off1, c));
        assert_eq!(core::mem::offset_of!(Off1, d), offset_of!(Off1, d));
    }
}

#[cfg(test)]
#[allow(unused)]
mod macro_checks {
    s! {
        pub struct S1 {
            pub a: u32,
            b: u32,
        }

        struct S1Priv {
            pub a: u32,
            b: u32,
        }
    }

    s_no_extra_traits! {
        pub struct S2 {
            pub a: u32,
            b: u32,
        }

        struct S2Priv {
            pub a: u32,
            b: u32,
        }

        pub union U2 {
            pub a: u32,
            b: f32,
        }

        union U2Priv {
            pub a: u32,
            b: f32,
        }
    }
}
