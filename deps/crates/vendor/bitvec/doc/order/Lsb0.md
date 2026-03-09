# Least-Significant-First Bit Traversal

This type orders the bits in an element with the least significant bit first and
the most significant bit last, in contiguous order across the element.

The guide has [a chapter][0] with more detailed information on the memory
representation this produces.

This is the default type parameter used throughout the crate. If you do not have
a desired memory representation, you should continue to use it, as it provides
the best codegen for bit manipulation.

[0]: https://bitvecto-rs.github.io/bitvec/memory-representation
