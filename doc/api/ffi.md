# FFI

<!--introduced_in=v26.1.0-->

<!-- YAML
added: v26.1.0
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
support. It can be disabled with the `--no-experimental-ffi` flag.

Building Node.js with `node:ffi` support is available via the bundled `libffi` on
platforms where `libffi` provides a compatible static backend, or via a
shared `libffi` using the `--shared-ffi` configure flag.
The unofficial GN build does not support `node:ffi`.

The following targets are not supported by bundled libffi:

* `s390x`.
* `mips`, `mipsel`, and `mips64el` on targets other than FreeBSD, Linux, and
  OpenBSD.
* `ppc64` on Android, CloudABI, iOS, OpenHarmony, OS/400, Solaris, and Windows.

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
* `char`
* `int8`
* `uint8`
* `int16`
* `uint16`
* `int32`
* `uint32`
* `int64`
* `uint64`
* `float32`
* `float64`
* `pointer`
* `string`
* `buffer`
* `arraybuffer`
* `function`

<details>
<summary>Alternative spellings</summary>

* `i8` for `int8`
* `u8` and `bool` for `uint8`
* `i16` for `int16`
* `u16` for `uint16`
* `i32` for `int32`
* `u32` for `uint32`
* `i64` for `int64`
* `u64` for `uint64`
* `f32` and `float` for `float32`
* `f64` and `double` for `float64`
* `ptr` for `pointer`
* `str` for `string`

</details>

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
is signed it behaves like `int8`; otherwise it behaves like `uint8`.

The `bool` type is marshaled as an 8-bit unsigned integer. Pass numeric values
such as `0` and `1`; JavaScript `true` and `false` are not accepted.

On optimized Fast FFI calls, `pointer` and `function` parameters accept raw
pointer `bigint` values. For pointer-like parameters, `null`, `undefined`,
strings, `Buffer`, typed array, `DataView`, and `ArrayBuffer` values are converted
on the JavaScript side before calling the optimized native wrapper.

Optimized Fast FFI calls fall back to another [call path][call paths] when a
function's arguments or return type do not fit the platform-specific fast
trampoline. Fast FFI calls support at most 8 total arguments, and the
register and argument limits differ per architecture:

| Architecture               | Max integer/pointer args                  | Max floating-point args | Buffer-shaped args | Buffer-shaped + FP together | Narrow (8/16-bit) return |
| -------------------------- | ----------------------------------------- | ----------------------- | ------------------ | --------------------------- | ------------------------ |
| AArch64                    | 7 (6 when a buffer-shaped arg is present) | 8                       | Supported          | Not supported               | Supported                |
| x86-64, Linux/macOS (SysV) | 6 (4 when a buffer-shaped arg is present) | 8                       | Supported          | Not supported               | Supported                |
| x86-64, Windows (Win64)    | 3 (total arguments also capped at 3)      | 3                       | Not supported      | N/A                         | Supported                |
| s390x                      | 4                                         | 4                       | Not supported      | N/A                         | Not supported            |
| PPC64LE                    | 7                                         | 8                       | Not supported      | N/A                         | Not supported            |
| LoongArch64                | 7                                         | 8                       | Not supported      | N/A                         | Not supported            |
| RISC-V (64-bit)            | 7                                         | 8                       | Not supported      | N/A                         | Not supported            |

PPC64BE has no fast-call trampoline and always uses the generic call path.
"Buffer-shaped args" means `Buffer`, typed array, `DataView`, or `ArrayBuffer`
values passed as pointer-like arguments. Functions whose argument or return
types exceed the limits for the current platform use one of the other
[call paths][] instead.

## Signature objects

Functions and callbacks are described with signature objects.

Signature objects may contain the following properties, both of which are
optional:

* `return` {string} A [type name][type names] specifying the return type of the
  function or callback. **Default:** `'void'`.
* `arguments` {string\[]} An array of [type names][] specifying the argument
  type list of the function or callback. **Default:** `[]`.

```js
const signature = {
  return: 'int32',
  arguments: ['int32', 'int32'],
};
```

## `ffi.suffix`

<!-- YAML
added: v26.1.0
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
added: v26.1.0
changes:
  - version: v26.10.0
    pr-url: https://github.com/nodejs/node/pull/65909
    description: Library paths inside a mounted virtual file system are now
                 supported.
-->

* `path` {string|null} Path to a dynamic library, or `null` to resolve symbols
  from the current process image.
* `definitions` {Object} Symbol definitions to resolve immediately.
* Returns: {Object}

Loads a dynamic library and resolves the requested function definitions.

On Windows passing `null` is not supported.

A `path` inside a mounted [virtual file system][] is supported: the
operating system's dynamic loader cannot open a virtual path, so the
library's bytes are read from the VFS and loaded from a private,
self-cleaning temporary image instead, while `lib.path` keeps reporting
the virtual path. Libraries on the real file system are unaffected and
load directly.

