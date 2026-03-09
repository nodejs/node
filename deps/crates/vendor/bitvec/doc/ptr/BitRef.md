# Proxy Bit-Reference

This structure simulates `&/mut bool` within `BitSlice` regions. It is analogous
to the C++ type [`std::bitset<N>::reference`][0].

This type wraps a [`BitPtr`] and caches a `bool` in one of the remaining padding
bytes. It is then able to freely give out references to its cached `bool`, and
commits the cached value back to the proxied location when dropped.

## Original

This is semantically equivalent to `&'a bool` or `&'a mut bool`.

## Quirks

Because this type has both a lifetime and a destructor, it can introduce an
uncommon syntax error condition in Rust. When an expression that produces this
type is in the final expression of a block, including if that expression is used
as a condition in a `match`, `if let`, or `if`, then the compiler will attempt
to extend the drop scope of this type to the outside of the block. This causes a
lifetime mismatch error if the source region from which this proxy is produced
begins its lifetime inside the block.

If you get a compiler error that this type causes something to be dropped while
borrowed, you can end the borrow by putting any expression-ending syntax element
after the offending expression that produces this type, including a semicolon or
an item definition.

## Examples

```rust
use bitvec::prelude::*;

let bits = bits![mut 0; 2];

let (left, right) = bits.split_at_mut(1);
let mut first = left.get_mut(0).unwrap();
let second = right.get_mut(0).unwrap();

// Writing through a dereference requires a `mut` binding.
*first = true;
// Writing through the explicit method call does not.
second.commit(true);

drop(first); // It’s not a reference, so NLL does not apply!
assert_eq!(bits, bits![1; 2]);
```

[0]: https://en.cppreference.com/w/cpp/utility/bitset/reference
