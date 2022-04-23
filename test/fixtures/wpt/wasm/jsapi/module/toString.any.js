// META: global=window,dedicatedworker,jsshell
// META: script=/wasm/jsapi/wasm-module-builder.js

test(() => {
  const emptyModuleBinary = new WasmModuleBuilder().toBuffer();
  const module = new WebAssembly.Module(emptyModuleBinary);
  assert_class_string(module, "WebAssembly.Module");
}, "Object.prototype.toString on an Module");

test(() => {
  assert_own_property(WebAssembly.Module.prototype, Symbol.toStringTag);

  const propDesc = Object.getOwnPropertyDescriptor(WebAssembly.Module.prototype, Symbol.toStringTag);
  assert_equals(propDesc.value, "WebAssembly.Module", "value");
  assert_equals(propDesc.configurable, true, "configurable");
  assert_equals(propDesc.enumerable, false, "enumerable");
  assert_equals(propDesc.writable, false, "writable");
}, "@@toStringTag exists on the prototype with the appropriate descriptor");
