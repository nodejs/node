// META: global=window,dedicatedworker,jsshell
// META: script=/wasm/jsapi/wasm-module-builder.js

test(() => {
  const emptyModuleBinary = new WasmModuleBuilder().toBuffer();
  const module = new WebAssembly.Module(emptyModuleBinary);
  const instance = new WebAssembly.Instance(module);
  assert_class_string(instance, "WebAssembly.Instance");
}, "Object.prototype.toString on an Instance");

test(() => {
  assert_own_property(WebAssembly.Instance.prototype, Symbol.toStringTag);

  const propDesc = Object.getOwnPropertyDescriptor(WebAssembly.Instance.prototype, Symbol.toStringTag);
  assert_equals(propDesc.value, "WebAssembly.Instance", "value");
  assert_equals(propDesc.configurable, true, "configurable");
  assert_equals(propDesc.enumerable, false, "enumerable");
  assert_equals(propDesc.writable, false, "writable");
}, "@@toStringTag exists on the prototype with the appropriate descriptor");
