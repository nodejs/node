// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --omit-default-ctors --allow-natives-syntax --turbofan
// Flags: --no-always-turbofan

(function OmitDefaultBaseCtor() {
  class A {};  // default base ctor -> will be omitted
  class B extends A {};
  %PrepareFunctionForOptimization(B);
  new B();
  %OptimizeFunctionOnNextCall(B);
  const o = new B();
  assertSame(B.prototype, o.__proto__);
  assertOptimized(B);  // No deopt.
})();

(function OmitDefaultDerivedCtor() {
  class A { constructor() {} };
  class B extends A {};  // default derived ctor -> will be omitted
  class C extends B {};
  %PrepareFunctionForOptimization(C);
  new C();
  %OptimizeFunctionOnNextCall(C);
  const o = new C();
  assertSame(C.prototype, o.__proto__);
  assertOptimized(C);  // No deopt.
})();

(function OmitDefaultBaseAndDerivedCtor() {
  class A {}; // default base ctor -> will be omitted
  class B extends A {};  // default derived ctor -> will be omitted
  class C extends B {};
  %PrepareFunctionForOptimization(C);
  new C();
  %OptimizeFunctionOnNextCall(C);
  const o = new C();
  assertSame(C.prototype, o.__proto__);
  assertOptimized(C);  // No deopt.
})();

(function OmitDefaultBaseCtorWithExplicitSuper() {
  class A {};  // default base ctor -> will be omitted
  class B extends A { constructor() { super(); } };
  %PrepareFunctionForOptimization(B);
  new B();
  %OptimizeFunctionOnNextCall(B);
  const o = new B();
  assertSame(B.prototype, o.__proto__);
  assertOptimized(B);  // No deopt.
})();

(function OmitDefaultDerivedCtorWithExplicitSuper() {
  class A { constructor() {} };
  class B extends A {};  // default derived ctor -> will be omitted
  class C extends B { constructor() { super(); } };
  %PrepareFunctionForOptimization(C);
  new C();
  %OptimizeFunctionOnNextCall(C);
  const o = new C();
  assertSame(C.prototype, o.__proto__);
  assertOptimized(C);  // No deopt.
})();

(function OmitDefaultBaseAndDerivedCtorWithExplicitSuper() {
  class A {}; // default base ctor -> will be omitted
  class B extends A {};  // default derived ctor -> will be omitted
  class C extends B { constructor() { super(); } };
  %PrepareFunctionForOptimization(C);
  new C();
  %OptimizeFunctionOnNextCall(C);
  const o = new C();
  assertSame(C.prototype, o.__proto__);
  assertOptimized(C);  // No deopt.
})();

(function OmitDefaultBaseCtorWithExplicitSuperAndNonFinalSpread() {
  class A {};  // default base ctor -> will be omitted
  class B extends A { constructor(...args) { super(1, ...args, 2); } };
  %PrepareFunctionForOptimization(B);
  new B();
  %OptimizeFunctionOnNextCall(B);
  const o = new B(3, 4);
  assertSame(B.prototype, o.__proto__);
  // See https://bugs.chromium.org/p/v8/issues/detail?id=13310
  // assertOptimized(B);  // No deopt.
  // This assert will fail when the above bug is fixed:
  assertFalse(isTurboFanned(B));
})();

(function OmitDefaultDerivedCtorWithExplicitSuperAndNonFinalSpread() {
  class A { constructor() {} };
  class B extends A {};  // default derived ctor -> will be omitted
  class C extends B { constructor(...args) { super(1, ...args, 2); } };
  %PrepareFunctionForOptimization(C);
  new C();
  %OptimizeFunctionOnNextCall(C);
  const o = new C(3, 4);
  assertSame(C.prototype, o.__proto__);
  // See https://bugs.chromium.org/p/v8/issues/detail?id=13310
  // assertOptimized(C);  // No deopt.
  // This assert will fail when the above bug is fixed:
  assertFalse(isTurboFanned(C));
})();

