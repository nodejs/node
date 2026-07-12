// META: global=window,dedicatedworker,shadowrealm
"use strict";
// https://webidl.spec.whatwg.org/#es-namespaces
// https://console.spec.whatwg.org/#console-namespace

test(() => {
  assert_true(self.hasOwnProperty("console"));
}, "console exists on the global object");

test(() => {
  const propDesc = Object.getOwnPropertyDescriptor(self, "console");
  assert_equals(propDesc.writable, true, "must be writable");
  assert_equals(propDesc.enumerable, false, "must not be enumerable");
  assert_equals(propDesc.configurable, true, "must be configurable");
  assert_equals(propDesc.value, console, "must have the right value");
}, "console has the right property descriptors");

test(() => {
  assert_false("Console" in self);
}, "Console (uppercase, as if it were an interface) must not exist");

test(() => {
  const prototype1 = Object.getPrototypeOf(console);
  const prototype2 = Object.getPrototypeOf(prototype1);

  assert_equals(Object.getOwnPropertyNames(prototype1).length, 0, "The [[Prototype]] must have no properties");
  assert_equals(prototype2, Object.prototype, "The [[Prototype]]'s [[Prototype]] must be %ObjectPrototype%");
}, "The prototype chain must be correct");
