# Single-Bit Pointers

This module defines single-bit pointers, which are required to be structures in
their own right and do not have an encoded form.

These pointers should generally not be used; [`BitSlice`] is more likely to be
correct and have better performance. They are provided for consistency, not for
hidden optimizations.

[`BitSlice`]: crate::slice::BitSlice
