// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";

// Static private methods
{
  let store = 1;
  class C {
    static #a() { return store; }
    static a() { return this.#a(); }
  }
  assertEquals(C.a(), store);
  assertThrows(() => C.a.call(new C), TypeError);
}

// Complementary static private accessors.
{
  let store = 1;
  class C {
    static get #a() { return store; }
    static set #a(val) { store = val; }
    static incA() { this.#a++; }
    static getA() { return this.#a; }
    static setA(val) { this.#a = val; }
  }
  assertEquals(C.getA(), 1);
  C.incA();
  assertEquals(store, 2);
  C.setA(3);
  assertEquals(store, 3);

  assertThrows(() => C.incA.call(new C), TypeError);
  assertThrows(() => C.getA.call(new C), TypeError);
  assertThrows(() => C.setA.call(new C), TypeError);

  assertThrows(() => { const incA = C.incA; incA(); }, TypeError);
  assertThrows(() => { const getA = C.getA; getA(); }, TypeError);
  assertThrows(() => { const setA = C.setA; setA(); }, TypeError);
}

// Static private methods accessed explicitly in an anonymous nested class.
{
  class Outer {
    #a() { return 'Outer'; }
    a() { return this.#a(); }
    test() {
      return class {
        static #a() { return 'Inner'; }
        static a() { return this.#a(); }
      };
    }
  }

  const obj = new Outer;
  const C = obj.test();
  assertEquals(C.a(), 'Inner');
  assertThrows(() => obj.a.call(C), TypeError);
  assertThrows(() => obj.a.call(new C), TypeError);
}

// Static private methods accessed explicitly in a named nested class.
{
  class Outer {
    #a() { return 'Outer'; }
    a() { return this.#a(); }
    test() {
      return class Inner {
        static #a() { return 'Inner'; }
        static a() { return this.#a(); }
      };
    }
  }

  const obj = new Outer;
  const C = obj.test();
  assertEquals(C.a(), 'Inner');
  assertThrows(() => obj.a.call(C), TypeError);
  assertThrows(() => obj.a.call(new C), TypeError);
}

// Static private methods accessed through eval in an anonymous nested class.
{
  class Outer {
    #a() { return 'Outer'; }
    a() { return this.#a(); }
    test() {
      return class {
        static #a() { return 'Inner'; }
        static a(str) { return eval(str); }
      };
    }
  }

  const obj = new Outer;
  const C = obj.test();
  assertEquals(C.a('this.#a()'), 'Inner');
  assertThrows(() => C.a('Outer.#a()'), TypeError);
}

// Static private methods accessed through eval in a named nested class.
{
  class Outer {
    #a() { return 'Outer'; }
    a() { return this.#a(); }
    test() {
      return class Inner {
        static #a() { return 'Inner'; }
        static a(str) { return eval(str); }
      };
    }
  }

  const obj = new Outer;
  const C = obj.test();
  assertEquals(C.a('this.#a()'), 'Inner');
  assertEquals(C.a('Inner.#a()'), 'Inner');
  assertThrows(() => C.a('Outer.#a()'), TypeError);
  assertThrows(() => C.run('(new Outer).#a()'), TypeError);
}

// Static private methods in the outer class accessed through eval
// in a named nested class.
{
  class Outer {
    static #a() { return 'Outer'; }
    static test() {
      return class Inner {
        static run(str) { return eval(str); }
      };
    }
  }

  const C = Outer.test();
  assertEquals(C.run('Outer.#a()'), 'Outer');
  assertThrows(() => C.run('this.#a()'), TypeError);
  assertThrows(() => C.run('Inner.#a()'), TypeError);
  assertThrows(() => C.run('(new Outer).#a()'), TypeError);
}

// Static private methods in the outer class accessed explicitly
// in a named nested class.
{
  class Outer {
    static #a() { return 'Outer'; }
    static test() {
      return class Inner {
        static getA(klass) { return klass.#a(); }
      };
    }
  }

  const C = Outer.test();
  assertEquals(C.getA(Outer), 'Outer');
  assertThrows(() => C.getA.call(C), TypeError);
  assertThrows(() => C.getA.call(new Outer), TypeError);
}

// Static private methods in the outer class accessed explicitly
// in an anonymous nested class.
{
  class Outer {
    static #a() { return 'Outer'; }
    static test() {
      return class {
        static getA(klass) { return klass.#a(); }
      };
    }
  }

  const C = Outer.test();
  assertEquals(C.getA(Outer), 'Outer');
  assertThrows(() => C.getA.call(C), TypeError);
  assertThrows(() => C.getA.call(new Outer), TypeError);
}

// Super property access in static private methods
{
  class A {
    static a = 1;
  }

  class B extends A {
    static #a() { return super.a; }
    static getA() { return this.#a(); }
  }

  assertEquals(B.getA(), 1);
}

// Invalid super property access in static private methods
{
  class A {
    static #a() { return 1; }
    static getA() { return this.#a(); }
  }

  class B extends A {
    static getA() { return super.getA(); }
  }

  assertThrows(() => B.getA(), TypeError);
}

// Static private methods accessed in eval.
{
  class C {
    static #m(v) { return v; }
    static test(str) {
      return eval(str);
    }
  }

  assertEquals(C.test('this.#m(1)'), 1);
}

// Test that the receiver is checked during run time.
{
  const C = class {
    static #a() { }
    static test(klass) { return klass.#a; }
  };
  const test = C.test;
  assertThrows(test, TypeError);
}

// Duplicate static private accessors and methods.
{
  assertThrows('class C { static get #a() {} static get #a() {} }', SyntaxError);
  assertThrows('class C { static get #a() {} static #a() {} }', SyntaxError);
  assertThrows('class C { static get #a() {} get #a() {} }', SyntaxError);
  assertThrows('class C { static get #a() {} set #a(val) {} }', SyntaxError);
  assertThrows('class C { static get #a() {} #a() {} }', SyntaxError);

  assertThrows('class C { static set #a(val) {} static set #a(val) {} }', SyntaxError);
  assertThrows('class C { static set #a(val) {} static #a() {} }', SyntaxError);
  assertThrows('class C { static set #a(val) {} get #a() {} }', SyntaxError);
  assertThrows('class C { static set #a(val) {} set #a(val) {} }', SyntaxError);
  assertThrows('class C { static set #a(val) {} #a() {} }', SyntaxError);

  assertThrows('class C { static #a() {} static #a() {} }', SyntaxError);
  assertThrows('class C { static #a() {} #a(val) {} }', SyntaxError);
  assertThrows('class C { static #a() {} set #a(val) {} }', SyntaxError);
  assertThrows('class C { static #a() {} get #a() {} }', SyntaxError);
}
