// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::Yokeable;
use core::mem;

/// This method casts `yokeable` between `&'a mut Y<'static>` and `&'a mut Y<'a>`,
/// and passes it to `f`.
///
/// See [`Yokeable::transform_mut`] for why this is safe, noting that no `'static` return type
/// can leak data from the cart or Yokeable.
#[inline]
pub(crate) fn transform_mut_yokeable<'a, Y, F, R>(yokeable: &'a mut Y, f: F) -> R
where
    Y: Yokeable<'a>,
    // be VERY CAREFUL changing this signature, it is very nuanced
    F: 'static + for<'b> FnOnce(&'b mut Y::Output) -> R,
    R: 'static,
{
    // Cast away the lifetime of `Y`
    // Safety: this is equivalent to f(transmute(yokeable)), and the documentation of
    // [`Yokeable::transform_mut`] and this function explain why doing so is sound.
    unsafe { f(mem::transmute::<&'a mut Y, &'a mut Y::Output>(yokeable)) }
}
