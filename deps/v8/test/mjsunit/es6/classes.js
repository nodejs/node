// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function TestBasics() {
  var C = class C {}
  assertEquals(typeof C, 'function');
  assertEquals(C.__proto__, Function.prototype);
  assertEquals(Object.prototype, Object.getPrototypeOf(C.prototype));
  assertEquals(Function.prototype, Object.getPrototypeOf(C));
  assertEquals('C', C.name);

  class D {}
  assertEquals(typeof D, 'function');
  assertEquals(D.__proto__, Function.prototype);
  assertEquals(Object.prototype, Object.getPrototypeOf(D.prototype));
  assertEquals(Function.prototype, Object.getPrototypeOf(D));
  assertEquals('D', D.name);

  class D2 { constructor() {} }
  assertEquals('D2', D2.name);

  var E = class {}
  assertEquals('E', E.name);

  var F = class { constructor() {} };
  assertEquals('F', F.name);

  var literal = { E: class {} };
  assertEquals('E', literal.E.name);

  literal = { E: class F {} };
  assertEquals('F', literal.E.name);

  literal = { __proto__: class {} };
  assertEquals('', literal.__proto__.name);
  assertEquals(
      undefined, Object.getOwnPropertyDescriptor(literal.__proto__, 'name'));

  literal = { __proto__: class F {} };
  assertEquals('F', literal.__proto__.name);
  assertNotEquals(
      undefined, Object.getOwnPropertyDescriptor(literal.__proto__, 'name'));

  class G {};
  literal = { __proto__: G };
  assertEquals('G', literal.__proto__.name);

  var H = class { static name() { return 'A'; } };
  literal = { __proto__ : H };
  assertEquals('A', literal.__proto__.name());

  literal = {
    __proto__: class {
      static name() { return 'A'; }
    }
  };
  assertEquals('A', literal.__proto__.name());
})();


(function TestBasicsExtends() {
  class C extends null {}
  assertEquals(typeof C, 'function');
  assertEquals(C.__proto__, Function.prototype);
  assertEquals(null, Object.getPrototypeOf(C.prototype));

  class D extends C {}
  assertEquals(typeof D, 'function');
  assertEquals(D.__proto__, C);
  assertEquals(C.prototype, Object.getPrototypeOf(D.prototype));
})();


(function TestSideEffectInExtends() {
  var calls = 0;
  class C {}
  class D extends (calls++, C) {}
  assertEquals(1, calls);
  assertEquals(typeof D, 'function');
  assertEquals(D.__proto__, C);
  assertEquals(C.prototype, Object.getPrototypeOf(D.prototype));
})();


(function TestInvalidExtends() {
  assertThrows(function() {
    class C extends 42 {}
  }, TypeError);

  assertThrows(function() {
    // Function but its .prototype is not null or a function.
    class C extends Math.abs {}
  }, TypeError);

  assertThrows(function() {
    Math.abs.prototype = 42;
    class C extends Math.abs {}
  }, TypeError);
  delete Math.abs.prototype;

  assertThrows(function() {
    function* g() {}
    class C extends g {}
  }, TypeError);
})();


(function TestConstructorProperty() {
  class C {}
  assertEquals(C, C.prototype.constructor);
  var descr = Object.getOwnPropertyDescriptor(C.prototype, 'constructor');
  assertTrue(descr.configurable);
  assertFalse(descr.enumerable);
  assertTrue(descr.writable);
})();


(function TestPrototypeProperty() {
  class C {}
  var descr = Object.getOwnPropertyDescriptor(C, 'prototype');
  assertFalse(descr.configurable);
  assertFalse(descr.enumerable);
  assertFalse(descr.writable);
})();


(function TestConstructor() {
  var count = 0;
  class C {
    constructor() {
      assertEquals(Object.getPrototypeOf(this), C.prototype);
      count++;
    }
  }
  assertEquals(C, C.prototype.constructor);
  var descr = Object.getOwnPropertyDescriptor(C.prototype, 'constructor');
  assertTrue(descr.configurable);
  assertFalse(descr.enumerable);
  assertTrue(descr.writable);

  var c = new C();
  assertEquals(1, count);
  assertEquals(Object.getPrototypeOf(c), C.prototype);
})();


