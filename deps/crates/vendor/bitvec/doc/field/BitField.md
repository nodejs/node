# C-Style Bit-Field Access

This trait describes data transfer between a [`BitSlice`] region and an ordinary
integer. It is not intended for use by any other types than the data structures
in this crate.

The methods in this trait always operate on the `bitslice.len()` least
significant bits of an integer, and ignore any remaining high bits. When
loading, any excess high bits not copied out of a bit-slice are cleared to zero.

## Usage

The trait methods all panic if called on a bit-slice that is wider than the
integer type being transferred. As such, the first step is generally to subslice
a larger data structure into exactly the region used for storage, with
`bits[start .. end]`. Then, call the desired method on the narrowed bit-slice.

## Target-Specific Behavior

If you do not care about the details of the memory layout of stored values, you
can use the [`.load()`] and [`.store()`] unadorned methods. These each forward
to their `_le` variant on little-endian targets, and their `_be` variant on
big-endian. These will provide a reasonable default behavior, but do not
guarantee a stable memory layout, and their buffers are not suitable for
de/serialization.

If you require a stable memory layout, you will need to choose a `BitSlice`
with a fixed `O: BitOrder` type parameter (not `LocalBits`), and use a fixed
method suffix (`_le` or `_be`). You should *probably* also use `u8` as your
`T: BitStore` parameter, in order to avoid any byte-ordering issues. `bitvec`
never interferes with processor concepts of wide-integer layout, and always
relies on the target machine’s behavior for this work.

## Element- and Bit- Ordering Combinations

Remember: the `_le` and `_be` method suffixes are completely independent of the
`Lsb0` and `Msb0` types! `_le` and `_be` refer to the order in which successive
memory elements are considered to gain numerical significance, while `BitOrder`
refers only to the order of successive bits in *one* memory element.

The `BitField` and `BitOrder` traits are ***not*** related.

When a load or store operation is contained in only one memory element, then the
`_le` and `_be` methods have the same behavior: they exchange an integer value
with the segment of the element that its governing `BitSlice` considers live.
Only when a `BitSlice` covers multiple elements does the distinction come into
play.

The `_le` methods consider numerical significance to start low and increase with
increasing memory address, while the `_be` methods consider numerical
significance to start high and *decrease* with increasing memory address. This
distinction affects the order in which memory elements are used to load or store
segments of the exchanged integer value.

Each trait method has detailed visual diagrams in its documentation.
Additionally, each *implementation*’s documentation has diagrams that show what
the governed bit-sections of elements are! Be sure to check each, or to run the
demonstration with `cargo run --example bitfield`.

## Bitfield Value Types

When interacting with a bit-slice as a C-style bitfield, it can *only* store the
signed or unsigned integer types. No other type is permitted, as the
implementation relies on the 2’s-complement significance behavior of processor
integers. Record types and floating-point numbers do not have this property, and
thus have no sensible default protocol for truncation and un/marshalling that
`bitvec` can use.

If you have such a protocol, you may implement it yourself by providing a
de/serialization transform between your type and the integers. For instance, a
numerically-correct protocol to store floating-point numbers in bitfields might
look like this:

```rust
use bitvec::mem::bits_of;
use funty::Floating;

fn to_storage<F>(num: F, width: usize) -> F::Raw
where F: Floating {
  num.to_bits() >> (bits_of::<F>() - width)
}

fn from_storage<F>(val: F::Raw, width: usize) -> F
where F: Floating {
  F::from_bits(val << (bits_of::<F>() - width))
}
```

This implements truncation in the least-significant bits, where floating-point
numbers store disposable bits in the mantissa, rather than in the
most-significant bits which contain the sign, exponent, and most significant
portion of the mantissa.

[`BitSlice`]: crate::slice::BitSlice
[`.load()`]: Self::load
[`.store()`]: Self::store
