# FFI

<!--introduced_in=REPLACEME-->

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

<!-- source_link=lib/ffi.js -->

The `node:ffi` module provides an experimental foreign function interface for
loading dynamic libraries and calling native symbols from JavaScript.

This API is unsafe. Passing invalid pointers, using an incorrect symbol
signature, or accessing memory after it has been freed can crash the process
or corrupt memory.

To access it:

```mjs
import ffi from 'node:ffi';
```

```cjs
const ffi = require('node:ffi');
```

This module is only available under the `node:` scheme in builds with FFI
support and is gated by the `--experimental-ffi` flag.

Bundled libffi support currently targets:

* macOS on `arm64` and `x64`
* Windows on `arm64` and `x64`
* FreeBSD on `arm`, `arm64`, and `x64`
* Linux on `arm`, `arm64`, and `x64`

Other targets require building Node.js against a shared libffi with
`--shared-ffi`. The unofficial GN build does not support `node:ffi`.

When using the [Permission Model][], FFI APIs are
restricted unless the [`--allow-ffi`][] flag is provided.

## Overview

The `node:ffi` module exposes two groups of APIs:

* Dynamic library APIs for loading libraries, resolving symbols, and creating
  callable JavaScript wrappers.
* Raw memory helpers for reading and writing primitive values through pointers,
  converting pointers to JavaScript strings, `Buffer` instances, and
  `ArrayBuffer` instances, and for copying data back into native memory.

## Type names

FFI signatures use string type names.

Supported type names:

* `void`
* `i8`, `int8`
* `u8`, `uint8`, `bool`, `char`
* `i16`, `int16`
* `u16`, `uint16`
* `i32`, `int32`
* `u32`, `uint32`
* `i64`, `int64`
* `u64`, `uint64`
* `f32`, `float`
* `f64`, `double`
* `pointer`, `ptr`
* `string`, `str`
* `buffer`
* `arraybuffer`
* `function`

These type names are also exposed as constants on `ffi.types`:

* `ffi.types.VOID` = `'void'`
* `ffi.types.POINTER` = `'pointer'`
* `ffi.types.BUFFER` = `'buffer'`
* `ffi.types.ARRAY_BUFFER` = `'arraybuffer'`
* `ffi.types.FUNCTION` = `'function'`
* `ffi.types.BOOL` = `'bool'`
* `ffi.types.CHAR` = `'char'`
* `ffi.types.STRING` = `'string'`
* `ffi.types.FLOAT` = `'float'`
* `ffi.types.DOUBLE` = `'double'`
* `ffi.types.INT_8` = `'int8'`
* `ffi.types.UINT_8` = `'uint8'`
* `ffi.types.INT_16` = `'int16'`
* `ffi.types.UINT_16` = `'uint16'`
* `ffi.types.INT_32` = `'int32'`
* `ffi.types.UINT_32` = `'uint32'`
* `ffi.types.INT_64` = `'int64'`
* `ffi.types.UINT_64` = `'uint64'`
* `ffi.types.FLOAT_32` = `'float32'`
* `ffi.types.FLOAT_64` = `'float64'`

Pointer-like types (`pointer`, `string`, `buffer`, `arraybuffer`, and
`function`) are all passed through the native layer as pointers.

When `Buffer`, `ArrayBuffer`, or typed array values are passed as pointer-like
arguments, Node.js borrows a raw pointer to their backing memory for the
duration of the native call. The caller must ensure that backing store remains
valid and stable for the entire call.

It is unsupported and dangerous to resize, transfer, detach, or otherwise
invalidate that backing store while the native call is active, including
through reentrant JavaScript such as FFI callbacks. Doing so may crash the
process, produce incorrect output, or corrupt memory.

The `char` type follows the platform C ABI. On platforms where plain C `char`
is signed it behaves like `i8`; otherwise it behaves like `u8`.

