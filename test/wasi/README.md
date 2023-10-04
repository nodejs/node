# WASI Tests

Compile with clang and `wasm32-wasi` target by using the wasi-sdk
[version 20](https://github.com/WebAssembly/wasi-sdk/releases/tag/wasi-sdk-20)

Install wasi-sdk and then set WASI\_SDK\_PATH to the root of the install.

You can then rebuild the wasm for the tests by running:

```bash
make CC=${WASI_SDK_PATH}/bin/clang SYSROOT=${WASI_SDK_PATH}/share/wasi-sysroot
```

If you update the version of the wasi-sdk to be used for the compile
remove all of the \*.wasm files in the wasm directory to ensure
you rebuild/test all of the tests with the new version.
