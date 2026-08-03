// META: global=window,dedicatedworker,jsshell

"use strict";
// https://webidl.spec.whatwg.org/#es-namespaces
// https://webassembly.github.io/spec/js-api/#namespacedef-webassembly

test(() => {
  assert_own_property(WebAssembly, Symbol.toStringTag);

  const propDesc = Object.getOwnPropertyDescriptor(WebAssembly, Symbol.toStringTag);
  assert_equals(propDesc.value, "WebAssembly", "value");
  assert_equals(propDesc.writable, false, "writable");
  assert_equals(propDesc.enumerable, false, "enumerable");
  assert_equals(propDesc.configurable, true, "configurable");
}, "@@toStringTag exists on the namespace object with the appropriate descriptor");

test(() => {
  assert_equals(WebAssembly.toString(), "[object WebAssembly]");
  assert_equals(Object.prototype.toString.call(WebAssembly), "[object WebAssembly]");
}, "Object.prototype.toString applied to the namespace object");

test(t => {
  assert_own_property(WebAssembly, Symbol.toStringTag, "Precondition: @@toStringTag on the namespace object");
  t.add_cleanup(() => {
    Object.defineProperty(WebAssembly, Symbol.toStringTag, { value: "WebAssembly" });
  });

  Object.defineProperty(WebAssembly, Symbol.toStringTag, { value: "Test" });
  assert_equals(WebAssembly.toString(), "[object Test]");
  assert_equals(Object.prototype.toString.call(WebAssembly), "[object Test]");
}, "Object.prototype.toString applied after modifying the namespace object's @@toStringTag");

test(t => {
  assert_own_property(WebAssembly, Symbol.toStringTag, "Precondition: @@toStringTag on the namespace object");
  t.add_cleanup(() => {
    Object.defineProperty(WebAssembly, Symbol.toStringTag, { value: "WebAssembly" });
  });

  assert_true(delete WebAssembly[Symbol.toStringTag]);
  assert_equals(WebAssembly.toString(), "[object Object]");
  assert_equals(Object.prototype.toString.call(WebAssembly), "[object Object]");
}, "Object.prototype.toString applied after deleting @@toStringTag");
