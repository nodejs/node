# Complete `BitOrder` Verification

This function checks some [`BitOrder`] implementation’s behavior on each of the
[`BitRegister`] types present on the target, and reports any violation of the
rules that it detects.

## Type Parameters

- `O`: The `BitOrder` implementation being tested.

## Parameters

- `verbose`: Controls whether the test should print diagnostic information to
  standard output. If this is false, then the test only prints a message on
  failure; if it is true, it emits a message for every test it executes.

## Panics

This panics when it detects a violation of the `BitOrder` rules. If it returns
normally, then the implementation is correct.

[`BitOrder`]: crate::order::BitOrder
[`BitRegister`]: crate::mem::BitRegister
