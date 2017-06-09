// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


(function TestClass() {
  'use strict';

  var calls = 0;
  class Base {
    constructor(_) {
      assertEquals(Base, new.target);
      calls++;
    }
  }
  assertInstanceof(new Base(1), Base);
  assertInstanceof(new Base(1, 2), Base);
  assertInstanceof(new Base(), Base);
  assertEquals(3, calls);
})();


(function TestDerivedClass() {
  'use strict';

  var calls = 0;
  class Base {
    constructor(expected) {
      assertEquals(expected, new.target);
    }
  }
  class Derived extends Base {
    constructor(expected) {
      super(expected);
      assertEquals(expected, new.target);
      calls++;
    }
  }
  new Derived(Derived, 'extra');
  new Derived(Derived);
  assertEquals(2, calls);

  class Derived2 extends Derived {}
  calls = 0;
  new Derived2(Derived2);
  new Derived2(Derived2, 'extra');
  assertEquals(2, calls);
})();


(function TestFunctionCall() {
  var calls;
  function f(expected) {
    calls++;
    assertEquals(expected, new.target);
  }

  calls = 0;
  f(undefined);
  f(undefined, 'extra');
  f();
  assertEquals(3, calls);

  calls = 0;
  f.call({}, undefined);
  f.call({}, undefined, 'extra');
  f.call({});
  assertEquals(3, calls);

  calls = 0;
  f.apply({}, [undefined]);
  f.apply({}, [undefined, 'extra']);
  f.apply({}, []);
  assertEquals(3, calls);
})();


(function TestFunctionConstruct() {
  var calls;
  function f(expected) {
    calls++;
    assertEquals(expected, new.target);
  }

  calls = 0;
  new f(f);
  new f(f, 'extra');
  assertEquals(2, calls);
})();


(function TestClassExtendsFunction() {
  'use strict';

  var calls = 0;
  function f(expected) {
    assertEquals(expected, new.target);
  }
  class Derived extends f {
    constructor(expected) {
      super(expected);
      assertEquals(expected, new.target);
      calls++;
    }
  }

  new Derived(Derived);
  new Derived(Derived, 'extra');
  assertEquals(2, calls);
})();


(function TestFunctionReturnObject() {
  function f(expected) {
    assertEquals(expected, new.target);
    return /abc/;
  }

  assertInstanceof(new f(f), RegExp);
  assertInstanceof(new f(f, 'extra'), RegExp);

  assertInstanceof(f(undefined), RegExp);
  assertInstanceof(f(), RegExp);
  assertInstanceof(f(undefined, 'extra'), RegExp);
})();


(function TestClassReturnObject() {
  'use strict';

  class Base {
    constructor(expected) {
      assertEquals(expected, new.target);
      return /abc/;
    }
  }

  assertInstanceof(new Base(Base), RegExp);
  assertInstanceof(new Base(Base, 'extra'), RegExp);

  class Derived extends Base {}
  assertInstanceof(new Derived(Derived), RegExp);
  assertInstanceof(new Derived(Derived, 'extra'), RegExp);

  class Derived2 extends Base {
    constructor(expected) {
      super(expected);
      assertInstanceof(this, RegExp);
    }
  }
  assertInstanceof(new Derived2(Derived2), RegExp);
  assertInstanceof(new Derived2(Derived2, 'extra'), RegExp);
})();


(function TestReflectConstruct() {
  var calls = 0;
  function f(expected) {
    calls++;
    assertEquals(expected, new.target);
  }

  var o = Reflect.construct(f, [f]);
  assertEquals(Object.getPrototypeOf(o), f.prototype);
  o = Reflect.construct(f, [f, 'extra']);
  assertEquals(Object.getPrototypeOf(o), f.prototype);
  assertEquals(2, calls);

  calls = 0;
  o = Reflect.construct(f, [f], f);
  assertEquals(Object.getPrototypeOf(o), f.prototype);
  o = Reflect.construct(f, [f, 'extra'], f);
  assertEquals(Object.getPrototypeOf(o), f.prototype);
  assertEquals(2, calls);


  function g() {}
  calls = 0;
  o = Reflect.construct(f, [g], g);
  assertEquals(Object.getPrototypeOf(o), g.prototype);
  o = Reflect.construct(f, [g, 'extra'], g);
  assertEquals(Object.getPrototypeOf(o), g.prototype);
  assertEquals(2, calls);
})();


(function TestRestParametersFunction() {
  function f(...rest) {
    assertEquals(rest[0], new.target);
  }

  assertInstanceof(new f(f), f);
  assertInstanceof(new f(f, 'extra'), f);
})();


(function TestRestParametersClass() {
  'use strict';

  class Base {
    constructor(...rest) {
      assertEquals(rest[0], new.target);
    }
  }

  assertInstanceof(new Base(Base), Base);
  assertInstanceof(new Base(Base, 'extra'), Base);

  class Derived extends Base {}

  assertInstanceof(new Derived(Derived), Derived);
  assertInstanceof(new Derived(Derived, 'extra'), Derived);
})();