The `bool` type is marshaled as an 8-bit unsigned integer. Pass numeric values
such as `0` and `1`; JavaScript `true` and `false` are not accepted.

## Signature objects

Functions and callbacks are described with signature objects.

Supported fields:

* `result`, `return`, or `returns` for the return type.
* `parameters` or `arguments` for the parameter type list.

Only one return-type field and one parameter-list field may be present in a
single signature object.

```cjs
const signature = {
  result: 'i32',
  parameters: ['i32', 'i32'],
};
```

## `ffi.suffix`

<!-- YAML
added: REPLACEME
-->

* {string}

The native shared library suffix for the current platform:

* `'dylib'` on macOS
* `'so'` on Unix-like platforms
* `'dll'` on Windows

This can be used to build portable library paths:

```cjs
const { suffix } = require('node:ffi');

const path = `libsqlite3.${suffix}`;
```

## `ffi.dlopen(path[, definitions])`

<!-- YAML
added: REPLACEME
-->

* `path` {string|null} Path to a dynamic library, or `null` to resolve symbols
  from the current process image.
* `definitions` {Object} Symbol definitions to resolve immediately.
* Returns: {Object}

Loads a dynamic library and resolves the requested function definitions.

On Windows passing `null` is not supported.

When `definitions` is omitted, `functions` is returned as an empty object until
symbols are resolved explicitly.

The returned object contains:

* `lib` {DynamicLibrary} The loaded library handle.
* `functions` {Object} Callable wrappers for the requested symbols.

```mjs
import { dlopen } from 'node:ffi';

const { lib, functions } = dlopen('./mylib.so', {
  add_i32: { parameters: ['i32', 'i32'], result: 'i32' },
  string_length: { parameters: ['pointer'], result: 'u64' },
});

console.log(functions.add_i32(20, 22));
```

```cjs
const { dlopen } = require('node:ffi');

const { lib, functions } = dlopen('./mylib.so', {
  add_i32: { parameters: ['i32', 'i32'], result: 'i32' },
  string_length: { parameters: ['pointer'], result: 'u64' },
});

console.log(functions.add_i32(20, 22));
```

## `ffi.dlclose(handle)`

<!-- YAML
added: REPLACEME
-->

* `handle` {DynamicLibrary}

Closes a dynamic library.

This is equivalent to calling `handle.close()`.

## `ffi.dlsym(handle, symbol)`

<!-- YAML
added: REPLACEME
-->

* `handle` {DynamicLibrary}
* `symbol` {string}
* Returns: {bigint}

Resolves a symbol address from a loaded library.

This is equivalent to calling `handle.getSymbol(symbol)`.

## Class: `DynamicLibrary`

<!-- YAML
added: REPLACEME
-->

Represents a loaded dynamic library.

### `new DynamicLibrary(path)`

* `path` {string|null} Path to a dynamic library, or `null` to resolve symbols
  from the current process image.

Loads the dynamic library without resolving any functions eagerly.

On Windows passing `null` is not supported.

```cjs
const { DynamicLibrary } = require('node:ffi');

const lib = new DynamicLibrary('./mylib.so');
```

### `library.path`

* {string}

The path used to load the library.

### `library.functions`

* {Object}

An object containing previously resolved function wrappers.

### `library.symbols`

* {Object}

An object containing previously resolved symbol addresses as `bigint` values.

### `library.close()`

Closes the library handle.

After a library has been closed:

* Resolved function wrappers become invalid.
* Further symbol and function resolution throws.
* Registered callbacks are invalidated.

Closing a library does not make previously exported callback pointers safe to
reuse. Node.js does not track or revoke callback pointers that have already
been handed to native code.

If native code still holds a callback pointer after `library.close()` or after
`library.unregisterCallback(pointer)`, invoking that pointer has undefined
behavior, is not allowed, and is dangerous: it can crash the process, produce
incorrect output, or corrupt memory. Native code must stop using callback
addresses before the library is closed or before the callback is unregistered.