(function OmitDefaultBaseAndDerivedCtorWithExplicitSuperAndNonFinalSpread() {
  class A {}; // default base ctor -> will be omitted
  class B extends A {};  // default derived ctor -> will be omitted
  class C extends B { constructor(...args) { super(1, ...args, 2); } };
  %PrepareFunctionForOptimization(C);
  new C();
  %OptimizeFunctionOnNextCall(C);
  const o = new C(3, 4);
  assertSame(C.prototype, o.__proto__);
  // See https://bugs.chromium.org/p/v8/issues/detail?id=13310
  // assertOptimized(C);  // No deopt.
  // This assert will fail when the above bug is fixed:
  assertFalse(isTurboFanned(C));
})();

(function NonDefaultBaseConstructorCalled() {
  let ctorCallCount = 0;
  let lastArgs;
  class Base {
    constructor(...args) {
      ++ctorCallCount;
      this.baseTagged = true;
      lastArgs = args;
    }
  };
  // Nothing will be omitted.
  class A extends Base {};
  %PrepareFunctionForOptimization(A);
  new A();
  %OptimizeFunctionOnNextCall(A);
  const a = new A(1, 2, 3);
  assertEquals(2, ctorCallCount);
  assertEquals([1, 2, 3], lastArgs);
  assertTrue(a.baseTagged);
  assertOptimized(A);  // No deopt.

  // 'A' default ctor will be omitted.
  class B1 extends A {};
  %PrepareFunctionForOptimization(B1);
  new B1();
  %OptimizeFunctionOnNextCall(B1);
  const b1 = new B1(4, 5, 6);
  assertEquals(4, ctorCallCount);
  assertEquals([4, 5, 6], lastArgs);
  assertTrue(b1.baseTagged);
  assertOptimized(B1);  // No deopt.

  // The same test with non-final spread; 'A' default ctor will be omitted.
  class B2 extends A {
    constructor(...args) { super(1, ...args, 2); }
  };
  %PrepareFunctionForOptimization(B2);
  new B2();
  %OptimizeFunctionOnNextCall(B2);
  const b2 = new B2(4, 5, 6);
  assertEquals(6, ctorCallCount);
  assertEquals([1, 4, 5, 6, 2], lastArgs);
  assertTrue(b2.baseTagged);
  // See https://bugs.chromium.org/p/v8/issues/detail?id=13310
  // assertOptimized(B2);  // No deopt.
  // This assert will fail when the above bug is fixed:
  assertFalse(isTurboFanned(B2));  // No deopt.
})();

(function NonDefaultDerivedConstructorCalled() {
  let ctorCallCount = 0;
  let lastArgs;
  class Base {};
  class Derived extends Base {
    constructor(...args) {
      super();
      ++ctorCallCount;
      this.derivedTagged = true;
      lastArgs = args;
    }
  };
  // Nothing will be omitted.
  class A extends Derived {};
  %PrepareFunctionForOptimization(A);
  new A();
  %OptimizeFunctionOnNextCall(A);
  const a = new A(1, 2, 3);
  assertEquals(2, ctorCallCount);
  assertEquals([1, 2, 3], lastArgs);
  assertTrue(a.derivedTagged);
  assertOptimized(A);  // No deopt.

  // 'A' default ctor will be omitted.
  class B1 extends A {};
  %PrepareFunctionForOptimization(B1);
  new B1();
  %OptimizeFunctionOnNextCall(B1);
  const b1 = new B1(4, 5, 6);
  assertEquals(4, ctorCallCount);
  assertEquals([4, 5, 6], lastArgs);
  assertTrue(b1.derivedTagged);
  assertOptimized(B1);  // No deopt.

  // The same test with non-final spread. 'A' default ctor will be omitted.
  class B2 extends A {
    constructor(...args) { super(1, ...args, 2); }
  };
  %PrepareFunctionForOptimization(B2);
  new B2();
  %OptimizeFunctionOnNextCall(B2);
  const b2 = new B2(4, 5, 6);
  assertEquals(6, ctorCallCount);
  assertEquals([1, 4, 5, 6, 2], lastArgs);
  assertTrue(b2.derivedTagged);
  // See https://bugs.chromium.org/p/v8/issues/detail?id=13310
  // assertOptimized(B2);  // No deopt.
  // This assert will fail when the above bug is fixed:
  assertFalse(isTurboFanned(B2));  // No deopt.
})();

