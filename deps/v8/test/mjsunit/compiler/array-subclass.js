// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Test Array subclass default constructor with no parameters.
(function() {
  const A = class A extends Array { };

  function foo() { return new A; }

  function test(foo) {
    %PrepareFunctionForOptimization(foo);
    assertInstanceof(foo(), A);
    assertEquals(0, foo().length);
    assertInstanceof(foo(), A);
    assertEquals(0, foo().length);
    %OptimizeFunctionOnNextCall(foo);
    assertInstanceof(foo(), A);
    assertEquals(0, foo().length);
  }
  test(foo);

  // Non-extensible
  function fooPreventExtensions() { return Object.preventExtensions(foo()); }
  test(fooPreventExtensions);

  // Sealed
  function fooSeal() { return Object.seal(foo()); }
  test(fooSeal);

  // Frozen
  function fooFreeze() { return Object.freeze(foo()); }
  test(fooFreeze);
})();

// Test Array subclass default constructor with small constant length.
(function() {
  const A = class A extends Array { };
  const L = 4;

  function foo() { return new A(L); }

  function test(foo) {
    %PrepareFunctionForOptimization(foo);
    assertInstanceof(foo(), A);
    assertEquals(L, foo().length);
    assertInstanceof(foo(), A);
    assertEquals(L, foo().length);
    %OptimizeFunctionOnNextCall(foo);
    assertInstanceof(foo(), A);
    assertEquals(L, foo().length);
  }
  test(foo);

  // Non-extensible
  function fooPreventExtensions() { return Object.preventExtensions(foo()); }
  test(fooPreventExtensions);

  // Sealed
  function fooSeal() { return Object.seal(foo()); }
  test(fooSeal);

  // Frozen
  function fooFreeze() { return Object.freeze(foo()); }
  test(fooFreeze);
})();

// Test Array subclass default constructor with large constant length.
(function() {
  const A = class A extends Array { };
  const L = 1024 * 1024;

  function foo() { return new A(L); }

  function test(foo) {
    %PrepareFunctionForOptimization(foo);
    assertInstanceof(foo(), A);
    assertEquals(L, foo().length);
    assertInstanceof(foo(), A);
    assertEquals(L, foo().length);
    %OptimizeFunctionOnNextCall(foo);
    assertInstanceof(foo(), A);
    assertEquals(L, foo().length);
  }
  test(foo);

  // Non-extensible
  function fooPreventExtensions() { return Object.preventExtensions(foo()); }
  test(fooPreventExtensions);

  // Sealed
  function fooSeal() { return Object.seal(foo()); }
  test(fooSeal);

  // Frozen
  function fooFreeze() { return Object.freeze(foo()); }
  test(fooFreeze);
})();

// Test Array subclass default constructor with known boolean.
(function() {
  const A = class A extends Array { };

  function foo() { return new A(true); }

  function test(foo) {
    %PrepareFunctionForOptimization(foo);
    assertInstanceof(foo(), A);
    assertEquals(1, foo().length);
    assertEquals(true, foo()[0]);
    assertInstanceof(foo(), A);
    assertEquals(1, foo().length);
    assertEquals(true, foo()[0]);
    %OptimizeFunctionOnNextCall(foo);
    assertInstanceof(foo(), A);
    assertEquals(1, foo().length);
    assertEquals(true, foo()[0]);
  }
  test(foo);

  // Non-extensible
  function fooPreventExtensions() { return Object.preventExtensions(foo()); }
  test(fooPreventExtensions);

  // Sealed
  function fooSeal() { return Object.seal(foo()); }
  test(fooSeal);

  // Frozen
  function fooFreeze() { return Object.freeze(foo()); }
  test(fooFreeze);
})();

