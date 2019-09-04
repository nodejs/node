# WASI Tests

Compile with clang and `wasm32-wasi` target. The clang version used must be
built with wasi-libc. You can specify the location for clang and the sysroot
if needed when running make:
```console
$ make CC=/usr/local/opt/llvm/bin/clang SYSROOT=/path/to/wasi-libc/sysroot
```