(function BaseFunctionCalled() {
  let baseFunctionCallCount = 0;
  function BaseFunction() {
    ++baseFunctionCallCount;
    this.baseTagged = true;
  }

  class A1 extends BaseFunction {};
  %PrepareFunctionForOptimization(A1);
  new A1();
  %OptimizeFunctionOnNextCall(A1);
  const a1 = new A1();
  assertEquals(2, baseFunctionCallCount);
  assertTrue(a1.baseTagged);
  assertOptimized(A1);  // No deopt.

  class A2 extends BaseFunction {
    constructor(...args) { super(1, ...args, 2); }
  };
  %PrepareFunctionForOptimization(A2);
  new A2();
  %OptimizeFunctionOnNextCall(A2);
  const a2 = new A2();
  assertEquals(4, baseFunctionCallCount);
  assertTrue(a2.baseTagged);
  assertOptimized(A2);  // No deopt.
})();

(function NonSuperclassCtor() {
  class A {};
  class B extends A {};
  class C extends B {};
  class D1 extends C {};
  class D2 extends C { constructor(...args) { super(1, ...args, 2); }};

  %PrepareFunctionForOptimization(C);
  %PrepareFunctionForOptimization(D1);
  %PrepareFunctionForOptimization(D2);
  new C();
  new D1();
  new D2();
  %OptimizeFunctionOnNextCall(C);
  %OptimizeFunctionOnNextCall(D1);
  %OptimizeFunctionOnNextCall(D2);

  // Install an object which is not a constructor into the class hierarchy.
  C.__proto__ = {};

  assertThrows(() => { new C(); }, TypeError);
  assertThrows(() => { new D1(); }, TypeError);
  assertThrows(() => { new D2(); }, TypeError);
})();

(function ArgumentsEvaluatedBeforeNonSuperclassCtorDetected() {
  class A {};
  class B extends A {};
  class C extends B {};
  class D1 extends C {};
  class D2 extends C { constructor(...args) { super(1, ...args, 2); }};

  %PrepareFunctionForOptimization(C);
  %PrepareFunctionForOptimization(D1);
  %PrepareFunctionForOptimization(D2);
  new C();
  new D1();
  new D2();
  %OptimizeFunctionOnNextCall(C);
  %OptimizeFunctionOnNextCall(D1);
  %OptimizeFunctionOnNextCall(D2);

  // Install an object which is not a constructor into the class hierarchy.
  C.__proto__ = {};

  let callCount = 0;
  function foo() {
    ++callCount;
  }

  assertThrows(() => { new C(foo()); }, TypeError);
  assertEquals(1, callCount);

  assertThrows(() => { new D1(foo()); }, TypeError);
  assertEquals(2, callCount);

  assertThrows(() => { new D2(foo()); }, TypeError);
  assertEquals(3, callCount);
})();

(function ArgumentsEvaluatedBeforeNonSuperclassCtorDetected2() {
  class A {};
  class B extends A {};
  class C extends B {};
  class D1 extends C {
    constructor() {
      super(foo());
    }
  };

  class D2 extends C {
    constructor(...args) {
      super(...args, foo());
    }
  };

  let callCount = 0;
  function foo() {
    ++callCount;
  }

  %PrepareFunctionForOptimization(D1);
  %PrepareFunctionForOptimization(D2);
  new D1();
  new D2();
  %OptimizeFunctionOnNextCall(D1);
  %OptimizeFunctionOnNextCall(D2);
  assertEquals(2, callCount);

  // Install an object which is not a constructor into the class hierarchy.
  C.__proto__ = {};

  assertThrows(() => { new D1(); }, TypeError);
  assertEquals(3, callCount);

  assertThrows(() => { new D2(); }, TypeError);
  assertEquals(4, callCount);
})();

