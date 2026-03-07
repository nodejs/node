# C-Compatible Bit Ordering

This type alias attempts to match the bitfield ordering used by GCC on your
target. The C standard permits ordering of single-bit bitfields in a structure
to be implementation-defined, and GCC has been observed to use Lsb0-ordering on
little-endian processors and Msb0-ordering on big-endian processors.

This has two important caveats:

- ordering of bits in an element is **completely** independent of the ordering
  of constituent bytes in memory. These have nothing to do with each other in
  any way. See [the user guide][0] for more information on memory
  representation.
- GCC wide bitfields on big-endian targets behave as `<T, Lsb0>` bit-slices
  using the `_be` variants of `BitField` accessors. They do not match `Msb0`
  bit-wise ordering.

This type is provided solely as a convenience for narrow use cases that *may*
match GCC’s `std::bitset<N>`. It makes no guarantee about what C compilers for
your target actually do, and you will need to do your own investigation if you
are exchanging a single buffer across FFI in this manner.

[0]: https://bitvecto-rs.github.io/bitvec/memory-representation
