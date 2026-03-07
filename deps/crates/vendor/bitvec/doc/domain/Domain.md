# Bit-Slice Element Partitioning

This structure provides the bridge between bit-precision memory modeling and
element-precision memory manipulation. It allows a bit-slice to provide a safe
and correct view of the underlying memory elements, without exposing the values,
or permitting mutation, of bits outside a bit-slice’s control but within the
elements the bit-slice uses.

Nearly all memory access that is not related to single-bit access goes through
this structure, and it is highly likely to be in your hot path. Its code is a
perpetual topic of optimization, and improvements are always welcome.

This is essentially a fully-decoded `BitSpan` handle, in that it addresses
memory elements directly and contains the bit-masks needed to selectively
interact with them. It is therefore by necessity a large structure, and is
usually only alive for a short time. It has a minimal API, as most of its
logical operations are attached to `BitSlice`, and merely route through it.

If your application cannot afford the cost of repeated `Domain` construction,
please [file an issue][0].

## Memory Model and Variants

A given `BitSlice` has essentially two possibilities for where it resides in
real memory:

- it can reside entirely in the interior of a exactly one memory element,
  touching neither edge bit, or
- it can touch at least one edge bit of zero or more elements.

These states correspond to the `Enclave` and `Region` variants, respectively.

When a `BitSlice` has only partial control of a given memory element, that
element can only be accessed through the bit-slice’s provenance by a
[`PartialElement`] handle. This handle is an appropriately-guarded reference to
the underlying element, as well as mask information needed to interact with the
raw bits and to manipulate the numerical contents. Each `PartialElement` guard
carries permissions for *its own bits* within the guarded element, independently
of any other handle that may access the element, and all handles are
appropriately synchronized with each other to prevent race conditions.

The `Enclave` variant is a single `PartialElement`. The `Region` variant is more
complex. It has:

1. an optional `PartialElement` for the case where the bit-slice only partially
   occupies the lowest-addressed memory element it governs, starting after
   bit-index 0 and extending up to the maximal bit-index,
1. a slice of zero or more fully-occupied memory elements,
1. an optional `PartialElement` for the case where it only partially occupies
   the highest-addressed memory element it governs, starting at bit-index 0 and
   ending before the maximal.

## Usage

Once created, match upon a `Domain` to access its fields. Each `PartialElement`
has a [`.load_value()`][`PartialElement::load_value`] method that produces its
stored value (with all ungoverned bits cleared to 0), and a `.store_value()`
that writes into its governed bits. If present, the fully-occupied slice can be
used as normal.

[0]: https://github.com/bitvecto-rs/bitvec/issues/new
[`PartialElement`]: crate::domain::PartialElement
[`PartialElement::load_value`]: crate::domain::PartialElement::load_value
