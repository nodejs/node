// META: global=window,dedicatedworker,jsshell

test(() => {
  const argument = { parameters: [] };
  const tag = new WebAssembly.Tag(argument);
  const exception = new WebAssembly.Exception(tag, []);
  assert_class_string(exception, "WebAssembly.Exception");
}, "Object.prototype.toString on an Exception");

test(() => {
  assert_own_property(WebAssembly.Exception.prototype, Symbol.toStringTag);

  const propDesc = Object.getOwnPropertyDescriptor(
    WebAssembly.Exception.prototype,
    Symbol.toStringTag
  );
  assert_equals(propDesc.value, "WebAssembly.Exception", "value");
  assert_equals(propDesc.configurable, true, "configurable");
  assert_equals(propDesc.enumerable, false, "enumerable");
  assert_equals(propDesc.writable, false, "writable");
}, "@@toStringTag exists on the prototype with the appropriate descriptor");
