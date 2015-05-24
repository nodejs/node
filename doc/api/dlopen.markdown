# dlopen

    Stability: 1 - Experimental

Dynamic library loader module.

Use `require('dlopen')` to access this module.

## dlopen.extension

* String

File extension used for dynamic libraries.

For example, on Mac OS X:

    console.log(dlopen.extension);
    // '.dylib'

## dlopen.dlopen(name)

* `name` String or `null` - library filename
* Return: Buffer - backing store for the `uv_lib_t` instance

Load and link a dynamic library with filename `name`.
If `null` is given as the name, then the current node process is
dynamically loaded instead (i.e. you can load symbols already
loaded into memory).

Example:

    var libc = dlopen.dlopen('libc.so');
    // <Buffer 98 b8 a9 6c ff 7f 00 00 00 00 00 00 00 00 00 00>

    // null for the current process' memory
    var currentProcess = dlopen.dlopen(null);
    // <Buffer fe ff ff ff ff ff ff ff 00 00 00 00 00 00 00 00>

    // error is thrown if something goes wrong
    dlopen.dlopen('libdoesnotexist.so')
    // Error: dlopen(libdoesnotexist.so, 1): image not found
    //     at Object.dlopen (dlopen.js:9:11)

## dlopen.dlclose(lib)

* `lib` Buffer - the buffer previously returned from `dlopen()`

Closes dynamic library `lib`.

Example:

    dlopen.dlclose(libc);

## dlopen.dlsym(lib, namem)

* `lib` Buffer - the buffer previously returned from `dlopen()`
* `name` String - name of the symbol to retrieve from `lib`
* Return: Buffer - a pointer-sized buffer containing the address of `name`

Get the memory address of symbol `name` from dynamic library `lib`.
A new Buffer instance is returned containing the memory address of
the loaded symbol.

Almost always, you will call one of the Buffer `readPointer*()`
functions on the returned buffer in order to interact with the symbol
further.

Example:

    var absSymPtr = dlopen.dlsym(libc, 'abs');
    // <Buffer 73 75 7d 98 ff 7f 00 00>

    // error is thrown if symbol does not exist
    dlopen.dlsym(libc, 'doesnotexist')
    // Error: dlsym(0x7fff6ad9f898, doesnotexist): symbol not found
    //     at Object.dlsym (dlopen.js:24:11)

## dlopen.dlerror(lib)

* `lib` Buffer - the buffer previously returned from `dlopen()`
* Return: String - most recent error that has occured on `lib`

Get previous error message from dynamic library `lib`.

You most likely won't need to use this function, since `dlopen()`
and `dlsym()` use them internally when something goes wrong.


Example:

    dlopen.dlerror(libc)
    // 'no error'
