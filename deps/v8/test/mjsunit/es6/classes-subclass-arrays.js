// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

(function TestDefaultConstructor() {
  class Stack extends Array { }
  {
    let s1 = new Stack();
    assertSame(Stack.prototype, s1.__proto__);
    assertTrue(Array.isArray(s1));
    assertSame(0, s1.length);
    s1[0] = 'xyz';
    assertSame(1, s1.length);
    assertSame('xyz', s1[0]);
    s1.push(42);
    assertSame(2, s1.length);
    assertSame('xyz', s1[0]);
    assertSame(42, s1[1]);
  }

  {
    let s2 = new Stack(10);
    assertSame(Stack.prototype, s2.__proto__);
    assertTrue(Array.isArray(s2));
    assertSame(10, s2.length);
    assertSame(undefined, s2[0]);
  }

  {
    let a = [1,2,3];
    let s3 = new Stack(a);
    assertSame(Stack.prototype, s3.__proto__);
    assertTrue(Array.isArray(s3));
    assertSame(1, s3.length);
    assertSame(a, s3[0]);
  }

  {
    let s4 = new Stack(1, 2, 3);
    assertSame(Stack.prototype, s4.__proto__);
    assertTrue(Array.isArray(s4));
    assertSame(3, s4.length);
    assertSame(1, s4[0]);
    assertSame(2, s4[1]);
    assertSame(3, s4[2]);
  }

  {
    let s5 = new Stack(undefined, undefined, undefined);
    assertSame(Stack.prototype, s5.__proto__);
    assertTrue(Array.isArray(s5));
    assertSame(3, s5.length);
    assertSame(undefined, s5[0]);
    assertSame(undefined, s5[1]);
    assertSame(undefined, s5[2]);
  }
}());


(function TestEmptyArgsSuper() {
  class Stack extends Array {
    constructor() { super(); }
  }
  let s1 = new Stack();
  assertSame(Stack.prototype, s1.__proto__);
  assertTrue(Array.isArray(s1));
  assertSame(0, s1.length);
  s1[0] = 'xyz';
  assertSame(1, s1.length);
  assertSame('xyz', s1[0]);
  s1.push(42);
  assertSame(2, s1.length);
  assertSame('xyz', s1[0]);
  assertSame(42, s1[1]);
}());


(function TestOneArgSuper() {
  class Stack extends Array {
    constructor(x) {
      super(x);
    }
  }

  {
    let s2 = new Stack(10, 'ignored arg');
    assertSame(Stack.prototype, s2.__proto__);
    assertTrue(Array.isArray(s2));
    assertSame(10, s2.length);
    assertSame(undefined, s2[0]);
  }

  {
    let a = [1,2,3];
    let s3 = new Stack(a, 'ignored arg');
    assertSame(Stack.prototype, s3.__proto__);
    assertTrue(Array.isArray(s3));
    assertSame(1, s3.length);
    assertSame(a, s3[0]);
  }
}());


(function TestMultipleArgsSuper() {
  class Stack extends Array {
    constructor(x, y, z) {
      super(x, y, z);
    }
  }
  {
    let s4 = new Stack(1, 2, 3, 4, 5);
    assertSame(Stack.prototype, s4.__proto__);
    assertTrue(Array.isArray(s4));
    assertSame(3, s4.length);
    assertSame(1, s4[0]);
    assertSame(2, s4[1]);
    assertSame(3, s4[2]);
  }

  {
    let s5 = new Stack(undefined);
    assertSame(Stack.prototype, s5.__proto__);
    assertTrue(Array.isArray(s5));
    assertTrue(s5.__proto__ == Stack.prototype);
    assertSame(3, s5.length);
    assertSame(undefined, s5[0]);
    assertSame(undefined, s5[1]);
    assertSame(undefined, s5[2]);
  }
}());


(function TestArrayConcat() {
  class Stack extends Array { }
  let s1 = new Stack(1,2,3);

  assertArrayEquals([1,2,3,4,5,6], s1.concat([4,5,6]));
  assertArrayEquals([4,5,6,1,2,3], [4,5,6].concat(s1));
}());


(function TestJSONStringify() {
  class Stack extends Array { }

  let s1 = new Stack(1,2,3);
  assertSame("[1,2,3]", JSON.stringify(s1));
}());
