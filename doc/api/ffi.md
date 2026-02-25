# FFI

<!--introduced_in=REPLACEME-->

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

<!-- source_link=lib/ffi.js -->

The `node:ffi` module provides an experimental foreign function interface (FFI)
for calling functions from dynamic libraries. This API is unsafe and can crash
the process if used incorrectly.

To access it:

```mjs
import ffi from 'node:ffi';
```

```cjs
const ffi = require('node:ffi');
```

This module is only available under the `node:` scheme and is gated by the
`--experimental-ffi` flag.

When using the [Permission Model][], access to this module is restricted unless
the [`--allow-ffi`][] flag is provided.

## `dlopen(path, symbols)`

<!-- YAML
added: REPLACEME
-->

* `path` {string} Path to a dynamic library.
* `symbols` {Object} A map of symbol names to signature objects.
* Returns: {Object} A library object with `symbols`, `close()`, and `getSymbol()`.

Loads a dynamic library and returns an object with resolved symbols. The
`symbols` object maps symbol names to signature objects with `parameters` and
`result` fields.

```mjs
import { dlopen } from 'node:ffi';
const lib = dlopen('libc.so.6', {
  abs: { parameters: ['i32'], result: 'i32' },
  strlen: { parameters: ['pointer'], result: 'u64' },
});

console.log(lib.symbols.abs(-42));
```

```cjs
const { dlopen } = require('node:ffi');
const lib = dlopen('libc.so.6', {
  abs: { parameters: ['i32'], result: 'i32' },
  strlen: { parameters: ['pointer'], result: 'u64' },
});

console.log(lib.symbols.abs(-42));
```

### Symbol signatures

Each symbol signature is an object with the following fields:

* `parameters` {(string|Object)\[]} Array of argument types.
* `result` {string|Object} Return type.

Supported type names:

* `void`
* `i8`, `u8`
* `i16`, `u16`
* `i32`, `u32`
* `i64`, `u64`
* `f32`, `f64`
* `pointer`
* `bool`
* `buffer`
* `function`
* `usize`, `isize`
* Struct descriptor objects: `{ struct: [/* field types */] }`

### `library.symbols`

The `symbols` property contains JavaScript functions for each symbol specified
in the `symbols` argument. These functions are named and expose the appropriate
`length` based on the symbol signature.

### `library.close()`

Closes the dynamic library handle. After closing, previously resolved symbols
may no longer be valid.

### `library.getSymbol(name)`

* `name` {string} Symbol name.
* Returns: {PointerObject} An opaque pointer object.

Resolves a symbol and returns a PointerObject that can be used with
`new UnsafeFnPointer()`.

## PointerObject

A PointerObject is an opaque, immutable object representing a memory pointer.
It has a null prototype and cannot be modified. The actual pointer address is
hidden and can only be accessed through `UnsafePointer.value()`.

PointerObjects are created by:

* `UnsafePointer.create(value)` - Create from BigInt
* `library.getSymbol(name)` - Get function pointer from library
* `UnsafePointer.offset(pointer, offset)` - Create offset pointer

## Struct types

Struct types are expressed inline as type descriptors instead of a class.

* Struct descriptor: `{ struct: [fieldType1, fieldType2, ...] }`
* Each field type can be any supported native type string or another struct
  descriptor.

### Using struct descriptors with `dlopen()`

```mjs
import { dlopen } from 'node:ffi';

const Point = { struct: ['i32', 'i32'] };

const lib = dlopen('./mylib.so', {
  make_point: {
    parameters: ['i32', 'i32'],
    result: Point,
  },
  point_distance_squared: {
    parameters: [Point, Point],
    result: 'i32',
  },
});

const p = lib.symbols.make_point(3, 4);
// Struct return values are raw bytes.
console.log(p instanceof Uint8Array); // true

const q = new Uint8Array(8);
new DataView(q.buffer).setInt32(0, 0, true);
new DataView(q.buffer).setInt32(4, 0, true);

console.log(lib.symbols.point_distance_squared(p, q)); // 25
```

```cjs
const { dlopen } = require('node:ffi');

const Point = { struct: ['i32', 'i32'] };

const lib = dlopen('./mylib.so', {
  make_point: {
    parameters: ['i32', 'i32'],
    result: Point,
  },
  point_distance_squared: {
    parameters: [Point, Point],
    result: 'i32',
  },
});

const p = lib.symbols.make_point(3, 4);
console.log(p instanceof Uint8Array); // true
```

