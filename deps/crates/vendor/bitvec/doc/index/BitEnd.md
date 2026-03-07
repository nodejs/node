# One-Bit-After Tail Index

This is a semantic bit-index within *or one bit after* an `R` register. It is
the index of the first “dead” bit after a “live” region, and corresponds to the
similar half-open range concept in the Rust `Range` type or the LLVM memory
model, pointer values include the address one object past the end of a region.

It is a counter in the ring `0 ..= R::BITS` (note the inclusive high end). Like
[`BitIdx`], this is a virtual semantic index with no bearing on real memory
effects; unlike `BitIdx`, it can never be translated to real memory because it
does not describe real memory.

This type is necessary in order to preserve the distinction between a dead
memory address that is *not* part of a region and a live memory address that
*is* within a region. Additionally, it makes computation of region extension or
offsets easy. `BitIdx` is insufficient to this task, and produces off-by-one
errors when used in its stead.

## Type Parameters

- `R`: The register element that this dead-bit index governs.

## Validity

Values of this type are **required** to be in the range `0 ..= R::BITS`. Any
value greater than [`R::BITS`] makes the program invalid and will likely cause
either a crash or incorrect memory access.

## Construction

This type cannot be publicly constructed except by using the iterators provided
for testing.

[`BitIdx`]: crate::index::BitIdx
[`R::BITS`]: funty::Integral::BITS