Calling `library.close()` from one of the library's active callbacks is
unsupported and dangerous. The callback must return before the library is
closed.

### `library.getFunction(name, signature)`

* `name` {string}
* `signature` {Object}
* Returns: {Function}

Resolves a symbol and returns a callable JavaScript wrapper.

The returned function has a `.pointer` property containing the native function
address as a `bigint`.

If the same symbol has already been resolved, requesting it again with a
different signature throws.

```cjs
const { DynamicLibrary } = require('node:ffi');

const lib = new DynamicLibrary('./mylib.so');
const add = lib.getFunction('add_i32', {
  parameters: ['i32', 'i32'],
  result: 'i32',
});

console.log(add(20, 22));
console.log(add.pointer);
```

### `library.getFunctions([definitions])`

* `definitions` {Object}
* Returns: {Object}

When `definitions` is provided, resolves each named symbol and returns an
object containing callable wrappers.

When `definitions` is omitted, returns wrappers for all functions that have
already been resolved on the library.

### `library.getSymbol(name)`

* `name` {string}
* Returns: {bigint}

Resolves a symbol and returns its native address as a `bigint`.

### `library.getSymbols()`

* Returns: {Object}

Returns an object containing all previously resolved symbol addresses.

### `library.registerCallback([signature,] callback)`

* `signature` {Object}
* `callback` {Function}
* Returns: {bigint}

Creates a native callback pointer backed by a JavaScript function.

When `signature` is omitted, the callback uses a default `void ()` signature.

The return value is the callback pointer address as a `bigint`. It can be
passed to native functions expecting a callback pointer.

```cjs
const { DynamicLibrary } = require('node:ffi');

const lib = new DynamicLibrary('./mylib.so');

const callback = lib.registerCallback(
  { parameters: ['i32'], result: 'i32' },
  (value) => value * 2,
);
```

Callbacks are subject to the following restrictions:

* They must be invoked on the same system thread where they were created.
* They must not throw exceptions.
* They must not return promises.
* They must return a value compatible with the declared result type.
* They must not call `library.close()` on their owning library while running.
* They must not unregister themselves while running.

Closing the owning library or unregistering the currently executing callback
from inside the callback is unsupported and dangerous. Doing so may crash the
process, produce incorrect output, or corrupt memory.

### `library.unregisterCallback(pointer)`

* `pointer` {bigint}

Releases a callback previously created with `library.registerCallback()`.

Calling `library.unregisterCallback(pointer)` for a callback that is currently
executing is unsupported and dangerous. The callback must return before it is
unregistered.

After `library.unregisterCallback(pointer)` returns, invoking that callback
pointer from native code has undefined behavior, is not allowed, and is
dangerous: it can crash the process, produce incorrect output, or corrupt
memory.

### `library.refCallback(pointer)`

* `pointer` {bigint}

Keeps the callback strongly referenced by JavaScript.

### `library.unrefCallback(pointer)`

* `pointer` {bigint}

Allows the callback to become weakly referenced by JavaScript.

If the callback function is later garbage collected, subsequent native
invocations become a no-op. Non-void return values are zero-initialized before
returning to native code.

## Calling native functions

Argument conversion depends on the declared FFI type.

For 8-, 16-, and 32-bit integer types and for floating-point types, pass
JavaScript `number` values that match the declared type.

For 64-bit integer types (`i64` and `u64`), pass JavaScript `bigint` values.

For pointer-like parameters:

* `null` and `undefined` are passed as null pointers.
* `string` values are copied to temporary NUL-terminated UTF-8 strings for the
  duration of the call.
* `Buffer`, typed arrays, and `DataView` instances pass a pointer to their
  backing memory.
* `ArrayBuffer` passes a pointer to its backing memory.
* `bigint` values are passed as raw pointer addresses.

Pointer return values are exposed as `bigint` addresses.

## Primitive memory access helpers

The following helpers read and write primitive values at a native pointer,
optionally with a byte offset:

