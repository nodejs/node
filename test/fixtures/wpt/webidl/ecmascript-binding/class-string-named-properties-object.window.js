"use strict";

const namedPropertiesObject = Object.getPrototypeOf(Window.prototype);

test(() => {
  assert_own_property(namedPropertiesObject, Symbol.toStringTag);

  const propDesc = Object.getOwnPropertyDescriptor(namedPropertiesObject, Symbol.toStringTag);
  assert_equals(propDesc.value, "WindowProperties", "value");
  assert_equals(propDesc.configurable, true, "configurable");
  assert_equals(propDesc.enumerable, false, "enumerable");
  assert_equals(propDesc.writable, false, "writable");
}, "@@toStringTag exists with the appropriate descriptor");

test(() => {
  assert_equals(Object.prototype.toString.call(namedPropertiesObject), "[object WindowProperties]");
}, "Object.prototype.toString");

// Chrome had a bug (https://bugs.chromium.org/p/chromium/issues/detail?id=793406) where if there
// was no @@toStringTag, it would fall back to a magic class string. Tests for this are present in
// the sibling class-string*.any.js tests. However, the named properties object always fails calls
// to [[DefineOwnProperty]] or [[SetPrototypeOf]] per the Web IDL spec, so there is no way to
// trigger the buggy behavior for it.
