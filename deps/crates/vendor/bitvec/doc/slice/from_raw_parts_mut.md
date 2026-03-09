# Raw Bit-Slice Construction

This produces an `&mut BitSlice<T, O>` reference handle from a
`BitPtr<Mut, T, O>` bit-pointer and a length.

## Parameters

1. `data`: a bit-pointer to the starting bit of the produced bit-slice. This
   should generally have been produced by `BitSlice::as_mut_ptr`, but you are
   able to construct these pointers directly if you wish.
1. `len`: the number of bits, beginning at `data`, that the produced bit-slice
   includes. This value cannot depart an allocation area, or exceed `BitSlice`’s
   encoding limitations.

## Returns

This returns a `Result`, because it can detect and gracefully fail if `len`
is too large, or if `data` is ill-formed. This fails if it has an error while
encoding the `&mut BitSlice`, and succeeds if it is able to produce a correctly
encoded value.

Note that this is not able to detect semantic violations of the memory model.
You are responsible for upholding memory safety.

## Original

[`slice::from_raw_parts_mut`](core::slice::from_raw_parts_mut)

## API Differences

This takes a [`BitPtr<Mut, T, O>`] instead of a hypothetical `*mut Bit`, because
`bitvec` is not able to express raw Rust pointers to individual bits.

Additionally, it returns a `Result` rather than a direct bit-slice, because the
given `len` argument may be invalid to encode into a `&mut BitSlice` reference.

## Safety

This has the same memory safety requirements as the standard-library function:

- `data` must be valid for reads and writes of at least `len` bits,
- The bits that the produced bit-slice refers to must be wholly unreachable by
  any other part of the program for the duration of the lifetime `'a`,

and additionally imposes some of its own:

- `len` cannot exceed [`BitSlice::MAX_BITS`].

## Examples

```rust
use bitvec::{
  prelude::*,
  index::BitIdx,
  ptr::Mut,
  slice as bv_slice,
};

let mut elem = 0u16;
let addr = (&mut elem).into();
let head = BitIdx::new(1).unwrap();

let data: BitPtr<Mut, u16> = BitPtr::new(addr, head).unwrap();
let bits = unsafe { bv_slice::from_raw_parts_mut(data, 3) };
bits.unwrap()[1 ..].fill(true);

assert_eq!(elem, 12);
```

[`BitSlice::MAX_BITS`]: crate::slice::BitSlice::MAX_BITS
