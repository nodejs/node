"use strict";

test(() => {
  assert_own_property(Blob.prototype, Symbol.toStringTag);

  const propDesc = Object.getOwnPropertyDescriptor(Blob.prototype, Symbol.toStringTag);
  assert_equals(propDesc.value, "Blob", "value");
  assert_equals(propDesc.configurable, true, "configurable");
  assert_equals(propDesc.enumerable, false, "enumerable");
  assert_equals(propDesc.writable, false, "writable");
}, "@@toStringTag exists on the prototype with the appropriate descriptor");

test(() => {
  assert_not_own_property(new Blob(), Symbol.toStringTag);
}, "@@toStringTag must not exist on the instance");

test(() => {
  assert_equals(Object.prototype.toString.call(Blob.prototype), "[object Blob]");
}, "Object.prototype.toString applied to the prototype");

test(() => {
  assert_equals(Object.prototype.toString.call(new Blob()), "[object Blob]");
}, "Object.prototype.toString applied to an instance");

test(t => {
  assert_own_property(Blob.prototype, Symbol.toStringTag, "Precondition for this test: @@toStringTag on the prototype");

  t.add_cleanup(() => {
    Object.defineProperty(Blob.prototype, Symbol.toStringTag, { value: "Blob" });
  });

  Object.defineProperty(Blob.prototype, Symbol.toStringTag, { value: "NotABlob" });
  assert_equals(Object.prototype.toString.call(Blob.prototype), "[object NotABlob]", "prototype");
  assert_equals(Object.prototype.toString.call(new Blob()), "[object NotABlob]", "instance");
}, "Object.prototype.toString applied after modifying the prototype's @@toStringTag");

test(t => {
  const instance = new Blob();
  assert_not_own_property(instance, Symbol.toStringTag, "Precondition for this test: no @@toStringTag on the instance");

  Object.defineProperty(instance, Symbol.toStringTag, { value: "NotABlob" });
  assert_equals(Object.prototype.toString.call(instance), "[object NotABlob]");
}, "Object.prototype.toString applied to the instance after modifying the instance's @@toStringTag");

// Chrome had a bug (https://bugs.chromium.org/p/chromium/issues/detail?id=793406) where if there
// was no @@toStringTag in the prototype, it would fall back to a magic class string. This tests
// that the bug is fixed.

test(() => {
  const instance = new Blob();
  Object.setPrototypeOf(instance, null);

  assert_equals(Object.prototype.toString.call(instance), "[object Object]");
}, "Object.prototype.toString applied to a null-prototype instance");

// This test must be last.
test(() => {
  delete Blob.prototype[Symbol.toStringTag];

  assert_equals(Object.prototype.toString.call(Blob.prototype), "[object Object]", "prototype");
  assert_equals(Object.prototype.toString.call(new Blob()), "[object Object]", "instance");
}, "Object.prototype.toString applied after deleting @@toStringTag");
