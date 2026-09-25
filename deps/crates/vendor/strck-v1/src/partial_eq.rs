//! Permutations of `PartialEq` between `Ck`, `Check`, `str`, and `String`.
use crate::{Check, Ck, Invariant};

// `Check`s don't need to have the same backing storage to be equal.
impl<'a, I, B1, B2> PartialEq<Check<I, B2>> for &'a Check<I, B1>
where
    I: Invariant,
    B1: AsRef<str>,
    B2: AsRef<str>,
{
    fn eq(&self, other: &Check<I, B2>) -> bool {
        self.as_str() == other.as_str()
    }
}

impl<'a, I, B1, B2> PartialEq<&'a Check<I, B2>> for Check<I, B1>
where
    I: Invariant,
    B1: AsRef<str>,
    B2: AsRef<str>,
{
    fn eq(&self, other: &&'a Check<I, B2>) -> bool {
        self.as_str() == other.as_str()
    }
}

macro_rules! impl_partial_eq {
    (<I> $a:ty = $b:ty) => {
        impl<I: Invariant> PartialEq<$a> for $b {
            fn eq(&self, other: &$a) -> bool {
                AsRef::<str>::as_ref(self) == AsRef::<str>::as_ref(other)
            }
        }
    };
    (<I, B> $a:ty = $b:ty) => {
        impl<I: Invariant, B: AsRef<str>> PartialEq<$a> for $b {
            fn eq(&self, other: &$a) -> bool {
                AsRef::<str>::as_ref(self) == AsRef::<str>::as_ref(other)
            }
        }
    };
    (<$lt:lifetime, I> $a:ty = $b:ty) => {
        impl<$lt, I: Invariant> PartialEq<$a> for $b {
            fn eq(&self, other: &$a) -> bool {
                AsRef::<str>::as_ref(self) == AsRef::<str>::as_ref(other)
            }
        }
    };
    (<$lt:lifetime, I, B> $a:ty = $b:ty) => {
        impl<$lt, I: Invariant, B: AsRef<str>> PartialEq<$a> for $b {
            fn eq(&self, other: &$a) -> bool {
                AsRef::<str>::as_ref(self) == AsRef::<str>::as_ref(other)
            }
        }
    };
}

impl_partial_eq!(<'a, I> Ck<I> = &'a Ck<I>);
impl_partial_eq!(<'a, I> &'a Ck<I> = Ck<I>);

impl_partial_eq!(<I, B> Ck<I> = Check<I, B>);
impl_partial_eq!(<I, B> Check<I, B> = Ck<I>);
impl_partial_eq!(<'a, I, B> &'a Ck<I> = Check<I, B>);
impl_partial_eq!(<'a, I, B> Check<I, B> = &'a Ck<I>);
impl_partial_eq!(<'a, I, B> Ck<I> = &'a Check<I, B>);
impl_partial_eq!(<'a, I, B> &'a Check<I, B> = Ck<I>);

impl_partial_eq!(<I, B> str = Check<I, B>);
impl_partial_eq!(<I, B> Check<I, B> = str);
impl_partial_eq!(<'a, I, B> &'a str = Check<I, B>);
impl_partial_eq!(<'a, I, B> Check<I, B> = &'a str);
impl_partial_eq!(<'a, I, B> str = &'a Check<I, B>);
impl_partial_eq!(<'a, I, B> &'a Check<I, B> = str);

impl_partial_eq!(<I, B> String = Check<I, B>);
impl_partial_eq!(<I, B> Check<I, B> = String);
impl_partial_eq!(<'a, I, B> &'a String = Check<I, B>);
impl_partial_eq!(<'a, I, B> Check<I, B> = &'a String);
impl_partial_eq!(<'a, I, B> String = &'a Check<I, B>);
impl_partial_eq!(<'a, I, B> &'a Check<I, B> = String);

impl_partial_eq!(<I> str = Ck<I>);
impl_partial_eq!(<I> Ck<I> = str);
impl_partial_eq!(<'a, I> &'a str = Ck<I>);
impl_partial_eq!(<'a, I> Ck<I> = &'a str);
impl_partial_eq!(<'a, I> str = &'a Ck<I>);
impl_partial_eq!(<'a, I> &'a Ck<I> = str);

impl_partial_eq!(<I> String = Ck<I>);
impl_partial_eq!(<I> Ck<I> = String);
impl_partial_eq!(<'a, I> &'a String = Ck<I>);
impl_partial_eq!(<'a, I> Ck<I> = &'a String);
impl_partial_eq!(<'a, I> String = &'a Ck<I>);
impl_partial_eq!(<'a, I> &'a Ck<I> = String);
