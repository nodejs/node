# FFI Test Library

This shared library is used by the `test/ffi` suite to exercise `node:ffi`.

## Building

Build it through the normal test build flow so the fixture uses the same toolchain
and platform settings as the rest of the Node.js test addons:

```bash
make build-ffi-tests
```

The output is written under `test/ffi/fixture_library/build/<mode>/`.

## Features

This addon provides comprehensive test coverage for FFI including:

* **Integer types**: i8, u8, i16, u16, i32, u32, i64, u64
* **Floating point**: f32, f64
* **Pointers**: pointer type and value conversions
* **Booleans**: tested using integer values
* **Void functions**: Functions with no return value
* **Callbacks**: Functions that accept function pointers
* **Edge cases**: Division by zero, null pointers, overflow
* **Multi-parameter**: Functions with many parameters
* **Mixed types**: Functions with different parameter types

## Test Coverage

The test file `test/ffi/test-ffi-calls.js` includes:

* ✅ Integer operations
* ✅ Floating point operations
* ✅ Pointer operations
* ✅ Boolean operations using integer values
* ✅ Void/stateless operations
* ✅ Callback operations
* ✅ Edge cases (division, null pointers)
* ✅ Multi-parameter functions
* ✅ Mixed type functions
