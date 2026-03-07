# Bit-Slice Partitioning

This enum partitions a bit-slice into its head- and tail- edge bit-slices, and
its interior body bit-slice, according to the definitions laid out in the module
documentation.

It fragments a [`BitSlice`] into smaller `BitSlice`s, and allows the interior
bit-slice to become `::Unalias`ed. This is useful when you need to retain a
bit-slice view of memory, but wish to remove synchronization costs imposed by a
prior call to [`.split_at_mut()`] for as much of the bit-slice as possible.

## Why Not `Option`?

The `Enclave` variant always contains as its single field the exact bit-slice
that created the `Enclave`. As such, this type is easily replaceäble with an
`Option` of the `Region` variant, which when `None` is understood to be the
original.

This exists as a dedicated enum, even with a technically useless variant, in
order to mirror the shape of the element-domain enum. This type should be
understood as a shortcut to the end result of splitting by element-domain, then
mapping each `PartialElement` and slice back into `BitSlice`s, rather than
testing whether a bit-slice can be split on alias boundaries.

You can get the alternate behavior, of testing whether or not a bit-slice can be
split into a `Region` or is unsplittable, by calling `.bit_domain().region()`
to produce exactly such an `Option`.

[`BitSlice`]: crate::slice::BitSlice
[`.split_at_mut()`]: crate::slice::BitSlice::split_at_mut
