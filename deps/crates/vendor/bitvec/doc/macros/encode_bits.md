# Bit-Sequence Buffer Encoding

This macro accepts a sequence of bit expressions from the public macros and
creates encoded `[T; N]` arrays from them. The public macros can then use these
encoded arrays as the basis of the requested data structure.

This is a complex macro that uses recursion to modify and inspect its input
tokens. It is divided into three major sections.

## Entry Points

The first section provides a series of entry points that the public macros
invoke. Each arm matches the syntax provided by public macros, and detects a
specific `BitStore` implementor name: `uN`, `Cell<uN>`, `AtomicUN`, or
`RadiumUN`, for each `N` in `8`, `16`, `32`, `64`, and `size`.

These arms then recurse, adding a token for the raw unsigned integer used as the
basis of the encoding. The `usize` arms take an additional recursion that routes
to the 32-bit or 64-bit encoding, depending on the target.

## Zero Extension

The next two arms handle extending the list of bit-expressions with 64 `0,`s.
The first arm captures initial reëntry and appends the zero-comma tokens, then
recurses to enter the chunking group. The second arm traps when recursion has
chunked all user-provided tokens, and only the literal `0,` tokens appended by
the first arm remain.

The second arm dispatches the chunked bit-expressions into the element encoder,
and is the exit point of the macro. Its output is an array of encoded memory
elements, typed as the initially-requested `BitStore` name.

The `0,` tokens remain matchable as text literals because they never depart
this macro: recursion within the same macro does not change the types in the
AST, while invoking a new macro causes already-known tokens to become opacified
into `:tt` whose contents cannot be matched. This is the reason that the macro
is recursive rather than dispatching.

## Chunking

The stream of user-provided bit-expressions, followed by the appended zero-comma
tokens, is divided into chunks by the width of the storage type.

Each width (8, 16, 32, 64) has an arm that munches from the token stream and
grows an opaque token-list containing munched groups. In syntax, this is
represented by the `[$([$($bit:tt,)+],)*];` cluster:

- it is an array
  - of zero or more arrays
    - of one or more bit expressions
    - each followed by a comma
  - each followed by a comma
- followed by a semicolon

By placing this array ahead of the bit-expression stream, we can use the array
as an append-only list (matched as `[$($elem:tt)*]`, emitted as
`[$($elem)* [new]]`) grown by munching from the token stream of unknown length
at the end of the argument set.

On each recursion, the second arm in zero-extension attempts to trap the input.
If it fails, then user-provided tokens remain; if it succeeds, then it discards
any remaining macro-appended zeros and terminates.
