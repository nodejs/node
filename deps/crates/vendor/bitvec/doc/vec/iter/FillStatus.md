# Fill Status

The standard library uses a `bool` flag to indicate whether a splicing operation
exhausted the source or filled the target, which is not very clear about what is
being signaled. This enum replaces it.

## Variants

- `FullSpan`: This marks that a drain span has been completely filled with
  replacement bits, and any further replacement would require insertion rather
  than overwriting dead storage.
- `EmptyInput`: This marks that a replacement source has been run to completion,
  but dead bits remain in a drain span, and the dead range will need to be
  overwritten.
