# FFI Test Addon

This is a native addon specifically designed for testing Node.js FFI functionality.

## Building

The addon is built automatically as part of the Node.js test build process using direct C compiler invocation:

**macOS:**

```bash
clang -dynamiclib -o test/ffi/fixture_library/build/Release/ffi_test_library.dylib test/ffi/fixture_library/ffi_test_library.c
```

**Linux:**

```bash
gcc -shared -fPIC -o test/ffi/fixture_library/build/Release/ffi_test_library.so test/ffi/fixture_library/ffi_test_library.c
```

## Features

This addon provides comprehensive test coverage for FFI including:

* **Integer types**: i32, u32, i64, u64 (tested via FFI)
  * Note: i8, u16, i16, u16 are provided in C but NOT supported as FFI function parameters
* **Floating point**: f32, f64
* **Pointers**: pointer type and value conversions
* **Booleans**: Tested using i32 type (0 or 1) since bool is not supported in FFI
* **Void functions**: Functions with no return value
* **Callbacks**: Functions that accept function pointers (skipped for now due to implementation issues)
* **Edge cases**: Division by zero, null pointers, overflow
* **Multi-parameter**: Functions with many parameters
* **Mixed types**: Functions with different parameter types

## FFI Type System Constraints

The FFI system only supports the following types as function parameters and return values:

* `i32`, `u32` - 32-bit signed/unsigned integers
* `i64`, `u64` - 64-bit signed/unsigned integers
* `f32`, `f64` - 32-bit and 64-bit floating point
* `pointer` - Pointer type
* `void` - No return value

**Unsupported types**: `i8`, `u8`, `i16`, `u16`, `bool`, `usize`,
`size_t`, `uintptr_t`

For functions that internally need unsupported types, cast them inside the
C function (e.g., accepting `u32` and casting to `uint8_t`).

## Test Coverage

The test file `test/ffi/test-ffi-calls.js` includes:

* ✅ Integer operations (i32, u32, i64 - u64 skipped due to BigInt handling)
* ✅ Floating point operations
* ✅ Pointer operations (some skipped)
* ✅ Boolean operations using i32
* ✅ Void/stateless operations
* ✅ Callback operations
* ✅ Edge cases (division, null pointers)
* ✅ Multi-parameter functions
* ✅ Mixed type functions