// Test Array subclass default constructor with known string.
(function() {
  const A = class A extends Array { };

  function foo() { return new A(""); }

  function test(foo) {
    %PrepareFunctionForOptimization(foo);
    assertInstanceof(foo(), A);
    assertEquals(1, foo().length);
    assertEquals("", foo()[0]);
    assertInstanceof(foo(), A);
    assertEquals(1, foo().length);
    assertEquals("", foo()[0]);
    %OptimizeFunctionOnNextCall(foo);
    assertInstanceof(foo(), A);
    assertEquals(1, foo().length);
    assertEquals("", foo()[0]);
  }
  test(foo);

  // Non-extensible
  function fooPreventExtensions() { return Object.preventExtensions(foo()); }
  test(fooPreventExtensions);

  // Sealed
  function fooSeal() { return Object.seal(foo()); }
  test(fooSeal);

  // Frozen
  function fooFreeze() { return Object.freeze(foo()); }
  test(fooFreeze);
})();

// Test Array subclass default constructor with known object.
(function() {
  const A = class A extends Array { };
  const O = {foo: "foo"};

  function foo() { return new A(O); }

  function test(foo) {
    %PrepareFunctionForOptimization(foo);
    assertInstanceof(foo(), A);
    assertEquals(1, foo().length);
    assertSame(O, foo()[0]);
    assertInstanceof(foo(), A);
    assertEquals(1, foo().length);
    assertSame(O, foo()[0]);
    %OptimizeFunctionOnNextCall(foo);
    assertInstanceof(foo(), A);
    assertEquals(1, foo().length);
    assertSame(O, foo()[0]);
  }
  test(foo);

  // Non-extensible
  function fooPreventExtensions() { return Object.preventExtensions(foo()); }
  test(fooPreventExtensions);

  // Sealed
  function fooSeal() { return Object.seal(foo()); }
  test(fooSeal);

  // Frozen
  function fooFreeze() { return Object.freeze(foo()); }
  test(fooFreeze);})();

// Test Array subclass default constructor with known small integers.
(function() {
  const A = class A extends Array { };

  function foo() { return new A(1, 2, 3); }

  function test(foo) {
    %PrepareFunctionForOptimization(foo);
    assertInstanceof(foo(), A);
    assertEquals(3, foo().length);
    assertEquals(1, foo()[0]);
    assertEquals(2, foo()[1]);
    assertEquals(3, foo()[2]);
    %OptimizeFunctionOnNextCall(foo);
    assertInstanceof(foo(), A);
    assertEquals(3, foo().length);
    assertEquals(1, foo()[0]);
    assertEquals(2, foo()[1]);
    assertEquals(3, foo()[2]);
  }
  test(foo);

  // Non-extensible
  function fooPreventExtensions() { return Object.preventExtensions(foo()); }
  test(fooPreventExtensions);

  // Sealed
  function fooSeal() { return Object.seal(foo()); }
  test(fooSeal);

  // Frozen
  function fooFreeze() { return Object.freeze(foo()); }
  test(fooFreeze);
})();

// Test Array subclass default constructor with known numbers.
(function() {
  const A = class A extends Array { };

  function foo() { return new A(1.1, 2.2, 3.3); }

  function test(foo) {
    %PrepareFunctionForOptimization(foo);
    assertInstanceof(foo(), A);
    assertEquals(3, foo().length);
    assertEquals(1.1, foo()[0]);
    assertEquals(2.2, foo()[1]);
    assertEquals(3.3, foo()[2]);
    %OptimizeFunctionOnNextCall(foo);
    assertInstanceof(foo(), A);
    assertEquals(3, foo().length);
    assertEquals(1.1, foo()[0]);
    assertEquals(2.2, foo()[1]);
    assertEquals(3.3, foo()[2]);
  }
  test(foo);

  // Non-extensible
  function fooPreventExtensions() { return Object.preventExtensions(foo()); }
  test(fooPreventExtensions);

  // Sealed
  function fooSeal() { return Object.seal(foo()); }
  test(fooSeal);

  // Frozen
  function fooFreeze() { return Object.freeze(foo()); }
  test(fooFreeze);
})();

