use std::{cell::RefCell, rc::Rc, sync::Arc};

use num_bigint::BigInt;

use crate::{BytePos, Span};

/// Derive with `#[derive(EqIgnoreSpan)]`.
pub trait EqIgnoreSpan {
    fn eq_ignore_span(&self, other: &Self) -> bool;
}

impl EqIgnoreSpan for Span {
    /// Always returns true
    #[inline]
    fn eq_ignore_span(&self, _: &Self) -> bool {
        true
    }
}

impl EqIgnoreSpan for swc_atoms::Atom {
    #[inline]
    fn eq_ignore_span(&self, r: &Self) -> bool {
        self == r
    }
}

impl EqIgnoreSpan for swc_atoms::Wtf8Atom {
    #[inline]
    fn eq_ignore_span(&self, r: &Self) -> bool {
        self == r
    }
}

impl<T> EqIgnoreSpan for [T]
where
    T: EqIgnoreSpan,
{
    fn eq_ignore_span(&self, other: &Self) -> bool {
        self.len() == other.len()
            && self
                .iter()
                .zip(other.iter())
                .all(|(a, b)| a.eq_ignore_span(b))
    }
}

impl<T> EqIgnoreSpan for Option<T>
where
    T: EqIgnoreSpan,
{
    fn eq_ignore_span(&self, other: &Self) -> bool {
        match (self, other) {
            (Some(l), Some(r)) => l.eq_ignore_span(r),
            (None, None) => true,
            _ => false,
        }
    }
}

impl<T> EqIgnoreSpan for Vec<T>
where
    T: EqIgnoreSpan,
{
    fn eq_ignore_span(&self, other: &Self) -> bool {
        self.len() == other.len()
            && self
                .iter()
                .zip(other.iter())
                .all(|(a, b)| a.eq_ignore_span(b))
    }
}

// nightly_only!(
//     impl<T> EqIgnoreSpan for swc_allocator::vec::Vec<T>
//     where
//         T: EqIgnoreSpan,
//     {
//         fn eq_ignore_span(&self, other: &Self) -> bool {
//             self.len() == other.len()
//                 && self
//                     .iter()
//                     .zip(other.iter())
//                     .all(|(a, b)| a.eq_ignore_span(b))
//         }
//     }
// );

/// Derive with `#[derive(TypeEq)]`.
pub trait TypeEq {
    /// **Note**: This method should return `true` for non-type values.
    fn type_eq(&self, other: &Self) -> bool;
}

impl TypeEq for Span {
    /// Always returns true
    #[inline]
    fn type_eq(&self, _: &Self) -> bool {
        true
    }
}

impl<T> TypeEq for Option<T>
where
    T: TypeEq,
{
    fn type_eq(&self, other: &Self) -> bool {
        match (self, other) {
            (Some(l), Some(r)) => l.type_eq(r),
            (None, None) => true,
            _ => false,
        }
    }
}

impl<T> TypeEq for Vec<T>
where
    T: TypeEq,
{
    fn type_eq(&self, other: &Self) -> bool {
        self.len() == other.len() && self.iter().zip(other.iter()).all(|(a, b)| a.type_eq(b))
    }
}

/// Implement traits using PartialEq
macro_rules! eq {
    ($T:ty) => {
        impl EqIgnoreSpan for $T {
            #[inline]
            fn eq_ignore_span(&self, other: &Self) -> bool {
                self == other
            }
        }

        impl TypeEq for $T {
            #[inline]
            fn type_eq(&self, other: &Self) -> bool {
                self == other
            }
        }
    };

    (
        $(
            $T:ty
        ),*
    ) => {
        $(
            eq!($T);
        )*
    };
}

eq!(BytePos);
eq!(bool);
eq!(usize, u8, u16, u32, u64, u128);
eq!(isize, i8, i16, i32, i64, i128);
eq!(f32, f64);
eq!(char, str, String);

