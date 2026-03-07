# Bit-Precision Dynamic Array

This is an analogue to `Vec<bool>` that stores its data using a compaction
scheme to ensure that each `bool` takes exactly one bit of memory. It is similar
to the C++ type [`std::vector<bool>`], but uses `bitvec`’s type parameter system
to provide more detailed control over the in-memory representation.

This is *always* a heap allocation. If you know your sizes at compile-time, you
may prefer to use [`BitArray`] instead, which is able to store its data as an
immediate value rather than through an indirection.

## Documentation Practices

`BitVec` exactly replicates the API of the standard-library `Vec` type,
including inherent methods, trait implementations, and relationships with the
[`BitSlice`] slice analogue.

Items that are either direct ports, or renamed variants, of standard-library
APIs will have a `## Original` section that links to their standard-library
documentation. Items that map to standard-library APIs but have a different API
signature will also have an `## API Differences` section that describes what
the difference is, why it exists, and how to transform your code to fit it. For
example:

## Original

[`Vec<T>`](alloc::vec::Vec)

## API Differences

As with all `bitvec` data structures, this takes two type parameters `<T, O>`
that govern the bit-vector’s storage representation in the underlying memory,
and does *not* take a type parameter to govern what data type it stores (always
`bool`)

## Suggested Uses

`BitVec` is able to act as a compacted `usize => bool` dictionary, and is useful
for holding large collections of truthiness. For instance, you might replace a
`Vec<Option<T>>` with a `(BitVec, Vec<MaybeUninit<T>>`) to cut down on the
resident size of the discriminant.

Through the [`BitField`] trait, `BitVec` is also able to act as a transport
buffer for data that can be marshalled as integers. Serializing data to a
narrower compacted form, or deserializing data *from* that form, can be easily
accomplished by viewing subsets of a bit-vector and storing integers into, or
loading integers out of, that subset. As an example, transporting four ten-bit
integers can be done in five bytes instead of eight like so:

```rust
use bitvec::prelude::*;

let mut bv = bitvec![u8, Msb0; 0; 40];
bv[0 .. 10].store::<u16>(0x3A8);
bv[10 .. 20].store::<u16>(0x2F9);
bv[20 .. 30].store::<u16>(0x154);
bv[30 .. 40].store::<u16>(0x06D);
```

If you wish to use bit-field memory representations as `struct` fields rather
than a transport buffer, consider `BitArray` instead: that type keeps its data
as an immediate, and is more likely to act like a C struct with bitfields.

## Examples

`BitVec` has exactly the same API as `Vec<bool>`, and even extends it with some
of `Vec<T>`’s behaviors. As a brief tour:

### Push and Pop

```rust
use bitvec::prelude::*;

let mut bv: BitVec = BitVec::new();
bv.push(false);
bv.push(true);

assert_eq!(bv.len(), 2);
assert_eq!(bv[0], false);

assert_eq!(bv.pop(), Some(true));
assert_eq!(bv.len(), 1);
```

### Writing Into a Bit-Vector

The only `Vec<bool>` API that `BitVec` does *not* implement is `IndexMut`,
because that is not yet possible. Instead, [`.get_mut()`] can produce a proxy
reference, or [`.set()`] can take an index and a value to write.

```rust
use bitvec::prelude::*;

let mut bv: BitVec = BitVec::new();
bv.push(false);

*bv.get_mut(0).unwrap() = true;
assert!(bv[0]);
bv.set(0, false);
assert!(!bv[0]);
```

### Macro Construction

Like `Vec`, `BitVec` also has a macro constructor: [`bitvec!`] takes a sequence
of bit expressions and encodes them at compile-time into a suitable buffer. At
run-time, this buffer is copied into the heap as a `BitVec` with no extra cost
beyond the allocation.

```rust
use bitvec::prelude::*;

let bv = bitvec![0; 10];
let bv = bitvec![0, 1, 0, 0, 1];
let bv = bitvec![u16, Msb0; 1; 20];
```

### Borrowing as `BitSlice`

`BitVec` lends its buffer as a `BitSlice`, so you can freely give permission to
view or modify the contained data without affecting the allocation:

```rust
use bitvec::prelude::*;

fn read_bitslice(bits: &BitSlice) {
  // …
}

let bv = bitvec![0; 30];
read_bitslice(&bv);
let bs: &BitSlice = &bv;
```

## Other Notes

The default type parameters are `<usize, Lsb0>`. This is the most performant
pair when operating on memory, but likely does not match your needs if you are
using `BitVec` to represent a transport buffer. See [the user guide][book] for
more details on how the type parameters govern memory representation.

Applications, or single-purpose libraries, built atop `bitvec` will likely want
to create a `type` alias with specific type parameters for their usage. `bitvec`
is fully generic over the ordering/storage types, but this generality is rarely
useful for client crates to propagate. `<usize, Lsb0>` is fastest; `<u8, Msb0>`
matches what most debugger views of memory will print, and the rest are
documented in the guide.

## Safety

Unlike the other data structures in this crate, `BitVec` is uniquely able to
hold uninitialized memory and produce pointers into it. As described in the
[`BitAccess`] documentation, this crate is categorically unable to operate on
uninitialized memory in any way. In particular, you may not allocate a buffer
using [`::with_capacity()`], then use [`.as_mut_bitptr()`] to create a pointer
used to write into the uninitialized buffer.

You must always initialize the buffer contents of a `BitVec` before attempting
to view its contents. You can accomplish this through safe APIs such as
`.push()`, `.extend()`, or `.reserve()`. These are all guaranteed to safely
initialize the memory elements underlying the `BitVec` buffer without incurring
undefined behavior in their operation.

[book]: https://bitvecto-rs.github.io/bitvec/type-parameters.html
[`BitAccess`]: crate::access::BitAccess
[`BitArray`]: crate::array::BitArray
[`BitField`]: crate::field::BitField
[`BitSlice`]: crate::slice::BitSlice
[`bitvec!`]: macro@crate::bitvec
[`std::vector<bool>`]: https://en.cppreference.com/w/cpp/container/vector_bool
[`.as_mut_bitptr()`]: crate::slice::BitSlice::as_mut_bitptr
[`.get_mut()`]: crate::slice::BitSlice::get_mut
[`.set()`]: crate::slice::BitSlice::set
[`::with_capacity()`]: Self::with_capacity
