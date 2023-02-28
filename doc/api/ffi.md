# Foreign Function Interface (FFI)

<!--introduced_in=REPLACEME-->

> Stability: 1 - Experimental

<!-- source_link=lib/ffi.js -->

The `node:ffi` module provides a foreign function interface (FFI). It can be
used to make native function calls without the use of native addons. To access
it:

```mjs
import ffi from 'node:ffi';
```

```cjs
const ffi = require('node:ffi');
```

This module is only available under the `node:` scheme. The following will not
work:

```mjs
import ffi from 'ffi';
```

```cjs
const ffi = require('ffi');
```

## Supported Types

The following types are supported.

* `char`
* `signed char`
* `unsigned char`
* `short`
* `short int`
* `signed short`
* `signed short int`
* `unsigned short`
* `unsigned short int`
* `int`
* `signed`
* `signed int`
* `unsigned`
* `unsigned int`
* `long`
* `long int`
* `signed long`
* `signed long int`
* `unsigned long`
* `unsigned long int`
* `long long`
* `long long int`
* `signed long long`
* `signed long long int`
* `unsigned long long`
* `unsigned long long int`
* `float`
* `double`
* `uint8_t`
* `int8_t`
* `uint16_t`
* `int16_t`
* `uint32_t`
* `int32_t`
* `uint64_t`
* `int64_t`

For functions with pointer arguments or return values, use the type `pointer`.
Any type provided that contains an asterisk (`*`) will be treated as `pointer`.

All types map to `number` in JavaScript, except:

* 64-bit integer types, which are always mapped to `bigint`
* Pointers, which are mapped to `bigint` on systems with 64-bit pointers.

Currently, no conversion is done between `number` and `bigint`, so be careful
when calling functions returned from `getNativeFunction`.

Function pointers are not currently supported.

Structs are not supported as arguments or return values, but pointers to them
are. Structs can be built up in or read from `Buffer`s, and `getBufferPointer`
can be used from there.

## `ffi.getBufferPointer(buffer)`

<!-- YAML
added: REPLACEME
-->

* `buffer` {Buffer|TypedArray|ArrayBuffer} A buffer to get the pointer of.
* Returns: {bigint} A pointer, represented as a `bigint`.

Gets the memory address of the first byte of `buffer`. If there's an offset,
that's added to the result.

## `ffi.getNativeFunction(library, func, retType, argTypes)`

<!-- YAML
added: REPLACEME
-->

* `library` {string|null} The path to the shared library.
* `func` {string} The function name inside the shared library.
* `retType` {string} The return value's type.
* `argTypes` {Array} The types of the arguments.
* Returns: {Function} A JavaScript wrapper for the native function.

Retrieves a native function from a given shared library (i.e. `.so`, `.dylib`,
or `.dll`), and returns a JavaScript wrapper function. The wrapper function does
the appropriate conversion from JavaScript `number`s and `bigints` to the
appropriate types as per the `argTypes` array, and similarly for return values.

If `null` is provided as the `library`, function symbols will be searched for in
the current process.

## `ffi.sizeof(type)`

<!-- YAML
added: REPLACEME
-->

* `type` {string} The name of the type to get the size of.
* Returns: {number} The size of the the type in bytes.

A passthrough to the C `sizeof` operator. The `type` must be one of the
supported types, or else the return value will be `undefined`.
