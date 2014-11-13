// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-classes

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

  var E = class {}
  assertEquals('', E.name);
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
  assertTrue(descr.enumerable);
  assertTrue(descr.writable);
  assertEquals('function', typeof descr.value);
  assertFalse('prototype' in descr.value);
}


function assertGetterDescriptor(object, name) {
  var descr = Object.getOwnPropertyDescriptor(object, name);
  assertTrue(descr.configurable);
  assertTrue(descr.enumerable);
  assertEquals('function', typeof descr.get);
  assertEquals(undefined, descr.set);
}


function assertSetterDescriptor(object, name) {
  var descr = Object.getOwnPropertyDescriptor(object, name);
  assertTrue(descr.configurable);
  assertTrue(descr.enumerable);
  assertEquals(undefined, descr.get);
  assertEquals('function', typeof descr.set);
}


function assertAccessorDescriptor(object, name) {
  var descr = Object.getOwnPropertyDescriptor(object, name);
  assertTrue(descr.configurable);
  assertTrue(descr.enumerable);
  assertEquals('function', typeof descr.get);
  assertEquals('function', typeof descr.set);
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
  }

  assertMethodDescriptor(B.prototype, '1');
  assertGetterDescriptor(B.prototype, '2');
  assertSetterDescriptor(B.prototype, '3');

  assertMethodDescriptor(B, '4');
  assertGetterDescriptor(B, '5');
  assertSetterDescriptor(B, '6');

  class C extends B {
    1() { return super[1](); }
    get 2() { return super[2]; }

    static 4() { return super[4](); }
    static get 5() { return super[5]; }
  }

  assertEquals(1, new C()[1]());
  assertEquals(2, new C()[2]);
  assertEquals(4, C[4]());
  assertEquals(5, C[5]);
})();


/* TODO(arv): Implement
(function TestNameBindingInConstructor() {
  class C {
    constructor() {
      assertThrows(function() {
        C = 42;
      }, ReferenceError);
    }
  }
  new C();
})();
*/
