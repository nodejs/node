# Bit View Adapters

This module provides extension traits that view ordinary memory as
bit-addressable.

The [`&BitSlice`][0] type is a reference view over memory managed elsewhere. The
inherent constructors are awkward to call, as they require function syntax and
a redundant type argument (the `T: BitStore` parameter is already known by the
data being viewed). As an alternative, the [`BitView`] trait provides methods
on `BitStore` scalars and arrays that are more convenient to create `BitSlice`
reference views.

Additionally, [`BitViewSized`], [`AsBits`], and [`AsMutBits`] inform the type
system about types that can be used as [`BitArray`] storage, immutably viewed as
bits, or mutably viewed as bits, respectively.

[0]: crate::slice::BitSlice
[`AsBits`]: self::AsBits
[`AsMutBits`]: self::AsMutBits
[`BitArray`]: crate::array::BitArray
[`BitView`]: self::BitView
[`BitViewSized`]: self::BitViewSized
