// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";

// Basic private method test
{
  let calledWith;
  class C {
    #a(arg) { calledWith = arg; }
    callA(arg) { this.#a(arg); }
  }

  const c = new C;
  assertEquals(undefined, c.a);
  assertEquals(undefined, calledWith);
  c.callA(1);
  assertEquals(1, calledWith);
}

// Call private method in another instance
{
  class C {
    #a(arg) { this.calledWith = arg; }
    callAIn(obj, arg) { obj.#a(arg); }
  }

  const c = new C;
  const c2 = new C;

  assertEquals(undefined, c.a);
  assertEquals(undefined, c.calledWith);
  assertEquals(undefined, c2.calledWith);

  c2.callAIn(c, 'fromC2');
  assertEquals('fromC2', c.calledWith);
  assertEquals(undefined, c2.calledWith);

  c2.callAIn(c2, 'c2');
  assertEquals('fromC2', c.calledWith);
  assertEquals('c2', c2.calledWith);

  assertThrows(() => { c2.callAIn({}); }, TypeError);
}

// Private methods and private fields
{
  class C {
    #a;
    constructor(a) {
      this.#a = a;
    }
    #getAPlus1() {
      return this.#a + 1;
    }
    equals(obj) {
      return this.#getAPlus1() === obj.#getAPlus1();
    }
  }
  const c = new C(0);
  const c2 = new C(2);
  const c3 = new C(2);
  assertEquals(true, c2.equals(c3));
  assertEquals(false, c2.equals(c));
  assertEquals(false, c3.equals(c));
}

// Class inheritance
{
  class A {
    #val;
    constructor(a) {
      this.#val = a;
    }
    #a() { return this.#val; }
    getA() { return this.#a(); }
  }
  class B extends A {
    constructor(b) {
      super(b);
    }
    b() { return this.getA() }
  }
  const b = new B(1);
  assertEquals(1, b.b());
}

// Private members should be accessed according to the class the
// invoked method is in.
{
  class A {
    #val;
    constructor(a) {
      this.#val = a;
    }
    #getVal() { return this.#val; }
    getA() { return this.#getVal(); }
    getVal() { return this.#getVal(); }
  }

  class B extends A {
    #val;
    constructor(a, b) {
      super(a);
      this.#val = b;
    }
    #getVal() { return this.#val; }
    getB() { return this.#getVal(); }
    getVal() { return this.#getVal(); }
  }

  const b = new B(1, 2);
  assertEquals(1, b.getA());
  assertEquals(2, b.getB());
  assertEquals(1, A.prototype.getVal.call(b));
  assertEquals(2, B.prototype.getVal.call(b));
  const a = new A(1);
  assertEquals(1, a.getA());
  assertThrows(() => B.prototype.getB.call(a), TypeError);
}

// Private methods in nested classes.
{
  class C {
    #b() {
      class B {
        #foo(arg) { return arg; }
        callFoo(arg) { return this.#foo(arg); }
      }
      return new B();
    }
    createB() { return this.#b(); }
  }
  const c = new C;
  const b = c.createB();
  assertEquals(1, b.callFoo(1));
}

// Private methods in nested classes with inheritance.
{
  class C {
    #b() {
      class B extends C {
        #foo(arg) { return arg; }
        callFoo(arg) { return this.#foo(arg); }
      }
      return new B();
    }
    createB() { return this.#b(); }
  }

  const c = new C;
  const b = c.createB();
  assertEquals(1, b.callFoo(1));
  const b2 = b.createB();
  assertEquals(1, b2.callFoo(1));
}

// Class expressions.
{
  const C = class {
    #a() { return 1; }
    callA(obj) { return obj.#a() }
  };
  const c = new C;
  const c2 = new C;
  assertEquals(1, c.callA(c));
  assertEquals(1, c.callA(c2));
}

// Nested class expressions.
{
  const C = class {
    #b() {
      const B = class {
        #foo(arg) { return arg; }
        callFoo(arg) { return this.#foo(arg); }
      };
      return new B();
    }
    createB() { return this.#b(); }
  };

  const c = new C;
  const b = c.createB();
  assertEquals(1, b.callFoo(1));
}


// Nested class expressions with hierarchy.
{
  const C = class {
    #b() {
      const B = class extends C {
        #foo(arg) { return arg; }
        callFoo(arg) { return this.#foo(arg); }
      }
      return new B();
    }
    createB() { return this.#b(); }
  }

  const c = new C;
  const b = c.createB();
  assertEquals(1, b.callFoo(1));
  const b2 = b.createB();
  assertEquals(1, b2.callFoo(1));
}

// Adding the brand twice on the same object should throw.
{
  class A {
    constructor(arg) {
      return arg;
    }
  }

  class C extends A {
    #x() { }

    constructor(arg) {
      super(arg);
    }
  }

  let c1 = new C({});
  assertThrows(() => new C(c1), TypeError);
}

// Private methods should be not visible to proxies.
{
  class X {
    #x() {}
    x() { this.#x(); };
    callX(obj) { obj.#x(); }
  }
  let handlerCalled = false;
  const x = new X();
  let p = new Proxy(new X, {
    apply(target, thisArg, argumentsList) {
      handlerCalled = true;
      Reflect.apply(target, thisArg, argumentsList);
    }
  });
  assertThrows(() => p.x(), TypeError);
  assertThrows(() => x.callX(p), TypeError);
  assertThrows(() => X.prototype.x.call(p), TypeError);
  assertThrows(() => X.prototype.callX(p), TypeError);
  assertEquals(false, handlerCalled);
}

// Reference outside of class.
{
  class C {
    #a() {}
  }
  assertThrows('new C().#a()');
}

// Duplicate private names.
{
  assertThrows('class C { #a = 1; #a() {} }');
}

{
  class A extends class { } {
    #a() {}
  }

  class D extends class {
    #c() {}
  } {
    #d() {}
  }

  class E extends D {
    #e() {}
  }

  new A;
  new D;
  new E;
}

// Super access within private methods.
{
  class A {
    foo() { return 1; }
  }

  class C extends A {
    #m() { return super.foo; }
    fn() { return this.#m()(); }
  }

  assertEquals(1, new C().fn());
}

{
  assertThrows(() => {
    class A {
      [this.#a] = 1;
      #a() { }
    }
  }, TypeError);
}
