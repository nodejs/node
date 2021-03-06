// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

{
  let desc = Object.getOwnPropertyDescriptor(Symbol.prototype, 'description');
  assertEquals(desc.set, undefined);
  assertEquals(desc.writable, undefined);
  assertFalse(desc.enumerable);
  assertTrue(desc.configurable);
}

{
  const empty = Symbol();
  assertEquals(empty.description, undefined);

  const symbol = Symbol('test');
  assertEquals(symbol.description, 'test');
  assertFalse(symbol.hasOwnProperty('description'));
}

{
  const empty = Object(Symbol());
  assertEquals(empty.description, undefined);

  const symbol = Object(Symbol('test'));
  assertEquals(symbol.description, 'test');
  assertFalse(symbol.hasOwnProperty('description'));
}

{
  assertThrows(function() {
    const proxy = new Proxy({}, {});
    Symbol.prototype.description.call(proxy);
  }, TypeError);

  assertThrows(function() {
    const smi = 123;
    Symbol.prototype.description.call(smi);
  }, TypeError);

  assertThrows(function() {
    const str = 'string';
    Symbol.prototype.description.call(string);
  }, TypeError);

  assertThrows(function() {
    const obj = { prop: 'test' };
    Symbol.prototype.description.call(obj);
  }, TypeError);
}
