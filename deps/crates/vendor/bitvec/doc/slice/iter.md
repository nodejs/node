# Bit-Slice Iteration

Like the standard-library slice, this module contains a great deal of
specialized iterators. In addition to the ports of the iterators in
[`core::slice`], this also defines iterators that seek out indices of set or
cleared bits in sparse collections.

Each iterator here is documented most extensively on the [`BitSlice`] method
that produces it, and has only light documentation on its own type or inherent
methods.

[`BitSlice`]: super::BitSlice
[`core::slice`]: core::slice
