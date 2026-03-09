# Single-Type `BitOrder` Verification

This function checks some [`BitOrder`] implementation’s behavior on only one
[`BitRegister`] type. It can be used when a program knows that it will only use
a limited set of storage types and does not need to check against all of them.

You should prefer to use [`verify`], as `bitvec` has no means of preventing the
use of a `BitRegister` storage type that your `BitOrder` implementation does not
satisfy.

## Type Parameters

- `O`: The `BitOrder` implementation being tested.
- `R`: The `BitRegister` type for which `O` is being tested.

## Parameters

- `verbose`: Controls whether the test should print diagnostic information to
  standard output. If this is false, then the test only prints a message on
  failure; if it is true, then it emits a message for every test it executes.

## Panics

This panics when it detects a violation of the `BitOrder` rules. If it returns
normally, then the implementation is correct for the given `R` type.

[`BitOrder`]: crate::order::BitOrder
[`BitRegister`]: crate::mem::BitRegister
[`verify`]: crate::order::verify.
