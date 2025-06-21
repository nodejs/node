// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

{
  assertThrows(() => {
    class A {
      [this.#a] = 1;
      get #a() {}
    }
  }, TypeError);

  assertThrows(() => {
    class A {
      [this.#a] = 1;
      set #a(val) {}
    }
  }, TypeError);

  assertThrows(() => {
    class A {
      [this.#a] = 1;
      set #a(val) {}
      get #a() {}
    }
  }, TypeError);
}

// Duplicate private accessors.
// https://tc39.es/proposal-private-methods/#sec-static-semantics-early-errors
{
  assertThrows('class C { get #a() {} get #a() {} }', SyntaxError);
  assertThrows('class C { set #a(val) {} set #a(val) {} }', SyntaxError);
}

// Test that the lhs don't get re-evaluated in the assignment, and it always
// gets evaluated before the rhs.
{
  let objectCount = 0;
  let operations = [];
  let lhsObject = {
    get property() {
      operations.push('lhsEvaluation');
      return new Foo();
    }
  };
  let rhs = () => {operations.push('rhsEvaluation'); return 1; };
  class Foo {
    id = objectCount++;
    get #foo() {
      operations.push('get', this.id);
      return 0;
    }
    set #foo(x) {
      operations.push('set', this.id);
    }
    static compound() {
      lhsObject.property.#foo += rhs();
    }
    static assign() {
      lhsObject.property.#foo = lhsObject.property.#foo + rhs();
    }
  }
  objectCount = 0;
  operations = [];
  Foo.compound();
  assertEquals(1, objectCount);
  assertEquals(
    ['lhsEvaluation', 'get', 0, 'rhsEvaluation', 'set', 0],
    operations);

  objectCount = 0;
  operations = [];
  Foo.assign();
  assertEquals(2, objectCount);
  assertEquals(
    ['lhsEvaluation', 'lhsEvaluation', 'get', 1, 'rhsEvaluation', 'set', 0],
    operations);
}

// Test that the brand checks are done on the lhs evaluated before the rhs.
{
  let objectCount = 0;
  let operations = [];
  let maximumObjects = 1;
  let lhsObject = {
    get property() {
      operations.push('lhsEvaluation');
      return (objectCount >= maximumObjects) ? {id: -1} : new Foo();
    }
  };
  let rhs = () => {operations.push('rhsEvaluation'); return 1; };

  class Foo {
    id = objectCount++;

    set #foo(val) {
      operations.push('set', this.id);
    }

    get #foo() {
      operations.push('get', this.id);
      return 0;
    }

    static compound() {
      lhsObject.property.#foo += rhs();
    }

    static assign() {
      lhsObject.property.#foo = lhsObject.property.#foo + rhs();
    }
  }

  objectCount = 0;
  operations = [];
  maximumObjects = 1;
  Foo.compound();
  assertEquals(1, objectCount);
  assertEquals(
    ['lhsEvaluation', 'get', 0, 'rhsEvaluation', 'set', 0],
    operations);

  objectCount = 0;
  operations = [];
  maximumObjects = 2;
  Foo.assign();
  assertEquals(2, objectCount);
  assertEquals(
    ['lhsEvaluation', 'lhsEvaluation', 'get', 1, 'rhsEvaluation', 'set', 0],
    operations);

  objectCount = 0;
  operations = [];
  maximumObjects = 1;
  assertThrows(() => Foo.assign(), TypeError, /Receiver must be an instance of class Foo/);
  assertEquals(1, objectCount);
  assertEquals(['lhsEvaluation', 'lhsEvaluation'], operations);
}