// Test Array subclass default constructor with known strings.
(function() {
  const A = class A extends Array { };

  function foo() { return new A("a", "b", "c", "d"); }

  function test(foo) {
    %PrepareFunctionForOptimization(foo);
    assertInstanceof(foo(), A);
    assertEquals(4, foo().length);
    assertEquals("a", foo()[0]);
    assertEquals("b", foo()[1]);
    assertEquals("c", foo()[2]);
    assertEquals("d", foo()[3]);
    %OptimizeFunctionOnNextCall(foo);
    assertInstanceof(foo(), A);
    assertEquals(4, foo().length);
    assertEquals("a", foo()[0]);
    assertEquals("b", foo()[1]);
    assertEquals("c", foo()[2]);
    assertEquals("d", foo()[3]);
  }
  test(foo);

  // Non-extensible
  function fooPreventExtensions() { return Object.preventExtensions(foo()); }
  test(fooPreventExtensions);

  // Sealed
  function fooSeal() { return Object.seal(foo()); }
  test(fooSeal);

  // Frozen
  function fooFreeze() { return Object.freeze(foo()); }
  test(fooFreeze);
})();

// Test Array subclass constructor with no parameters.
(function() {
  const A = class A extends Array {
    constructor() {
      super();
      this.bar = 1;
    }
  };

  function foo() { return new A; }

  function test(foo) {
    %PrepareFunctionForOptimization(foo);
    assertInstanceof(foo(), A);
    assertEquals(0, foo().length);
    assertEquals(1, foo().bar);
    assertInstanceof(foo(), A);
    assertEquals(0, foo().length);
    assertEquals(1, foo().bar);
    %OptimizeFunctionOnNextCall(foo);
    assertInstanceof(foo(), A);
    assertEquals(0, foo().length);
    assertEquals(1, foo().bar);
  }
  test(foo);

  // Non-extensible
  function fooPreventExtensions() { return Object.preventExtensions(foo()); }
  test(fooPreventExtensions);

  // Sealed
  function fooSeal() { return Object.seal(foo()); }
  test(fooSeal);

  // Frozen
  function fooFreeze() { return Object.freeze(foo()); }
  test(fooFreeze);})();

// Test Array subclass constructor with small constant length.
(function() {
  const A = class A extends Array {
    constructor(n) {
      super(n);
      this.bar = 1;
    }
  };
  const L = 4;

  function foo() { return new A(L); }

  function test(foo) {
    %PrepareFunctionForOptimization(foo);
    assertInstanceof(foo(), A);
    assertEquals(L, foo().length);
    assertEquals(1, foo().bar);
    assertInstanceof(foo(), A);
    assertEquals(L, foo().length);
    assertEquals(1, foo().bar);
    %OptimizeFunctionOnNextCall(foo);
    assertInstanceof(foo(), A);
    assertEquals(L, foo().length);
    assertEquals(1, foo().bar);
  }
  test(foo);

  // Non-extensible
  function fooPreventExtensions() { return Object.preventExtensions(foo()); }
  test(fooPreventExtensions);

  // Sealed
  function fooSeal() { return Object.seal(foo()); }
  test(fooSeal);

  // Frozen
  function fooFreeze() { return Object.freeze(foo()); }
  test(fooFreeze);
})();

// Test Array subclass constructor with large constant length.
(function() {
  const A = class A extends Array {
    constructor(n) {
      super(n);
      this.bar = 1;
    }
  };
  const L = 1024 * 1024;

  function foo() { return new A(L); }

  function test(foo) {
    %PrepareFunctionForOptimization(foo);
    assertInstanceof(foo(), A);
    assertEquals(L, foo().length);
    assertEquals(1, foo().bar);
    assertInstanceof(foo(), A);
    assertEquals(L, foo().length);
    assertEquals(1, foo().bar);
    %OptimizeFunctionOnNextCall(foo);
    assertInstanceof(foo(), A);
    assertEquals(L, foo().length);
    assertEquals(1, foo().bar);
  }
  test(foo);

  // Non-extensible
  function fooPreventExtensions() { return Object.preventExtensions(foo()); }
  test(fooPreventExtensions);

  // Sealed
  function fooSeal() { return Object.seal(foo()); }
  test(fooSeal);

  // Frozen
  function fooFreeze() { return Object.freeze(foo()); }
  test(fooFreeze);
})();

