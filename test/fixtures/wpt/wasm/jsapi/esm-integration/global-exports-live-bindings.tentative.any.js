// META: global=window,dedicatedworker,jsshell,shadowrealm

promise_test(async () => {
  const wasmExports = await import("./resources/globals.wasm");

  wasmExports.setLocalMutI32(555);
  assert_equals(wasmExports.getLocalMutI32(), 555);
  assert_equals(wasmExports.localMutI32, 555);

  wasmExports.setLocalMutI64(444n);
  assert_equals(wasmExports.getLocalMutI64(), 444n);
  assert_equals(wasmExports.localMutI64, 444n);

  wasmExports.setLocalMutF32(3.33);
  assert_equals(Math.round(wasmExports.getLocalMutF32() * 100) / 100, 3.33);
  assert_equals(Math.round(wasmExports.localMutF32 * 100) / 100, 3.33);

  wasmExports.setLocalMutF64(2.22);
  assert_equals(wasmExports.getLocalMutF64(), 2.22);
  assert_equals(wasmExports.localMutF64, 2.22);

  const anotherTestObj = { another: "test object" };
  wasmExports.setLocalMutExternref(anotherTestObj);
  assert_equals(wasmExports.getLocalMutExternref(), anotherTestObj);
  assert_equals(wasmExports.localMutExternref, anotherTestObj);
}, "Local mutable global exports should be live bindings");

promise_test(async () => {
  const wasmExports = await import("./resources/globals.wasm");

  wasmExports.setDepMutI32(3001);
  assert_equals(wasmExports.getDepMutI32(), 3001);
  assert_equals(wasmExports.depMutI32, 3001);

  wasmExports.setDepMutI64(30000000001n);
  assert_equals(wasmExports.getDepMutI64(), 30000000001n);
  assert_equals(wasmExports.depMutI64, 30000000001n);

  wasmExports.setDepMutF32(30.01);
  assert_equals(Math.round(wasmExports.getDepMutF32() * 100) / 100, 30.01);
  assert_equals(Math.round(wasmExports.depMutF32 * 100) / 100, 30.01);

  wasmExports.setDepMutF64(300.0001);
  assert_equals(wasmExports.getDepMutF64(), 300.0001);
  assert_equals(wasmExports.depMutF64, 300.0001);
}, "Dep module mutable global exports should be live bindings");
