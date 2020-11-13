// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --super-ic

function forceDictionaryMode(obj) {
  for (let i = 0; i < 2000; ++i) {
    obj["prop" + i] = "prop_value";
  }
}

(function TestHomeObjectPrototypeNull() {
  class A {}

  class B extends A {
    bar() {
      return super.y;
    }
  };

  let o = new B();
  B.prototype.__proto__ = null;
  assertThrows(() => { o.bar(); });
})();

(function TestMonomorphic() {
  class A {}
  A.prototype.bar = "wrong value: A.prototype.bar";

  class B extends A {};
  B.prototype.bar = "correct value";

  class C extends B {};

  class D extends C {
    foo() { return super.bar; }
  }
  D.prototype.bar = "wrong value: D.prototype.bar";

  let o = new D();
  o.bar = "wrong value: o.bar";
  for (let i = 0; i < 100; ++i) {
    const r = o.foo();
    assertEquals("correct value", r);
  }
})();

(function TestMonomorphicWithGetter() {
  class A {}
  A.prototype.bar = "wrong value: A.prototype.bar";

  class B extends A {
    get bar() {
      return this.test_value;
    }
  };

  class C extends B {}

  class D extends C {
    foo() {
      const b = super.bar;
      return b;
    }
  }
  D.prototype.bar = "wrong value: D.prototype.bar";

  let o = new D();
  o.bar = "wrong value: o.bar";
  o.test_value = "correct value";
  for (let i = 0; i < 1000; ++i) {
    const r = o.foo();
    assertEquals("correct value", r);
  }
})();

(function TestPolymorphic() {
  class A {}
  A.prototype.bar = "wrong value: A.prototype.bar";

  class B extends A {}
  B.prototype.bar = "correct value";

  class C extends B {}

  class D extends C {
    foo() { return super.bar; }
  }
  D.prototype.bar = "wrong value: D.prototype.bar";

  const o = new D();

  // Create objects which will act as the "home object's prototype" later.
  const prototypes = [{"a": 0}, {"b": 0}];
  for (let i = 0; i < prototypes.length; ++i) {
    prototypes[i].__proto__ = B.prototype;
  }

  for (let i = 0; i < 1000; ++i) {
    D.prototype.__proto__ = prototypes[i % prototypes.length];
    const r = o.foo();
    assertEquals("correct value", r);
  }
})();

(function TestPolymorphicWithGetter() {
  class A {}
  A.prototype.bar = "wrong value: A.prototype.bar";

  class B extends A {
    get bar() {
      return this.test_value;
    }
  };

  class C extends B {}

  class D extends C {
    foo() { return super.bar; }
  }
  D.prototype.bar = "wrong value: D.prototype.bar";

  const o = new D();
  o.test_value = "correct value";

  // Create objects which will act as the "home object's prototype" later.
  const prototypes = [{"a": 0}, {"b": 0}];
  for (let i = 0; i < prototypes.length; ++i) {
    prototypes[i].__proto__ = B.prototype;
  }

  for (let i = 0; i < 1000; ++i) {
    D.prototype.__proto__ = prototypes[i % prototypes.length];
    const r = o.foo();
    assertEquals("correct value", r);
  }
})();

(function TestMegamorphic() {
  class A {}
  A.prototype.bar = "wrong value: A.prototype.bar";

  class B extends A {}
  B.prototype.bar = "correct value";

  class C extends B {}

  class D extends C {
    foo() { return super.bar; }
  }
  D.prototype.bar = "wrong value: D.prototype.bar";

  const o = new D();

  // Create objects which will act as the "home object's prototype" later.
  const prototypes = [{"a": 0}, {"b": 0}, {"c": 0}, {"d": 0}, {"e": 0},
                      {"f": 0}, {"g": 0}, {"e": 0}];

  for (let i = 0; i < prototypes.length; ++i) {
    prototypes[i].__proto__ = B.prototype;
  }

  for (let i = 0; i < 1000; ++i) {
    D.prototype.__proto__ = prototypes[i % prototypes.length];
    const r = o.foo();
    assertEquals("correct value", r);
  }
})();

