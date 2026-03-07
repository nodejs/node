# Bit-Pointer Ranges

This module defines ports of the `Range` type family to work with `BitPtr`s.
Rust’s own ranges have unstable internal details that make them awkward to use
within the standard library, and essentially impossible outside it, with
anything other than the numeric fundamentals.

In particular, `bitvec` uses a half-open range of `BitPtr`s to represent
C++-style dual-pointer memory regions (such as `BitSlice` iterators). Rust’s own
slice iterators also do this, but because `*T` does not implement the [`Step`]
trait, the standard library duplicates some work done by `Range` types in the
slice iterators just to be able to alter the views.

As such, `Range<BitPtr<_, _, _>>` has the same functionality as
`Range<*const _>`: almost none. As this is undesirable, this module defines
equivalent types that implement the full desired behavior of a pointer range.
These are primarily used as crate internals, but may also be of interest to
users.

[`Step`]: core::iter::Step
