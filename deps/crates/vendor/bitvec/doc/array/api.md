# Port of Array Inherent Methods

This module ports the inherent methods available on the [array] fundamental
type.

As of 1.56, only `.map()` is stable. The `.as_slice()` and `.as_mut_slice()`
methods are ported, as the *behavior* has always been stable, and only the name
is new.

The remaining methods (as of 1.56, `.each_mut()`, `.each_ref()`, `.zip()`) are
not ported. While `BitArray` is capable of implementing their behavior with the
existing crate APIs, the `const`-generic system is not yet able to allow
construction of an array whose length is dependent on an associated `const` in a
type parameter.

These methods will not be available until the `const`-generic system improves
enough for `bitvec 2` to use the proper `BitArray` API.

[array]: https://doc.rust-lang.org/std/primitive.array.html