For struct parameters, pass an `ArrayBuffer` or `ArrayBufferView`
(`Uint8Array`, `DataView`, etc.) containing bytes that match the target native
struct layout.

## Class: `UnsafeFnPointer`

<!-- YAML
added: REPLACEME
-->

Create a callable wrapper around a function pointer.

### `new UnsafeFnPointer(pointer, definition)`

* `pointer` {PointerObject|bigint} A PointerObject or BigInt representing the
  function pointer address.
* `definition` {Object} Function signature definition.
  * `parameters` {(string|Object)\[]} Parameter types.
  * `result` {string|Object} Return type.
  * `nonblocking` {boolean} When `true`, function calls will run on a dedicated
    blocking thread and will return a Promise. **Default:** `false`.

Creates a callable function wrapper around a native function pointer.

```mjs
import { dlopen, UnsafeFnPointer } from 'node:ffi';

const lib = dlopen('libc.so.6', {});
const mallocSymbol = lib.getSymbol('malloc');

const malloc = new UnsafeFnPointer(mallocSymbol, {
  parameters: ['u64'],
  result: 'pointer',
});

const ptr = malloc(1024n);
```

```cjs
const { dlopen, UnsafeFnPointer } = require('node:ffi');

const lib = dlopen('libc.so.6', {});
const mallocSymbol = lib.getSymbol('malloc');

const malloc = new UnsafeFnPointer(mallocSymbol, {
  parameters: ['u64'],
  result: 'pointer',
});

const ptr = malloc(1024n);
```

Instances can be called directly as functions.

## Class: `UnsafeCallback`

<!-- YAML
added: REPLACEME
-->

Create a native callback that can be passed to FFI functions.

### `new UnsafeCallback(definition, callback)`

* `definition` {Object} Callback signature definition.
  * `parameters` {string\[]} Parameter types.
  * `result` {string} Return type.
* `callback` {Function} JavaScript callback function.

Creates an unsafe callback function pointer.

### `unsafeCallback.definition`

* {Object} The callback signature definition used to create the callback.

### `unsafeCallback.callback`

* {Function} The JavaScript callback function.

### `unsafeCallback.pointer`

* {PointerObject} Native function pointer for the callback.

### `unsafeCallback.ref()`

* Returns: {number} The new reference count.

Increments the callback reference count.

### `unsafeCallback.unref()`

* Returns: {number} The new reference count.

Decrements the callback reference count.

### `unsafeCallback.close()`

Closes the callback and releases native callback resources.

### `UnsafeCallback.threadSafe(definition, callback)`

* `definition` {Object} Callback signature definition.
* `callback` {Function} JavaScript callback function.
* Returns: {UnsafeCallback}

Creates an `UnsafeCallback` and calls `ref()` once on the instance.

```mjs
import { UnsafeCallback, UnsafePointer } from 'node:ffi';

const cb = new UnsafeCallback(
  { parameters: ['i32', 'i32'], result: 'i32' },
  (a, b) => a + b,
);

const ptr = cb.pointer;
console.log(UnsafePointer.value(ptr));

cb.close();
```

```cjs
const { UnsafeCallback, UnsafePointer } = require('node:ffi');

const cb = new UnsafeCallback(
  { parameters: ['i32', 'i32'], result: 'i32' },
  (a, b) => a + b,
);

const ptr = cb.pointer;
console.log(UnsafePointer.value(ptr));

cb.close();
```

## PointerObject

A PointerObject is an opaque, immutable object representing a memory pointer.
It has a null prototype and cannot be modified. The actual pointer address is
hidden and can only be accessed through `UnsafePointer.value()`.

PointerObjects are created by:

* `UnsafePointer.create(value)` - Create from BigInt
* `library.getSymbol(name)` - Get function pointer from library
* `UnsafePointer.offset(pointer, offset)` - Create offset pointer

## Class: `UnsafePointer`

<!-- YAML
added: REPLACEME
-->

Utility class providing static methods for working with pointers.

### `UnsafePointer.create(value)`

<!-- YAML
added: REPLACEME
-->

* `value` {bigint} The pointer address.
* Returns: {PointerObject}

Creates a PointerObject from a numeric pointer address. This is dangerous and
should only be used when you know the address is valid.

```mjs
import { UnsafePointer } from 'node:ffi';
const ptr = UnsafePointer.create(0x12345678n);
```

```cjs
const { UnsafePointer } = require('node:ffi');
const ptr = UnsafePointer.create(0x12345678n);
```

