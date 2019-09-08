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
      '', Object.getOwnPropertyDescriptor(literal.__proto__, 'name').value);

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
