// META: global=window,dedicatedworker,jsshell

test(() => {
  const argument = { "initial": 0 };
  const memory = new WebAssembly.Memory(argument);
  assert_class_string(memory, "WebAssembly.Memory");
}, "Object.prototype.toString on an Memory");

test(() => {
  assert_own_property(WebAssembly.Memory.prototype, Symbol.toStringTag);

  const propDesc = Object.getOwnPropertyDescriptor(WebAssembly.Memory.prototype, Symbol.toStringTag);
  assert_equals(propDesc.value, "WebAssembly.Memory", "value");
  assert_equals(propDesc.configurable, true, "configurable");
  assert_equals(propDesc.enumerable, false, "enumerable");
  assert_equals(propDesc.writable, false, "writable");
}, "@@toStringTag exists on the prototype with the appropriate descriptor");