### `UnsafePointer.value(pointer)`

<!-- YAML
added: REPLACEME
-->

* `pointer` {PointerObject} The pointer to extract the address from.
* Returns: {bigint}

Returns the numeric address of a PointerObject.

```mjs
import { UnsafePointer } from 'node:ffi';
const ptr = UnsafePointer.create(0x12345678n);
const addr = UnsafePointer.value(ptr);
console.log(addr); // 0x12345678n
```

```cjs
const { UnsafePointer } = require('node:ffi');
const ptr = UnsafePointer.create(0x12345678n);
const addr = UnsafePointer.value(ptr);
console.log(addr); // 0x12345678n
```

### `UnsafePointer.offset(pointer, offset)`

<!-- YAML
added: REPLACEME
-->

* `pointer` {PointerObject} The base pointer.
* `offset` {number|bigint} The byte offset.
* Returns: {PointerObject}

Returns a new PointerObject offset from the original by the specified number
of bytes.

```mjs
import { UnsafePointer } from 'node:ffi';
const ptr = UnsafePointer.create(0x1000n);
const offsetPtr = UnsafePointer.offset(ptr, 16);
const addr = UnsafePointer.value(offsetPtr);
console.log(addr); // 0x1010n
```

```cjs
const { UnsafePointer } = require('node:ffi');
const ptr = UnsafePointer.create(0x1000n);
const offsetPtr = UnsafePointer.offset(ptr, 16);
const addr = UnsafePointer.value(offsetPtr);
console.log(addr); // 0x1010n
```

### `UnsafePointer.equals(a, b)`

<!-- YAML
added: REPLACEME
-->

* `a` {PointerObject} The first pointer.
* `b` {PointerObject} The second pointer.
* Returns: {boolean}

Returns `true` if the two pointers point to the same address.

```mjs
import { UnsafePointer } from 'node:ffi';
const ptr1 = UnsafePointer.create(0x1000n);
const ptr2 = UnsafePointer.create(0x1000n);
const ptr3 = UnsafePointer.create(0x2000n);

console.log(UnsafePointer.equals(ptr1, ptr2)); // true
console.log(UnsafePointer.equals(ptr1, ptr3)); // false
```

```cjs
const { UnsafePointer } = require('node:ffi');
const ptr1 = UnsafePointer.create(0x1000n);
const ptr2 = UnsafePointer.create(0x1000n);
const ptr3 = UnsafePointer.create(0x2000n);

console.log(UnsafePointer.equals(ptr1, ptr2)); // true
console.log(UnsafePointer.equals(ptr1, ptr3)); // false
```

## Class: `UnsafePointerView`

<!-- YAML
added: REPLACEME
-->

Provides a DataView-like interface for reading and writing values at memory
locations specified by PointerObjects. This class implements the JavaScript
[`DataView`][] interface along with additional methods for FFI operations.

**Warning**: This API is extremely dangerous. Incorrect usage can lead to
memory corruption, crashes, and security vulnerabilities.

### `new UnsafePointerView(pointer)`

<!-- YAML
added: REPLACEME
-->

* `pointer` {PointerObject} A pointer to the memory location.

Creates a new UnsafePointerView for the specified pointer.

```mjs
import { UnsafePointer, UnsafePointerView } from 'node:ffi';
const ptr = UnsafePointer.create(0x1000n);
const view = new UnsafePointerView(ptr);
```

```cjs
const { UnsafePointer, UnsafePointerView } = require('node:ffi');
const ptr = UnsafePointer.create(0x1000n);
const view = new UnsafePointerView(ptr);
```

### `view.getInt8([offset])`

<!-- YAML
added: REPLACEME
-->

* `offset` {number|bigint} Byte offset from the pointer. **Default:** `0`.
* Returns: {number}

Reads a signed 8-bit integer from the specified offset.

### `view.getUint8([offset])`

<!-- YAML
added: REPLACEME
-->

* `offset` {number|bigint} Byte offset from the pointer. **Default:** `0`.
* Returns: {number}

Reads an unsigned 8-bit integer from the specified offset.

### `view.getInt16([offset])`

<!-- YAML
added: REPLACEME
-->

* `offset` {number|bigint} Byte offset from the pointer. **Default:** `0`.
* Returns: {number}

Reads a signed 16-bit integer from the specified offset.

### `view.getUint16([offset])`

<!-- YAML
added: REPLACEME
-->

* `offset` {number|bigint} Byte offset from the pointer. **Default:** `0`.
* Returns: {number}

