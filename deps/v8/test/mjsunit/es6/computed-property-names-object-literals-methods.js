// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


function ID(x) {
  return x;
}


(function TestMethodComputedNameString() {
  var object = {
    a() { return 'A'},
    ['b']() { return 'B'; },
    c() { return 'C'; },
    [ID('d')]() { return 'D'; },
  };
  assertEquals('A', object.a());
  assertEquals('B', object.b());
  assertEquals('C', object.c());
  assertEquals('D', object.d());
  assertArrayEquals(['a', 'b', 'c', 'd'], Object.keys(object));
})();


(function TestMethodComputedNameNumber() {
  var object = {
    a() { return 'A'; },
    [1]() { return 'B'; },
    c() { return 'C'; },
    [ID(2)]() { return 'D'; },
  };
  assertEquals('A', object.a());
  assertEquals('B', object[1]());
  assertEquals('C', object.c());
  assertEquals('D', object[2]());
  // Array indexes first.
  assertArrayEquals(['1', '2', 'a', 'c'], Object.keys(object));
})();


(function TestMethodComputedNameSymbol() {
  var sym1 = Symbol();
  var sym2 = Symbol();
  var object = {
    a() { return 'A'; },
    [sym1]() { return 'B'; },
    c() { return 'C'; },
    [ID(sym2)]() { return 'D'; },
  };
  assertEquals('A', object.a());
  assertEquals('B', object[sym1]());
  assertEquals('C', object.c());
  assertEquals('D', object[sym2]());
  assertArrayEquals(['a', 'c'], Object.keys(object));
  assertArrayEquals([sym1, sym2], Object.getOwnPropertySymbols(object));
})();


function assertIteratorResult(value, done, result) {
  assertEquals({ value: value, done: done}, result);
}


(function TestGeneratorComputedName() {
  var object = {
    *['a']() {
      yield 1;
      yield 2;
    }
  };
  var iter = object.a();
  assertIteratorResult(1, false, iter.next());
  assertIteratorResult(2, false, iter.next());
  assertIteratorResult(undefined, true, iter.next());
  assertArrayEquals(['a'], Object.keys(object));
})();


(function TestToNameSideEffects() {
  var counter = 0;
  var key1 = {
    toString: function() {
      assertEquals(0, counter++);
      return 'b';
    }
  };
  var key2 = {
    toString: function() {
      assertEquals(1, counter++);
      return 'd';
    }
  };
  var object = {
    a() { return 'A'; },
    [key1]() { return 'B'; },
    c() { return 'C'; },
    [key2]() { return 'D'; },
  };
  assertEquals(2, counter);
  assertEquals('A', object.a());
  assertEquals('B', object.b());
  assertEquals('C', object.c());
  assertEquals('D', object.d());
  assertArrayEquals(['a', 'b', 'c', 'd'], Object.keys(object));
})();


(function TestDuplicateKeys() {
  'use strict';
  // ES5 does not allow duplicate keys.
  // ES6 does but we haven't changed our code yet.

  var object = {
    a() { return 1; },
    ['a']() { return 2; },
  };
  assertEquals(2, object.a());
})();
