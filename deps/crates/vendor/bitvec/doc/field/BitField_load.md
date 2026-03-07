# Integer Loading

This method reads the contents of a bit-slice region as an integer. The region
may be shorter than the destination integer type, in which case the loaded value
will be zero-extended (when `I: Unsigned`) or sign-extended from the most
significant loaded bit (when `I: Signed`).

The region may not be zero bits, nor wider than the destination type. Attempting
to load a `u32` from a bit-slice of length 33 will panic the program.

## Operation and Endianness Handling

Each element in the bit-slice contains a segment of the value to be loaded. If
the bit-slice contains more than one element, then the numerical significance of
each loaded segment is interpreted according to the target’s endianness:

- little-endian targets consider each *`T` element* to have increasing numerical
  significance, starting with the least-significant segment at the low address
  and ending with the most-significant segment at the high address.
- big-endian targets consider each *`T` element* to have decreasing numerical
  significance, starting with the most-significant segment at the high address
  and ending with the least-significant segment at the low address.

See the documentation for [`.load_le()`] and [`.load_be()`] for more detail on
what this means for how the in-memory representation of bit-slices translates to
loaded values.

You must always use the loading method that exactly corresponds to the storing
method previously used to insert data into the bit-slice: same suffix on the
method name (none, `_le`, `_be`) and same integer type. `bitvec` is not required
to, and will not, guarantee round-trip consistency if you change any of these
parameters.

## Type Parameters

- `I`: The integer type being loaded. This can be any of the signed or unsigned
  integers.

## Parameters

- `&self`: A bit-slice region whose length is in the range `1 ..= I::BITS`.

## Returns

The contents of the bit-slice, interpreted as an integer.

## Panics

This panics if `self.len()` is 0, or greater than `I::BITS`.

## Examples

This method is inherently non-portable, and changes behavior depending on the
target characteristics. If your target is little-endian, see [`.load_le()`]; if
your target is big-endian, see [`.load_be()`].

[`.load_be()`]: Self::load_be
[`.load_le()`]: Self::load_le
