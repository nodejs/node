# Bit-Slice Indexing

This trait, like its mirror in `core`, unifies various types that can be used to
index within a bit-slice. Individual `usize` indices can refer to exactly one
bit within a bit-slice, and `R: RangeBounds<usize>` ranges can refer to
subslices of any length within a bit-slice.

The three operations (get, get unchecked, and index) reflect the three theories
of lookup within a collection: fallible, pre-checked, and crashing on failure.

You will likely not use this trait directly; its methods all have corresponding
methods on [`BitSlice`] that delegate to particular implementations of it.

## Original

[`slice::SliceIndex`](core::slice::SliceIndex)

## API Differences

The [`SliceIndex::Output`] type is not usable here, because `bitvec` cannot
manifest a `&mut bool` reference. Work to unify referential values in the trait
system is ongoing, and in the future this functionality *may* be approximated.

Instead, this uses two output types, [`Immut`] and [`Mut`], that are the
referential structures produced by indexing immutably or mutably, respectively.
This allows the range implementations to produce `&/mut BitSlice` as expected,
while `usize` produces the proxy structure.

[`Immut`]: Self::Immut
[`Mut`]: Self::Mut
[`SliceIndex::Output`]: core::slice::SliceIndex::Output