(function TestImplicitConstructor() {
  class C {}
  var c = new C();
  assertEquals(Object.getPrototypeOf(c), C.prototype);
})();


(function TestConstructorStrict() {
  class C {
    constructor() {
      assertThrows(function() {
        nonExistingBinding = 42;
      }, ReferenceError);
    }
  }
  new C();
})();


(function TestSuperInConstructor() {
  var calls = 0;
  class B {}
  B.prototype.x = 42;

  class C extends B {
    constructor() {
      super();
      calls++;
      assertEquals(42, super.x);
    }
  }

  new C;
  assertEquals(1, calls);
})();


(function TestStrictMode() {
  class C {}

  with ({a: 1}) {
    assertEquals(1, a);
  }

  assertThrows('class C extends function B() { with ({}); return B; }() {}',
               SyntaxError);

  var D = class extends function() {
    this.args = arguments;
  } {};
  assertThrows(function() {
    Object.getPrototypeOf(D).arguments;
  }, TypeError);
  var e = new D();
  assertThrows(() => e.args.callee, TypeError);
  assertEquals(undefined, Object.getOwnPropertyDescriptor(e.args, 'caller'));
  assertFalse('caller' in e.args);
})();


(function TestToString() {
  class C {}
  assertEquals('class C {}', C.toString());

  class D { constructor() { 42; } }
  assertEquals('class D { constructor() { 42; } }', D.toString());

  class E { x() { 42; } }
  assertEquals('class E { x() { 42; } }', E.toString());
})();


function assertMethodDescriptor(object, name) {
  var descr = Object.getOwnPropertyDescriptor(object, name);
  assertTrue(descr.configurable);
  assertFalse(descr.enumerable);
  assertTrue(descr.writable);
  assertEquals('function', typeof descr.value);
  assertFalse('prototype' in descr.value);
  assertEquals(name, descr.value.name);
}


function assertGetterDescriptor(object, name) {
  var descr = Object.getOwnPropertyDescriptor(object, name);
  assertTrue(descr.configurable);
  assertFalse(descr.enumerable);
  assertEquals('function', typeof descr.get);
  assertFalse('prototype' in descr.get);
  assertEquals(undefined, descr.set);
  assertEquals("get " + name, descr.get.name);
}


function assertSetterDescriptor(object, name) {
  var descr = Object.getOwnPropertyDescriptor(object, name);
  assertTrue(descr.configurable);
  assertFalse(descr.enumerable);
  assertEquals(undefined, descr.get);
  assertEquals('function', typeof descr.set);
  assertFalse('prototype' in descr.set);
  assertEquals("set " + name, descr.set.name);
}


function assertAccessorDescriptor(object, name) {
  var descr = Object.getOwnPropertyDescriptor(object, name);
  assertTrue(descr.configurable);
  assertFalse(descr.enumerable);
  assertEquals('function', typeof descr.get);
  assertEquals('function', typeof descr.set);
  assertFalse('prototype' in descr.get);
  assertFalse('prototype' in descr.set);
  assertEquals("get " + name, descr.get.name);
  assertEquals("set " + name, descr.set.name);
}


(function TestMethods() {
  class C {
    method() { return 1; }
    static staticMethod() { return 2; }
    method2() { return 3; }
    static staticMethod2() { return 4; }
  }

  assertMethodDescriptor(C.prototype, 'method');
  assertMethodDescriptor(C.prototype, 'method2');
  assertMethodDescriptor(C, 'staticMethod');
  assertMethodDescriptor(C, 'staticMethod2');

  assertEquals(1, new C().method());
  assertEquals(2, C.staticMethod());
  assertEquals(3, new C().method2());
  assertEquals(4, C.staticMethod2());
})();


(function TestGetters() {
  class C {
    get x() { return 1; }
    static get staticX() { return 2; }
    get y() { return 3; }
    static get staticY() { return 4; }
  }

  assertGetterDescriptor(C.prototype, 'x');
  assertGetterDescriptor(C.prototype, 'y');
  assertGetterDescriptor(C, 'staticX');
  assertGetterDescriptor(C, 'staticY');

  assertEquals(1, new C().x);
  assertEquals(2, C.staticX);
  assertEquals(3, new C().y);
  assertEquals(4, C.staticY);
})();