* `ffi.getInt8(pointer[, offset])`
* `ffi.getUint8(pointer[, offset])`
* `ffi.getInt16(pointer[, offset])`
* `ffi.getUint16(pointer[, offset])`
* `ffi.getInt32(pointer[, offset])`
* `ffi.getUint32(pointer[, offset])`
* `ffi.getInt64(pointer[, offset])`
* `ffi.getUint64(pointer[, offset])`
* `ffi.getFloat32(pointer[, offset])`
* `ffi.getFloat64(pointer[, offset])`
* `ffi.setInt8(pointer, offset, value)`
* `ffi.setUint8(pointer, offset, value)`
* `ffi.setInt16(pointer, offset, value)`
* `ffi.setUint16(pointer, offset, value)`
* `ffi.setInt32(pointer, offset, value)`
* `ffi.setUint32(pointer, offset, value)`
* `ffi.setInt64(pointer, offset, value)`
* `ffi.setUint64(pointer, offset, value)`
* `ffi.setFloat32(pointer, offset, value)`
* `ffi.setFloat64(pointer, offset, value)`

These helpers perform direct memory reads and writes. `pointer` must be a
`bigint` referring to valid readable or writable native memory. `offset`, when
provided, is interpreted as a byte offset from `pointer`.

The getter helpers return JavaScript `number` values for 8-, 16-, and 32-bit
integer types and for floating-point types. They return `bigint` values for
64-bit integer types.

The setter helpers require an explicit byte offset and validate the supplied
JavaScript value against the target native type before writing it into memory.
For `setInt64()` and `setUint64()`, `bigint` values are accepted directly;
numeric inputs must be integers within JavaScript's safe integer range.

```cjs
const {
  getInt32,
  setInt32,
} = require('node:ffi');

setInt32(ptr, 0, 42);
console.log(getInt32(ptr, 0));
```

Like the other raw memory helpers in this module, these APIs do not track
ownership, bounds, or lifetime. Passing an invalid pointer, using the wrong
offset, or writing through a stale pointer can corrupt memory or crash the
process.

## `ffi.toString(pointer)`

<!-- YAML
added: REPLACEME
-->

* `pointer` {bigint}
* Returns: {string|null}

Reads a NUL-terminated UTF-8 string from native memory.

If `pointer` is `0n`, `null` is returned.

This function does not validate that `pointer` refers to readable memory or
that the pointed-to data is terminated with `\0`. Passing an invalid pointer,
a pointer to freed memory, or a pointer to bytes without a terminating NUL can
read unrelated memory, crash the process, or produce truncated or garbled
output.

```cjs
const { toString } = require('node:ffi');

const value = toString(ptr);
```

## `ffi.toBuffer(pointer, length[, copy])`

<!-- YAML
added: REPLACEME
-->

* `pointer` {bigint}
* `length` {number}
* `copy` {boolean} When `false`, creates a zero-copy view. **Default:** `true`.
* Returns: {Buffer}

Creates a `Buffer` from native memory.

When `copy` is `true`, the returned `Buffer` owns its own copied memory.
When `copy` is `false`, the returned `Buffer` references the original native
memory directly.

Using `copy: false` is a zero-copy escape hatch. The returned `Buffer` is a
writable view onto foreign memory, so writes in JavaScript update the original
native memory directly. The caller must guarantee that:

* `pointer` remains valid for the entire lifetime of the returned `Buffer`.
* `length` stays within the allocated native region.
* no native code frees or repurposes that memory while JavaScript still uses
  the `Buffer`.

If these guarantees are not met, reading or writing the `Buffer` can corrupt
memory or crash the process.

## `ffi.toArrayBuffer(pointer, length[, copy])`

<!-- YAML
added: REPLACEME
-->

* `pointer` {bigint}
* `length` {number}
* `copy` {boolean} When `false`, creates a zero-copy view. **Default:** `true`.
* Returns: {ArrayBuffer}