Reads an unsigned 16-bit integer from the specified offset.

### `view.getInt32([offset])`

<!-- YAML
added: REPLACEME
-->

* `offset` {number|bigint} Byte offset from the pointer. **Default:** `0`.
* Returns: {number}

Reads a signed 32-bit integer from the specified offset.

### `view.getUint32([offset])`

<!-- YAML
added: REPLACEME
-->

* `offset` {number|bigint} Byte offset from the pointer. **Default:** `0`.
* Returns: {number}

Reads an unsigned 32-bit integer from the specified offset.

### `view.getBigInt64([offset])`

<!-- YAML
added: REPLACEME
-->

* `offset` {number|bigint} Byte offset from the pointer. **Default:** `0`.
* Returns: {bigint}

Reads a signed 64-bit integer from the specified offset.

### `view.getBigUint64([offset])`

<!-- YAML
added: REPLACEME
-->

* `offset` {number|bigint} Byte offset from the pointer. **Default:** `0`.
* Returns: {bigint}

Reads an unsigned 64-bit integer from the specified offset.

### `view.getFloat32([offset])`

<!-- YAML
added: REPLACEME
-->

* `offset` {number|bigint} Byte offset from the pointer. **Default:** `0`.
* Returns: {number}

Reads a 32-bit floating point number from the specified offset.

### `view.getFloat64([offset])`

<!-- YAML
added: REPLACEME
-->

* `offset` {number|bigint} Byte offset from the pointer. **Default:** `0`.
* Returns: {number}

Reads a 64-bit floating point number from the specified offset.

### `view.setInt8(value, offset)`

<!-- YAML
added: REPLACEME
-->

* `value` {number} The value to write.
* `offset` {number|bigint} Byte offset from the pointer.

Writes a signed 8-bit integer to the specified offset.

### `view.setUint8(value, offset)`

<!-- YAML
added: REPLACEME
-->

* `value` {number} The value to write.
* `offset` {number|bigint} Byte offset from the pointer.

Writes an unsigned 8-bit integer to the specified offset.

### `view.setInt16(value, offset)`

<!-- YAML
added: REPLACEME
-->

* `value` {number} The value to write.
* `offset` {number|bigint} Byte offset from the pointer.

Writes a signed 16-bit integer to the specified offset.

### `view.setUint16(value, offset)`

<!-- YAML
added: REPLACEME
-->

* `value` {number} The value to write.
* `offset` {number|bigint} Byte offset from the pointer.

Writes an unsigned 16-bit integer to the specified offset.

### `view.setInt32(value, offset)`

<!-- YAML
added: REPLACEME
-->

* `value` {number} The value to write.
* `offset` {number|bigint} Byte offset from the pointer.

Writes a signed 32-bit integer to the specified offset.

### `view.setUint32(value, offset)`

<!-- YAML
added: REPLACEME
-->

* `value` {number} The value to write.
* `offset` {number|bigint} Byte offset from the pointer.

Writes an unsigned 32-bit integer to the specified offset.

### `view.setBigInt64(value, offset)`

<!-- YAML
added: REPLACEME
-->

* `value` {bigint} The value to write.
* `offset` {number|bigint} Byte offset from the pointer.

Writes a signed 64-bit integer to the specified offset.

### `view.setBigUint64(value, offset)`

<!-- YAML
added: REPLACEME
-->

* `value` {bigint} The value to write.
* `offset` {number|bigint} Byte offset from the pointer.

Writes an unsigned 64-bit integer to the specified offset.

### `view.setFloat32(value, offset)`

<!-- YAML
added: REPLACEME
-->

* `value` {number} The value to write.
* `offset` {number|bigint} Byte offset from the pointer.

Writes a 32-bit floating point number to the specified offset.

### `view.setFloat64(value, offset)`

<!-- YAML
added: REPLACEME
-->

* `value` {number} The value to write.
* `offset` {number|bigint} Byte offset from the pointer.

Writes a 64-bit floating point number to the specified offset.

### `view.getBool([offset])`

<!-- YAML
added: REPLACEME
-->

* `offset` {number|bigint} Byte offset from the pointer. **Default:** `0`.
* Returns: {boolean}

Reads a boolean value from the specified offset.

### `view.getCString([offset])`

<!-- YAML
added: REPLACEME
-->

* `offset` {number|bigint} Byte offset from the pointer. **Default:** `0`.
* Returns: {string}

Reads a null-terminated UTF-8 string from the specified offset. Reading stops
at the first null byte.

