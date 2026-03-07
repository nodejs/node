# Port of the `[bool]` Inherent API

This module provides a port of the standard-library’s slice primitive, and
associated special-purpose items.

It is intended to contain the contents of every `impl<T> [T]` block in the
standard library (with a few exceptions due to impossibility or uselessness).
The sibling modules `iter`, `ops`, and `traits` contain slice APIs that relate
specifically to iteration, the sigil operators, or general-purpose traits.

Documentation for each ported API strives to be *inspired by*, but not a
transliteration of, the documentation in the standard library. `bitvec`
generally assumes that you are already familiar with the standard library, and
links each ported item to the original in the event that you are not.
