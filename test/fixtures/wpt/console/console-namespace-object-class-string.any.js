"use strict";
// https://heycam.github.io/webidl/#es-namespaces
// https://console.spec.whatwg.org/#console-namespace

test(() => {
  assert_own_property(console, Symbol.toStringTag);

  const propDesc = Object.getOwnPropertyDescriptor(console, Symbol.toStringTag);
  assert_equals(propDesc.value, "console", "value");
  assert_equals(propDesc.writable, false, "writable");
  assert_equals(propDesc.enumerable, false, "enumerable");
  assert_equals(propDesc.configurable, true, "configurable");
}, "@@toStringTag exists on the namespace object with the appropriate descriptor");

test(() => {
  assert_equals(console.toString(), "[object console]");
  assert_equals(Object.prototype.toString.call(console), "[object console]");
}, "Object.prototype.toString applied to the namespace object");

test(t => {
  assert_own_property(console, Symbol.toStringTag, "Precondition: @@toStringTag on the namespace object");
  t.add_cleanup(() => {
    Object.defineProperty(console, Symbol.toStringTag, { value: "console" });
  });

  Object.defineProperty(console, Symbol.toStringTag, { value: "Test" });
  assert_equals(console.toString(), "[object Test]");
  assert_equals(Object.prototype.toString.call(console), "[object Test]");
}, "Object.prototype.toString applied after modifying the namespace object's @@toStringTag");

test(t => {
  assert_own_property(console, Symbol.toStringTag, "Precondition: @@toStringTag on the namespace object");
  t.add_cleanup(() => {
    Object.defineProperty(console, Symbol.toStringTag, { value: "console" });
  });

  assert_true(delete console[Symbol.toStringTag]);
  assert_equals(console.toString(), "[object Object]");
  assert_equals(Object.prototype.toString.call(console), "[object Object]");
}, "Object.prototype.toString applied after deleting @@toStringTag");
