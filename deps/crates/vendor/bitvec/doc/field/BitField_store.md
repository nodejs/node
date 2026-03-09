# Integer Storing

This method writes an integer into the contents of a bit-slice region. The
region may be shorter than the source integer type, in which case the stored
value will be truncated. On load, it may be zero-extended (unsigned destination)
or sign-extended from the most significant **stored** bit (signed destination).

The region may not be zero bits, nor wider than the source type. Attempting
to store a `u32` into a bit-slice of length 33 will panic the program.

## Operation and Endianness Handling

The value to be stored is broken into segments according to the elements of the
bit-slice receiving it. If the bit-slice contains more than one element, then
the numerical significance of each segment routes to a storage element according
to the target’s endianness:

- little-endian targets consider each *`T` element* to have increasing numerical
  significance, starting with the least-significant segment at the low address
  and ending with the most-significant segment at the high address.
- big-endian targets consider each *`T` element* to have decreasing numerical
  significance, starting with the most-significant segment at the high address
  and ending with the least-significant segment at the low address.

See the documentation for [`.store_le()`] and [`.store_be()`] for more detail on
what this means for how the in-memory representation of bit-slices translates to
stored values.

You must always use the loading method that exactly corresponds to the storing
method previously used to insert data into the bit-slice: same suffix on the
method name (none, `_le`, `_be`) and same integer type. `bitvec` is not required
to, and will not, guarantee round-trip consistency if you change any of these
parameters.

## Type Parameters

- `I`: The integer type being stored. This can be any of the signed or unsigned
  integers.

## Parameters

- `&self`: A bit-slice region whose length is in the range `1 ..= I::BITS`.
- `value`: An integer value whose `self.len()` least numerically significant
  bits will be written into `self`.

## Panics

This panics if `self.len()` is 0, or greater than `I::BITS`.

## Examples

This method is inherently non-portable, and changes behavior depending on the
target characteristics. If your target is little-endian, see [`.store_le()`]; if
your target is big-endian, see [`.store_be()`].

[`.store_be()`]: Self::store_be
[`.store_le()`]: Self::store_le
