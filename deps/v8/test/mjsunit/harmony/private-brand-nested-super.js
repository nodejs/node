// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";

// Tests that brand initialization works when super() is called in a nested
// arrow function or in eval().

// IIFE nested super().
{
  class A extends class {} {
    #method() { }
    constructor() {
      (() => super())();
    }
    test() { this.#method(); }
    check() { return #method in this; }
  }
  const a = new A();
  a.test();
  assertTrue(a.check());
}

// Non-IIFE nested super().
{
  class A extends class {} {
    #method() { }
    constructor() {
      const callSuper = () => super();
      callSuper();
    }
    test() { this.#method(); }
    check() { return #method in this; }
  }
  const a = new A();
  a.test();
  assertTrue(a.check());
}

// Eval'ed nested super().
{
  class A extends class {} {
    #method() { }
    constructor(str) {
      eval(str);
    }

    test() { this.#method(); }
    check() { return #method in this; }
  }
  const a = new A("super()");
  a.test();
  assertTrue(a.check());
}

// Test that private brands don't leak into class in heritage
// position with the class scope optimized away.
{
  class A extends class B extends class {} {
    constructor() { (() => super())(); }
    static get B() { return B; }
  } {
    #method() {}
    static run(obj) { obj.#method(); }
    static get B() { return super.B; }
  }

  const b = new (A.B)();
  assertThrows(() => A.run(b));
}

{
  class C {
    #c() { }
    #field = 1;
    static A = class A extends class B extends Object {
      constructor() {
        (() => super())();
      }
      field(obj) { return obj.#field; }
    } {};
    static run(obj) { obj.#c(); }
  }
  const a = new (C.A);
  assertThrows(() => C.run(a));
  const c = new C;
  assertEquals(a.field(c), 1);
}

{
  class C {
    #c() { }
    #field = 1;
    static A = class A extends class B extends Object {
      constructor() {
        (() => {
          eval("super()");
        })();
      }
      field(obj) { return obj.#field; }
    } {};
    static run(obj) { obj.#c(); }
  }
  const a = new (C.A);
  assertThrows(() => C.run(a));
  const c = new C;
  assertEquals(a.field(c), 1);
}

{
  class C {
    #c() { }
    #field = 1;
    static A = class A extends class B extends Object {
      constructor() {
        (() => {
          {
            super();
          }
        })();
      }
      field(obj) { return obj.#field; }
    } {};
    static run(obj) { obj.#c(); }
  }
  const a = new (C.A);
  assertThrows(() => C.run(a));
  const c = new C;
  assertEquals(a.field(c), 1);
}