(function TestSetters() {
  var x, staticX, y, staticY;
  class C {
    set x(v) { x = v; }
    static set staticX(v) { staticX = v; }
    set y(v) { y = v; }
    static set staticY(v) { staticY = v; }
  }

  assertSetterDescriptor(C.prototype, 'x');
  assertSetterDescriptor(C.prototype, 'y');
  assertSetterDescriptor(C, 'staticX');
  assertSetterDescriptor(C, 'staticY');

  assertEquals(1, new C().x = 1);
  assertEquals(1, x);
  assertEquals(2, C.staticX = 2);
  assertEquals(2, staticX);
  assertEquals(3, new C().y = 3);
  assertEquals(3, y);
  assertEquals(4, C.staticY = 4);
  assertEquals(4, staticY);
})();


(function TestSideEffectsInPropertyDefine() {
  function B() {}
  B.prototype = {
    constructor: B,
    set m(v) {
      throw Error();
    }
  };

  class C extends B {
    m() { return 1; }
  }

  assertEquals(1, new C().m());
})();


(function TestAccessors() {
  class C {
    constructor(x) {
      this._x = x;
    }

    get x() { return this._x; }
    set x(v) { this._x = v; }

    static get staticX() { return this._x; }
    static set staticX(v) { this._x = v; }
  }

  assertAccessorDescriptor(C.prototype, 'x');
  assertAccessorDescriptor(C, 'staticX');

  var c = new C(1);
  c._x = 1;
  assertEquals(1, c.x);
  c.x = 2;
  assertEquals(2, c._x);

  C._x = 3;
  assertEquals(3, C.staticX);
  C._x = 4;
  assertEquals(4, C.staticX );
})();


(function TestProto() {
  class C {
    __proto__() { return 1; }
  }
  assertMethodDescriptor(C.prototype, '__proto__');
  assertEquals(1, new C().__proto__());
})();


(function TestProtoStatic() {
  class C {
    static __proto__() { return 1; }
  }
  assertMethodDescriptor(C, '__proto__');
  assertEquals(1, C.__proto__());
})();


(function TestProtoAccessor() {
  class C {
    get __proto__() { return this._p; }
    set __proto__(v) { this._p = v; }
  }
  assertAccessorDescriptor(C.prototype, '__proto__');
  var c = new C();
  c._p = 1;
  assertEquals(1, c.__proto__);
  c.__proto__ = 2;
  assertEquals(2, c.__proto__);
})();


(function TestStaticProtoAccessor() {
  class C {
    static get __proto__() { return this._p; }
    static set __proto__(v) { this._p = v; }
  }
  assertAccessorDescriptor(C, '__proto__');
  C._p = 1;
  assertEquals(1, C.__proto__);
  C.__proto__ = 2;
  assertEquals(2, C.__proto__);
})();


(function TestSettersOnProto() {
  function Base() {}
  Base.prototype = {
    set constructor(_) {
      assertUnreachable();
    },
    set m(_) {
      assertUnreachable();
    }
  };
  Object.defineProperty(Base, 'staticM', {
    set: function() {
      assertUnreachable();
    }
  });

  class C extends Base {
    m() {
      return 1;
    }
    static staticM() {
      return 2;
    }
  }

  assertEquals(1, new C().m());
  assertEquals(2, C.staticM());
})();


(function TestConstructableButNoPrototype() {
  var Base = function() {}.bind();
  assertThrows(function() {
    class C extends Base {}
  }, TypeError);
})();


(function TestPrototypeGetter() {
  var calls = 0;
  var Base = function() {}.bind();
  Object.defineProperty(Base, 'prototype', {
    get: function() {
      calls++;
      return null;
    },
    configurable: true
  });
  class C extends Base {}
  assertEquals(1, calls);

  calls = 0;
  Object.defineProperty(Base, 'prototype', {
    get: function() {
      calls++;
      return 42;
    },
    configurable: true
  });
  assertThrows(function() {
    class C extends Base {}
  }, TypeError);
  assertEquals(1, calls);
})();


(function TestPrototypeSetter() {
  var Base = function() {}.bind();
  Object.defineProperty(Base, 'prototype', {
    set: function() {
      assertUnreachable();
    }
  });
  assertThrows(function() {
    class C extends Base {}
  }, TypeError);
})();


