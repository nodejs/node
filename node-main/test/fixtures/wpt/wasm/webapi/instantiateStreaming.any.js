// META: global=window,worker
// META: script=/wasm/jsapi/wasm-module-builder.js
// META: script=/wasm/jsapi/assertions.js
// META: script=/wasm/jsapi/instanceTestFactory.js

let emptyModuleBinary;
setup(() => {
  emptyModuleBinary = new WasmModuleBuilder().toBuffer();
});

for (const [name, fn] of instanceTestFactory) {
  promise_test(async () => {
    const { buffer, args, exports, verify } = fn();
    const response = new Response(buffer, { "headers": { "Content-Type": "application/wasm" } });
    const result = await WebAssembly.instantiateStreaming(response, ...args);
    assert_WebAssemblyInstantiatedSource(result, exports);
    verify(result.instance);
  }, name);
}

promise_test(async () => {
  const builder = new WasmModuleBuilder();
  builder.addImportedGlobal("module", "global", kWasmI32);
  const buffer = builder.toBuffer();
  const response = new Response(buffer, { "headers": { "Content-Type": "application/wasm" } });
  const order = [];

  const imports = {
    get module() {
      order.push("module getter");
      return {
        get global() {
          order.push("global getter");
          return 0;
        },
      }
    },
  };

  const expected = [
    "module getter",
    "global getter",
  ];
  const p = WebAssembly.instantiateStreaming(response, imports);
  assert_array_equals(order, []);
  const result = await p;
  assert_WebAssemblyInstantiatedSource(result, {});
  assert_array_equals(order, expected);
}, "Synchronous options handling");
