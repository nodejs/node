// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --omit-default-ctors

(function OmitDefaultBaseCtor() {
  class A {}  // default base ctor -> will be omitted
  class B extends A {}
  const o = new B();
  assertSame(B.prototype, o.__proto__);
})();

(function OmitDefaultDerivedCtor() {
  class A { constructor() {} }
  class B extends A {}  // default derived ctor -> will be omitted
  class C extends B {}
  const o = new C();
  assertSame(C.prototype, o.__proto__);
})();

(function OmitDefaultBaseAndDerivedCtor() {
  class A {} // default base ctor -> will be omitted
  class B extends A {}  // default derived ctor -> will be omitted
  class C extends B {}
  const o = new C();
  assertSame(C.prototype, o.__proto__);
})();

(function OmitDefaultBaseCtorWithExplicitSuper() {
  class A {}  // default base ctor -> will be omitted
  class B extends A { constructor() { super(); } }
  const o = new B();
  assertSame(B.prototype, o.__proto__);
})();

(function OmitDefaultDerivedCtorWithExplicitSuper() {
  class A { constructor() {} }
  class B extends A {}  // default derived ctor -> will be omitted
  class C extends B { constructor() { super(); } }
  const o = new C();
  assertSame(C.prototype, o.__proto__);
})();

(function OmitDefaultBaseAndDerivedCtorWithExplicitSuper() {
  class A {} // default base ctor -> will be omitted
  class B extends A {}  // default derived ctor -> will be omitted
  class C extends B { constructor() { super(); } }
  const o = new C();
  assertSame(C.prototype, o.__proto__);
})();

(function OmitDefaultBaseCtorWithExplicitSuperAndNonFinalSpread() {
  class A {}  // default base ctor -> will be omitted
  class B extends A { constructor(...args) { super(1, ...args, 2); } }
  const o = new B(3, 4);
  assertSame(B.prototype, o.__proto__);
})();

(function OmitDefaultDerivedCtorWithExplicitSuperAndNonFinalSpread() {
  class A { constructor() {} }
  class B extends A {}  // default derived ctor -> will be omitted
  class C extends B { constructor(...args) { super(1, ...args, 2); } }
  const o = new C(3, 4);
  assertSame(C.prototype, o.__proto__);
})();

(function OmitDefaultBaseAndDerivedCtorWithExplicitSuperAndNonFinalSpread() {
  class A {} // default base ctor -> will be omitted
  class B extends A {}  // default derived ctor -> will be omitted
  class C extends B { constructor(...args) { super(1, ...args, 2); } }
  const o = new C(3, 4);
  assertSame(C.prototype, o.__proto__);
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
  }
  // Nothing will be omitted.
  class A extends Base {}
  const a = new A(1, 2, 3);
  assertEquals(1, ctorCallCount);
  assertEquals([1, 2, 3], lastArgs);
  assertTrue(a.baseTagged);

  // 'A' default ctor will be omitted.
  class B1 extends A {}
  const b1 = new B1(4, 5, 6);
  assertEquals(2, ctorCallCount);
  assertEquals([4, 5, 6], lastArgs);
  assertTrue(b1.baseTagged);

  // The same test with non-final spread; 'A' default ctor will be omitted.
  class B2 extends A {
    constructor(...args) { super(1, ...args, 2); }
  }
  const b2 = new B2(4, 5, 6);
  assertEquals(3, ctorCallCount);
  assertEquals([1, 4, 5, 6, 2], lastArgs);
  assertTrue(b2.baseTagged);
})();

(function NonDefaultDerivedConstructorCalled() {
  let ctorCallCount = 0;
  let lastArgs;
  class Base {}
  class Derived extends Base {
    constructor(...args) {
      super();
      ++ctorCallCount;
      this.derivedTagged = true;
      lastArgs = args;
    }
  }
  // Nothing will be omitted.
  class A extends Derived {}
  const a = new A(1, 2, 3);
  assertEquals(1, ctorCallCount);
  assertEquals([1, 2, 3], lastArgs);
  assertTrue(a.derivedTagged);

  // 'A' default ctor will be omitted.
  class B1 extends A {}
  const b1 = new B1(4, 5, 6);
  assertEquals(2, ctorCallCount);
  assertEquals([4, 5, 6], lastArgs);
  assertTrue(b1.derivedTagged);

  // The same test with non-final spread. 'A' default ctor will be omitted.
  class B2 extends A {
    constructor(...args) { super(1, ...args, 2); }
  }
  const b2 = new B2(4, 5, 6);
  assertEquals(3, ctorCallCount);
  assertEquals([1, 4, 5, 6, 2], lastArgs);
  assertTrue(b2.derivedTagged);
})();