(function TestSuperInMethods() {
  class B {
    method() {
      return 1;
    }
    get x() {
      return 2;
    }
  }
  class C extends B {
    method() {
      assertEquals(2, super.x);
      return super.method();
    }
  }
  assertEquals(1, new C().method());
})();


(function TestSuperInGetter() {
  class B {
    method() {
      return 1;
    }
    get x() {
      return 2;
    }
  }
  class C extends B {
    get y() {
      assertEquals(2, super.x);
      return super.method();
    }
  }
  assertEquals(1, new C().y);
})();


(function TestSuperInSetter() {
  class B {
    method() {
      return 1;
    }
    get x() {
      return 2;
    }
  }
  class C extends B {
    set y(v) {
      assertEquals(3, v);
      assertEquals(2, super.x);
      assertEquals(1, super.method());
    }
  }
  assertEquals(3, new C().y = 3);
})();


(function TestSuperInStaticMethods() {
  class B {
    static method() {
      return 1;
    }
    static get x() {
      return 2;
    }
  }
  class C extends B {
    static method() {
      assertEquals(2, super.x);
      return super.method();
    }
  }
  assertEquals(1, C.method());
})();


(function TestSuperInStaticGetter() {
  class B {
    static method() {
      return 1;
    }
    static get x() {
      return 2;
    }
  }
  class C extends B {
    static get x() {
      assertEquals(2, super.x);
      return super.method();
    }
  }
  assertEquals(1, C.x);
})();


(function TestSuperInStaticSetter() {
  class B {
    static method() {
      return 1;
    }
    static get x() {
      return 2;
    }
  }
  class C extends B {
    static set x(v) {
      assertEquals(3, v);
      assertEquals(2, super.x);
      assertEquals(1, super.method());
    }
  }
  assertEquals(3, C.x = 3);
})();


(function TestNumericPropertyNames() {
  class B {
    1() { return 1; }
    get 2() { return 2; }
    set 3(_) {}

    static 4() { return 4; }
    static get 5() { return 5; }
    static set 6(_) {}

    2147483649() { return 2147483649; }
    get 2147483650() { return 2147483650; }
    set 2147483651(_) {}

    static 2147483652() { return 2147483652; }
    static get 2147483653() { return 2147483653; }
    static set 2147483654(_) {}

    4294967294() { return 4294967294; }
    4294967295() { return 4294967295; }
    static 4294967294() { return 4294967294; }
    static 4294967295() { return 4294967295; }
  }

  assertMethodDescriptor(B.prototype, '1');
  assertGetterDescriptor(B.prototype, '2');
  assertSetterDescriptor(B.prototype, '3');
  assertMethodDescriptor(B.prototype, '2147483649');
  assertGetterDescriptor(B.prototype, '2147483650');
  assertSetterDescriptor(B.prototype, '2147483651');
  assertMethodDescriptor(B.prototype, '4294967294');
  assertMethodDescriptor(B.prototype, '4294967295');

  assertMethodDescriptor(B, '4');
  assertGetterDescriptor(B, '5');
  assertSetterDescriptor(B, '6');
  assertMethodDescriptor(B, '2147483652');
  assertGetterDescriptor(B, '2147483653');
  assertSetterDescriptor(B, '2147483654');
  assertMethodDescriptor(B, '4294967294');
  assertMethodDescriptor(B, '4294967295');

  class C extends B {
    1() { return super[1](); }
    get 2() { return super[2]; }

    static 4() { return super[4](); }
    static get 5() { return super[5]; }

    2147483649() { return super[2147483649](); }
    get 2147483650() { return super[2147483650]; }

    static 2147483652() { return super[2147483652](); }
    static get 2147483653() { return super[2147483653]; }

  }

  assertEquals(1, new C()[1]());
  assertEquals(2, new C()[2]);
  assertEquals(2147483649, new C()[2147483649]());
  assertEquals(2147483650, new C()[2147483650]);
  assertEquals(4, C[4]());
  assertEquals(5, C[5]);
  assertEquals(2147483652, C[2147483652]());
  assertEquals(2147483653, C[2147483653]);
})();


