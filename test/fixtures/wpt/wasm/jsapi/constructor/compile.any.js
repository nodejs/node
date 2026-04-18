// META: global=window,dedicatedworker,jsshell,shadowrealm
// META: script=/wasm/jsapi/wasm-module-builder.js

function assert_Module(module) {
  assert_equals(Object.getPrototypeOf(module), WebAssembly.Module.prototype,
                "Prototype");
  assert_true(Object.isExtensible(module), "Extensibility");
}

let emptyModuleBinary;
setup(() => {
  emptyModuleBinary = new WasmModuleBuilder().toBuffer();
});

promise_test(t => {
  return promise_rejects_js(t, TypeError, WebAssembly.compile());
}, "Missing argument");

promise_test(t => {
  const invalidArguments = [
    undefined,
    null,
    true,
    "",
    Symbol(),
    1,
    {},
    ArrayBuffer,
    ArrayBuffer.prototype,
    Array.from(emptyModuleBinary),
  ];
  return Promise.all(invalidArguments.map(argument => {
    return promise_rejects_js(t, TypeError, WebAssembly.compile(argument),
                           `compile(${format_value(argument)})`);
  }));
}, "Invalid arguments");

promise_test(() => {
  const fn = WebAssembly.compile;
  const thisValues = [
    undefined,
    null,
    true,
    "",
    Symbol(),
    1,
    {},
    WebAssembly,
  ];
  return Promise.all(thisValues.map(thisValue => {
    return fn.call(thisValue, emptyModuleBinary).then(assert_Module);
  }));
}, "Branding");

test(() => {
  const promise = WebAssembly.compile(emptyModuleBinary);
  assert_equals(Object.getPrototypeOf(promise), Promise.prototype, "prototype");
  assert_true(Object.isExtensible(promise), "extensibility");
}, "Promise type");

promise_test(t => {
  const buffer = new Uint8Array();
  return promise_rejects_js(t, WebAssembly.CompileError, WebAssembly.compile(buffer));
}, "Empty buffer");

promise_test(t => {
  const buffer = new Uint8Array(Array.from(emptyModuleBinary).concat([0, 0]));
  return promise_rejects_js(t, WebAssembly.CompileError, WebAssembly.compile(buffer));
}, "Invalid code");

promise_test(() => {
  return WebAssembly.compile(emptyModuleBinary).then(assert_Module);
}, "Result type");

promise_test(() => {
  return WebAssembly.compile(emptyModuleBinary, {}).then(assert_Module);
}, "Stray argument");

promise_test(() => {
  const buffer = new WasmModuleBuilder().toBuffer();
  assert_equals(buffer[0], 0);
  const promise = WebAssembly.compile(buffer);
  buffer[0] = 1;
  return promise.then(assert_Module);
}, "Changing the buffer");
