// META: global=window,dedicatedworker

// https://tc39.es/proposal-error-stack-accessor/
// https://github.com/whatwg/webidl/pull/1421

"use strict";

test(() => {
  const e = new DOMException("some message", "SyntaxError");
  assert_equals(typeof e.stack, "string", "stack must be a string");
}, "new DOMException() has a stack property that is a string");

test(() => {
  const e = new DOMException();
  assert_equals(typeof e.stack, "string", "stack must be a string");
}, "new DOMException() with no arguments has a stack property that is a string");

if (typeof document !== "undefined") {
  test(() => {
    let caught;
    try {
      document.createElement("");
    } catch (e) {
      caught = e;
    }
    assert_true(caught instanceof DOMException, "must be a DOMException");
    assert_equals(typeof caught.stack, "string", "stack must be a string");
  }, "thrown DOMException from DOM API has a stack property that is a string");
}

test(() => {
  const e = new DOMException("some message", "SyntaxError");
  assert_false(e.hasOwnProperty("stack"), "stack must not be an own property of the instance");
}, "DOMException instance does not have an own stack property");

test(() => {
  assert_false(DOMException.prototype.hasOwnProperty("stack"),
    "DOMException.prototype must not have an own stack property");
}, "DOMException.prototype does not have an own stack property");

test(() => {
  const desc = Object.getOwnPropertyDescriptor(Error.prototype, "stack");
  assert_not_equals(desc, undefined, "Error.prototype must have a stack property descriptor");
  assert_equals(typeof desc.get, "function", "stack must have a getter");
  assert_equals(typeof desc.set, "function", "stack must have a setter");
  assert_false(desc.enumerable, "stack must not be enumerable");
  assert_true(desc.configurable, "stack must be configurable");
}, "Error.prototype.stack is an accessor property with correct attributes");

test(() => {
  const getter = Object.getOwnPropertyDescriptor(Error.prototype, "stack").get;
  const e = new DOMException("some message", "SyntaxError");
  const stack = getter.call(e);
  assert_equals(typeof stack, "string", "getter must return a string for DOMException");
  assert_equals(stack, e.stack, "getter result must match e.stack");
}, "Error.prototype.stack getter works on DOMException instances");

test(() => {
  const setter = Object.getOwnPropertyDescriptor(Error.prototype, "stack").set;
  const e = new DOMException("some message", "SyntaxError");
  setter.call(e, "custom stack");
  assert_true(e.hasOwnProperty("stack"), "after setter, stack must be an own property");
  assert_equals(e.stack, "custom stack", "own stack property must have the set value");

  const desc = Object.getOwnPropertyDescriptor(e, "stack");
  assert_equals(desc.value, "custom stack", "must be a data property");
  assert_true(desc.writable, "must be writable");
  assert_true(desc.enumerable, "must be enumerable");
  assert_true(desc.configurable, "must be configurable");
}, "Error.prototype.stack setter installs own data property on DOMException instances");

test(() => {
  const setter = Object.getOwnPropertyDescriptor(Error.prototype, "stack").set;
  // SetterThatIgnoresPrototypeProperties should not install a property on Error.prototype itself
  const originalStack = Object.getOwnPropertyDescriptor(Error.prototype, "stack");
  setter.call(Error.prototype, "custom stack");
  const afterStack = Object.getOwnPropertyDescriptor(Error.prototype, "stack");
  assert_equals(typeof afterStack.get, "function",
    "Error.prototype.stack must still be an accessor after calling setter on it");
}, "Error.prototype.stack setter ignores Error.prototype itself");
