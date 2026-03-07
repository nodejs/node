# Boolean Arithmetic

This merges another bit-slice into `self` with a Boolean arithmetic operation.
If the other bit-slice is shorter than `self`, it is zero-extended. For `BitAnd`,
this clears all excess bits of `self` to `0`; for `BitOr` and `BitXor`, it
leaves them untouched

## Behavior

The Boolean operation proceeds across each bit-slice in iteration order. This is
`3O(n)` in the length of the shorter of `self` and `rhs`. However, it can be
accelerated if `rhs` has the same type parameters as `self`, and both are using
one of the orderings provided by `bitvec`. In this case, the implementation
specializes to use `BitField` batch operations to operate on the slices one word
at a time, rather than one bit.

Acceleration is not currently provided for custom bit-orderings that use the
same storage type.

## Pre-`1.0` Behavior

In the `0.` development series, Boolean arithmetic was implemented against all
`I: Iterator<Item = bool>`. This allowed code such as `bits |= [false, true];`,
but forbad acceleration in the most common use case (combining two bit-slices)
because `BitSlice` is not such an iterator.

Usage surveys indicate that it is better for the arithmetic operators to operate
on bit-slices, and to allow the possibility of specialized acceleration, rather
than to allow folding against any iterator of `bool`s.

If pre-`1.0` code relies on this behavior specifically, and has non-`BitSlice`
arguments to the Boolean sigils, then they will need to be replaced with the
equivalent loop.

## Examples

```rust
use bitvec::prelude::*;

let a = bits![mut 0, 0, 1, 1];
let b = bits![    0, 1, 0, 1];

*a ^= b;
assert_eq!(a, bits![0, 1, 1, 0]);

let c = bits![mut 0, 0, 1, 1];
let d = [false, true, false, true];

// no longer allowed
// c &= d.into_iter().by_vals();
for (mut c, d) in c.iter_mut().zip(d.into_iter())
{
  *c ^= d;
}
assert_eq!(c, bits![0, 1, 1, 0]);
```
