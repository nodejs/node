// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-private-fields --no-lazy --allow-natives-syntax


"use strict";

// TODO(gsathya): Missing tests:
// (a) check private field access on proxies.
// (b) throw reference error on missing private field access.
// (c) throw when private fields are set without being declared.
// (d) throw when private fields are accessed outside class bodies.
// (e) check that private field isn't called 'constructor'.
// (f) tests involving eval
{
  class C {
    #a;
    getA() { return this.#a; }
  }

  assertEquals(undefined, C.a);

  let c = new C;
  assertEquals(undefined, c.a);
  assertEquals(undefined, c.getA());
}

{
  class C {
    #a = 1;
    getA() { return this.#a; }
  }

  assertEquals(undefined, C.a);

  let c = new C;
  assertEquals(undefined, c.a);
  assertEquals(1, c.getA());
}

{
  class C {
    #a = 1;
    #b = this.#a;
    getB() { return this.#b; }
  }

  let c = new C;
  assertEquals(1, c.getB());
}

{
  class C {
    #b = this.#a;
    #a = 1;
    getB() { return this.#b; }
  }

  let c = new C;
  assertEquals(undefined, c.getB());
}

{
  class C {
    #a = 1;
    getA() { return this.#a; }
    constructor() {
      assertEquals(1, this.#a);
      this.#a = 5;
    }
  }

  let c = new C;
  assertEquals(5, c.getA());
}

{
  class C {
    #a = this;
    #b = () => this;
    getA() { return this.#a; }
    getB() { return this.#b; }
  }

  let c1 = new C;
  assertSame(c1, c1.getA());
  assertSame(c1, c1.getB()());
  let c2 = new C;
  assertSame(c1, c1.getB().call(c2));
}

{
  class C {
    #a = this;
    #b = function() { return this; };
    getA() { return this.#a; }
    getB() { return this.#b; }
  }

  let c1 = new C;
  assertSame(c1, c1.getA());
  assertSame(c1, c1.getB().call(c1));
  let c2 = new C;
  assertSame(c2, c1.getB().call(c2));
}


{
  class C {
    #a = function() { return 1 };
    getA() {return this.#a;}
  }

  let c = new C;
  assertEquals('#a', c.getA().name);
}

{
  let d = function() { return new.target; }
  class C {
    #c = d;
    getC() { return this.#c; }
  }

  let c = new C;
  assertEquals(undefined, c.getC()());
  assertSame(new d, new (c.getC()));
}

{
  class C {
    #b = new.target;
    #c = () => new.target;
    getB() { return this.#b; }
    getC() { return this.#c; }
  }

  let c = new C;
  assertEquals(undefined, c.getB());
  assertEquals(undefined, c.getC()());
}

{
  class C {
    #a = 1;
    #b = () => this.#a;
    getB() { return this.#b; }
  }

  let c1 = new C;
  assertSame(1, c1.getB()());
}

{
  class C {
    #a = 1;
    getA(instance) { return instance.#a; }
  }

  class B { }
  let c = new C;
  assertEquals(undefined, c.a);
  assertEquals(1, c.getA(c));

  //TODO(gsathya): This should be a TypeError.
  //assertThrows(() => c.getA(new B), TypeError);
}

{
  let prototypeLookup = false;
  class A {
    set a(val) {
      prototypeLookup = true;
    }

    get a() { return undefined; }
  }

  class C extends A {
    #a = 1;
    getA() { return this.#a; }
  }

  let c = new C;
  assertEquals(1, c.getA());
  assertEquals(false, prototypeLookup);
}

{
  class A {
    constructor() { this.a = 1; }
  }

  class B extends A {
    #b = this.a;
    getB() { return this.#b; }
  }

  let b = new B;
  assertEquals(1, b.getB());
}

{
  class A {
    #a = 1;
    getA() { return this.#a; }
  }

  class B extends A {
    #b = super.getA();
    getB() { return this.#b; }
  }

  let b = new B;
  assertEquals(1, b.getB());
}

{
  class A {
    #a = 1;
    getA() { return this.#a;}
  }

  class B extends A {
    #a = 2;
    get_A() { return this.#a;}
  }

  let a = new A;
  let b = new B;
  assertEquals(1, a.getA());
  assertEquals(1, b.getA());
  assertEquals(2, b.get_A());
}

{
  let foo = undefined;
  class A {
    #a = 1;
    constructor() {
      foo = this.#a;
    }
  }

  let a = new A;
  assertEquals(1, foo);
}

{
  let foo = undefined;
  class A extends class {} {
    #a = 1;
    constructor() {
      super();
      foo = this.#a;
    }
  }

  let a = new A;
  assertEquals(1, foo);
}

{
  function makeClass() {
    return class {
      #a;
      setA(val) { this.#a = val; }
      getA() { return this.#a; }
    }
  }

  let classA = makeClass();
  let a = new classA;
  let classB = makeClass();
  let b = new classB;

  assertEquals(undefined, a.getA());
  assertEquals(undefined, b.getA());

  a.setA(3);
  assertEquals(3, a.getA());
  assertEquals(undefined, b.getA());

  b.setA(5);
  assertEquals(3, a.getA());
  assertEquals(5, b.getA());

  // assertThrows(() => a.getA.call(b), ReferenceError);
  // assertThrows(() => b.getA.call(a), ReferenceError);
}

{
  let value = undefined;

  new class {
    #a = 1;
    getA() { return this.#a; }

    constructor() {
      new class {
        #a = 2;
        constructor() {
          value = this.#a;
        }
      }
    }
  }

  assertEquals(2, value);
}

{
  class A {
    #a = 1;
    b = class {
      getA() { return this.#a; }
      get_A(val) { return val.#a; }
    }
  }

  let a = new A();
  let b = new a.b;
  assertEquals(1, b.getA.call(a));
  assertEquals(1, b.get_A(a));
}
// {
//   class C {
//     b = this.#a;
//     #a = 1;
//   }

//   assertThrows(() => new C, ReferenceError);
// }

{
  let symbol = Symbol();

  class C {
    #a = 1;
    [symbol] = 1;
    getA() { return this.#a; }
  }

  var p = new Proxy(new C, {
    get: function(target, name) {
      if (typeof(arg) === 'symbol') {
        assertFalse(%SymbolIsPrivate(name));
      }
      return target[name];
    }
  });

  //assertThrows(() => p.getA(), ReferenceError);
  assertEquals(1, p[symbol]);
}
