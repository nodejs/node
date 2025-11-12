// META: global=window,dedicatedworker,jsshell

test(() => {
  const argument = { parameters: [] };
  const tag = new WebAssembly.Tag(argument);
  assert_class_string(tag, "WebAssembly.Tag");
}, "Object.prototype.toString on a Tag");

test(() => {
  assert_own_property(WebAssembly.Tag.prototype, Symbol.toStringTag);

  const propDesc = Object.getOwnPropertyDescriptor(
    WebAssembly.Tag.prototype,
    Symbol.toStringTag
  );
  assert_equals(propDesc.value, "WebAssembly.Tag", "value");
  assert_equals(propDesc.configurable, true, "configurable");
  assert_equals(propDesc.enumerable, false, "enumerable");
  assert_equals(propDesc.writable, false, "writable");
}, "@@toStringTag exists on the prototype with the appropriate descriptor");
