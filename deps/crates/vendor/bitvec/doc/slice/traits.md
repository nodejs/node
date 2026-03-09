# `Bit-Slice` Trait Implementations

`BitSlice` implements all of the traits that `[bool]` does, as well as a number
that it does not but are useful for bit-slices. These additions include numeric
formatting, so that any bit-slice can have its memory representation printed, as
well as a permutation of `PartialEq` and `PartialOrd` implementations so that
various `bitvec` containers can be easily compared with each other.
