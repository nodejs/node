// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';
(function TestArgumentsAccess() {
  class Base {
    constructor() {
      assertEquals(2, arguments.length);
      assertEquals(1, arguments[0]);
      assertEquals(2, arguments[1]);
    }
  }

  let b = new Base(1,2);

  class Subclass extends Base {
    constructor() {
      assertEquals(2, arguments.length);
      assertEquals(3, arguments[0]);
      assertEquals(4, arguments[1]);
      super(1,2);
    }
  }

  let s = new Subclass(3,4);
  assertEquals(0, Subclass.length);

  class Subclass2 extends Base {
    constructor(x,y) {
      assertEquals(2, arguments.length);
      assertEquals(3, arguments[0]);
      assertEquals(4, arguments[1]);
      super(1,2);
    }
  }

  let s2 = new Subclass2(3,4);
  assertEquals(2, Subclass2.length);
}());

(function TestThisAccessRestriction() {
  class Base {
    constructor(a, b) {
      let o = new Object();
      o.prp = a + b;
      return o;
    }
  }

  class Subclass extends Base {
    constructor(a, b) {
      var exn;
      try {
        this.prp1 = 3;
      } catch (e) {
        exn = e;
      }
      assertTrue(exn instanceof ReferenceError);
      super(a, b);
      assertSame(a + b, this.prp);
      assertSame(undefined, this.prp1);
      assertFalse(this.hasOwnProperty("prp1"));
      return this;
    }
  }

  let b = new Base(1, 2);
  assertSame(3, b.prp);


  let s = new Subclass(2, -1);
  assertSame(1, s.prp);
  assertSame(undefined, s.prp1);
  assertFalse(s.hasOwnProperty("prp1"));

  class Subclass2 extends Base {
    constructor(x) {
      super(1,2);

      if (x < 0) return;

      let called = false;
      function tmp() { called = true; return 3; }
      var exn = null;
      try {
        super(tmp(),4);
      } catch (e) { exn = e; }
      assertTrue(exn instanceof ReferenceError);
      assertTrue(called);
    }
  }

  var s2 = new Subclass2(1);
  assertSame(3, s2.prp);

  var s3 = new Subclass2(-1);
  assertSame(3, s3.prp);

  assertThrows(function() { Subclass.call(new Object(), 1, 2); }, TypeError);
  assertThrows(function() { Base.call(new Object(), 1, 2); }, TypeError);

  class BadSubclass extends Base {
    constructor() {}
  }

  assertThrows(function() { new BadSubclass(); }, ReferenceError);
}());

(function TestThisCheckOrdering() {
  let baseCalled = 0;
  class Base {
    constructor() { baseCalled++ }
  }

  let fCalled = 0;
  function f() { fCalled++; return 3; }

  class Subclass1 extends Base {
    constructor() {
      baseCalled = 0;
      super();
      assertEquals(1, baseCalled);
      let obj = this;

      let exn = null;
      baseCalled = 0;
      fCalled = 0;
      try {
        super(f());
      } catch (e) { exn = e; }
      assertTrue(exn instanceof ReferenceError);
      assertEquals(1, fCalled);
      assertEquals(1, baseCalled);
      assertSame(obj, this);

      exn = null;
      baseCalled = 0;
      fCalled = 0;
      try {
        super(super(), f());
      } catch (e) { exn = e; }
      assertTrue(exn instanceof ReferenceError);
      assertEquals(0, fCalled);
      assertEquals(1, baseCalled);
      assertSame(obj, this);

      exn = null;
      baseCalled = 0;
      fCalled = 0;
      try {
        super(f(), super());
      } catch (e) { exn = e; }
      assertTrue(exn instanceof ReferenceError);
      assertEquals(1, fCalled);
      assertEquals(1, baseCalled);
      assertSame(obj, this);
    }
  }

  new Subclass1();
}());