// Test Array subclass constructor with known boolean.
(function() {
  const A = class A extends Array {
    constructor(n) {
      super(n);
      this.bar = 1;
    }
  };

  function foo() { return new A(true); }

  function test(foo) {
    %PrepareFunctionForOptimization(foo);
    assertInstanceof(foo(), A);
    assertEquals(1, foo().length);
    assertEquals(true, foo()[0]);
    assertEquals(1, foo().bar);
    assertInstanceof(foo(), A);
    assertEquals(1, foo().length);
    assertEquals(true, foo()[0]);
    assertEquals(1, foo().bar);
    %OptimizeFunctionOnNextCall(foo);
    assertInstanceof(foo(), A);
    assertEquals(1, foo().length);
    assertEquals(true, foo()[0]);
    assertEquals(1, foo().bar);
  }
  test(foo);

  // Non-extensible
  function fooPreventExtensions() { return Object.preventExtensions(foo()); }
  test(fooPreventExtensions);

  // Sealed
  function fooSeal() { return Object.seal(foo()); }
  test(fooSeal);

  // Frozen
  function fooFreeze() { return Object.freeze(foo()); }
  test(fooFreeze);
})();

// Test Array subclass constructor with known string.
(function() {
  const A = class A extends Array {
    constructor(n) {
      super(n);
      this.bar = 1;
    }
  };

  function foo() { return new A(""); }

  function test(foo) {
    %PrepareFunctionForOptimization(foo);
    assertInstanceof(foo(), A);
    assertEquals(1, foo().length);
    assertEquals("", foo()[0]);
    assertEquals(1, foo().bar);
    assertInstanceof(foo(), A);
    assertEquals(1, foo().length);
    assertEquals("", foo()[0]);
    assertEquals(1, foo().bar);
    %OptimizeFunctionOnNextCall(foo);
    assertInstanceof(foo(), A);
    assertEquals(1, foo().length);
    assertEquals("", foo()[0]);
    assertEquals(1, foo().bar);
  }
  test(foo);

  // Non-extensible
  function fooPreventExtensions() { return Object.preventExtensions(foo()); }
  test(fooPreventExtensions);

  // Sealed
  function fooSeal() { return Object.seal(foo()); }
  test(fooSeal);

  // Frozen
  function fooFreeze() { return Object.freeze(foo()); }
  test(fooFreeze);
})();

// Test Array subclass constructor with known object.
(function() {
  const A = class A extends Array {
    constructor(n) {
      super(n);
      this.bar = 1;
    }
  };
  const O = {foo: "foo"};

  function foo() { return new A(O); }

  function test(foo) {
    %PrepareFunctionForOptimization(foo);
    assertInstanceof(foo(), A);
    assertEquals(1, foo().length);
    assertSame(O, foo()[0]);
    assertEquals(1, foo().bar);
    assertInstanceof(foo(), A);
    assertEquals(1, foo().length);
    assertSame(O, foo()[0]);
    assertEquals(1, foo().bar);
    %OptimizeFunctionOnNextCall(foo);
    assertInstanceof(foo(), A);
    assertEquals(1, foo().length);
    assertSame(O, foo()[0]);
    assertEquals(1, foo().bar);
  }
  test(foo);

  // Non-extensible
  function fooPreventExtensions() { return Object.preventExtensions(foo()); }
  test(fooPreventExtensions);

  // Sealed
  function fooSeal() { return Object.seal(foo()); }
  test(fooSeal);

  // Frozen
  function fooFreeze() { return Object.freeze(foo()); }
  test(fooFreeze);
})();

