# Encoded Bit-Span Descriptor

This structure is used as the actual in-memory value of `BitSlice` pointers
(including both `*{const,mut} BitSlice` and `&/mut BitSlice`). It is **not**
public API, and the encoding scheme does not support external modification.

Rust slices encode a base element address and an element count into a single
`&[T]` two-word value. `BitSpan` encodes a third value, the index of the base
bit within the base element, into unused bits of the address and length counter.

The slice reference has the ABI `(*T, usize)`, which is exactly two processor
words in size. `BitSpan` matches this ABI so that it can be cast into
`&/mut BitSlice` and used in reference-demanding APIs.

## Layout

This structure is a more complex version of the `(*const T, usize)` tuple that
Rust uses to represent slices throughout the language. It breaks the pointer and
counter fundamentals into sub-field components. Rust does not have bitfield
syntax, so the below description of the structure layout is in C++.

```cpp
template <typename T>
struct BitSpan {
  uintptr_t ptr_head : __builtin_ctzll(alignof(T));
  uintptr_t ptr_addr : sizeof(uintptr_T) * 8 - __builtin_ctzll(alignof(T));

  size_t len_head : 3;
  size_t len_bits : sizeof(size_t) * 8 - 3;
};
```

This means that the `BitSpan<O, T>` has three *logical* fields, stored in four
segments, across the two *structural* fields of the type. The widths and
placements of each segment are functions of the size of `*const T`, `usize`, and
of the alignment of the `T` referent buffer element type.

## Fields

### Base Address

The address of the base element in a memory region is stored in all but the
lowest bits of the `ptr` field. An aligned pointer to `T` will always have its
lowest log<sub>2</sub>(byte width) bits zeroed, so those bits can be used to
store other information, as long as they are erased before dereferencing the
address as a pointer to `T`.

### Head Bit Index

For any referent element type `T`, the selection of a single bit within the
element requires log<sub>2</sub>(byte width) bits to select a byte within the
element `T`, and another three bits to select a bit within the selected byte.

|Type |Alignment|Trailing Zeros|Count Bits|
|:----|--------:|-------------:|---------:|
|`u8` |        1|             0|         3|
|`u16`|        2|             1|         4|
|`u32`|        4|             2|         5|
|`u64`|        8|             3|         6|

The index of the first live bit in the base element is split to have its three
least significant bits stored in the least significant edge of the `len` field,
and its remaining bits stored in the least significant edge of the `ptr` field.

### Length Counter

All but the lowest three bits of the `len` field are used to store a counter of
live bits in the referent region. When this is zero, the region is empty.
Because it is missing three bits, a `BitSpan` has only ⅛ of the index space of
a `usize` value.

## Significant Values

The following values represent significant instances of the `BitSpan` type.

### Null Slice

The fully-zeroed slot is not a valid member of the `BitSpan<O, T>` type; it is
reserved instead as the sentinel value for `Option::<BitSpan<O, T>>::None`.

### Canonical Empty Slice

All pointers with a `bits: 0` logical field are empty. Pointers that are used to
maintain ownership of heap buffers are not permitted to erase their `addr`
field. The canonical form of the empty slice has an `addr` value of
[`NonNull::<T>::dangling()`], but all pointers to an empty region are equivalent
regardless of address.

#### Uninhabited Slices

Any empty pointer with a non-[`dangling()`] base address is considered to be an
uninhabited region. `BitSpan` never discards its address information, even as
operations may alter or erase its head-index or length values.

## Type Parameters

- `T`: The memory type of the referent region. `BitSpan<O, T>` is a specialized
  `*[T]` slice pointer, and operates on memory in terms of the `T` type for
  access instructions and pointer calculation.
- `O`: The ordering within the register type. The bit-ordering used within a
  region colors all pointers to the region, and orderings can never mix.

## Safety

`BitSpan` values may only be constructed from pointers provided by the
surrounding program.

## Undefined Behavior

Values of this type are binary-incompatible with slice pointers. Transmutation
of these values into any other type will result in an incorrect program, and
permit the program to begin illegal or undefined behaviors. This type may never
be manipulated in any way by user code outside of the APIs it offers to this
`bitvec`; it certainly may not be seen or observed by other crates.

## Design Notes

Accessing the `.head` logical field would be faster if it inhabited the least
significant byte of `.len`, and was not partitioned into `.ptr` as well.
This implementation was chosen against in order to minimize the loss of bits in
the length counter; if user studies indicate that bit-slices do not **ever**
require more than 2<sup>24</sup> bits on 32-bit systems, this may be revisited.

The `ptr_metadata` feature, tracked in [Issue #81513], defines a trait `Pointee`
that regions such as `BitSlice` can implement and define a `Metadata` type that
carries all information other than a dereferenceable memory address. For regular
slices, this would be `impl<T> Pointee for [T] { type Metadata = usize; }`. For
`BitSlice`, it would be `(usize, BitIdx<T::Mem>)` and obviate this module
entirely. But until it stabilizes, this remains.

[Issue #81513]: https://github.com/rust-lang/rust/issues/81513

[`NonNull::<T>::dangling()`]: core::ptr::NonNull::dangling
[`dangling()`]: core::ptr::NonNull::dangling