(function TestPrototypeWiring() {
  class Base {
    constructor(x) {
      this.foobar = x;
    }
  }

  class Subclass extends Base {
    constructor(x) {
      super(x);
    }
  }

  let s = new Subclass(1);
  assertSame(1, s.foobar);
  assertSame(Subclass.prototype, s.__proto__);

  let s1 = new Subclass(1, 2);
  assertSame(1, s1.foobar);
  assertTrue(s1.__proto__ === Subclass.prototype);

  let s2 = new Subclass();
  assertSame(undefined, s2.foobar);
  assertSame(Subclass.prototype, s2.__proto__);
  assertThrows(function() { Subclass(1); }, TypeError);
  assertThrows(function() { Subclass(1,2,3,4); }, TypeError);

  class Subclass2 extends Subclass {
    constructor() {
      super(5, 6, 7);
    }
  }

  let ss2 = new Subclass2();
  assertSame(5, ss2.foobar);
  assertSame(Subclass2.prototype, ss2.__proto__);

  class Subclass3 extends Base {
    constructor(x,y) {
      super(x + y);
    }
  }

  let ss3 = new Subclass3(27,42-27);
  assertSame(42, ss3.foobar);
  assertSame(Subclass3.prototype, ss3.__proto__);
}());

(function TestSublclassingBuiltins() {
  class ExtendedUint8Array extends Uint8Array {
    constructor() {
      super(10);
      this[0] = 255;
      this[1] = 0xFFA;
    }
  }

  var eua = new ExtendedUint8Array();
  assertEquals(10, eua.length);
  assertEquals(10, eua.byteLength);
  assertEquals(0xFF, eua[0]);
  assertEquals(0xFA, eua[1]);
  assertSame(ExtendedUint8Array.prototype, eua.__proto__);
  assertEquals("[object Uint8Array]", Object.prototype.toString.call(eua));
}());

(function TestSubclassingNull() {
  let N = null;

  class Foo extends N {
    constructor(x,y) {
      assertSame(1, x);
      assertSame(2, y);
      return {};
    }
  }

  new Foo(1,2);
}());

(function TestSubclassBinding() {
  class Base {
    constructor(x, y) {
      this.x = x;
      this.y = y;
    }
  }

  let obj = {};
  class Subclass extends Base {
    constructor(x,y) {
      super(x,y);
      assertTrue(this !== obj);
    }
  }

  let f = Subclass.bind(obj);
  assertThrows(function () { f(1, 2); }, TypeError);
  let s = new f(1, 2);
  assertSame(1, s.x);
  assertSame(2, s.y);
  assertSame(Subclass.prototype, s.__proto__);

  let s1 = new f(1);
  assertSame(1, s1.x);
  assertSame(undefined, s1.y);
  assertSame(Subclass.prototype, s1.__proto__);

  let g = Subclass.bind(obj, 1);
  assertThrows(function () { g(8); }, TypeError);
  let s2 = new g(8);
  assertSame(1, s2.x);
  assertSame(8, s2.y);
  assertSame(Subclass.prototype, s.__proto__);
}());


(function TestDefaultConstructor() {
  class Base1 { }
  assertThrows(function() { Base1(); }, TypeError);

  class Subclass1 extends Base1 { }

  assertThrows(function() { Subclass1(); }, TypeError);

  let s1 = new Subclass1();
  assertSame(s1.__proto__, Subclass1.prototype);

  class Base2 {
    constructor(x, y) {
      this.x = x;
      this.y = y;
    }
  }

  class Subclass2 extends Base2 {};

  let s2 = new Subclass2(1, 2);

  assertSame(s2.__proto__, Subclass2.prototype);
  assertSame(1, s2.x);
  assertSame(2, s2.y);

  let f = Subclass2.bind({}, 3, 4);
  let s2prime = new f();
  assertSame(s2prime.__proto__, Subclass2.prototype);
  assertSame(3, s2prime.x);
  assertSame(4, s2prime.y);

  let obj = {};
  class Base3 {
    constructor() {
      return obj;
    }
  }

  class Subclass3 extends Base3 {};

  let s3 = new Subclass3();
  assertSame(obj, s3);

  class ExtendedUint8Array extends Uint8Array { }

  var eua = new ExtendedUint8Array(10);
  assertEquals(10, eua.length);
  assertEquals(10, eua.byteLength);
  eua[0] = 0xFF;
  eua[1] = 0xFFA;
  assertEquals(0xFF, eua[0]);
  assertEquals(0xFA, eua[1]);
  assertSame(ExtendedUint8Array.prototype, eua.__proto__);
  assertEquals("[object Uint8Array]", Object.prototype.toString.call(eua));
}());
