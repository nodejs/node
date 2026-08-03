// META: global=window,dedicatedworker,jsshell

test(() => {
  const argument = { "value": "i32" };
  const global = new WebAssembly.Global(argument);
  assert_class_string(global, "WebAssembly.Global");
}, "Object.prototype.toString on an Global");

test(() => {
  assert_own_property(WebAssembly.Global.prototype, Symbol.toStringTag);

  const propDesc = Object.getOwnPropertyDescriptor(WebAssembly.Global.prototype, Symbol.toStringTag);
  assert_equals(propDesc.value, "WebAssembly.Global", "value");
  assert_equals(propDesc.configurable, true, "configurable");
  assert_equals(propDesc.enumerable, false, "enumerable");
  assert_equals(propDesc.writable, false, "writable");
}, "@@toStringTag exists on the prototype with the appropriate descriptor");
