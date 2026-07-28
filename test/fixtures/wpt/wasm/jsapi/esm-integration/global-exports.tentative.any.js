// META: global=window,dedicatedworker,jsshell,shadowrealm

promise_test(async () => {
  const wasmModule = await import("./resources/globals.wasm");

  assert_equals(wasmModule.importedI32, 42);
  assert_equals(wasmModule.importedI64, 9223372036854775807n);
  assert_equals(Math.round(wasmModule.importedF32 * 100000) / 100000, 3.14159);
  assert_equals(wasmModule.importedF64, 3.141592653589793);
  assert_not_equals(wasmModule.importedExternref, null);
  assert_equals(wasmModule.importedNullExternref, null);
}, "WebAssembly module global values should be unwrapped when importing in ESM integration");

promise_test(async () => {
  const wasmModule = await import("./resources/globals.wasm");

  assert_equals(wasmModule.importedMutI32, 100);
  assert_equals(wasmModule.importedMutI64, 200n);
  assert_equals(
    Math.round(wasmModule.importedMutF32 * 100000) / 100000,
    2.71828
  );
  assert_equals(wasmModule.importedMutF64, 2.718281828459045);
  assert_not_equals(wasmModule.importedMutExternref, null);
  assert_equals(wasmModule.importedMutExternref.mutable, "global");
}, "WebAssembly mutable global values should be unwrapped when importing in ESM integration");

promise_test(async () => {
  const wasmModule = await import("./resources/globals.wasm");

  assert_equals(wasmModule["🚀localI32"], 42);
  assert_equals(wasmModule.localMutI32, 100);
  assert_equals(wasmModule.localI64, 9223372036854775807n);
  assert_equals(wasmModule.localMutI64, 200n);
  assert_equals(Math.round(wasmModule.localF32 * 100000) / 100000, 3.14159);
  assert_equals(Math.round(wasmModule.localMutF32 * 100000) / 100000, 2.71828);
  assert_equals(wasmModule.localF64, 2.718281828459045);
  assert_equals(wasmModule.localMutF64, 3.141592653589793);
}, "WebAssembly local global values should be unwrapped when exporting in ESM integration");

promise_test(async () => {
  const wasmModule = await import("./resources/globals.wasm");

  assert_equals(wasmModule.depI32, 1001);
  assert_equals(wasmModule.depMutI32, 2001);
  assert_equals(wasmModule.depI64, 10000000001n);
  assert_equals(wasmModule.depMutI64, 20000000001n);
  assert_equals(Math.round(wasmModule.depF32 * 100) / 100, 10.01);
  assert_equals(Math.round(wasmModule.depMutF32 * 100) / 100, 20.01);
  assert_equals(wasmModule.depF64, 100.0001);
  assert_equals(wasmModule.depMutF64, 200.0001);
}, "WebAssembly module globals from imported WebAssembly modules should be unwrapped");

promise_test(async () => {
  const wasmModule = await import("./resources/globals.wasm");

  assert_equals(wasmModule.importedI32, 42);
  assert_equals(wasmModule.importedMutI32, 100);
  assert_equals(wasmModule.importedI64, 9223372036854775807n);
  assert_equals(wasmModule.importedMutI64, 200n);
  assert_equals(Math.round(wasmModule.importedF32 * 100000) / 100000, 3.14159);
  assert_equals(
    Math.round(wasmModule.importedMutF32 * 100000) / 100000,
    2.71828
  );
  assert_equals(wasmModule.importedF64, 3.141592653589793);
  assert_equals(wasmModule.importedMutF64, 2.718281828459045);
  assert_equals(wasmModule.importedExternref !== null, true);
  assert_equals(wasmModule.importedMutExternref !== null, true);
  assert_equals(wasmModule.importedNullExternref, null);

  assert_equals(wasmModule["🚀localI32"], 42);
  assert_equals(wasmModule.localMutI32, 100);
  assert_equals(wasmModule.localI64, 9223372036854775807n);
  assert_equals(wasmModule.localMutI64, 200n);
  assert_equals(Math.round(wasmModule.localF32 * 100000) / 100000, 3.14159);
  assert_equals(Math.round(wasmModule.localMutF32 * 100000) / 100000, 2.71828);
  assert_equals(wasmModule.localF64, 2.718281828459045);
  assert_equals(wasmModule.localMutF64, 3.141592653589793);

  assert_equals(wasmModule.getImportedMutI32(), 100);
  assert_equals(wasmModule.getImportedMutI64(), 200n);
  assert_equals(
    Math.round(wasmModule.getImportedMutF32() * 100000) / 100000,
    2.71828
  );
  assert_equals(wasmModule.getImportedMutF64(), 2.718281828459045);
  assert_equals(wasmModule.getImportedMutExternref() !== null, true);

  assert_equals(wasmModule.getLocalMutI32(), 100);
  assert_equals(wasmModule.getLocalMutI64(), 200n);
  assert_equals(
    Math.round(wasmModule.getLocalMutF32() * 100000) / 100000,
    2.71828
  );
  assert_equals(wasmModule.getLocalMutF64(), 3.141592653589793);
  assert_equals(wasmModule.getLocalMutExternref(), null);

  assert_equals(wasmModule.depI32, 1001);
  assert_equals(wasmModule.depMutI32, 2001);
  assert_equals(wasmModule.getDepMutI32(), 2001);
  assert_equals(wasmModule.depI64, 10000000001n);
  assert_equals(wasmModule.depMutI64, 20000000001n);
  assert_equals(wasmModule.getDepMutI64(), 20000000001n);
  assert_equals(Math.round(wasmModule.depF32 * 100) / 100, 10.01);
  assert_equals(Math.round(wasmModule.depMutF32 * 100) / 100, 20.01);
  assert_equals(Math.round(wasmModule.getDepMutF32() * 100) / 100, 20.01);
  assert_equals(wasmModule.depF64, 100.0001);
  assert_equals(wasmModule.depMutF64, 200.0001);
  assert_equals(wasmModule.getDepMutF64(), 200.0001);
}, "WebAssembly should properly handle all global types");
