# ffi

    Stability: 1 - Experimental

A low-level "foreign function interface" module. Provides a programming
interface to various "calling conventions". This allows a programmer to
call any C function at run-time, without writing any C code.

Use `require('ffi')` to access this module.

## ffi.sizeof

* Object

Map of `sizeof()` results for basic C types. Some of the base types
have a fixed size, and others are dynamic based on the host processor.

Useful for creating new Buffer instances of correct size for FFI
arguments and return values.

For example, on a 64-bit processor:

    console.log(ffi.sizeof.int);
    // 4

    console.log(ffi.sizeof.pointer);
    // 8

## ffi.alignof

* Object

Map of `alignof()` results for basic C types. Some of the base types have
a fixed alignment, and others are dynamic based on the host processor.

For example, on a 64-bit processor:

    console.log(ffi.alignof.int);
    // 4

    console.log(ffi.alignof.pointer);
    // 8

## ffi.abi

* Object

Map of valid `ffi_abi` (application binary interface) values to pass
to `ffi.CIF()`. Valid values are dependant on your operating system,
however `ffi.abi.default` is guaranteed to always resolve to a valid ABI.

For example, on Mac OS:

    console.log(ffi.abi);
    // { default: 2, first: 0, last: 3, sysv: 1, unix64: 2 }

## ffi.types

* Object

Map of valid `ffi_type` Buffer instances. These are used to describe the
return value type and argument types for the CIF object. The Buffer instances
have a length of 0, and are meant to be used as opaque pointers passed
to `ffi.CIF()`.

For example:

    console.log(ffi.types.int64);
    // <Buffer@0x1014ebdc0 >

## ffi.CIF([abi,] rType, aTypes...)

* `abi` Buffer - The ABI, or "calling convention" to invoke this function with.
  Defaults to `ffi.abi.default` if none is specified.
* `rType` Buffer - A "type" Buffer describing the return value to the function
* `aTypes` Buffers - Zero or more "type" Buffers describing the
  arguments to the function
* Return: Buffer - an `ffi_cif`-sized buffer representing the CIF

Given a C function: `int (*)(double)`

    var cif = ffi.CIF(ffi.abi.unix64, ffi.types.int, ffi.types.double);
    // <Buffer@0x1014ebdc0 ...>

    // pass the cif to ffi.call(), ffi.Closure(), etc.

## ffi.call(cif, fn, rValue, aValues)

* `cif` Buffer - CIF buffer previously returned from `ffi.CIF()`
* `fn` Buffer - C function pointer Buffer instance (from `dlsym()`, etc.)
* `rValue` Buffer - A Buffer instance large enough to hold the return value
* `aValues` Buffer - A Buffer instance large enough to hold pointers to
  each argument

Invokes the C function pointer `fn` using the description provided by `cif`.

`rValue` must point to storage that is `ffi.sizeof.long` or larger.
For smaller return value sizes, the `ffi.sizeof.ffi_arg` or
`ffi.sizeof.ffi_sarg` integral type must be used to hold the return value.

`aValues` is essentially an array of pointers, pointing to the argument
values. It must point to storage that is at least
`ffi.sizeof.pointer * numArgs`.

Example:

    var libc = dlopen.dlopen('libc.dylib');
    var absPtr = dlopen.dlsym(libc, 'abs')['readPointer' + endianness](0);

    var cif = ffi.CIF(ffi.types.int, ffi.types.int);

    var rValue = new Buffer(Math.max(ffi.sizeof.int, ffi.sizeof.ffi_sarg));
    var aValues = new Buffer(ffi.sizeof.pointer * 1);

    // storage space for the single argument to `abs()`
    var arg = new Buffer(ffi.sizeof.int);
    aValues['writePointer' + endianness](arg, 0);

    arg['writeInt32' + endianness](-5, 0);
    ffi.call(cif, absPtr, rValue, aValues);
    console.log('abs(-5): %j', rValue['readInt32' + endianness](0));
    // abs(-5): 5

    arg['writeInt32' + endianness](3, 0);
    ffi.call(cif, absPtr, rValue, aValues);
    console.log('abs(3): %j', rValue['readInt32' + endianness](0));
    // abs(-5): 3

    dlopen.dlclose(libc);