(function EvaluatingArgumentsChangesClassHierarchy() {
  let ctorCallCount = 0;
  class A {};
  class B extends A { constructor() {
    super();
    ++ctorCallCount;
  }};
  class C extends B {};
  class D extends C {
    constructor() {
      super(foo());
    }
  };

  let fooCallCount = 0;
  let changeHierarchy = false;
  function foo() {
    if (changeHierarchy) {
      C.__proto__ = A;
      C.prototype.__proto__ = A.prototype;
    }
    ++fooCallCount;
  }

  %PrepareFunctionForOptimization(D);
  new D();
  assertEquals(1, fooCallCount);
  assertEquals(1, ctorCallCount);
  %OptimizeFunctionOnNextCall(D);
  changeHierarchy = true;

  new D();
  assertEquals(2, fooCallCount);
  assertEquals(1, ctorCallCount);
  assertFalse(isTurboFanned(D));  // Deopt.
})();

// The same test as the previous one, but with a ctor with a non-final spread.
(function EvaluatingArgumentsChangesClassHierarchyThisTimeWithNonFinalSpread() {
  let ctorCallCount = 0;
  class A {};
  class B extends A { constructor() {
    super();
    ++ctorCallCount;
  }};
  class C extends B {};
  class D extends C {
    constructor(...args) {
      super(...args, foo());
    }
  };

  let fooCallCount = 0;
  let changeHierarchy = false;
  function foo() {
    if (changeHierarchy) {
      C.__proto__ = A;
      C.prototype.__proto__ = A.prototype;
    }
    ++fooCallCount;
  }

  %PrepareFunctionForOptimization(D);
  new D();
  assertEquals(1, fooCallCount);
  assertEquals(1, ctorCallCount);
  %OptimizeFunctionOnNextCall(D);
  changeHierarchy = true;

  new D();
  assertEquals(2, fooCallCount);
  assertEquals(1, ctorCallCount);
  assertFalse(isTurboFanned(D));  // Deopt.
})();

(function BasePrivateField() {
  class A {
   #aBrand = true;
   isA() {
    return #aBrand in this;
   }
  };
  class B extends A {};
  class C1 extends B {};
  class C2 extends B { constructor(...args) { super(1, ...args, 2); }};

  %PrepareFunctionForOptimization(B);
  new B();
  %OptimizeFunctionOnNextCall(B);

  const b = new B();
  assertTrue(b.isA());
  assertOptimized(B);  // No deopt.

  %PrepareFunctionForOptimization(C1);
  new C1();
  %OptimizeFunctionOnNextCall(C1);

  const c1 = new C1();
  assertTrue(c1.isA());
  assertOptimized(C1);  // No deopt.

  %PrepareFunctionForOptimization(C2);
  new C2();
  %OptimizeFunctionOnNextCall(C2);

  const c2 = new C2();
  assertTrue(c2.isA());
  assertOptimized(C2);  // No deopt.
})();

(function DerivedPrivateField() {
  class A {};
  class B extends A {
    #bBrand = true;
    isB() {
     return #bBrand in this;
    }
  };
  class C1 extends B {};
  class C2 extends B { constructor(...args) { super(1, ...args, 2); }};

  %PrepareFunctionForOptimization(C1);
  new C1();
  %OptimizeFunctionOnNextCall(C1);

  const c1 = new C1();
  assertTrue(c1.isB());
  assertOptimized(C1);  // No deopt.

  %PrepareFunctionForOptimization(C2);
  new C2();
  %OptimizeFunctionOnNextCall(C2);

  const c2 = new C2();
  assertTrue(c2.isB());
  assertOptimized(C2);  // No deopt.
})();

