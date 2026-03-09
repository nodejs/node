# One-Hot Bit Selection Mask

This type selects exactly one bit in a register. It is a [`BitPos`] shifted from
a counter to a selector, and is used to apply test and write operations to real
memory.

## Type Parameters

- `R`: The register element this selector governs.

## Validity

Values of this type are **required** to have exactly one bit set and all others
cleared. Any other value makes the program incorrect, and will cause memory
corruption.

## Construction

This type is only constructed from `BitPos`, and is always equivalent to
`1 << BitPos`.

The chain of custody from known-good [`BitIdx`] values, through proven-good
[`BitOrder`] implementations, into `BitPos` and then `BitSel` proves that values
of this type are always correct to apply to real memory.

[`BitIdx`]: crate::index::BitIdx
[`BitOrder`]: crate::order::BitOrder
[`BitPos`]: crate::index::BitPos
