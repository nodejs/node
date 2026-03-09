# Fixed-Size, Heap-Allocated, Bit Slice

`BitBox` is a heap-allocated [`BitSlice`] region. It is a distinct type because
the implementation of bit-slice pointers means that `Box<BitSlice>` cannot
exist. It can be created by cloning a bit-slice into the heap, or by freezing
the allocation of a [`BitVec`]

## Original

[`Box<[T]>`](alloc::boxed::Box)

## API Differences

As with `BitSlice`, this takes a pair of [`BitOrder`] and [`BitStore`] type
parameters to govern the buffer’s memory representation. Because `BitSlice` is
unsized, `BitBox` has almost none of the `Box` API, and is difficult to use
directly.

## Behavior

`BitBox`, like `&BitSlice`, is an opaque pointer to a bit-addressed slice
region. Unlike `&BitSlice`, it uses the allocator to guarantee that it is the
sole accessor to the referent buffer, and is able to use that uniqueness
guarantee to specialize some `BitSlice` behavior to be faster or more efficient.

## Safety

`BitBox` is, essentially, a `NonNull<BitSlice<T, O>>` pointer. The internal
value is opaque and cannot be inspected or modified by user code.

If you attempt to do so, your program becomes inconsistent. You will likely
break the allocator’s internal state and cause a crash. No guarantees of crash
*or* recovery are provided. Do not inspect or modify the `BitBox` handle value.

## Construction

The simplest way to construct a `BitBox` is by using the [`bitbox!`] macro. You
can also explicitly clone a `BitSlice` with [`BitBox::from_bitslice`], or freeze
a `BitVec` with [`BitVec::into_boxed_bitslice`].

## Examples

```rust
use bitvec::prelude::*;

let a = BitBox::from_bitslice(bits![1, 0, 1, 1, 0]);
let b = bitbox![0, 1, 0, 0, 1];

let b_raw: *mut BitSlice = BitBox::into_raw(b);
let b_reformed = unsafe { BitBox::from_raw(b_raw) };
```

[`BitBox::from_bitslice`]: self::BitBox::from_bitslice
[`BitOrder`]: crate::order::BitOrder
[`BitSlice`]: crate::slice::BitSlice
[`BitStore`]: crate::store::BitStore
[`BitVec`]: crate::vec::BitVec
[`BitVec::into_boxed_bitslice`]: crate::vec::BitVec::into_boxed_bitslice
[`bitbox!`]: macro@crate::bitbox
