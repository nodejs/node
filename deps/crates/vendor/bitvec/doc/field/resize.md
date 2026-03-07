# Value Resizing

This zero-extends or truncates a source value to fit into a target type.

## Type Parameters

- `T`: The initial integer type of the value being resized.
- `U`: The destination type of the value after resizing.

## Parameters

- `value`: Any (unsigned) integer.

## Returns

`value`, either zero-extended in the most-significant bits (if `U` is wider than
`T`) or truncated retaining the least-significant bits (if `U` is narrower than
`T`).