(function TestDefaultConstructorNoCrash() {
  // Regression test for https://code.google.com/p/v8/issues/detail?id=3661
  class C {}
  assertThrows(function () {C();}, TypeError);
  assertThrows(function () {C(1);}, TypeError);
  assertTrue(new C() instanceof C);
  assertTrue(new C(1) instanceof C);
})();


(function TestConstructorCall(){
  var realmIndex = Realm.create();
  var otherTypeError = Realm.eval(realmIndex, "TypeError");
  var A = Realm.eval(realmIndex, '"use strict"; class A {}; A');
  var instance = new A();
  var constructor = instance.constructor;
  var otherTypeError = Realm.eval(realmIndex, 'TypeError');
  if (otherTypeError === TypeError) {
    throw Error('Should not happen!');
  }

  // ES6 9.2.1[[Call]] throws a TypeError in the caller context/Realm when the
  // called function is a classConstructor
  assertThrows(function() { Realm.eval(realmIndex, "A()") }, otherTypeError);
  assertThrows(function() { instance.constructor() }, TypeError);
  assertThrows(function() { A() }, TypeError);

  // ES6 9.3.1 call() first activates the callee context before invoking the
  // method. The TypeError from the constructor is thus thrown in the other
  // Realm.
  assertThrows(function() { Realm.eval(realmIndex, "A.call()") },
      otherTypeError);
  assertThrows(function() { constructor.call() }, otherTypeError);
  assertThrows(function() { A.call() }, otherTypeError);
})();


(function TestConstructorCallOptimized() {
  class A { };

  function invoke_constructor() { A() }
  function call_constructor() { A.call() }
  function apply_constructor() { A.apply() }
  %PrepareFunctionForOptimization(invoke_constructor);
  %PrepareFunctionForOptimization(call_constructor);
  %PrepareFunctionForOptimization(apply_constructor);

  for (var i=0; i<3; i++) {
    assertThrows(invoke_constructor);
    assertThrows(call_constructor);
    assertThrows(apply_constructor);
  }
  // Make sure we still check for class constructors when calling optimized
  // code.
  %OptimizeFunctionOnNextCall(invoke_constructor);
  assertThrows(invoke_constructor);
  %OptimizeFunctionOnNextCall(call_constructor);
  assertThrows(call_constructor);
  %OptimizeFunctionOnNextCall(apply_constructor);
  assertThrows(apply_constructor);
})();


(function TestDefaultConstructor() {
  var calls = 0;
  class Base {
    constructor() {
      calls++;
    }
  }
  class Derived extends Base {}
  var object = new Derived;
  assertEquals(1, calls);

  calls = 0;
  assertThrows(function() { Derived(); }, TypeError);
  assertEquals(0, calls);
})();


(function TestDefaultConstructorArguments() {
  var args, self;
  class Base {
    constructor() {
      self = this;
      args = arguments;
    }
  }
  class Derived extends Base {}

  new Derived;
  assertEquals(0, args.length);

  new Derived(0, 1, 2);
  assertEquals(3, args.length);
  assertTrue(self instanceof Derived);

  var arr = new Array(100);
  var obj = {};
  assertThrows(function() {Derived.apply(obj, arr);}, TypeError);
})();


(function TestDefaultConstructorArguments2() {
  var args;
  class Base {
    constructor(x, y) {
      args = arguments;
    }
  }
  class Derived extends Base {}

  new Derived;
  assertEquals(0, args.length);

  new Derived(1);
  assertEquals(1, args.length);
  assertEquals(1, args[0]);

  new Derived(1, 2, 3);
  assertEquals(3, args.length);
  assertEquals(1, args[0]);
  assertEquals(2, args[1]);
  assertEquals(3, args[2]);
})();


(function TestNameBindingConst() {
  assertThrows('class C { constructor() { C = 42; } }; new C();', TypeError);
  assertThrows('new (class C { constructor() { C = 42; } })', TypeError);
  assertThrows('class C { m() { C = 42; } }; new C().m()', TypeError);
  assertThrows('new (class C { m() { C = 42; } }).m()', TypeError);
  assertThrows('class C { get x() { C = 42; } }; new C().x', TypeError);
  assertThrows('(new (class C { get x() { C = 42; } })).x', TypeError);
  assertThrows('class C { set x(_) { C = 42; } }; new C().x = 15;', TypeError);
  assertThrows('(new (class C { set x(_) { C = 42; } })).x = 15;', TypeError);
})();