(function BaseFunctionCalled() {
  let baseFunctionCallCount = 0;
  function BaseFunction() {
    ++baseFunctionCallCount;
    this.baseTagged = true;
  }

  class A1 extends BaseFunction {}
  const a1 = new A1();
  assertEquals(1, baseFunctionCallCount);
  assertTrue(a1.baseTagged);

  class A2 extends BaseFunction {
    constructor(...args) { super(1, ...args, 2); }
  }
  const a2 = new A2();
  assertEquals(2, baseFunctionCallCount);
  assertTrue(a2.baseTagged);
})();

(function NonSuperclassCtor() {
  class A {};
  class B extends A {};
  class C extends B {};
  class D1 extends C {};
  class D2 extends C { constructor(...args) { super(1, ...args, 2); }}

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
  class D2 extends C { constructor(...args) { super(1, ...args, 2); }}

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

  // Install an object which is not a constructor into the class hierarchy.
  C.__proto__ = {};

  let callCount = 0;
  function foo() {
    ++callCount;
  }

  assertThrows(() => { new D1(); }, TypeError);
  assertEquals(1, callCount);

  assertThrows(() => { new D2(); }, TypeError);
  assertEquals(2, callCount);
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
  function foo() {
    C.__proto__ = A;
    C.prototype.__proto__ = A.prototype;
    ++fooCallCount;
  }

  new D();
  assertEquals(1, fooCallCount);
  assertEquals(0, ctorCallCount);
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
  function foo() {
    C.__proto__ = A;
    C.prototype.__proto__ = A.prototype;
    ++fooCallCount;
  }

  new D();
  assertEquals(1, fooCallCount);
  assertEquals(0, ctorCallCount);
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

  const b = new B();
  assertTrue(b.isA());

  const c1 = new C1();
  assertTrue(c1.isA());

  const c2 = new C2();
  assertTrue(c2.isA());
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

  const c1 = new C1();
  assertTrue(c1.isB());

  const c2 = new C2();
  assertTrue(c2.isB());
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

  const b = new B();
  assertEquals('private', b.callPrivate());

  const c1 = new C1();
  assertEquals('private', c1.callPrivate());

  const c2 = new C2();
  assertEquals('private', c2.callPrivate());
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

  const c1 = new C1();
  assertEquals('private', c1.callPrivate());

  const c2 = new C2();
  assertEquals('private', c2.callPrivate());
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

  const b = new B();
  assertEquals('private', b.getPrivate());

  const c1 = new C1();
  assertEquals('private', c1.getPrivate());

  const c2 = new C2();
  assertEquals('private', c2.getPrivate());
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

  const c1 = new C1();
  assertEquals('private', c1.getPrivate());

  const c2 = new C2();
  assertEquals('private', c2.getPrivate());
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

  const b = new B();
  b.setPrivate();
  assertEquals('private', b.secret);

  const c1 = new C1();
  c1.setPrivate();
  assertEquals('private', c1.secret);

  const c2 = new C2();
  c2.setPrivate();
  assertEquals('private', c2.secret);
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

  const c1 = new C1();
  c1.setPrivate();
  assertEquals('private', c1.secret);

  const c2 = new C2();
  c2.setPrivate();
  assertEquals('private', c2.secret);
})();

(function BaseClassFields() {
  class A {
   aField = true;
  };
  class B extends A {};
  class C1 extends B {};
  class C2 extends B { constructor(...args) { super(1, ...args, 2); }};

  const b = new B();
  assertTrue(b.aField);

  const c1 = new C1();
  assertTrue(c1.aField);

  const c2 = new C2();
  assertTrue(c2.aField);
})();

(function DerivedClassFields() {
  class A {};
  class B extends A {
    bField = true;
  };
  class C1 extends B {};
  class C2 extends B { constructor(...args) { super(1, ...args, 2); }};

  const c1 = new C1();
  assertTrue(c1.bField);

  const c2 = new C2();
  assertTrue(c2.bField);
})();
