// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --harmony-classes


(function TestSuperNamedLoads() {
  function Base() { }
  function Derived() {
    this.derivedDataProperty = "xxx";
  }
  Derived.prototype = Object.create(Base.prototype);

  function fBase() { return "Base " + this.toString(); }

  Base.prototype.f = fBase.toMethod(Base.prototype);

  function fDerived() {
     assertEquals("Base this is Derived", super.f());
     var a = super.x;
     assertEquals(15, a);
     assertEquals(15, super.x);
     assertEquals(27, this.x);

     return "Derived"
  }

  Base.prototype.x = 15;
  Base.prototype.toString = function() { return "this is Base"; };
  Derived.prototype.toString = function() { return "this is Derived"; };
  Derived.prototype.x = 27;
  Derived.prototype.f = fDerived.toMethod(Derived.prototype);

  assertEquals("Base this is Base", new Base().f());
  assertEquals("Derived", new Derived().f());
}());


(function TestSuperKeywordNonMethod() {
  function f() {
    super.unknown();
  }

  assertThrows(f, ReferenceError);
}());


(function TestGetter() {
  function Base() {}
  var derived;
  Base.prototype = {
    constructor: Base,
    get x() {
      assertSame(this, derived);
      return this._x;
    },
    _x: 'base'
  };

  function Derived() {}
  Derived.__proto__ = Base;
  Derived.prototype = {
    __proto__: Base.prototype,
    constructor: Derived,
    _x: 'derived'
  };
  Derived.prototype.testGetter = function() {
    return super.x;
  }.toMethod(Derived.prototype);
  Derived.prototype.testGetterStrict = function() {
    'use strict';
    return super.x;
  }.toMethod(Derived.prototype);
  derived = new Derived();
  assertEquals('derived', derived.testGetter());
  derived = new Derived();
  assertEquals('derived', derived.testGetterStrict());
}());


(function TestSetter() {
  function Base() {}
  Base.prototype = {
    constructor: Base,
    get x() {
      return this._x;
    },
    set x(v) {
      this._x = v;
    },
    _x: 'base'
  };

  function Derived() {}
  Derived.__proto__ = Base;
  Derived.prototype = {
    __proto__: Base.prototype,
    constructor: Derived,
    _x: 'derived'
  };
  Derived.prototype.testSetter = function() {
    assertEquals('foobar', super.x = 'foobar');
    assertEquals('foobarabc', super.x += 'abc');
  }.toMethod(Derived.prototype);
  var d = new Derived();
  d.testSetter();
  assertEquals('base', Base.prototype._x);
  assertEquals('foobarabc', d._x);
  d._x = '';
  Derived.prototype.testSetterStrict = function() {
    'use strict';
    assertEquals('foobar', super.x = 'foobar');
    assertEquals('foobarabc', super.x += 'abc');
  }.toMethod(Derived.prototype);
  d.testSetterStrict();
  assertEquals('base', Base.prototype._x);
  assertEquals('foobarabc', d._x);
}());


(function TestAccessorsOnPrimitives() {
  var getCalled = false;
  var setCalled = false;
  function Base() {}
  Base.prototype = {
    constructor: Base,
    get x() {
      getCalled = true;
      return 1;
    },
    set x(v) {
      setCalled = true;
      return v;
    },
  };

  function Derived() {}
  Derived.prototype = {
    __proto__: Base.prototype,
    constructor: Derived,
  };
  Derived.prototype.testSetter = function() {
    assertTrue(42 == this);
    getCalled = false;
    setCalled = false;
    assertEquals(1, super.x);
    assertTrue(getCalled);
    assertFalse(setCalled);

    setCalled = false;
    getCalled = false;
    assertEquals(5, super.x = 5);
    assertFalse(getCalled);
    assertTrue(setCalled);

    getCalled = false;
    setCalled = false;
    assertEquals(6, super.x += 5);
    assertTrue(getCalled);
    assertTrue(setCalled);
  }.toMethod(Derived.prototype);

  Derived.prototype.testSetterStrict = function() {
    'use strict';
    assertTrue(42 == this);
    getCalled = false;
    setCalled = false;
    assertEquals(1, super.x);
    assertTrue(getCalled);
    assertFalse(setCalled);

    setCalled = false;
    getCalled = false;
    assertEquals(5, super.x = 5);
    assertFalse(getCalled);
    assertTrue(setCalled);

    getCalled = false;
    setCalled = false;
    assertEquals(6, super.x += 5);
    assertTrue(getCalled);
    assertTrue(setCalled);
  }.toMethod(Derived.prototype);

  Derived.prototype.testSetter.call(42);
  Derived.prototype.testSetterStrict.call(42);

  function DerivedFromString() {}
  DerivedFromString.prototype = Object.create(String.prototype);

  function f() {
    'use strict';
    assertTrue(42 == this);
    assertEquals(String.prototype.toString, super.toString);
    var except = false;
    try {
      super.toString();
    } catch(e) { except = true; }
    assertTrue(except);
  }
  f.toMethod(DerivedFromString.prototype).call(42);
}());


(function TestSetterFailures() {
  function Base() {}
  function Derived() {}
  Derived.prototype = { __proto__ : Base.prototype };
  Derived.prototype.mSloppy = function () {
    super.x = 10;
    assertEquals(undefined, super.x);
  }.toMethod(Derived.prototype);

  Derived.prototype.mStrict = function () {
    "use strict";
    super.x = 10;
  }.toMethod(Derived.prototype);
  var d = new Derived();
  d.mSloppy();
  assertEquals(undefined, d.x);
  var d1 = new Derived();
  assertThrows(function() { d.mStrict(); }, ReferenceError);
  assertEquals(undefined, d.x);
}());


(function TestUnsupportedCases() {
  function f1(x) { return super[x]; }
  var o = {}
  assertThrows(function(){f1.toMethod(o)(x);}, ReferenceError);
  function f2() { super.x++; }
  assertThrows(function(){f2.toMethod(o)();}, ReferenceError);
}());
