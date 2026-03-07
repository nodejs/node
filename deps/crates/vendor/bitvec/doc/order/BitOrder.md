# In-Element Bit Ordering

This trait manages the translation of semantic bit indices into electrical
positions within storage elements of a memory region.

## Usage

`bitvec` APIs operate on semantic index counters that exist in an abstract
memory space independently of the real memory that underlies them. In order to
affect real memory, `bitvec` must translate these indices into real values. The
[`at`] function maps abstract index values into their corresponding real
positions that can then be used to access memory.

You will likely never call any of the trait functions yourself. They are used by
`bitvec` internals to operate on memory regions; all you need to do is provide
an implementation of this trait as a type parameter to `bitvec` data structures.

## Safety

`BitOrder` is unsafe to implement because its translation of index to position
cannot be forcibly checked by `bitvec` itself, and an improper implementation
will lead to memory unsafety errors and unexpected collisions. The trait has
strict requirements for each function. If these are not upheld, then the
implementation is considered undefined at the library level and its use may
produce incorrect or undefined behavior during compilation.

You are responsible for running [`verify_for_type`] or [`verify`] in your test
suite if you implement `BitOrder`.

## Implementation Rules

Values of this type are never constructed or passed to `bitvec` functions. Your
implementation does not need to be zero-sized, but it will never have access to
an instance to view its state. It *may* refer to other global state, but per the
rules of `at`, that state may not change while any `bitvec` data structures are
alive.

The only function you *need* to provide is `at`. Its requirements are listed in
its trait documentation.

You *may* also choose to provide implementations of `select` and `mask`. These
have a default implementation that is correct, but may be unoptimized for your
implementation. As such, you may replace them with a better version, but your
implementation of these functions must be exactly equal to the default
implementation for all possible inputs.

This requirement is checked by the `verify_for_type` function.

## Verification

The `verify_for_type` function verifies that a `BitOrder` implementation is
correct for a single `BitStore` implementor, and the `verify` function runs
`verify_for_type` on all unsigned integers that implement `BitStore` on a
target. If you run these functions in your test suite, they will provide
detailed information if your implementation is incorrect.

## Examples

Implementations are not required to remain contiguous over a register, and may
have any mapping they wish as long as it is total and bijective. This example
swizzles the high and low halves of each byte.

```rust
use bitvec::{
  order::BitOrder,
  index::{BitIdx, BitPos},
  mem::BitRegister,
};

pub struct HiLo;

unsafe impl BitOrder for HiLo {
  fn at<R>(index: BitIdx<R>) -> BitPos<R>
  where R: BitRegister {
    unsafe { BitPos::new_unchecked(index.into_inner() ^ 4) }
  }
}

#[test]
#[cfg(test)]
fn prove_hilo() {
  bitvec::order::verify::<HiLo>();
}
```

Once a `BitOrder` implementation passes the test suite, it can be freely used as
a type parameter in `bitvec` data structures. The translation takes place
automatically, and you never need to look at this trait again.

[`at`]: Self::at
[`verify`]: crate::order::verify
[`verify_for_type`]: crate::order::verify_for_type
