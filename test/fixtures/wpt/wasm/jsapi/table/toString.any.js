// META: global=window,dedicatedworker,jsshell,shadowrealm

test(() => {
  const argument = { "element": "anyfunc", "initial": 0 };
  const table = new WebAssembly.Table(argument);
  assert_class_string(table, "WebAssembly.Table");
}, "Object.prototype.toString on an Table");

test(() => {
  assert_own_property(WebAssembly.Table.prototype, Symbol.toStringTag);

  const propDesc = Object.getOwnPropertyDescriptor(WebAssembly.Table.prototype, Symbol.toStringTag);
  assert_equals(propDesc.value, "WebAssembly.Table", "value");
  assert_equals(propDesc.configurable, true, "configurable");
  assert_equals(propDesc.enumerable, false, "enumerable");
  assert_equals(propDesc.writable, false, "writable");
}, "@@toStringTag exists on the prototype with the appropriate descriptor");
