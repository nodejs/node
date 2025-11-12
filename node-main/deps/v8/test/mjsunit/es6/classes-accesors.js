// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var b = 'b';

(function TestOverwritingInstanceAccessors() {
  var C, desc;
  C = class {
    [b]() { return 'B'; };
    get b() { return 'get B'; };
  };
  desc = Object.getOwnPropertyDescriptor(C.prototype, 'b');
  assertFalse(desc.enumerable);
  assertTrue(desc.configurable);
  assertEquals('get B', desc.get());
  assertEquals(undefined, desc.set);

  C = class {
    [b]() { return 'B'; };
    set b(v) { return 'set B'; };
  };
  desc = Object.getOwnPropertyDescriptor(C.prototype, 'b');
  assertFalse(desc.enumerable);
  assertTrue(desc.configurable);
  assertEquals(undefined, desc.get);
  assertEquals('set B', desc.set());

  C = class {
    set b(v) { return 'get B'; };
    [b]() { return 'B'; };
    get b() { return 'get B'; };
  };
  desc = Object.getOwnPropertyDescriptor(C.prototype, 'b');
  assertFalse(desc.enumerable);
  assertTrue(desc.configurable);
  assertEquals('get B', desc.get());
  assertEquals(undefined, desc.set);

  C = class {
    get b() { return 'get B'; };
    [b]() { return 'B'; };
    set b(v) { return 'set B'; };
  };
  desc = Object.getOwnPropertyDescriptor(C.prototype, 'b');
  assertFalse(desc.enumerable);
  assertTrue(desc.configurable);
  assertEquals(undefined, desc.get);
  assertEquals('set B', desc.set());
})();

(function TestOverwritingStaticAccessors() {
  var C, desc;
  C = class {
    static [b]() { return 'B'; };
    static get b() { return 'get B'; };
  };
  desc = Object.getOwnPropertyDescriptor(C, 'b');
  assertFalse(desc.enumerable);
  assertTrue(desc.configurable);
  assertEquals('get B', desc.get());
  assertEquals(undefined, desc.set);

  C = class {
    static [b]() { return 'B'; };
    static set b(v) { return 'set B'; };
  };
  desc = Object.getOwnPropertyDescriptor(C, 'b');
  assertFalse(desc.enumerable);
  assertTrue(desc.configurable);
  assertEquals(undefined, desc.get);
  assertEquals('set B', desc.set());

  C = class {
    static set b(v) { return 'get B'; };
    static [b]() { return 'B'; };
    static get b() { return 'get B'; };
  };
  desc = Object.getOwnPropertyDescriptor(C, 'b');
  assertFalse(desc.enumerable);
  assertTrue(desc.configurable);
  assertEquals('get B', desc.get());
  assertEquals(undefined, desc.set);

  C = class {
    static get b() { return 'get B'; };
    static [b]() { return 'B'; };
    static set b(v) { return 'set B'; };
  };
  desc = Object.getOwnPropertyDescriptor(C, 'b');
  assertFalse(desc.enumerable);
  assertTrue(desc.configurable);
  assertEquals(undefined, desc.get);
  assertEquals('set B', desc.set());
})();