(function TestNameBinding() {
  var C2;
  class C {
    constructor() {
      C2 = C;
    }
    m() {
      C2 = C;
    }
    get x() {
      C2 = C;
    }
    set x(_) {
      C2 = C;
    }
  }
  new C();
  assertEquals(C, C2);

  C2 = undefined;
  new C().m();
  assertEquals(C, C2);

  C2 = undefined;
  new C().x;
  assertEquals(C, C2);

  C2 = undefined;
  new C().x = 1;
  assertEquals(C, C2);
})();


(function TestNameBindingExpression() {
  var C3;
  var C = class C2 {
    constructor() {
      assertEquals(C2, C);
      C3 = C2;
    }
    m() {
      assertEquals(C2, C);
      C3 = C2;
    }
    get x() {
      assertEquals(C2, C);
      C3 = C2;
    }
    set x(_) {
      assertEquals(C2, C);
      C3 = C2;
    }
  }
  new C();
  assertEquals(C, C3);

  C3 = undefined;
  new C().m();
  assertEquals(C, C3);

  C3 = undefined;
  new C().x;
  assertEquals(C, C3);

  C3 = undefined;
  new C().x = 1;
  assertEquals(C, C3);
})();


(function TestNameBindingInExtendsExpression() {
  assertThrows(function() {
    class x extends x {}
  }, ReferenceError);

  assertThrows(function() {
    (class x extends x {});
  }, ReferenceError);

  assertThrows(function() {
    var x = (class x extends x {});
  }, ReferenceError);
})();


(function TestThisAccessRestriction() {
  class Base {}
  (function() {
    class C extends Base {
      constructor() {
        var y;
        super();
      }
    }; new C();
  }());
  assertThrows(function() {
    class C extends Base {
      constructor() {
        super(this.x);
      }
    }; new C();
  }, ReferenceError);
  assertThrows(function() {
    class C extends Base {
      constructor() {
        super(this);
      }
    }; new C();
  }, ReferenceError);
  assertThrows(function() {
    class C extends Base {
      constructor() {
        super.method();
        super(this);
      }
    }; new C();
  }, ReferenceError);
  assertThrows(function() {
    class C extends Base {
      constructor() {
        super(super.method());
      }
    }; new C();
  }, ReferenceError);
  assertThrows(function() {
    class C extends Base {
      constructor() {
        super(super());
      }
    }; new C();
  }, ReferenceError);
  assertThrows(function() {
    class C extends Base {
      constructor() {
        super(1, 2, Object.getPrototypeOf(this));
      }
    }; new C();
  }, ReferenceError);
  (function() {
    class C extends Base {
      constructor() {
        { super(1, 2); }
      }
    }; new C();
  }());
  (function() {
    class C extends Base {
      constructor() {
        if (1) super();
      }
    }; new C();
  }());

  class C1 extends Object {
    constructor() {
      'use strict';
      super();
    }
  };
  new C1();

  class C2 extends Object {
    constructor() {
      ; 'use strict';;;;;
      super();
    }
  };
  new C2();

  class C3 extends Object {
    constructor() {
      ; 'use strict';;;;;
      // This is a comment.
      super();
    }
  };
  new C3();
}());


function testClassRestrictedProperties(C) {
  assertEquals(false, C.hasOwnProperty("arguments"));
  assertThrows(function() { return C.arguments; }, TypeError);
  assertThrows(function() { C.arguments = {}; }, TypeError);

  assertEquals(false, C.hasOwnProperty("caller"));
  assertThrows(function() { return C.caller; }, TypeError);
  assertThrows(function() { C.caller = {}; }, TypeError);

  assertEquals(false, (new C).method.hasOwnProperty("arguments"));
  assertThrows(function() { return new C().method.arguments; }, TypeError);
  assertThrows(function() { new C().method.arguments = {}; }, TypeError);

  assertEquals(false, (new C).method.hasOwnProperty("caller"));
  assertThrows(function() { return new C().method.caller; }, TypeError);
  assertThrows(function() { new C().method.caller = {}; }, TypeError);
}