(function BasePrivateMethod() {
  class A {
   #m() { return 'private'; }
   callPrivate() {
    return this.#m();
   }
  };
  class B extends A {};
  class C1 extends B {};
  class C2 extends B { constructor(...args) { super(1, ...args, 2); }};

  %PrepareFunctionForOptimization(B);
  new B();
  %OptimizeFunctionOnNextCall(B);

  const b = new B();
  assertEquals('private', b.callPrivate());
  assertOptimized(B);  // No deopt.

  %PrepareFunctionForOptimization(C1);
  new C1();
  %OptimizeFunctionOnNextCall(C1);

  const c1 = new C1();
  assertEquals('private', c1.callPrivate());
  assertOptimized(C1);  // No deopt.

  %PrepareFunctionForOptimization(C2);
  new C2();
  %OptimizeFunctionOnNextCall(C2);

  const c2 = new C2();
  assertEquals('private', c2.callPrivate());
  assertOptimized(C2);  // No deopt.
})();

(function DerivedPrivateMethod() {
  class A {};
  class B extends A {
    #m() { return 'private'; }
    callPrivate() {
     return this.#m();
    }
  };
  class C1 extends B {};
  class C2 extends B { constructor(...args) { super(1, ...args, 2); }};

  %PrepareFunctionForOptimization(C1);
  new C1();
  %OptimizeFunctionOnNextCall(C1);

  const c1 = new C1();
  assertEquals('private', c1.callPrivate());
  assertOptimized(C1);  // No deopt.

  %PrepareFunctionForOptimization(C2);
  new C2();
  %OptimizeFunctionOnNextCall(C2);

  const c2 = new C2();
  assertEquals('private', c2.callPrivate());
  assertOptimized(C2);  // No deopt.
})();

(function BasePrivateGetter() {
  class A {
   get #p() { return 'private'; }
   getPrivate() {
    return this.#p;
   }
  };
  class B extends A {};
  class C1 extends B {};
  class C2 extends B { constructor(...args) { super(1, ...args, 2); }};

  %PrepareFunctionForOptimization(B);
  new B();
  %OptimizeFunctionOnNextCall(B);

  const b = new B();
  assertEquals('private', b.getPrivate());
  assertOptimized(B);  // No deopt.

  %PrepareFunctionForOptimization(C1);
  new C1();
  %OptimizeFunctionOnNextCall(C1);

  const c1 = new C1();
  assertEquals('private', c1.getPrivate());
  assertOptimized(C1);  // No deopt.

  %PrepareFunctionForOptimization(C2);
  new C2();
  %OptimizeFunctionOnNextCall(C2);

  const c2 = new C2();
  assertEquals('private', c2.getPrivate());
  assertOptimized(C2);  // No deopt.
})();

(function DerivedPrivateGetter() {
  class A {};
  class B extends A {
    get #p() { return 'private'; }
    getPrivate() {
     return this.#p;
    }
  };
  class C1 extends B {};
  class C2 extends B { constructor(...args) { super(1, ...args, 2); }};

  %PrepareFunctionForOptimization(C1);
  new C1();
  %OptimizeFunctionOnNextCall(C1);

  const c1 = new C1();
  assertEquals('private', c1.getPrivate());
  assertOptimized(C1);  // No deopt.

  %PrepareFunctionForOptimization(C2);
  new C2();
  %OptimizeFunctionOnNextCall(C2);

  const c2 = new C2();
  assertEquals('private', c2.getPrivate());
  assertOptimized(C2);  // No deopt.
})();

