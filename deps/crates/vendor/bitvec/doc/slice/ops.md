# Bit-Slice Operator Implementations

The standard-library slices only implement the indexing operator `[]`.
`BitSlice` additionally implements the Boolean operators `&`, `|`, `^`, and `!`.

The dyadic Boolean arithmetic operators all take any `bitvec` container as their
second argument, and apply the operation in-place to the left-hand bit-slice. If
the second argument exhausts before `self` does, then it is implicitly
zero-extended. This means that `&=` zeros excess bits in `self`, while `|=` and
`^=` do not modify them.

The monadic operator `!` inverts the entire bit-slice at once. Its API requires
*taking* a `&mut BitSlice` reference and returning it, so you will need to
structure your code accordingly.

[`BitSlice::domain_mut`]: crate::slice::BitSlice::domain_mut
