# Memory Region Description

This module bridges the abstract [`BitSlice`] region to real memory by
segmenting any bit-slice along its maybe-aliased and known-unaliased boundaries.
This segmentation applies to both bit-slice and ordinary-element views of
memory, and can be used to selectively remove alias restrictions or to enable
access to the underlying memory with ordinary types.

The four enums in this module all intentionally have the same variants by name
and shape, in order to maintain textual consistency.

## Memory Layout Model

Any bit-slice resident in memory has one of two major kinds, which the enums in
this module refer to as `Enclave` and `Region`

### Enclave

An `Enclave` layout occurs when a bit-slice is contained entirely within a
single memory element, and does not include either the initial or final semantic
index in its span.

```text
[ 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 ]
[     ^^^^^^^^^^^^^^^^^^^^^     ]
```

In an 8-bit element, a bit-slice is considered to be an `Enclave` if it is
contained entirely in the marked interior bits, and touches *neither* bit 7 nor
bit 0. Wider elements may touch interior byte boundaries, and only restrict bits
0 and `width - 1`.

### Region

A `Region` layout occurs when a bit-slice consists of:

- zero or one half-spanned head element (excludes bit 0, includes `width - 1`)
- zero or more fully-spanned elements body (includes both 0 and `width - 1`)
- zero or one half-spanned tail element (includes bit 0, excludes `width - 1`)

Each of these three sections is optionally present independently of the other
two. That is, in the following three bytes, all of the following bit-slices have
the `Region` layout:

```text
[ 7 6 5 4 3 2 1 0 ] [ 7 6 5 4 3 2 1 0 ] [ 7 6 5 4 3 2 1 0 ]
[                                                         ]

[         h h h h                                         ]
[                     b b b b b b b b                     ]
[                                         t t t t         ]

[         h h h h     t t t t                             ]

[         h h h h     b b b b b b b b                     ]
[                     b b b b b b b b     t t t t         ]
[         h h h h     b b b b b b b b     t t t t         ]
```

1. The empty bit-slice is a region with all of its segments blank.
1. A bit-slice with one element that touches `width - 1` but not 0 has a head,
   but no body or tail.
1. A bit-slice that touches both `0` and `width - 1` of any number of elements
   has a body, but no head or tail.
1. A bit-slice with one element that touches 0 but not `width - 1` has a tail,
   but no head or body.
1. A bit-slice with two elements, that touches neither 0 of the first nor
   `width - 1` of the second (but by definition `width - 1` of the first and 0
   of the second; bit-slices are contiguous) has a head and tail, but no body.

The final three rows show how the individual segments can be composed to
describe all possible bit-slices.

## Aliasing Awareness

The contiguity property of `BitSlice` combines with the `&`/`&mut` exclusion
rules of the Rust language to provide additional information about the state of
the program that allows a given bit-slice to exist.

Specifically, any well-formed Rust program knows that *if* a bit-slice is able
to produce a `Region.body` segment, *then* that body is not aliased by `bitvec`,
and can safely transition to the `T::Unalias` state. Alias-permitting types like
`Cell` and the atomics will never change their types (because `bitvec` cannot
know that there are no views to a region other than what it has been given), but
a tainted `BitSlice<O, u8::Alias>` bit-slice can revert its interior body back
to `u8` and no longer require the alias tainting.

The head and tail segments do not retain their history, and cannot tell whether
they have been created by splitting or by shrinking, so they do not change their
types at all.

## Raw Memory Access

The [`BitDomain`] enum only splits a bit-slice along these boundaries, and
allows a bit-slice view to safely shed aliasing protection added to it by
[`.split_at_mut()`].

The [`Domain`] enum completely sheds its bit-precision views, and reverts to
ordinary element accesses. The body segment is an ordinary Rust slice with no
additional information or restriction; it can be freely used without regard for
any of `bitvec`’s constraints.

In order to preserve the rules that any given bit-slice can never be used to
affect bits outside of its own view of memory, the underlying memory of the head
and tail segments is only made accessible through a [`PartialElement`] reference
guard. This guard is an opaque proxy to the memory location, and holds both a
reference and the bit-mask required to prevent reading from or writing to the
bits outside the scope of the originating bit-slice.

## Generics

This module, and the contents of [`ptr`], make extensive use of a trait-level
mutability and reference tracking system in order to reduce code duplication and
provide a more powerful development environment than would be achieved with
macros.

As such, the trait bounds on types in this module are more intense than the
standard `<T, O>` fare in the crate’s main data structures. However, they are
only ever instantiated with shared or exclusive references, and all of the
bounds are a much more verbose way of saying “a reference, that is maybe-mut and
maybe-slice, of `T`”.

User code does not need to be aware of any of this: the `BitSlice` APIs that
call into this module always result in structures where the complex bounds are
reduced to ordinary slice references.

[`BitDomain`]: BitDomain
[`BitSlice`]: crate::slice::BitSlice
[`Domain`]: Domain
[`PartialElement`]: PartialElement
[`ptr`]: crate::ptr
[`.split_at_mut()`]: crate::slice::BitSlice::split_at_mut