When `definitions` is omitted, `functions` is returned as an empty object until
symbols are resolved explicitly.

The returned object contains:

* `lib` {DynamicLibrary} The loaded library handle.
* `functions` {Object} Callable wrappers for the requested symbols.

The returned object also implements the explicit resource management protocol,
so it can be used with the [`using`][] declaration. Disposing the returned
object closes the library handle.

```mjs
import { dlopen, suffix } from 'node:ffi';

{
  using handle = dlopen(`./mylib.${suffix}`, {
    add_i32: { arguments: ['int32', 'int32'], return: 'int32' },
  });
  console.log(handle.functions.add_i32(20, 22));
} // handle.lib.close() is invoked automatically here.
```

```mjs
import { dlopen, suffix } from 'node:ffi';

const { lib, functions } = dlopen(`./mylib.${suffix}`, {
  add_i32: { arguments: ['int32', 'int32'], return: 'int32' },
  string_length: { arguments: ['pointer'], return: 'uint64' },
});

console.log(functions.add_i32(20, 22));
```

```cjs
const { dlopen, suffix } = require('node:ffi');

const { lib, functions } = dlopen(`./mylib.${suffix}`, {
  add_i32: { arguments: ['int32', 'int32'], return: 'int32' },
  string_length: { arguments: ['pointer'], return: 'uint64' },
});

console.log(functions.add_i32(20, 22));
```

## `ffi.dlclose(handle)`

<!-- YAML
added: v26.1.0
-->

* `handle` {DynamicLibrary}

Closes a dynamic library.

This is equivalent to calling `handle.close()`.

## `ffi.dlsym(handle, symbol)`

<!-- YAML
added: v26.1.0
-->

* `handle` {DynamicLibrary}
* `symbol` {string}
* Returns: {bigint}

Resolves a symbol address from a loaded library.

This is equivalent to calling `handle.getSymbol(symbol)`.

## Class: `DynamicLibrary`

<!-- YAML
added: v26.1.0
-->

Represents a loaded dynamic library.

### `new DynamicLibrary(path)`

<!-- YAML
changes:
  - version: v26.10.0
    pr-url: https://github.com/nodejs/node/pull/65909
    description: Library paths inside a mounted virtual file system are now
                 supported.
-->

* `path` {string|null} Path to a dynamic library, or `null` to resolve symbols
  from the current process image.

Loads the dynamic library without resolving any functions eagerly.

On Windows passing `null` is not supported.

A `path` inside a mounted [virtual file system][] loads the same way as
with [`ffi.dlopen()`][].

```cjs
const { DynamicLibrary, suffix } = require('node:ffi');

const lib = new DynamicLibrary(`./mylib.${suffix}`);
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

`DynamicLibrary` implements the explicit resource management protocol, so a
library instance can be managed with the [`using`][] declaration. Leaving the
enclosing scope invokes `library.close()` automatically.

```mjs
import { DynamicLibrary, suffix } from 'node:ffi';

{
  using lib = new DynamicLibrary(`./mylib.${suffix}`);
  // Use `lib` here; `lib.close()` is called when the block exits.
}
```

Calling `library.close()` (or disposing the library) more than once is a no-op.

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

### `library[Symbol.dispose]()`

<!-- YAML
added: v26.1.0
-->

Calls `library.close()`. This allows `DynamicLibrary` instances to be used with
the [`using`][] declaration for automatic cleanup when the enclosing scope
exits. It is a no-op on a library that has already been closed.

### `library.getFunction(name, signature)`

* `name` {string}
* `signature` {Object}
* Returns: {Function}

Resolves a symbol and returns a callable JavaScript wrapper.

The returned function has a `.pointer` property containing the native function
address as a `bigint`.

If the same symbol has already been resolved, requesting it again with a
different signature throws. Requesting it again with the same signature returns
the same function, as does reading it from [`library.functions`][].

```cjs
const { DynamicLibrary, suffix } = require('node:ffi');