macro_rules! deref {
    ($T:ident) => {
        impl<N> EqIgnoreSpan for $T<N>
        where
            N: EqIgnoreSpan,
        {
            #[inline]
            fn eq_ignore_span(&self, other: &Self) -> bool {
                (**self).eq_ignore_span(&**other)
            }
        }

        impl<N> TypeEq for $T<N>
        where
            N: TypeEq,
        {
            #[inline]
            fn type_eq(&self, other: &Self) -> bool {
                (**self).type_eq(&**other)
            }
        }
    };


    (
        $(
            $T:ident
        ),*
    ) => {
        $(
            deref!($T);
        )*
    };
}

deref!(Box, Rc, Arc);

// swc_allocator::nightly_only!(
//     impl<N> EqIgnoreSpan for swc_allocator::boxed::Box<N>
//     where
//         N: EqIgnoreSpan,
//     {
//         #[inline]
//         fn eq_ignore_span(&self, other: &Self) -> bool {
//             (**self).eq_ignore_span(&**other)
//         }
//     }

//     impl<N> TypeEq for swc_allocator::boxed::Box<N>
//     where
//         N: TypeEq,
//     {
//         #[inline]
//         fn type_eq(&self, other: &Self) -> bool {
//             (**self).type_eq(&**other)
//         }
//     }
// );

impl<N> EqIgnoreSpan for &N
where
    N: EqIgnoreSpan,
{
    #[inline]
    fn eq_ignore_span(&self, other: &Self) -> bool {
        (**self).eq_ignore_span(&**other)
    }
}

impl<N> TypeEq for &N
where
    N: TypeEq,
{
    #[inline]
    fn type_eq(&self, other: &Self) -> bool {
        (**self).type_eq(&**other)
    }
}

impl<N> EqIgnoreSpan for RefCell<N>
where
    N: EqIgnoreSpan,
{
    fn eq_ignore_span(&self, other: &Self) -> bool {
        self.borrow().eq_ignore_span(&*other.borrow())
    }
}

impl<N> TypeEq for RefCell<N>
where
    N: TypeEq,
{
    fn type_eq(&self, other: &Self) -> bool {
        self.borrow().type_eq(&*other.borrow())
    }
}

impl EqIgnoreSpan for BigInt {
    fn eq_ignore_span(&self, other: &Self) -> bool {
        self == other
    }
}
impl TypeEq for BigInt {
    fn type_eq(&self, other: &Self) -> bool {
        self == other
    }
}

macro_rules! tuple {
    (
        $num:tt: $F:ident
    ) => {};


    (
        $first:tt: $F:ident,
        $(
            $num:tt: $N:ident
        ),*
    ) =>{
        tuple!($($num: $N),*);

        impl<$F: EqIgnoreSpan, $($N: EqIgnoreSpan),*> EqIgnoreSpan for ($F, $($N,)*) {
            fn eq_ignore_span(&self,rhs: &Self) -> bool {
                self.$first.eq_ignore_span(&rhs.$first) &&
                $(
                    self.$num.eq_ignore_span(&rhs.$num)
                )
                && *
            }
        }

        impl<$F: TypeEq, $($N: TypeEq),*> TypeEq for ($F, $($N,)*) {
            fn type_eq(&self,rhs: &Self) -> bool {
                self.$first.type_eq(&rhs.$first) &&
                $(
                    self.$num.type_eq(&rhs.$num)
                )
                && *
            }
        }
    };
}

tuple!(
    25: Z,
    24: Y,
    23: X,
    22: W,
    21: V,
    20: U,
    19: T,
    18: S,
    17: R,
    16: Q,
    15: P,
    14: O,
    13: N,
    12: M,
    11: L,
    10: K,
    9: J,
    8: I,
    7: H,
    6: G,
    5: F,
    4: E,
    3: D,
    2: C,
    1: B,
    0: A
);
