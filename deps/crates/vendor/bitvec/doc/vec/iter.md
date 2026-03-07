# Bit-Vector Iteration

This module provides iteration protocols for `BitVec`, including:

- extension of existing bit-vectors with new data
- collection of data into new bit-vectors
- iteration over the contents of a bit-vector
- draining and splicing iteration over parts of a bit-vector.

`BitVec` implements `Extend` and `FromIterator` for both sources of individual
bits and sources of `T` memory elements.

The by-value `bool` iterator is defined in `boxed::iter`, rather than here. The
`Drain` and `Splice` iterators remain here in their original location.
