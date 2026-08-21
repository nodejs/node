// META: global=window,dedicatedworker,jsshell
// META: script=/wasm/jsapi/wasm-module-builder.js

let emptyModuleBinary;
setup(() => {
  emptyModuleBinary = new WasmModuleBuilder().toBuffer();
});

test(() => {
  const thisValues = [
    undefined,
    null,
    true,
    "",
    Symbol(),
    1,
    {},
    WebAssembly.Instance,
    WebAssembly.Instance.prototype,
  ];

  const desc = Object.getOwnPropertyDescriptor(WebAssembly.Instance.prototype, "exports");
  assert_equals(typeof desc, "object");

  const getter = desc.get;
  assert_equals(typeof getter, "function");

  assert_equals(typeof desc.set, "undefined");

  for (const thisValue of thisValues) {
    assert_throws_js(TypeError, () => getter.call(thisValue), `this=${format_value(thisValue)}`);
  }
}, "Branding");

test(() => {
  const module = new WebAssembly.Module(emptyModuleBinary);
  const instance = new WebAssembly.Instance(module);
  const exports = instance.exports;

  const desc = Object.getOwnPropertyDescriptor(WebAssembly.Instance.prototype, "exports");
  assert_equals(typeof desc, "object");

  const getter = desc.get;
  assert_equals(typeof getter, "function");

  assert_equals(getter.call(instance, {}), exports);
}, "Stray argument");

test(() => {
  const module = new WebAssembly.Module(emptyModuleBinary);
  const instance = new WebAssembly.Instance(module);
  const exports = instance.exports;
  instance.exports = {};
  assert_equals(instance.exports, exports, "Should not change the exports");
}, "Setting (sloppy mode)");

test(() => {
  const module = new WebAssembly.Module(emptyModuleBinary);
  const instance = new WebAssembly.Instance(module);
  const exports = instance.exports;
  assert_throws_js(TypeError, () => {
    "use strict";
    instance.exports = {};
  });
  assert_equals(instance.exports, exports, "Should not change the exports");
}, "Setting (strict mode)");