(function TestArrowFunction() {
  function f(expected) {
    (() => {
      assertEquals(expected, new.target);
    })();
  }

  assertInstanceof(new f(f), f);
  assertInstanceof(new f(f, 'extra'), f);
})();


(function TestRestParametersClass() {
  'use strict';

  class Base {
    constructor(expected) {
      (() => {
        assertEquals(expected, new.target);
      })();
    }
  }

  assertInstanceof(new Base(Base), Base);
  assertInstanceof(new Base(Base, 'extra'), Base);

  class Derived extends Base {}

  assertInstanceof(new Derived(Derived), Derived);
  assertInstanceof(new Derived(Derived, 'extra'), Derived);
})();


(function TestSloppyArguments() {
  var length, a0, a1, a2, nt;
  function f(x) {
    assertEquals(length, arguments.length);
    assertEquals(a0, arguments[0]);
    assertEquals(a1, arguments[1]);
    assertEquals(a2, arguments[2]);
    assertEquals(nt, new.target);

    if (length > 0) {
      x = 42;
      assertEquals(42, x);
      assertEquals(42, arguments[0]);

      arguments[0] = 33;
      assertEquals(33, x);
      assertEquals(33, arguments[0]);
    }
  }

  nt = f;
  length = 0;
  new f();

  length = 1;
  a0 = 1;
  new f(1);

  length = 2;
  a0 = 1;
  a1 = 2;
  new f(1, 2);

  length = 3;
  a0 = 1;
  a1 = 2;
  a2 = 3;
  new f(1, 2, 3);

  nt = undefined;
  a0 = a1 = a2 = undefined;
  length = 0;
  f();

  length = 1;
  a0 = 1;
  f(1);

  length = 2;
  a0 = 1;
  a1 = 2;
  f(1, 2);

  length = 3;
  a0 = 1;
  a1 = 2;
  a2 = 3;
  f(1, 2, 3);
})();


(function TestStrictArguments() {
  var length, a0, a1, a2, nt;
  function f(x) {
    'use strict';
    assertEquals(length, arguments.length);
    assertEquals(a0, arguments[0]);
    assertEquals(a1, arguments[1]);
    assertEquals(a2, arguments[2]);
    assertEquals(nt, new.target);

    if (length > 0) {
      x = 42;
      assertEquals(a0, arguments[0]);

      arguments[0] = 33;
      assertEquals(33, arguments[0]);
    }
  }

  nt = f;
  length = 0;
  new f();

  length = 1;
  a0 = 1;
  new f(1);

  length = 2;
  a0 = 1;
  a1 = 2;
  new f(1, 2);

  length = 3;
  a0 = 1;
  a1 = 2;
  a2 = 3;
  new f(1, 2, 3);

  nt = undefined;
  a0 = a1 = a2 = undefined;
  length = 0;
  f();

  length = 1;
  a0 = 1;
  f(1);

  length = 2;
  a0 = 1;
  a1 = 2;
  f(1, 2);

  length = 3;
  a0 = 1;
  a1 = 2;
  a2 = 3;
  f(1, 2, 3);
})();


(function TestOtherScopes() {
  function f1() { return eval("'use strict'; new.target") }
  assertSame(f1, new f1);
  function f2() { with ({}) return new.target }
  assertSame(f2, new f2);
  function f3({a}) { return new.target }
  assertSame(f3, new f3({}));
  function f4(...a) { return new.target }
  assertSame(f4, new f4);
  function f5() { 'use strict'; { let x; return new.target } }
  assertSame(f5, new f5);
  function f6() { with ({'new.target': 42}) return new.target }
  assertSame(f6, new f6);
})();


// Has to be top-level to be inlined.
function get_new_target() { return new.target; }
(function TestInlining() {
  "use strict";
  new function() { assertEquals(undefined, get_new_target()); }
  new function() { assertEquals(get_new_target, new get_new_target()); }

  class A extends get_new_target {
    constructor() {
      var new_target = super();
      this.new_target = new_target;
    }
  }
  assertEquals(A, new A().new_target);
})();


(function TestEarlyErrors() {
  assertThrows(function() { Function("new.target = 42"); }, ReferenceError);
  assertThrows(function() { Function("var foo = 1; new.target = foo = 42"); }, ReferenceError);
  assertThrows(function() { Function("var foo = 1; foo = new.target = 42"); }, ReferenceError);
  assertThrows(function() { Function("new.target--"); }, ReferenceError);
  assertThrows(function() { Function("--new.target"); }, ReferenceError);
  assertThrows(function() { Function("(new.target)++"); }, ReferenceError);
  assertThrows(function() { Function("++(new.target)"); }, ReferenceError);
  assertThrows(function() { Function("for (new.target of {});"); }, SyntaxError);
})();