Creates an `ArrayBuffer` from native memory.

When `copy` is `true`, the returned `ArrayBuffer` contains copied bytes.
When `copy` is `false`, the returned `ArrayBuffer` references the original
native memory directly.

The same lifetime and bounds requirements described for
[`ffi.toBuffer(pointer, length, copy)`][] apply
here. With `copy: false`, the
returned `ArrayBuffer` is a zero-copy view of foreign memory and is only safe
while that memory remains allocated, unchanged in layout, and valid for the
entire exposed range.

## `ffi.exportString(string, pointer, length[, encoding])`

<!-- YAML
added: REPLACEME
-->

* `string` {string}
* `pointer` {bigint}
* `length` {number}
* `encoding` {string} **Default:** `'utf8'`.

Copies a JavaScript string into native memory and appends a trailing NUL
terminator.

`length` must be large enough to hold the full encoded string plus the trailing
NUL terminator. For UTF-16 and UCS-2 encodings, the trailing terminator uses
two zero bytes.

`pointer` must refer to writable native memory with at least `length` bytes of
available storage. This function does not allocate memory on its own.

`string` must be a JavaScript string. `encoding` must be a string.

## `ffi.exportBuffer(buffer, pointer, length)`

<!-- YAML
added: REPLACEME
-->

* `buffer` {Buffer}
* `pointer` {bigint}
* `length` {number}

Copies bytes from a `Buffer` into native memory.

`length` must be at least `buffer.length`.

`pointer` must refer to writable native memory with at least `length` bytes of
available storage. This function does not allocate memory on its own.

`buffer` must be a Node.js `Buffer`.

## `ffi.exportArrayBuffer(arrayBuffer, pointer, length)`

<!-- YAML
added: REPLACEME
-->

* `arrayBuffer` {ArrayBuffer}
* `pointer` {bigint}
* `length` {number}

Copies bytes from an `ArrayBuffer` into native memory.

`length` must be at least `arrayBuffer.byteLength`.

`pointer` must refer to writable native memory with at least `length` bytes of
available storage. This function does not allocate memory on its own.

## `ffi.exportArrayBufferView(arrayBufferView, pointer, length)`

<!-- YAML
added: REPLACEME
-->

* `arrayBufferView` {ArrayBufferView}
* `pointer` {bigint}
* `length` {number}

Copies bytes from an `ArrayBufferView` into native memory.

`length` must be at least `arrayBufferView.byteLength`.

`pointer` must refer to writable native memory with at least `length` bytes of
available storage. This function does not allocate memory on its own.

## `ffi.getRawPointer(source)`

<!-- YAML
added: REPLACEME
-->

* `source` {Buffer|ArrayBuffer|ArrayBufferView}
* Returns: {bigint}

Returns the raw memory address of JavaScript-managed byte storage.

This is unsafe and dangerous. The returned pointer can become invalid if the
underlying memory is detached, resized, transferred, or otherwise invalidated.
Using stale pointers can cause memory corruption or process crashes.

## Safety notes

The `node:ffi` module does not track pointer validity, memory ownership, or
native object lifetimes.

In particular:

* Do not read from or write to freed memory.
* Do not use zero-copy views after the native memory has been released.
* Do not declare incorrect signatures for native symbols.
* Do not unregister callbacks while native code may still call them.
* Do not call callback pointers after `library.close()` or
  `library.unregisterCallback(pointer)`.
* Assume undefined callback behavior can crash the process, produce incorrect
  output, or corrupt memory.
* Do not assume pointer return values imply ownership; whether the caller must
  free the returned address depends entirely on the native API.

As a general rule, prefer copied values unless zero-copy access is required,
and keep callback and pointer lifetimes explicit on the native side.

[Permission Model]: permissions.md#permission-model
[`--allow-ffi`]: cli.md#--allow-ffi
[`ffi.toBuffer(pointer, length, copy)`]: #ffitobufferpointer-length-copy