// Test Array subclass constructor with known small integers.
(function() {
  const A = class A extends Array {
    constructor(x, y, z) {
      super(x, y, z);
      this.bar = 1;
    }
  };

  function foo() { return new A(1, 2, 3); }

  function test(foo) {
    %PrepareFunctionForOptimization(foo);
    assertInstanceof(foo(), A);
    assertEquals(3, foo().length);
    assertEquals(1, foo()[0]);
    assertEquals(2, foo()[1]);
    assertEquals(3, foo()[2]);
    assertEquals(1, foo().bar);
    %OptimizeFunctionOnNextCall(foo);
    assertInstanceof(foo(), A);
    assertEquals(3, foo().length);
    assertEquals(1, foo()[0]);
    assertEquals(2, foo()[1]);
    assertEquals(3, foo()[2]);
    assertEquals(1, foo().bar);
  }
  test(foo);

  // Non-extensible
  function fooPreventExtensions() { return Object.preventExtensions(foo()); }
  test(fooPreventExtensions);

  // Sealed
  function fooSeal() { return Object.seal(foo()); }
  test(fooSeal);

  // Frozen
  function fooFreeze() { return Object.freeze(foo()); }
  test(fooFreeze);
})();

// Test Array subclass constructor with known numbers.
(function() {
  const A = class A extends Array {
    constructor(x, y, z) {
      super(x, y, z);
      this.bar = 1;
    }
  };

  function foo() { return new A(1.1, 2.2, 3.3); }

  function test(foo) {
    %PrepareFunctionForOptimization(foo);
    assertInstanceof(foo(), A);
    assertEquals(3, foo().length);
    assertEquals(1.1, foo()[0]);
    assertEquals(2.2, foo()[1]);
    assertEquals(3.3, foo()[2]);
    assertEquals(1, foo().bar);
    %OptimizeFunctionOnNextCall(foo);
    assertInstanceof(foo(), A);
    assertEquals(3, foo().length);
    assertEquals(1.1, foo()[0]);
    assertEquals(2.2, foo()[1]);
    assertEquals(3.3, foo()[2]);
    assertEquals(1, foo().bar);
  }
  test(foo);

  // Non-extensible
  function fooPreventExtensions() { return Object.preventExtensions(foo()); }
  test(fooPreventExtensions);

  // Sealed
  function fooSeal() { return Object.seal(foo()); }
  test(fooSeal);

  // Frozen
  function fooFreeze() { return Object.freeze(foo()); }
  test(fooFreeze);
})();

// Test Array subclass constructor with known strings.
(function() {
  const A = class A extends Array {
    constructor(a, b, c, d) {
      super(a, b, c, d);
      this.bar = 1;
    }
  };

  function foo() { return new A("a", "b", "c", "d"); }

  function test(foo) {
    %PrepareFunctionForOptimization(foo);
    assertInstanceof(foo(), A);
    assertEquals(4, foo().length);
    assertEquals("a", foo()[0]);
    assertEquals("b", foo()[1]);
    assertEquals("c", foo()[2]);
    assertEquals("d", foo()[3]);
    assertEquals(1, foo().bar);
    %OptimizeFunctionOnNextCall(foo);
    assertInstanceof(foo(), A);
    assertEquals(4, foo().length);
    assertEquals("a", foo()[0]);
    assertEquals("b", foo()[1]);
    assertEquals("c", foo()[2]);
    assertEquals("d", foo()[3]);
    assertEquals(1, foo().bar);
  }
  test(foo);

  // Non-extensible
  function fooPreventExtensions() { return Object.preventExtensions(foo()); }
  test(fooPreventExtensions);

  // Sealed
  function fooSeal() { return Object.seal(foo()); }
  test(fooSeal);

  // Frozen
  function fooFreeze() { return Object.freeze(foo()); }
  test(fooFreeze);
})();
