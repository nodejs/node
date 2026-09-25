"use strict";

const iteratorProto = Object.getPrototypeOf((new URLSearchParams()).entries());

test(() => {
  assert_own_property(iteratorProto, Symbol.toStringTag);

  const propDesc = Object.getOwnPropertyDescriptor(iteratorProto, Symbol.toStringTag);
  assert_equals(propDesc.value, "URLSearchParams Iterator", "value");
  assert_equals(propDesc.configurable, true, "configurable");
  assert_equals(propDesc.enumerable, false, "enumerable");
  assert_equals(propDesc.writable, false, "writable");
}, "@@toStringTag exists with the appropriate descriptor");

test(() => {
  assert_equals(Object.prototype.toString.call(iteratorProto), "[object URLSearchParams Iterator]");
}, "Object.prototype.toString");

test(t => {
  assert_own_property(iteratorProto, Symbol.toStringTag, "Precondition for this test: @@toStringTag exists");

  t.add_cleanup(() => {
    Object.defineProperty(iteratorProto, Symbol.toStringTag, { value: "URLSearchParams Iterator" });
  });

  Object.defineProperty(iteratorProto, Symbol.toStringTag, { value: "Not URLSearchParams Iterator" });
  assert_equals(Object.prototype.toString.call(iteratorProto), "[object Not URLSearchParams Iterator]");
}, "Object.prototype.toString applied after modifying @@toStringTag");

// Chrome had a bug (https://bugs.chromium.org/p/chromium/issues/detail?id=793406) where if there
// was no @@toStringTag, it would fall back to a magic class string. This tests that the bug is
// fixed.

test(() => {
  const iterator = (new URLSearchParams()).keys();
  assert_equals(Object.prototype.toString.call(iterator), "[object URLSearchParams Iterator]");

  Object.setPrototypeOf(iterator, null);
  assert_equals(Object.prototype.toString.call(iterator), "[object Object]");
}, "Object.prototype.toString applied to a null-prototype instance");

test(t => {
  const proto = Object.getPrototypeOf(iteratorProto);
  t.add_cleanup(() => {
    Object.setPrototypeOf(iteratorProto, proto);
  });

  Object.setPrototypeOf(iteratorProto, null);

  assert_equals(Object.prototype.toString.call(iteratorProto), "[object URLSearchParams Iterator]");
}, "Object.prototype.toString applied after nulling the prototype");
