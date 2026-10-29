// META: global=window,dedicatedworker,jsshell,shadowrealm
// META: script=/wasm/jsapi/wasm-module-builder.js
// META: script=/wasm/jsapi/assertions.js

let emptyModuleBinary;
setup(() => {
  emptyModuleBinary = new WasmModuleBuilder().toBuffer();
});

test(() => {
  assert_function_name(WebAssembly.Module, "Module", "WebAssembly.Module");
}, "name");

test(() => {
  assert_function_length(WebAssembly.Module, 1, "WebAssembly.Module");
}, "length");

test(() => {
  assert_throws_js(TypeError, () => new WebAssembly.Module());
}, "No arguments");

test(() => {
  assert_throws_js(TypeError, () => WebAssembly.Module(emptyModuleBinary));
}, "Calling");

test(() => {
  const invalidArguments = [
    undefined,
    null,
    true,
    "test",
    Symbol(),
    7,
    NaN,
    {},
    ArrayBuffer,
    ArrayBuffer.prototype,
    Array.from(emptyModuleBinary),
  ];
  for (const argument of invalidArguments) {
    assert_throws_js(TypeError, () => new WebAssembly.Module(argument),
                     `new Module(${format_value(argument)})`);
  }
}, "Invalid arguments");

test(() => {
  const buffer = new Uint8Array();
  assert_throws_js(WebAssembly.CompileError, () => new WebAssembly.Module(buffer));
}, "Empty buffer");

test(() => {
  const buffer = new Uint8Array(Array.from(emptyModuleBinary).concat([0, 0]));
  assert_throws_js(WebAssembly.CompileError, () => new WebAssembly.Module(buffer));
}, "Invalid code");

test(() => {
  const module = new WebAssembly.Module(emptyModuleBinary);
  assert_equals(Object.getPrototypeOf(module), WebAssembly.Module.prototype);
}, "Prototype");

test(() => {
  const module = new WebAssembly.Module(emptyModuleBinary);
  assert_true(Object.isExtensible(module));
}, "Extensibility");

test(() => {
  const module = new WebAssembly.Module(emptyModuleBinary, {});
  assert_equals(Object.getPrototypeOf(module), WebAssembly.Module.prototype);
}, "Stray argument");
