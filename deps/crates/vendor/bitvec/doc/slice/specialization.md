# Bit-Slice Specialization

This module provides specialized implementations for `BitSlice<T, Lsb0>` and
`BitSlice<T, Msb0>`. These implementations are able to use knowledge of their
bit-ordering behavior to be faster and operate in batches.

Since true specialization is not available in the language yet, this uses the
`any::TypeId` system to detect if a type parameter is identical to a known type
and conditionally force a cast and branch. Since type identifiers are compiler
intrinsics produced during compilation, during monomorphization each branch
has its conditional replaced with a compile-time constant value. The `if true`
branch is retained, the `if false` branches are discarded, and the
monomorphization proceeds with the specialized function replacing the generic
body.

The `.coerce()` and `.coerce_mut()` methods detect whether a bit-slice with
generic type parameters matches statically-known type parameters, and return an
`Option` of a value-identical bit-slice reference with the statically-known type
parameters which can then invoke a specialization method.

Generic methods can be specialized whenever their implementation is dependent on
the `O` type parameter and the map of positions the ordering produces is
easily legible to processor instructions. Because language-level specialization
is unavailable, dispatch is only done in `bitvec` and cannot be extended to
third-party crates.

The `lsb0` and `msb0` modules should have identical symbols present. For
implementation, remember that `Lsb0` and `Msb0` orderings **are** correlated
with little-endian and big-endian byte operations!