(function TestMegamorphicWithGetter() {
  class A {}
  A.prototype.bar = "wrong value: A.prototype.bar";

  class B extends A {
    get bar() {
      return this.test_value;
    }
  };

  class C extends B {}

  class D extends C {
    foo() { return super.bar;}
  }
  D.prototype.bar = "wrong value: D.prototype.bar";

  const o = new D();
  o.test_value = "correct value";

  // Create objects which will act as the "home object's prototype" later.
  const prototypes = [{"a": 0}, {"b": 0}, {"c": 0}, {"d": 0}, {"e": 0},
                      {"f": 0}, {"g": 0}, {"e": 0}];
  for (let i = 0; i < prototypes.length; ++i) {
    prototypes[i].__proto__ = B.prototype;
  }

  for (let i = 0; i < 1000; ++i) {
    D.prototype.__proto__ = prototypes[i % prototypes.length];
    const r = o.foo();
    assertEquals("correct value", r);
  }
})();

(function TestHolderHandledCorrectlyAfterOptimization() {
  class A {
    m() { return "m return value"; }
    get boom() { return this.m; }
  }
  class B extends A { f() { return super.boom() } }

  %PrepareFunctionForOptimization(B.prototype.f);
  const r1 = new B().f();
  assertEquals("m return value", r1);
  const r2 = new B().f();
  assertEquals("m return value", r2);
})();

(function TestHolderHandledCorrectlyAfterOptimization2() {
  class A {
    m() { return "m return value"; }
    get boom() { return this.m; }
  }
  class Middle1 extends A {}
  class Middle2 extends Middle1 {}
  class B extends Middle2 { f() { return super.boom() } }

  %PrepareFunctionForOptimization(B.prototype.f);
  const r1 = new B().f();
  assertEquals("m return value", r1);
  const r2 = new B().f();
  assertEquals("m return value", r2);
})();

(function TestStubCacheConfusion() {
  // Regression test for using the wrong stub from the stub cache.

  class A {};
  A.prototype.foo = "foo from A.prototype";

  class B extends A {
    m() { return super.foo; }
  }

  // Create objects which will act as receivers for the method call. All of
  // them will have a different map.
  class C0 extends B { foo = "foo from C0"; };
  class C1 extends B { foo = "foo from C1"; };
  class C2 extends B { foo = "foo from C2"; };
  class C3 extends B { foo = "foo from C3"; };
  class C4 extends B { foo = "foo from C4"; };
  class C5 extends B { foo = "foo from C5"; };
  class C6 extends B { foo = "foo from C6"; };
  class C7 extends B { foo = "foo from C7"; };
  class C8 extends B { foo = "foo from C8"; };
  class C9 extends B { foo = "foo from C9"; };

  let receivers = [
    new C0(), new C1(), new C2(), new C3(), new C4(), new C5(), new C6(), new C7(),
    new C8(), new C9()
  ];

  // Fill the stub cache with handlers which access "foo" from the receivers.
  function getfoo(o) {
    return o.foo;
  }

  for (let i = 0; i < 1000; ++i) {
    getfoo(receivers[i % receivers.length]);
  }

  // Create objects which will act as the "home object's prototype" later.
  const prototypes = [{"a": "prop in prototypes[0]"},
                      {"b": "prop in prototypes[1]"},
                      {"c": "prop in prototypes[2]"},
                      {"d": "prop in prototypes[3]"},
                      {"e": "prop in prototypes[4]"},
                      {"f": "prop in prototypes[5]"},
                      {"g": "prop in prototypes[6]"},
                      {"h": "prop in prototypes[7]"},
                      {"i": "prop in prototypes[8]"},
                      {"j": "prop in prototypes[9]"}];
  for (let i = 0; i < prototypes.length; ++i) {
    prototypes[i].__proto__ = A.prototype;
  }

  for (let i = 0; i < 1000; ++i) {
    // Make the property load for "super.foo" megamorphic in terms of the home
    // object's prototype.
    B.prototype.__proto__ = prototypes[i % prototypes.length];
    const r = receivers[i % receivers.length].m();
    // The bug was that we used the same handlers which were accessing "foo"
    // from the receivers, instead of accessing "foo" from the home object's
    // prototype.
    assertEquals("foo from A.prototype", r);
  }
})();