(function testRestrictedPropertiesStrict() {
  "use strict";
  class ClassWithDefaultConstructor {
    method() {}
  }
  class Class {
    constructor() {}
    method() {}
  }
  class DerivedClassWithDefaultConstructor extends Class {}
  class DerivedClass extends Class { constructor() { super(); } }

  testClassRestrictedProperties(ClassWithDefaultConstructor);
  testClassRestrictedProperties(Class);
  testClassRestrictedProperties(DerivedClassWithDefaultConstructor);
  testClassRestrictedProperties(DerivedClass);
  testClassRestrictedProperties(class { method() {} });
  testClassRestrictedProperties(class { constructor() {} method() {} });
  testClassRestrictedProperties(class extends Class { });
  testClassRestrictedProperties(
      class extends Class { constructor() { super(); } });
})();


(function testRestrictedPropertiesSloppy() {
  class ClassWithDefaultConstructor {
    method() {}
  }
  class Class {
    constructor() {}
    method() {}
  }
  class DerivedClassWithDefaultConstructor extends Class {}
  class DerivedClass extends Class { constructor() { super(); } }

  testClassRestrictedProperties(ClassWithDefaultConstructor);
  testClassRestrictedProperties(Class);
  testClassRestrictedProperties(DerivedClassWithDefaultConstructor);
  testClassRestrictedProperties(DerivedClass);
  testClassRestrictedProperties(class { method() {} });
  testClassRestrictedProperties(class { constructor() {} method() {} });
  testClassRestrictedProperties(class extends Class { });
  testClassRestrictedProperties(
      class extends Class { constructor() { super(); } });
})();


(function testReturnFromClassLiteral() {

  function usingYieldInBody() {
    function* foo() {
      class C {
        [yield]() {}
      }
    }
    var g = foo();
    g.next();
    return g.return(42).value;
  }
  assertEquals(42, usingYieldInBody());

  function usingYieldInExtends() {
    function* foo() {
      class C extends (yield) {};
    }
    var g = foo();
    g.next();
    return g.return(42).value;
  }
  assertEquals(42, usingYieldInExtends());

})();


(function testLargeClassesMethods() {
  const kLimit = 2000;
  let evalString = "(function(i) { " +
      "let clazz = class { " +
      "   constructor(i) { this.value = i; } ";
  for (let i = 0; i < 2000; i++) {
    evalString  += "property"+i+"() { return "+i+"; }; "
  }
  evalString += "};" +
      " return new clazz(i); })";

  let fn = eval(evalString);
  %PrepareFunctionForOptimization(fn);
  assertEquals(fn(1).value, 1);
  assertEquals(fn(2).value, 2);
  assertEquals(fn(3).value, 3);
  %OptimizeFunctionOnNextCall(fn);
  assertEquals(fn(4).value, 4);

  let instance = fn(1);
  assertEquals(Object.getOwnPropertyNames(instance).length, 1);
  assertEquals(Object.getOwnPropertyNames(instance.__proto__).length,
      kLimit + 1);

  // Call all instance functions.
  for (let i = 0; i < kLimit; i++) {
    const key = "property" + i;
    assertEquals(instance[key](), i);
  }
})();


(function testLargeClassesStaticMethods() {
  const kLimit = 2000;
  let evalString = "(function(i) { " +
      "let clazz = class { " +
      "   constructor(i) { this.value = i; } ";
  for (let i = 0; i < kLimit; i++) {
    evalString  += "static property"+i+"() { return "+i+" }; "
  }
  evalString += "};" +
      " return new clazz(i); })";

  let fn = eval(evalString);

  %PrepareFunctionForOptimization(fn);
  assertEquals(fn(1).value, 1);
  assertEquals(fn(2).value, 2);
  assertEquals(fn(3).value, 3);
  %OptimizeFunctionOnNextCall(fn);
  assertEquals(fn(4).value, 4);

  let instance = fn(1);
  assertEquals(Object.getOwnPropertyNames(instance).length, 1);
  assertEquals(instance.value, 1);
  instance.value = 10;
  assertEquals(instance.value, 10);

  // kLimit + nof default properties (length, prototype, name).
  assertEquals(Object.getOwnPropertyNames(instance.constructor).length,
      kLimit + 3);

  // Call all static properties.
  for (let i = 0; i < kLimit; i++) {
    const key = "property" + i;
    assertEquals(instance.constructor[key](), i);
  }
})();


