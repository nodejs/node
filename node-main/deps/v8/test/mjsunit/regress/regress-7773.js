// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function testFunctionNames() {
  let descriptor = {
    value: '',
    writable: false,
    enumerable: false,
    configurable: true
  };
  // Functions have a "name" property by default.
  assertEquals(
      descriptor, Object.getOwnPropertyDescriptor(function(){}, 'name'));
  let a = { fn: function(){} };
  assertSame('fn', a.fn.name);
  descriptor.value = 'fn';
  assertEquals(descriptor, Object.getOwnPropertyDescriptor(a.fn, 'name'));

  let b = { __proto__: function(){} };
  assertSame('', b.__proto__.name);
  descriptor.value = '';
  assertEquals(
      descriptor, Object.getOwnPropertyDescriptor(b.__proto__, 'name'));

  let c = { fn: function F(){} };
  assertSame('F', c.fn.name);
  descriptor.value = 'F';
  assertEquals(descriptor, Object.getOwnPropertyDescriptor(c.fn, 'name'));

  let d = { __proto__: function E(){} };
  assertSame('E', d.__proto__.name);
  descriptor.value = 'E';
  assertEquals(
      descriptor, Object.getOwnPropertyDescriptor(d.__proto__, 'name'));
})();

(function testClassNames() {
  let descriptor = {
    value: '',
    writable: false,
    enumerable: false,
    configurable: true
  };

  // Anonymous classes do not have a "name" property by default.
  descriptor.value = '';
  assertEquals(descriptor, Object.getOwnPropertyDescriptor(class {}, 'name'));
  descriptor.value = 'C';
  assertEquals(descriptor, Object.getOwnPropertyDescriptor(class C {}, 'name'));

  let a = { fn: class {} };
  assertSame('fn', a.fn.name);
  descriptor.value = 'fn';
  assertEquals(descriptor, Object.getOwnPropertyDescriptor(a.fn, 'name'));

  let b = { __proto__: class {} };
  assertSame('', b.__proto__.name);
  descriptor.value = '';
  assertEquals(
    descriptor, Object.getOwnPropertyDescriptor(b.__proto__, 'name'));

  let c = { fn: class F {} };
  assertSame('F', c.fn.name);
  descriptor.value = 'F';
  assertEquals(descriptor, Object.getOwnPropertyDescriptor(c.fn, 'name'));

  let d = { __proto__: class F {} };
  assertSame('F', d.__proto__.name);
  descriptor.value = 'F';
  assertEquals(
      descriptor, Object.getOwnPropertyDescriptor(d.__proto__, 'name'));
})();