(function TestLengthConfusion() {
  // Regression test for confusion between bound function length and array
  // length.
  class A {};

  class B extends A {
    m() {
      return super.length;
    }
  }

  // Create a "home object proto" object which is a bound function.
  let home_object_proto = (function() {}).bind({});
  forceDictionaryMode(home_object_proto);
  B.prototype.__proto__ = home_object_proto;

  assertEquals(0, home_object_proto.length);

  for (let i = 0; i < 1000; ++i) {
    const r = B.prototype.m.call([1, 2]);
    // The bug was that here we retrieved the length of the receiver array
    // instead of the home object's __proto__.
    assertEquals(home_object_proto.length, r);
  }
})();

(function TestSuperInsideArrowFunction() {
  class A {};
  A.prototype.foo = "correct value";

  class B extends A {
    m() {
      const bar = () => {
        return super.foo;
      }
      return bar();
    }

    n() {
      const bar = () => {
        return super.foo;
      }
      return bar;
    }
  };

  assertEquals(A.prototype.foo, (new B).m());
  assertEquals(A.prototype.foo, (new B).n()());
})();

// Regression test for a receiver vs lookup start object confusion.
(function TestProxyAsLookupStartObject1() {
  class A {}
  class B extends A {
    bar() {
      return super.foo;
    }
  }

  const o = new B();
  B.prototype.__proto__ = new Proxy({}, {});
  for (let i = 0; i < 1000; ++i) {
    assertEquals(undefined, o.bar());
  }
})();

(function TestProxyAsLookupStartObject2() {
  class A {}
  class B extends A {
    bar() {
      return super.foo;
    }
  }

  const o = new B();
  forceDictionaryMode(o);
  o.foo = "wrong value";
  B.prototype.__proto__ = new Proxy({}, {});

  for (let i = 0; i < 1000; ++i) {
    assertEquals(undefined, o.bar());
  }
})();

(function TestProxyAsLookupStartObject3() {
  class A {}
  class B extends A {
    bar() {
      return super.foo;
    }
  }

  const o = new B();
  B.prototype.__proto__ = new Proxy({}, {});
  B.prototype.__proto__.foo = "correct value";

  for (let i = 0; i < 1000; ++i) {
    assertEquals(B.prototype.__proto__.foo, o.bar());
  }
})();

(function TestDictionaryModeHomeObjectProto1() {
  class A {}
  forceDictionaryMode(A.prototype);
  A.prototype.foo = "correct value";
  class B extends A {
    bar() {
      return super.foo;
    }
  }
  const o = new B();
  for (let i = 0; i < 1000; ++i) {
    assertEquals(A.prototype.foo, o.bar());
  }
})();

(function TestDictionaryModeHomeObjectProto2() {
  class A {}
  A.prototype.foo = "correct value";
  class B extends A {};
  forceDictionaryMode(B.prototype);
  class C extends B {
    bar() {
      return super.foo;
    }
  }
  const o = new C();
  for (let i = 0; i < 1000; ++i) {
    assertEquals(A.prototype.foo, o.bar());
  }
})();

(function TestHomeObjectProtoIsGlobalThis() {
  class A {};
  class B extends A {
    bar() { return super.foo; }
  }
  B.prototype.__proto__ = globalThis;

  const o = new B();
  for (let i = 0; i < 1000; ++i) {
    assertEquals(undefined, o.bar());
  }
})();

// Regression test for (mis)using the prototype validity cell mechanism.
(function TestLoadFromDictionaryModePrototype() {
  const obj1 = {};
  const obj2 = {__proto__: obj1};
  forceDictionaryMode(obj1);

  for (let i = 0; i < 1000; ++i) {
    assertEquals(undefined, obj1.x);
  }

  obj1.x = "added";
  assertEquals("added", obj1.x);
})();
