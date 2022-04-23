// META: global=window,dedicatedworker,jsshell
// META: script=/wasm/jsapi/wasm-module-builder.js
// META: script=/wasm/jsapi/assertions.js
// META: script=/wasm/jsapi/instanceTestFactory.js

let emptyModuleBinary;
setup(() => {
  emptyModuleBinary = new WasmModuleBuilder().toBuffer();
});

test(() => {
  assert_function_name(WebAssembly.Instance, "Instance", "WebAssembly.Instance");
}, "name");

test(() => {
  assert_function_length(WebAssembly.Instance, 1, "WebAssembly.Instance");
}, "length");

test(() => {
  assert_throws_js(TypeError, () => new WebAssembly.Instance());
}, "No arguments");

test(() => {
  const invalidArguments = [
    undefined,
    null,
    true,
    "",
    Symbol(),
    1,
    {},
    WebAssembly.Module,
    WebAssembly.Module.prototype,
  ];
  for (const argument of invalidArguments) {
    assert_throws_js(TypeError, () => new WebAssembly.Instance(argument),
                     `new Instance(${format_value(argument)})`);
  }
}, "Non-Module arguments");

test(() => {
  const module = new WebAssembly.Module(emptyModuleBinary);
  assert_throws_js(TypeError, () => WebAssembly.Instance(module));
}, "Calling");

for (const [name, fn] of instanceTestFactory) {
  test(() => {
    const { buffer, args, exports, verify } = fn();
    const module = new WebAssembly.Module(buffer);
    const instance = new WebAssembly.Instance(module, ...args);
    assert_Instance(instance, exports);
    verify(instance);
  }, name);
}
