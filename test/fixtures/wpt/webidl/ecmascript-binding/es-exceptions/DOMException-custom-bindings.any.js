"use strict";

test(() => {
  assert_throws_js(TypeError, () => DOMException());
}, "Cannot construct without new");

test(() => {
  assert_equals(Object.getPrototypeOf(DOMException.prototype), Error.prototype);
}, "inherits from Error: prototype-side");

test(() => {
  assert_equals(Object.getPrototypeOf(DOMException), Function.prototype);
}, "does not inherit from Error: class-side");

test(() => {
  const e = new DOMException("message", "name");
  assert_false(e.hasOwnProperty("message"), "property is not own");

  const propDesc = Object.getOwnPropertyDescriptor(DOMException.prototype, "message");
  assert_equals(typeof propDesc.get, "function", "property descriptor is a getter");
  assert_equals(propDesc.set, undefined, "property descriptor is not a setter");
  assert_true(propDesc.enumerable, "property descriptor enumerable");
  assert_true(propDesc.configurable, "property descriptor configurable");
}, "message property descriptor");

test(() => {
  const getter = Object.getOwnPropertyDescriptor(DOMException.prototype, "message").get;

  assert_throws_js(TypeError, () => getter.apply({}));
}, "message getter performs brand checks (i.e. is not [LegacyLenientThis])");

test(() => {
  const e = new DOMException("message", "name");
  assert_false(e.hasOwnProperty("name"), "property is not own");

  const propDesc = Object.getOwnPropertyDescriptor(DOMException.prototype, "name");
  assert_equals(typeof propDesc.get, "function", "property descriptor is a getter");
  assert_equals(propDesc.set, undefined, "property descriptor is not a setter");
  assert_true(propDesc.enumerable, "property descriptor enumerable");
  assert_true(propDesc.configurable, "property descriptor configurable");
}, "name property descriptor");

test(() => {
  const getter = Object.getOwnPropertyDescriptor(DOMException.prototype, "name").get;

  assert_throws_js(TypeError, () => getter.apply({}));
}, "name getter performs brand checks (i.e. is not [LegacyLenientThis])");

test(() => {
  const e = new DOMException("message", "name");
  assert_false(e.hasOwnProperty("code"), "property is not own");

  const propDesc = Object.getOwnPropertyDescriptor(DOMException.prototype, "code");
  assert_equals(typeof propDesc.get, "function", "property descriptor is a getter");
  assert_equals(propDesc.set, undefined, "property descriptor is not a setter");
  assert_true(propDesc.enumerable, "property descriptor enumerable");
  assert_true(propDesc.configurable, "property descriptor configurable");
}, "code property descriptor");

test(() => {
  const getter = Object.getOwnPropertyDescriptor(DOMException.prototype, "code").get;

  assert_throws_js(TypeError, () => getter.apply({}));
}, "code getter performs brand checks (i.e. is not [LegacyLenientThis])");

test(() => {
  const e = new DOMException("message", "InvalidCharacterError");
  assert_equals(e.code, 5, "Initially the code is set to 5");

  Object.defineProperty(e, "name", {
    value: "WrongDocumentError"
  });

  assert_equals(e.code, 5, "The code is still set to 5");
}, "code property is not affected by shadowing the name property");

test(() => {
  const e = new DOMException("message", "name");
  assert_equals(Object.prototype.toString.call(e), "[object DOMException]");
}, "Object.prototype.toString behavior is like other interfaces");

test(() => {
  const e = new DOMException("message", "name");
  assert_false(e.hasOwnProperty("toString"), "toString must not exist on the instance");
  assert_false(DOMException.prototype.hasOwnProperty("toString"), "toString must not exist on DOMException.prototype");
  assert_equals(typeof e.toString, "function", "toString must still exist (via Error.prototype)");
}, "Inherits its toString() from Error.prototype");

test(() => {
  const e = new DOMException("message", "name");
  assert_equals(e.toString(), "name: message",
    "The default Error.prototype.toString() behavior must work on supplied name and message");

  Object.defineProperty(e, "name", { value: "new name" });
  Object.defineProperty(e, "message", { value: "new message" });
  assert_equals(e.toString(), "new name: new message",
    "The default Error.prototype.toString() behavior must work on shadowed names and messages");
}, "toString() behavior from Error.prototype applies as expected");

test(() => {
  assert_throws_js(TypeError, () => DOMException.prototype.toString());
}, "DOMException.prototype.toString() applied to DOMException.prototype throws because of name/message brand checks");

test(() => {
  let stackOnNormalErrors;
  try {
    throw new Error("normal error");
  } catch (e) {
    stackOnNormalErrors = e.stack;
  }

  let stackOnDOMException;
  try {
    throw new DOMException("message", "name");
  } catch (e) {
    stackOnDOMException = e.stack;
  }

  assert_equals(typeof stackOnDOMException, typeof stackOnNormalErrors, "The typeof values must match");
}, "If the implementation has a stack property on normal errors, it also does on DOMExceptions");
