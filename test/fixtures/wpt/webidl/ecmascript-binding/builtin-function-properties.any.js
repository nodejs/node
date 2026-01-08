"use strict";

test(() => {
  const ownPropKeys = Reflect.ownKeys(Blob).slice(0, 3);
  assert_array_equals(ownPropKeys, ["length", "name", "prototype"]);
}, 'Constructor property enumeration order of "length", "name", and "prototype"');

test(() => {
  assert_own_property(Blob.prototype, "slice");

  const ownPropKeys = Reflect.ownKeys(Blob.prototype.slice).slice(0, 2);
  assert_array_equals(ownPropKeys, ["length", "name"]);
}, 'Method property enumeration order of "length" and "name"');

test(() => {
  assert_own_property(Blob.prototype, "size");

  const desc = Reflect.getOwnPropertyDescriptor(Blob.prototype, "size");
  assert_equals(typeof desc.get, "function");

  const ownPropKeys = Reflect.ownKeys(desc.get).slice(0, 2);
  assert_array_equals(ownPropKeys, ["length", "name"]);
}, 'Getter property enumeration order of "length" and "name"');
