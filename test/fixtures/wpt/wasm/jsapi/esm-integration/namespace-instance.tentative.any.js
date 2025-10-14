// META: global=window,dedicatedworker,jsshell,shadowrealm

promise_test(async () => {
  const wasmNamespace = await import("./resources/mutable-global-export.wasm");
  const instance = WebAssembly.namespaceInstance(wasmNamespace);

  assert_true(instance instanceof WebAssembly.Instance);

  wasmNamespace.setGlobal(999);
  assert_equals(instance.exports.getGlobal(), 999);

  instance.exports.setGlobal(888);
  assert_equals(wasmNamespace.getGlobal(), 888);
}, "WebAssembly.namespaceInstance() should return the underlying instance with shared state");

promise_test(async () => {
  assert_throws_js(TypeError, () => WebAssembly.namespaceInstance({}));
  assert_throws_js(TypeError, () => WebAssembly.namespaceInstance(null));
  assert_throws_js(TypeError, () => WebAssembly.namespaceInstance(undefined));
  assert_throws_js(TypeError, () => WebAssembly.namespaceInstance(42));
  assert_throws_js(TypeError, () =>
    WebAssembly.namespaceInstance("not a namespace")
  );
  assert_throws_js(TypeError, () => WebAssembly.namespaceInstance([]));
  assert_throws_js(TypeError, () =>
    WebAssembly.namespaceInstance(function () {})
  );

  const jsModule = await import("./resources/globals.js");
  assert_throws_js(TypeError, () => WebAssembly.namespaceInstance(jsModule));
}, "WebAssembly.namespaceInstance() should throw TypeError for non-WebAssembly namespaces");

promise_test(async () => {
  const exportsModule = await import("./resources/exports.wasm");
  const globalsModule = await import("./resources/globals.wasm");

  const exportsInstance = WebAssembly.namespaceInstance(exportsModule);
  const globalsInstance = WebAssembly.namespaceInstance(globalsModule);

  assert_not_equals(exportsInstance, globalsInstance);
  assert_true(exportsInstance.exports.func instanceof Function);
  assert_true(globalsInstance.exports.getLocalMutI32 instanceof Function);

  globalsModule.setLocalMutI32(12345);
  assert_equals(globalsInstance.exports.getLocalMutI32(), 12345);

  globalsInstance.exports.setLocalMutI32(54321);
  assert_equals(globalsModule.getLocalMutI32(), 54321);

  const exportsInstance2 = WebAssembly.namespaceInstance(exportsModule);
  assert_equals(exportsInstance, exportsInstance2);
}, "WebAssembly.namespaceInstance() should work correctly with multiple modules");
