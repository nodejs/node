# Element Encoder Macro

This macro is invoked by `__encode_bits!` with a set of bits that exactly fills
some `BitStore` element type. It is responsible for encoding those bits into the
raw memory bytes and assembling them into a whole integer.

It works by inspecting the `$order` argument. If it is one of `LocalBits`,
`Lsb0`, or `Msb0`, then it can do the construction in-place, and get solved
during `const` evaluation. If it is any other ordering, then it emits runtime
code to do the translation and defers to the optimizer for evaluation.

It divides the input into clusters of eight bit expressions, then uses the
`$order` argument to choose whether the bits are accumulated into a `u8` using
`Lsb0`, `Msb0`, or `LocalBits` ordering. The accumulated byte array is then
converted into an integer using the corresponding `uN::from_{b,l,n}e_bytes`
function in `__ty_from_bytes!`.

Once assembled, the raw integer is changed into the requested final type. This
currently routes through a helper type that unifies `const fn` constructors for
each of the raw integer fundamentals, cells, and atomics in order to avoid
transmutes.