const lib = new DynamicLibrary(`./mylib.${suffix}`);
const add = lib.getFunction('add_i32', {
  arguments: ['int32', 'int32'],
  return: 'int32',
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
const { DynamicLibrary, suffix } = require('node:ffi');

const lib = new DynamicLibrary(`./mylib.${suffix}`);

const callback = lib.registerCallback(
  { arguments: ['int32'], return: 'int32' },
  (value) => value * 2,
);
```

Callbacks are subject to the following restrictions:

* They must be invoked on the same system thread where they were created.
* They must not throw exceptions.
* They must not return promises.
* They must return a value compatible with the declared return type.
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

Throws `ERR_INVALID_ARG_VALUE` if the callback function has already been
garbage collected after a previous `library.unrefCallback(pointer)` call, since
a collected function cannot be referenced again.

### `library.unrefCallback(pointer)`

* `pointer` {bigint}

Allows the callback to become weakly referenced by JavaScript.

If the callback function is later garbage collected, subsequent native
invocations become a no-op. Non-void return values are zero-initialized before
returning to native code.

Throws `ERR_INVALID_ARG_VALUE` if the callback function has already been
garbage collected.

## Calling native functions

Argument conversion depends on the declared FFI type.

For 8-, 16-, and 32-bit integer types and for floating-point types, pass
JavaScript `number` values that match the declared type.

For 64-bit integer types (`int64` and `uint64`), pass JavaScript `bigint`
values within the declared type's range or safe integer `number` values.
For `int64`, numbers must be between `Number.MIN_SAFE_INTEGER` and
`Number.MAX_SAFE_INTEGER`, inclusive. For `uint64`, numbers must be between
`0` and `Number.MAX_SAFE_INTEGER`, inclusive. This allows buffer lengths such
as `buffer.byteLength` to be passed without an explicit `BigInt()` conversion.
Use `bigint` for integers outside JavaScript's safe integer range.

Invalid arguments, including fractional numbers, `NaN`, infinities, and values
outside these ranges, throw `ERR_INVALID_ARG_VALUE`. Return values for 64-bit
integer types are always exposed as `bigint` values.

For pointer-like arguments:

* `null` and `undefined` are passed as null pointers.
* `string` values are copied to temporary NUL-terminated UTF-8 strings for the
  duration of the call.
* `Buffer`, typed arrays, and `DataView` instances pass a pointer to their
  backing memory.
* `ArrayBuffer` passes a pointer to its backing memory.
* `bigint` values are passed as raw pointer addresses.

Pointer return values are exposed as `bigint` addresses.

## Call paths

When a symbol is resolved through [`ffi.dlopen()`][],
[`library.getFunction()`][], or [`library.getFunctions()`][], Node.js selects
one of three native call paths for the returned wrapper. The selection is based
on the declared signature, on the current platform, and on the capabilities of
the current process. It is made once when the function is created, cannot be
configured, and is not observable from JavaScript.

The call paths are designed to accept the same JavaScript values for each
[type name][type names], to perform the same validation, and to throw the same
errors, so that applications do not need to know which call path a particular
function uses. They differ in how much work is done per call. The paths exist
so that common signatures can be called with as little overhead as possible
while every supported signature keeps working.

Node.js tries the call paths in the following order and uses the first one that
supports the signature:

1. The [Fast API call path][], which lets optimized JavaScript call the native
   symbol directly through a generated per-signature trampoline.
2. The [shared buffer call path][], which passes arguments through a
   preallocated buffer instead of converting each argument across the
   JavaScript and C++ boundary on every call.
3. The [generic call path][], which converts each argument in C++ and calls the
   symbol through `libffi`. This path supports every signature.

The contributor guide [FFI Fast API internals][] describes the implementation of
these call paths in detail.

### Fast API call path

The Fast API call path binds the wrapper as a V8 Fast API function. When
JavaScript code calling the wrapper is optimized by V8, the call goes from the
optimized code straight into a small native trampoline that Node.js generates
for the exact signature when the function is created. The trampoline moves the
arguments into the registers expected by the native symbol and calls it. For
the scalar entry point, there is no intermediate argument conversion in C++.

Functions on this path keep a conventional native entry point as well. Calls
from code that V8 has not optimized, or that V8 deoptimizes, use that entry
point, which behaves like the [generic call path][]. This is transparent to the
caller.

Pointer-like arguments are prepared in JavaScript before the trampoline runs:

* `null` and `undefined` become null pointers.
* `string` values are copied into temporary NUL-terminated UTF-8 buffers for
  the duration of the call.
* `Buffer`, typed array, `DataView`, and `ArrayBuffer` values are converted to
  raw pointer `bigint` values, unless the alternate entry point described below
  handles them.
* `bigint` values are passed through unchanged.

For signatures with a single `pointer`, `buffer`, or `arraybuffer` argument,
Node.js also creates an alternate Fast API entry point that receives `Buffer`,
typed array, `DataView`, and `ArrayBuffer` values directly. The JavaScript
wrapper dispatches to it when the argument is such a value, and a native helper
extracts the pointer from the backing store instead of converting the value in
JavaScript.

A function uses this call path only when all of the following conditions are
met:

* The process runs on a supported 64-bit architecture: AArch64, x86-64,
  PPC64LE, LoongArch64, RISC-V 64, or s390x. 32-bit platforms and big-endian
  PPC64 always use another call path.
* The process can allocate executable memory. Node.js checks once per process
  whether it can allocate memory and mark it executable. If that check fails,
  this path is disabled for the entire process.
* Neither the return type nor any argument type is `function`.
* The signature has at most 8 arguments, and every argument fits in the
  argument registers available to the trampoline on the current platform.
  Arguments that would have to be passed on the native stack are not supported.

The register limits are platform-specific. Integer and pointer-like arguments
share one set of registers, and floating-point arguments share another. The
limits for each architecture are listed in [Type names][].

A signature that fails any of these checks is not an error. The function is
created on the next call path that supports it.

### Shared buffer call path

The shared buffer call path is used for signatures that the Fast API call path
does not support. When the function is created, Node.js allocates a small
per-function buffer with one 8-byte slot for the return value and one 8-byte
slot for each argument. On every call, the JavaScript wrapper validates the
arguments, writes them into their slots, invokes the native symbol through
`libffi` without passing any JavaScript arguments, and then reads the return
value back from the buffer. This avoids converting each argument individually
across the JavaScript and C++ boundary.

A function uses this call path when all of the following conditions are met:

* The Fast API call path is not available for the signature.
* The host is little-endian.
* The signature has at least one argument. Zero-argument functions gain nothing
  from the shared buffer and use another call path instead.

All type names are supported on this path, and there is no limit on the number
of arguments.

Pointer-like arguments (`pointer`, `string`, `buffer`, `arraybuffer`, and
`function`) are written to the shared buffer only when the value is a `bigint`,
`null`, or `undefined`. When a call passes a string, `Buffer`, typed array,
`DataView`, or `ArrayBuffer` to a pointer-like parameter, that individual call
is handed off to the [generic call path][], which performs the conversion in
C++. The function itself stays on the shared buffer call path for later calls.

The shared buffer is private to each function. Reentrant calls to the same
function, for example from an FFI callback, are safe because the native side
copies the arguments out of the buffer before invoking the symbol.

### Generic call path

The generic call path converts each JavaScript argument to its native
representation in C++ and calls the symbol through `libffi`. It supports every
signature that `node:ffi` accepts and is the reference implementation for the
argument validation and error behavior that the other call paths reproduce.

A function is created directly on this call path when the Fast API call path
is unavailable and either the host is big-endian or the signature has no
arguments.

The generic call path also serves individual calls handed off by the other call
paths, such as unoptimized or deoptimized call sites of a Fast API function and
shared buffer calls that pass non-`bigint` pointer-like values.

Callbacks created with [`library.registerCallback()`][] are always implemented
with `libffi` closures. They are independent of the call path used by any
function.

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
added: v26.1.0
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
added: v26.1.0
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
* Memory protection is observed. For example, read-only memory pages must not
  be written to.

If these guarantees are not met, reading or writing the `Buffer` can corrupt
memory or crash the process.

## `ffi.toArrayBuffer(pointer, length[, copy])`

<!-- YAML
added: v26.1.0
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
added: v26.1.0
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
added: v26.1.0
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
added: v26.1.0
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
added: v26.1.0
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
added: v26.1.0
-->

* `source` {Buffer|ArrayBuffer|SharedArrayBuffer|ArrayBufferView}
* Returns: {bigint}

Returns the raw memory address of JavaScript-managed byte storage.

This is unsafe and dangerous. The returned pointer can become invalid if the
underlying memory is detached, resized, transferred, or otherwise invalidated.
Using stale pointers can cause memory corruption or process crashes.

## `ffi.getCurrentEventLoop()`

<!-- YAML
added: v26.6.0
-->

* Returns: {bigint}

Returns the address of the current thread's `uv_loop_t` as a `bigint`.

The returned address is for the current Node.js environment. In the main thread,
this is the main thread event loop. In a worker thread, this is that worker's
event loop.

This is unsafe and dangerous. The returned pointer is only valid for the lifetime
of the current environment. Using it after the environment exits, or from native
code that assumes a different thread or lifetime, can crash the process or
corrupt memory.

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

[FFI Fast API internals]: https://github.com/nodejs/node/blob/HEAD/doc/contributing/ffi-fast-api-internals.md
[Fast API call path]: #fast-api-call-path
[Permission Model]: permissions.md#permission-model
[`--allow-ffi`]: cli.md#--allow-ffi
[`ffi.dlopen()`]: #ffidlopenpath-definitions
[`ffi.toBuffer(pointer, length, copy)`]: #ffitobufferpointer-length-copy
[`library.functions`]: #libraryfunctions
[`library.getFunction()`]: #librarygetfunctionname-signature
[`library.getFunctions()`]: #librarygetfunctionsdefinitions
[`library.registerCallback()`]: #libraryregistercallbacksignature-callback
[`using`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Statements/using
[call paths]: #call-paths
[generic call path]: #generic-call-path
[shared buffer call path]: #shared-buffer-call-path
[type names]: #type-names
[virtual file system]: vfs.md