(function BasePrivateSetter() {
  class A {
   set #p(value) { this.secret = value; }
   setPrivate() {
    this.#p = 'private';
   }
  };
  class B extends A {};
  class C1 extends B {};
  class C2 extends B { constructor(...args) { super(1, ...args, 2); }};

  %PrepareFunctionForOptimization(B);
  new B();
  %OptimizeFunctionOnNextCall(B);

  const b = new B();
  b.setPrivate();
  assertEquals('private', b.secret);

  %PrepareFunctionForOptimization(C1);
  new C1();
  %OptimizeFunctionOnNextCall(C1);

  const c1 = new C1();
  c1.setPrivate();
  assertEquals('private', c1.secret);
  assertOptimized(C1);  // No deopt.

  %PrepareFunctionForOptimization(C2);
  new C2();
  %OptimizeFunctionOnNextCall(C2);

  const c2 = new C2();
  c2.setPrivate();
  assertEquals('private', c2.secret);
  assertOptimized(C2);  // No deopt.
})();

(function DerivedPrivateSetter() {
  class A {};
  class B extends A {
    set #p(value) { this.secret = value; }
    setPrivate() {
     this.#p = 'private';
    }
  };
  class C1 extends B {};
  class C2 extends B { constructor(...args) { super(1, ...args, 2); }};

  %PrepareFunctionForOptimization(C1);
  new C1();
  %OptimizeFunctionOnNextCall(C1);

  const c1 = new C1();
  c1.setPrivate();
  assertEquals('private', c1.secret);
  assertOptimized(C1);  // No deopt.

  %PrepareFunctionForOptimization(C2);
  new C2();
  %OptimizeFunctionOnNextCall(C2);

  const c2 = new C2();
  c2.setPrivate();
  assertEquals('private', c2.secret);
  assertOptimized(C2);  // No deopt.
})();

(function BaseClassFields() {
  class A {
   aField = true;
  };
  class B extends A {};
  class C1 extends B {};
  class C2 extends B { constructor(...args) { super(1, ...args, 2); }};

  %PrepareFunctionForOptimization(B);
  new B();
  %OptimizeFunctionOnNextCall(B);

  const b = new B();
  assertTrue(b.aField);

  %PrepareFunctionForOptimization(C1);
  new C1();
  %OptimizeFunctionOnNextCall(C1);

  const c1 = new C1();
  assertTrue(c1.aField);
  assertOptimized(C1);  // No deopt.

  %PrepareFunctionForOptimization(C2);
  new C2();
  %OptimizeFunctionOnNextCall(C2);

  const c2 = new C2();
  assertTrue(c2.aField);
  assertOptimized(C2);  // No deopt.
})();

(function DerivedClassFields() {
  class A {};
  class B extends A {
    bField = true;
  };
  class C1 extends B {};
  class C2 extends B { constructor(...args) { super(1, ...args, 2); }};

  %PrepareFunctionForOptimization(C1);
  new C1();
  %OptimizeFunctionOnNextCall(C1);

  const c1 = new C1();
  assertTrue(c1.bField);
  assertOptimized(C1);  // No deopt.

  %PrepareFunctionForOptimization(C2);
  new C2();
  %OptimizeFunctionOnNextCall(C2);

  const c2 = new C2();
  assertTrue(c2.bField);
  assertOptimized(C2);  // No deopt.
})();

(function SuperInTryCatchDefaultCtor() {
  class A {};
  class B extends A {
    constructor() {
      try {
        super();
      } catch {
        assertUnreachable();
      }
    }
  };

  %PrepareFunctionForOptimization(B);
  new B();
  %OptimizeFunctionOnNextCall(B);

  const b = new B();
  assertSame(B.prototype, b.__proto__);
  assertOptimized(B);  // No deopt.
})();

(function SuperInTryCatchNonDefaultCtor() {
  class A { constructor() {} };
  class B extends A {
    constructor() {
      try {
        super();
      } catch {
        assertUnreachable();
      }
    }
  };

  %PrepareFunctionForOptimization(B);
  new B();
  %OptimizeFunctionOnNextCall(B);

  const b = new B();
  assertSame(B.prototype, b.__proto__);
  assertOptimized(B);  // No deopt.
})();
