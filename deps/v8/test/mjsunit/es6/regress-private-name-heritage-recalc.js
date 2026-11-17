// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test 1: Nested class in heritage expression with lazy method compilation
// and PreparseData.
{
  class B {
    #x = "from B";
    static {
      class C extends (class D {
        static m() {
          return (() => function(o) { return o.#x; })();
        }
      }) {
        #x = "from C";
      }
      assertEquals("from B", C.m()(new B()));
      assertThrows(() => C.m()(new C()), TypeError);
    }
  }
}

// Test 2: Direct eval in heritage expression of a class that does not allocate
// a context.
{
  let f;
  class B {
    #x = "from B";
    [(class extends (f = (o) => eval("o.#x"), Object) {})];
  }
  assertEquals("from B", f(new B()));
}

// Test 3: Direct eval in nested function inside heritage expression where
// the outer heritage function does not allocate a context.
{
  class B {
    #x = "from B";
    static test(b) {
      let f;
      class C extends (() => {
        f = (o) => eval("o.#x");
        return Object;
      })() {
        #x = "from C";
      }
      assertThrows(() => f(new C()), TypeError);
      return f(b);
    }
  }
  assertEquals("from B", B.test(new B()));
}

// Test 4: Nested class with its own private field inside a lazy heritage
// function that does not allocate a context, using direct eval.
{
  class B {
    #x = "from B";
    static {
      let f;
      class C extends (f = () => class D {
        #y = "from D";
        static m(o) {
          return eval("o.#x");
        }
      }, Object) {
        #x = "from C";
      }
      assertEquals("from B", f().m(new B()));
      assertThrows(() => f().m(new C()), TypeError);
    }
  }
}

// Test 5: Nested class with its own private field inside heritage expression
// of an anonymous class that does not allocate a context, using direct eval.
{
  let D_cls;
  class B {
    #x = "from B";
    [(class extends (D_cls = class D {
      #y = "from D";
      static m(o) {
        return eval("o.#x");
      }
    }) {})];
  }
  assertEquals("from B", D_cls.m(new B()));
}
