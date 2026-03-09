# Partially-Owned Memory Element

This type is a guarded reference to memory that permits interacting with it as
an integer, but only allows views to the section of the integer that the
producing handle has permission to observe. Unlike the `BitSafe` type family in
the [`access`] module, it is not a transparent wrapper that can be used for
reference conversion; it is a “heavy reference” that carries the mask and

## Type Parameters

- `T`: The type, including register width and alias information, of the
  bit-slice handle that created it.
- `O`: This propagates the bit-ordering type used by the [`BitSlice`] handle
  that created it.

## Lifetime

This carries the lifetime of the bit-slice handle that created it.

## Usage

This structure is only created as part of the [`Domain`] region descriptions,
and refers to partially-occupied edge elements. The underlying referent memory
can be read with `.load_value()` or written with `.store_value()`, and the
appropriate masking will be applied in order to restrict access to only the
permitted bits.

[`access`]: crate::access
[`BitSlice`]: crate::slice::BitSlice
[`Domain`]: Domain
