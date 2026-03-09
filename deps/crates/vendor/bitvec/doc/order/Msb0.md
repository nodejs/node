# Most-Significant-First Bit Traversal

This type orders the bits in an element with the most significant bit first and
the least significant bit last, in contiguous order across the element.

The guide has [a chapter][0] with more detailed information on the memory
representation this produces.

This type likely matches the ordering of bits you would expect to see in a
debugger, but has worse codegen than `Lsb0`, and is not encouraged if you are
not doing direct memory inspection.

[0]: https://bitvecto-rs.github.io/bitvec/memory-representation
