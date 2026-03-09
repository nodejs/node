# Multi-Bit Selection Mask

Unlike [`BitSel`], which enforces a strict one-hot mask encoding, this type
permits any number of bits to be set or cleared. This is used to accumulate
selections for batched operations on a register in real memory.

## Type Parameters

- `R`: The register element that this mask governs.

## Construction

This must only be constructed by combining `BitSel` selection masks produced
through the accepted chains of custody beginning with [`BitIdx`] values.
Bit-masks not constructed in this manner are not guaranteed to be correct in the
caller’s context and may lead to incorrect memory behaviors.

[`BitIdx`]: crate::index::BitIdx
[`BitSel`]: crate::index::BitSel
