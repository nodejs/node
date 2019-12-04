// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-private-methods

"use strict";

// Complementary private accessors.
{
  let store = 1;
  class C {
    get #a() { return store; }
    set #a(val) { store = val; }
    incA() { this.#a++; }  // CountOperation
    setA(val) { this.#a = val; }
    getA() { return this.#a; }
  }

  const c = new C;
  assertEquals(store, c.getA());
  assertEquals(1, c.getA());
  c.setA(2);
  assertEquals(store, c.getA());
  assertEquals(2, c.getA());
  c.incA();
  assertEquals(store, c.getA());
  assertEquals(3, c.getA());
}

// Compound assignment.
{
  let store;
  class A {
    get #a() { return store; }
    set #a(val) { store = val; }
    getA() { return this.#a; }
    constructor(val) {
      ({ y: this.#a } = val);
    }
  }

  const a = new A({y: 'test'});
  assertEquals('test', a.getA());
}

// Accessing super in private accessors.
{
  class A { foo(val) {} }
  class C extends A {
    set #a(val) { super.foo(val); }
  }
  new C();

  class D extends A {
    get #a() { return super.foo; }
  }
  new D();

  class E extends A {
    set #a(val) { super.foo(val); }
    get #a() { return super.foo; }
  }
  new E();
}

// Nested private accessors.
{
  class C {
    get #a() {
      let storeD = 'd';
      class D {
        // Shadows outer #a
        get #a() { return storeD; }
        getD() { return this.#a; }
      }
      return new D;
    }
    getA() {
      return this.#a;
    }
  }
  assertEquals('d', new C().getA().getD());
}

// Duplicate private accessors.
// https://tc39.es/proposal-private-methods/#sec-static-semantics-early-errors
{
  assertThrows('class C { get #a() {} get #a() {} }', SyntaxError);
  assertThrows('class C { set #a(val) {} set #a(val) {} }', SyntaxError);
}