```mjs
import { UnsafePointer, UnsafePointerView, dlopen } from 'node:ffi';

const lib = dlopen('libc.so.6', {
  getenv: { parameters: ['pointer'], result: 'pointer' },
});

// Assuming getenv returns a pointer to a C string
const resultPtr = lib.symbols.getenv(stringToPointer('PATH'));
const view = new UnsafePointerView(resultPtr);
const path = view.getCString();
console.log(path);
```

```cjs
const { UnsafePointer, UnsafePointerView, dlopen } = require('node:ffi');

const lib = dlopen('libc.so.6', {
  getenv: { parameters: ['pointer'], result: 'pointer' },
});

const resultPtr = lib.symbols.getenv(stringToPointer('PATH'));
const view = new UnsafePointerView(resultPtr);
const path = view.getCString();
console.log(path);
```

### `view.getPointer([offset])`

<!-- YAML
added: REPLACEME
-->

* `offset` {number|bigint} Byte offset from the pointer. **Default:** `0`.
* Returns: {PointerObject}

Reads a pointer value from the specified offset and returns it as a
PointerObject.

### `view.copyInto(destination[, offset])`

<!-- YAML
added: REPLACEME
-->

* `destination` {TypedArray|DataView} The destination to copy into.
* `offset` {number|bigint} Byte offset from the pointer. **Default:** `0`.

Copies memory from the pointer into the destination TypedArray or DataView.
The number of bytes copied is determined by the destination's `byteLength`.

```mjs
import { UnsafePointer, UnsafePointerView } from 'node:ffi';

const ptr = UnsafePointer.create(0x1000n);
const view = new UnsafePointerView(ptr);
const buffer = new Uint8Array(16);
view.copyInto(buffer); // Copies 16 bytes
```

```cjs
const { UnsafePointer, UnsafePointerView } = require('node:ffi');

const ptr = UnsafePointer.create(0x1000n);
const view = new UnsafePointerView(ptr);
const buffer = new Uint8Array(16);
view.copyInto(buffer); // Copies 16 bytes
```

### `view.getArrayBuffer(byteLength[, offset])`

<!-- YAML
added: REPLACEME
-->

* `byteLength` {number} Number of bytes to include in the ArrayBuffer.
* `offset` {number|bigint} Byte offset from the pointer. **Default:** `0`.
* Returns: {ArrayBuffer}

Returns an ArrayBuffer view of the memory at the pointer. The returned
ArrayBuffer does not own the memory and modifications are reflected in the
original memory location.

**Warning**: The returned ArrayBuffer does not prevent the underlying memory
from being freed. Ensure the memory remains valid while using the ArrayBuffer.

### `UnsafePointerView.getCString(pointer[, offset])`

<!-- YAML
added: REPLACEME
-->

* `pointer` {PointerObject} The pointer to read from.
* `offset` {number|bigint} Byte offset from the pointer. **Default:** `0`.
* Returns: {string}

Static method to read a null-terminated UTF-8 string from the specified
pointer without creating an UnsafePointerView instance.

```mjs
import { UnsafePointer, UnsafePointerView } from 'node:ffi';
const ptr = UnsafePointer.create(0x1000n);
const str = UnsafePointerView.getCString(ptr);
```

```cjs
const { UnsafePointer, UnsafePointerView } = require('node:ffi');
const ptr = UnsafePointer.create(0x1000n);
const str = UnsafePointerView.getCString(ptr);
```

### `UnsafePointerView.copyInto(pointer, destination[, offset])`

<!-- YAML
added: REPLACEME
-->

* `pointer` {PointerObject} The pointer to read from.
* `destination` {TypedArray|DataView} The destination to copy into.
* `offset` {number|bigint} Byte offset from the pointer. **Default:** `0`.

Static method to copy memory from the pointer into the destination without
creating an UnsafePointerView instance.

### `UnsafePointerView.getArrayBuffer(pointer, byteLength[, offset])`

<!-- YAML
added: REPLACEME
-->

* `pointer` {PointerObject} The pointer to read from.
* `byteLength` {number} Number of bytes to include in the ArrayBuffer.
* `offset` {number|bigint} Byte offset from the pointer. **Default:** `0`.
* Returns: {ArrayBuffer}

Static method to create an ArrayBuffer view of the memory at the pointer
without creating an UnsafePointerView instance.

[Permission Model]: permissions.md#permission-model
[`--allow-ffi`]: cli.md#--allow-ffi
[`DataView`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/DataView