(function testLargeClassesProperties(){
  const kLimit = 2000;
  let evalString = "(function(i) { " +
      "let clazz = class { " +
      "   constructor(i) { this.value = i;";
  for (let i = 0; i < kLimit ; i++) {
    evalString  += "this.property"+i +" = "+i+"; "
  }
  evalString += "}};" +
      " return (new clazz(i)); })";

  let fn = eval(evalString);
  %PrepareFunctionForOptimization(fn);
  assertEquals(fn(1).value, 1);
  assertEquals(fn(2).value, 2);
  assertEquals(fn(3).value, 3);
  %OptimizeFunctionOnNextCall(fn);
  assertEquals(fn(4).value, 4);

  let instance = fn(1);
  assertEquals(Object.getOwnPropertyNames(instance).length, kLimit+1);

  // Get and set all properties.
  for (let i = 0; i < kLimit; i++) {
    const key = "property" + i;
    assertEquals(instance[key], i);
    const value = "value"+i;
    instance[key] = value;
    assertEquals(instance[key], value);
  }
})();

var b = 'b';

(function TestOverwritingInstanceAccessors() {
  var C, desc;
  C = class {
    [b]() { return 'B'; };
    get b() { return 'get B'; };
  };
  desc = Object.getOwnPropertyDescriptor(C.prototype, 'b');
  assertFalse(desc.enumerable);
  assertTrue(desc.configurable);
  assertEquals('get B', desc.get());
  assertEquals(undefined, desc.set);

  C = class {
    [b]() { return 'B'; };
    set b(v) { return 'set B'; };
  };
  desc = Object.getOwnPropertyDescriptor(C.prototype, 'b');
  assertFalse(desc.enumerable);
  assertTrue(desc.configurable);
  assertEquals(undefined, desc.get);
  assertEquals('set B', desc.set());

  C = class {
    set b(v) { return 'get B'; };
    [b]() { return 'B'; };
    get b() { return 'get B'; };
  };
  desc = Object.getOwnPropertyDescriptor(C.prototype, 'b');
  assertFalse(desc.enumerable);
  assertTrue(desc.configurable);
  assertEquals('get B', desc.get());
  assertEquals(undefined, desc.set);

  C = class {
    get b() { return 'get B'; };
    [b]() { return 'B'; };
    set b(v) { return 'set B'; };
  };
  desc = Object.getOwnPropertyDescriptor(C.prototype, 'b');
  assertFalse(desc.enumerable);
  assertTrue(desc.configurable);
  assertEquals(undefined, desc.get);
  assertEquals('set B', desc.set());
})();

(function TestOverwritingStaticAccessors() {
  var C, desc;
  C = class {
    static [b]() { return 'B'; };
    static get b() { return 'get B'; };
  };
  desc = Object.getOwnPropertyDescriptor(C, 'b');
  assertFalse(desc.enumerable);
  assertTrue(desc.configurable);
  assertEquals('get B', desc.get());
  assertEquals(undefined, desc.set);

  C = class {
    static [b]() { return 'B'; };
    static set b(v) { return 'set B'; };
  };
  desc = Object.getOwnPropertyDescriptor(C, 'b');
  assertFalse(desc.enumerable);
  assertTrue(desc.configurable);
  assertEquals(undefined, desc.get);
  assertEquals('set B', desc.set());

  C = class {
    static set b(v) { return 'get B'; };
    static [b]() { return 'B'; };
    static get b() { return 'get B'; };
  };
  desc = Object.getOwnPropertyDescriptor(C, 'b');
  assertFalse(desc.enumerable);
  assertTrue(desc.configurable);
  assertEquals('get B', desc.get());
  assertEquals(undefined, desc.set);

  C = class {
    static get b() { return 'get B'; };
    static [b]() { return 'B'; };
    static set b(v) { return 'set B'; };
  };
  desc = Object.getOwnPropertyDescriptor(C, 'b');
  assertFalse(desc.enumerable);
  assertTrue(desc.configurable);
  assertEquals(undefined, desc.get);
  assertEquals('set B', desc.set());
})();
