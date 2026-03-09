use std::{
    borrow::Cow,
    mem::{forget, ManuallyDrop},
};

use crate::{
    dynamic::{global_atom, global_wtf8_atom},
    wtf8::{Wtf8, Wtf8Buf},
    Atom, Wtf8Atom,
};

macro_rules! direct_from_impl {
    ($T:ty) => {
        impl From<$T> for Atom {
            fn from(s: $T) -> Self {
                global_atom(&s)
            }
        }
    };
}

direct_from_impl!(&'_ str);
direct_from_impl!(Cow<'_, str>);
direct_from_impl!(String);

impl From<Box<str>> for crate::Atom {
    fn from(s: Box<str>) -> Self {
        global_atom(&s)
    }
}

macro_rules! direct_from_impl_wtf8 {
    ($T:ty) => {
        impl From<$T> for Wtf8Atom {
            fn from(s: $T) -> Self {
                global_wtf8_atom(s.as_bytes())
            }
        }
    };
}

direct_from_impl_wtf8!(&'_ str);
direct_from_impl_wtf8!(Cow<'_, str>);
direct_from_impl_wtf8!(String);
direct_from_impl_wtf8!(&'_ Wtf8);
direct_from_impl_wtf8!(Wtf8Buf);

impl From<&Atom> for crate::Wtf8Atom {
    fn from(s: &Atom) -> Self {
        forget(s.clone());
        Wtf8Atom {
            unsafe_data: s.unsafe_data,
        }
    }
}

impl From<Atom> for crate::Wtf8Atom {
    fn from(s: Atom) -> Self {
        let s = ManuallyDrop::new(s);
        Wtf8Atom {
            unsafe_data: s.unsafe_data,
        }
    }
}

impl From<Box<str>> for crate::Wtf8Atom {
    fn from(s: Box<str>) -> Self {
        global_wtf8_atom(s.as_bytes())
    }
}
