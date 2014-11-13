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


(function TestSuperKeyedLoads() {
  var x = 'x';
  var derivedDataProperty = 'derivedDataProperty';
  var f = 'f';
  function Base() { }
  function Derived() {
    this[derivedDataProperty] = 'xxx';
  }
  Derived.prototype = Object.create(Base.prototype);

  function fBase() { return "Base " + this.toString(); }

  Base.prototype[f] = fBase.toMethod(Base.prototype);

  function fDerived() {
     assertEquals("Base this is Derived", super[f]());
     var a = super[x];
     assertEquals(15, a);
     assertEquals(15, super[x]);
     assertEquals(27, this[x]);

     return "Derived"
  }

  Base.prototype[x] = 15;
  Base.prototype.toString = function() { return "this is Base"; };
  Derived.prototype.toString = function() { return "this is Derived"; };
  Derived.prototype[x] = 27;
  Derived.prototype[f] = fDerived.toMethod(Derived.prototype);

  assertEquals("Base this is Base", new Base().f());
  assertEquals("Derived", new Derived().f());
}());


(function TestSuperNumericKeyedLoads() {
  var x = 1;
  var derivedDataProperty = 2;
  var f = 3;
  function Base() { }
  function Derived() {
    this[derivedDataProperty] = 'xxx';
  }
  Derived.prototype = Object.create(Base.prototype);

  function fBase() { return "Base " + this.toString(); }

  Base.prototype[f] = fBase.toMethod(Base.prototype);

  function fDerived() {
     assertEquals("Base this is Derived", super[f]());
     var a = super[x];
     assertEquals(15, a);
     assertEquals(15, super[x]);
     assertEquals(27, this[x]);

     return "Derived"
  }

  Base.prototype[x] = 15;
  Base.prototype.toString = function() { return "this is Base"; };
  Derived.prototype.toString = function() { return "this is Derived"; };
  Derived.prototype[x] = 27;
  Derived.prototype[f] = fDerived.toMethod(Derived.prototype);

  assertEquals("Base this is Base", new Base()[f]());
  assertEquals("Derived", new Derived()[f]());
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


(function TestGetterKeyed() {
  var x = 'x';
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
    return super[x];
  }.toMethod(Derived.prototype);
  Derived.prototype.testGetterStrict = function() {
    'use strict';
    return super[x];
  }.toMethod(Derived.prototype);
  Derived.prototype.testGetterWithToString = function() {
    var toStringCalled;
    var o = { toString: function() {
      toStringCalled++;
      return 'x';
    } };

    toStringCalled = 0;
    assertEquals('derived', super[o]);
    assertEquals(1, toStringCalled);

    var eToThrow = new Error();
    var oThrowsInToString = { toString: function() {
      throw eToThrow;
    } };

    var ex = null;
    try {
      super[oThrowsInToString];
    } catch(e) { ex = e }
    assertEquals(eToThrow, ex);

    var oReturnsNumericString = { toString: function() {
      return "1";
    } };

    assertEquals(undefined, super[oReturnsNumericString]);
    assertEquals(undefined, super[1]);
  }.toMethod(Derived.prototype);
  derived = new Derived();
  assertEquals('derived', derived.testGetter());
  derived = new Derived();
  assertEquals('derived', derived.testGetterStrict());
  derived = new Derived();
  derived.testGetterWithToString();
}());


(function TestGetterNumericKeyed() {
  var x = 42;
  function Base() {}
  var derived;
  Base.prototype = {
    constructor: Base,
    _x: 'base'
  };

  Object.defineProperty(Base.prototype, x, { get: function() {
      assertSame(this, derived);
      return this._x;
  }});

  function Derived() {}
  Derived.__proto__ = Base;
  Derived.prototype = {
    __proto__: Base.prototype,
    constructor: Derived,
    _x: 'derived'
  };
  Derived.prototype.testGetter = function() {
    return super[x];
  }.toMethod(Derived.prototype);
  Derived.prototype.testGetterStrict = function() {
    'use strict';
    return super[x];
  }.toMethod(Derived.prototype);

  Derived.prototype.testGetterWithToString = function() {
    var toStringCalled;
    var o = { toString: function() {
      toStringCalled++;
      return '42';
    } };

    toStringCalled = 0;
    assertEquals('derived', super[o]);
    assertEquals(1, toStringCalled);

    var eToThrow = new Error();
    var oThrowsInToString = { toString: function() {
      throw eToThrow;
    } };

    var ex = null;
    try {
      super[oThrowsInToString];
    } catch(e) { ex = e }
    assertEquals(eToThrow, ex);

    var oReturnsNumericString = { toString: function() {
      return "42";
    } };

    assertEquals('derived', super[oReturnsNumericString]);
    assertEquals('derived', super[42]);
  }.toMethod(Derived.prototype);
  derived = new Derived();
  assertEquals('derived', derived.testGetter());
  derived = new Derived();
  assertEquals('derived', derived.testGetterStrict());
  derived = new Derived();
  derived.testGetterWithToString();
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


(function TestSetterNumericKeyed() {
  var x = 42;
  function Base() {}
  Base.prototype = {
    constructor: Base,
    _x: 'base'
  };

  Object.defineProperty(Base.prototype, x,
    { get: function() { return this._x; },
      set: function(v) { this._x = v; }
    });

  function Derived() {}
  Derived.__proto__ = Base;
  Derived.prototype = {
    __proto__: Base.prototype,
    constructor: Derived,
    _x: 'derived'
  };
  Derived.prototype.testSetter = function() {
    assertEquals('foobar', super[x] = 'foobar');
    assertEquals('foobarabc', super[x] += 'abc');
  }.toMethod(Derived.prototype);
  var d = new Derived();
  d.testSetter();
  assertEquals('base', Base.prototype._x);
  assertEquals('foobarabc', d._x);
  d._x = '';
  Derived.prototype.testSetterStrict = function() {
    'use strict';
    assertEquals('foobar', super[x] = 'foobar');
    assertEquals('foobarabc', super[x] += 'abc');
  }.toMethod(Derived.prototype);
  d.testSetterStrict();
  assertEquals('base', Base.prototype._x);
  assertEquals('foobarabc', d._x);


  Derived.prototype.testSetterWithToString = function() {
    var toStringCalled;
    var o = { toString: function() {
      toStringCalled++;
      return x;
    } };

    toStringCalled = 0;
    super[o] = 'set';
    assertEquals(1, toStringCalled);
    assertEquals('set', this._x);

    var eToThrow = new Error();
    var oThrowsInToString = { toString: function() {
      throw eToThrow;
    } };

    var ex = null;
    try {
      super[oThrowsInToString] = 'xyz';
    } catch(e) { ex = e }
    assertEquals(eToThrow, ex);
    assertEquals('set', this._x);
  }.toMethod(Derived.prototype);
  d = new Derived();
  d.testSetterWithToString();
}());


(function TestSetterKeyed() {
  var x = 'x';
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
    assertEquals('foobar', super[x] = 'foobar');
    assertEquals('foobarabc', super[x] += 'abc');
  }.toMethod(Derived.prototype);
  var d = new Derived();
  d.testSetter();
  assertEquals('base', Base.prototype._x);
  assertEquals('foobarabc', d._x);
  d._x = '';
  Derived.prototype.testSetterStrict = function() {
    'use strict';
    assertEquals('foobar', super[x] = 'foobar');
    assertEquals('foobarabc', super[x] += 'abc');
  }.toMethod(Derived.prototype);
  d.testSetterStrict();
  assertEquals('base', Base.prototype._x);
  assertEquals('foobarabc', d._x);


  Derived.prototype.testSetterWithToString = function() {
    var toStringCalled;
    var o = { toString: function() {
      toStringCalled++;
      return 'x';
    } };

    toStringCalled = 0;
    super[o] = 'set';
    assertEquals(1, toStringCalled);
    assertEquals('set', this._x);

    var eToThrow = new Error();
    var oThrowsInToString = { toString: function() {
      throw eToThrow;
    } };

    var ex = null;
    try {
      super[oThrowsInToString] = 'xyz';
    } catch(e) { ex = e }
    assertEquals(eToThrow, ex);
    assertEquals('set', this._x);

    var oReturnsNumericString = { toString: function() {
      return "1";
    } };

    assertEquals('abc', super[oReturnsNumericString] = 'abc');

    assertEquals('set', this._x);

    assertEquals(10,  super[1] = 10);
  }.toMethod(Derived.prototype);
  d = new Derived();
  d.testSetterWithToString();
}());


(function TestSetterDataProperties() {
  function Base() {}
  Base.prototype = {
    constructor: Base,
    x: 'x from Base'
  };

  function Derived() {}
  Derived.prototype = {
    __proto__: Base.prototype,
    constructor: Derived,
  };

  Derived.prototype.testSetter = function() {
    assertEquals('x from Base', super.x);
    super.x = 'data property';
    assertEquals('x from Base', super.x);
    assertEquals('data property', this.x);
  }.toMethod(Derived.prototype);

  new Derived().testSetter();
}());


(function TestKeyedSetterDataProperties() {
  var x = 'x';
  function Base() {}
  Base.prototype = {
    constructor: Base,
    x: 'x from Base'
  };

  function Derived() {}
  Derived.prototype = {
    __proto__: Base.prototype,
    constructor: Derived,
  };

  Derived.prototype.testSetter = function() {
    assertEquals('x from Base', super[x]);
    super[x] = 'data property';
    assertEquals('x from Base', super[x]);
    assertEquals('data property', this[x]);
  }.toMethod(Derived.prototype);

  new Derived().testSetter();
}());


(function TestKeyedNumericSetterDataProperties() {
  var x = 42;
  function Base() {}
  Base.prototype = {
    constructor: Base,
    42: 'x from Base'
  };

  function Derived() {}
  Derived.prototype = {
    __proto__: Base.prototype,
    constructor: Derived,
  };

  Derived.prototype.testSetter = function() {
    assertEquals('x from Base', super[x]);
    super[x] = 'data property';
    assertEquals('x from Base', super[x]);
    assertEquals('data property', this[x]);
  }.toMethod(Derived.prototype);

  new Derived().testSetter();
}());


(function TestAccessorsOnPrimitives() {
  var getCalled = 0;
  var setCalled = 0;
  function Base() {}
  Base.prototype = {
    constructor: Base,
    get x() {
      getCalled++;
      return 1;
    },
    set x(v) {
      setCalled++;
      return v;
    },
  };

  function Derived() {}
  Derived.prototype = {
    __proto__: Base.prototype,
    constructor: Derived,
  };
  Derived.prototype.testSetter = function() {
    setCalled = 0;
    getCalled = 0;
    assertEquals('object', typeof this);
    assertTrue(this instanceof Number)
    assertEquals(42, this.valueOf());
    assertEquals(1, super.x);
    assertEquals(1, getCalled);
    assertEquals(0, setCalled);

    assertEquals(5, super.x = 5);
    assertEquals(1, getCalled);
    assertEquals(1, setCalled);

    assertEquals(6, super.x += 5);
    assertEquals(2, getCalled);
    assertEquals(2, setCalled);

    super.newProperty = 15;
    assertEquals(15, this.newProperty);
    assertEquals(undefined, super.newProperty);
  }.toMethod(Derived.prototype);

  Derived.prototype.testSetterStrict = function() {
    'use strict';
    getCalled = 0;
    setCalled = 0;
    assertTrue(42 === this);

    assertEquals(1, super.x);
    assertEquals(1, getCalled);
    assertEquals(0, setCalled);

    assertEquals(5, super.x = 5);
    assertEquals(1, getCalled);
    assertEquals(1, setCalled);

    assertEquals(6, super.x += 5);
    assertEquals(2, getCalled);
    assertEquals(2, setCalled);

    var ex;
    try {
      super.newProperty = 15;
    } catch (e) { ex = e; }
    assertTrue(ex instanceof TypeError);
  }.toMethod(Derived.prototype);

  Derived.prototype.testSetter.call(42);
  Derived.prototype.testSetterStrict.call(42);

  function DerivedFromString() {}
  DerivedFromString.prototype = Object.create(String.prototype);

  function f() {
    'use strict';
    assertTrue(42 === this);
    assertEquals(String.prototype.toString, super.toString);
    var ex;
    try {
      super.toString();
    } catch(e) { ex = e; }

    assertTrue(ex instanceof TypeError);
  }
  f.toMethod(DerivedFromString.prototype).call(42);
}());


(function TestKeyedAccessorsOnPrimitives() {
  var x = 'x';
  var newProperty = 'newProperty';
  var toString = 'toString';
  var getCalled = 0;
  var setCalled = 0;
  function Base() {}
  Base.prototype = {
    constructor: Base,
    get x() {
      getCalled++;
      return 1;
    },
    set x(v) {
      setCalled++;
      return v;
    },
  };

  function Derived() {}
  Derived.prototype = {
    __proto__: Base.prototype,
    constructor: Derived,
  };
  Derived.prototype.testSetter = function() {
    setCalled = 0;
    getCalled = 0;
    assertEquals('object', typeof this);
    assertTrue(this instanceof Number)
    assertEquals(42, this.valueOf());
    assertEquals(1, super[x]);
    assertEquals(1, getCalled);
    assertEquals(0, setCalled);

    assertEquals(5, super[x] = 5);
    assertEquals(1, getCalled);
    assertEquals(1, setCalled);

    assertEquals(6, super[x] += 5);
    assertEquals(2, getCalled);
    assertEquals(2, setCalled);

    super[newProperty] = 15;
    assertEquals(15, this[newProperty]);
    assertEquals(undefined, super[newProperty]);
  }.toMethod(Derived.prototype);

  Derived.prototype.testSetterStrict = function() {
    'use strict';
    getCalled = 0;
    setCalled = 0;
    assertTrue(42 === this);

    assertEquals(1, super[x]);
    assertEquals(1, getCalled);
    assertEquals(0, setCalled);

    assertEquals(5, super[x] = 5);
    assertEquals(1, getCalled);
    assertEquals(1, setCalled);

    assertEquals(6, super[x] += 5);
    assertEquals(2, getCalled);
    assertEquals(2, setCalled);

    var ex;
    try {
      super[newProperty] = 15;
    } catch (e) { ex = e; }
    assertTrue(ex instanceof TypeError);
  }.toMethod(Derived.prototype);

  Derived.prototype.testSetter.call(42);
  Derived.prototype.testSetterStrict.call(42);

  function DerivedFromString() {}
  DerivedFromString.prototype = Object.create(String.prototype);

  function f() {
    'use strict';
    assertTrue(42 === this);
    assertEquals(String.prototype.toString, super[toString]);
    var ex;
    try {
      super[toString]();
    } catch(e) { ex = e; }

    assertTrue(ex instanceof TypeError);
  }
  f.toMethod(DerivedFromString.prototype).call(42);
}());


(function TestNumericKeyedAccessorsOnPrimitives() {
  var x = 42;
  var newProperty = 43;
  var getCalled = 0;
  var setCalled = 0;
  function Base() {}
  Base.prototype = {
    constructor: Base,
  };

  Object.defineProperty(Base.prototype, x, {
    get: function() {
      getCalled++;
      return 1;
    },
    set: function(v) {
      setCalled++;
      return v;
    }
  });

  function Derived() {}
  Derived.prototype = {
    __proto__: Base.prototype,
    constructor: Derived,
  };
  Derived.prototype.testSetter = function() {
    setCalled = 0;
    getCalled = 0;
    assertEquals('object', typeof this);
    assertTrue(this instanceof Number)
    assertEquals(42, this.valueOf());
    assertEquals(1, super[x]);
    assertEquals(1, getCalled);
    assertEquals(0, setCalled);

    assertEquals(5, super[x] = 5);
    assertEquals(1, getCalled);
    assertEquals(1, setCalled);

    assertEquals(6, super[x] += 5);
    assertEquals(2, getCalled);
    assertEquals(2, setCalled);

    super[newProperty] = 15;
    assertEquals(15, this[newProperty]);
    assertEquals(undefined, super[newProperty]);
  }.toMethod(Derived.prototype);

  Derived.prototype.testSetterStrict = function() {
    'use strict';
    getCalled = 0;
    setCalled = 0;
    assertTrue(42 === this);

    assertEquals(1, super[x]);
    assertEquals(1, getCalled);
    assertEquals(0, setCalled);

    assertEquals(5, super[x] = 5);
    assertEquals(1, getCalled);
    assertEquals(1, setCalled);

    assertEquals(6, super[x] += 5);
    assertEquals(2, getCalled);
    assertEquals(2, setCalled);

    var ex;
    try {
      super[newProperty] = 15;
    } catch (e) { ex = e; }
    assertTrue(ex instanceof TypeError);
  }.toMethod(Derived.prototype);

  Derived.prototype.testSetter.call(42);
  Derived.prototype.testSetterStrict.call(42);
}());


(function TestKeyedNumericSetterOnExotics() {
  function Base() {}
  function Derived() {}
  Derived.prototype = { __proto__: Base.prototype };

  Derived.prototype.callSetterOnArray = function() {
    super[42] = 1;
  }.toMethod(Derived.prototype);

  Derived.prototype.callStrictSetterOnString = function() {
    'use strict';
    assertEquals('string', typeof this);
    assertTrue('abcdef' === this);
    var ex = null;
    try {
      super[5] = 'q';
    } catch(e) { ex = e; }
    assertTrue(ex instanceof TypeError);

    ex = null;
    try {
      super[1024] = 'q';
    } catch(e) { ex = e; }
    assertTrue(ex instanceof TypeError);
  }.toMethod(Derived.prototype);

  var x = [];
  assertEquals(0, x.length);
  Derived.prototype.callSetterOnArray.call(x);
  assertEquals(43, x.length);
  assertEquals(1, x[42]);

  var s = 'abcdef';
  Derived.prototype.callStrictSetterOnString.call(s)
}());


(function TestSetterUndefinedProperties() {
  function Base() {}
  function Derived() {}
  Derived.prototype = { __proto__ : Base.prototype };
  Derived.prototype.mSloppy = function () {
    assertEquals(undefined, super.x);
    assertEquals(undefined, this.x);
    super.x = 10;
    assertEquals(10, this.x);
    assertEquals(undefined, super.x);
  }.toMethod(Derived.prototype);

  Derived.prototype.mStrict = function () {
    'use strict';
    assertEquals(undefined, super.x);
    assertEquals(undefined, this.x);
    super.x = 10;
    assertEquals(10, this.x);
    assertEquals(undefined, super.x);
  }.toMethod(Derived.prototype);
  var d = new Derived();
  d.mSloppy();
  assertEquals(10, d.x);
  var d1 = new Derived();
  d1.mStrict();
  assertEquals(10, d.x);
}());


(function TestKeyedSetterUndefinedProperties() {
  var x = 'x';
  function Base() {}
  function Derived() {}
  Derived.prototype = { __proto__ : Base.prototype };
  Derived.prototype.mSloppy = function () {
    assertEquals(undefined, super[x]);
    assertEquals(undefined, this[x]);
    super[x] = 10;
    assertEquals(10, this[x]);
    assertEquals(undefined, super[x]);
  }.toMethod(Derived.prototype);

  Derived.prototype.mStrict = function () {
    'use strict';
    assertEquals(undefined, super[x]);
    assertEquals(undefined, this[x]);
    super[x] = 10;
    assertEquals(10, this[x]);
    assertEquals(undefined, super[x]);
  }.toMethod(Derived.prototype);
  var d = new Derived();
  d.mSloppy();
  assertEquals(10, d.x);
  var d1 = new Derived();
  d1.mStrict();
  assertEquals(10, d.x);
}());


(function TestKeyedNumericSetterUndefinedProperties() {
  var x = 42;
  function Base() {}
  function Derived() {}
  Derived.prototype = { __proto__ : Base.prototype };
  Derived.prototype.mSloppy = function () {
    assertEquals(undefined, super[x]);
    assertEquals(undefined, this[x]);
    super[x] = 10;
    assertEquals(10, this[x]);
    assertEquals(undefined, super[x]);
  }.toMethod(Derived.prototype);

  Derived.prototype.mStrict = function () {
    'use strict';
    assertEquals(undefined, super[x]);
    assertEquals(undefined, this[x]);
    super[x] = 10;
    assertEquals(10, this[x]);
    assertEquals(undefined, super[x]);
  }.toMethod(Derived.prototype);
  var d = new Derived();
  d.mSloppy();
  assertEquals(10, d[x]);
  var d1 = new Derived();
  d1.mStrict();
  assertEquals(10, d[x]);
}());


(function TestSetterCreatingOwnProperties() {
  function Base() {}
  function Derived() {}
  Derived.prototype = { __proto__ : Base.prototype };
  var setterCalled;

  Derived.prototype.mSloppy = function() {
    assertEquals(42, this.ownReadOnly);
    super.ownReadOnly = 55;
    assertEquals(42, this.ownReadOnly);

    assertEquals(15, this.ownReadonlyAccessor);
    super.ownReadonlyAccessor = 55;
    assertEquals(15, this.ownReadonlyAccessor);

    setterCalled = 0;
    super.ownSetter = 42;
    assertEquals(1, setterCalled);
  }.toMethod(Derived.prototype);

  Derived.prototype.mStrict = function() {
    'use strict';
    assertEquals(42, this.ownReadOnly);
    var ex;
    try {
      super.ownReadOnly = 55;
    } catch(e) { ex = e; }
    assertTrue(ex instanceof TypeError);
    assertEquals(42, this.ownReadOnly);

    assertEquals(15, this.ownReadonlyAccessor);
    ex = null;
    try {
      super.ownReadonlyAccessor = 55;
    } catch(e) { ex = e; }
    assertTrue(ex instanceof TypeError);
    assertEquals(15, this.ownReadonlyAccessor);

    setterCalled = 0;
    super.ownSetter = 42;
    assertEquals(1, setterCalled);
  }.toMethod(Derived.prototype);

  var d = new Derived();
  Object.defineProperty(d, 'ownReadOnly', { value : 42, writable : false });
  Object.defineProperty(d, 'ownSetter',
      { set : function() { setterCalled++; } });
  Object.defineProperty(d, 'ownReadonlyAccessor',
      { get : function() { return 15; }});
  d.mSloppy();
  d.mStrict();
}());


(function TestSetterInForIn() {
  var setCalled = 0;
  var getCalled = 0;
  function Base() {}
  Base.prototype = {
    constructor: Base,
    get x() {
      getCalled++;
      return 1;
    },
    set x(v) {
      setCalled++;
      this.x_.push(v);
    },
  };

  function Derived() {
    this.x_ = [];
  }
  Derived.prototype = {
    __proto__: Base.prototype,
    constructor: Derived,
  };

  Derived.prototype.testIter = function() {
    setCalled = 0;
    getCalled = 0;
    for (super.x in [1,2,3]) {}
    assertEquals(0, getCalled);
    assertEquals(3, setCalled);
    assertEquals(["0","1","2"], this.x_);
  }.toMethod(Derived.prototype);

  new Derived().testIter();

  var x = 'x';
  Derived.prototype.testIterKeyed = function() {
    setCalled = 0;
    getCalled = 0;
    for (super[x] in [1,2,3]) {}
    assertEquals(0, getCalled);
    assertEquals(3, setCalled);
    assertEquals(["0","1","2"], this.x_);

    this.x_ = [];
    setCalled = 0;
    getCalled = 0;
    var toStringCalled = 0;
    var o = {toString: function () { toStringCalled++; return x }};
    for (super[o] in [1,2,3]) {}
    assertEquals(0, getCalled);
    assertEquals(3, setCalled);
    assertEquals(3, toStringCalled);
    assertEquals(["0","1","2"], this.x_);
  }.toMethod(Derived.prototype);

  new Derived().testIterKeyed();
}());


(function TestKeyedSetterCreatingOwnProperties() {
  var ownReadOnly = 'ownReadOnly';
  var ownReadonlyAccessor = 'ownReadonlyAccessor';
  var ownSetter = 'ownSetter';
  function Base() {}
  function Derived() {}
  Derived.prototype = { __proto__ : Base.prototype };
  var setterCalled;

  Derived.prototype.mSloppy = function() {
    assertEquals(42, this[ownReadOnly]);
    super[ownReadOnly] = 55;
    assertEquals(42, this[ownReadOnly]);

    assertEquals(15, this[ownReadonlyAccessor]);
    super[ownReadonlyAccessor] = 55;
    assertEquals(15, this[ownReadonlyAccessor]);

    setterCalled = 0;
    super[ownSetter] = 42;
    assertEquals(1, setterCalled);
  }.toMethod(Derived.prototype);

  Derived.prototype.mStrict = function() {
    'use strict';
    assertEquals(42, this[ownReadOnly]);
    var ex;
    try {
      super[ownReadOnly] = 55;
    } catch(e) { ex = e; }
    assertTrue(ex instanceof TypeError);
    assertEquals(42, this[ownReadOnly]);

    assertEquals(15, this[ownReadonlyAccessor]);
    ex = null;
    try {
      super[ownReadonlyAccessor] = 55;
    } catch(e) { ex = e; }
    assertTrue(ex instanceof TypeError);
    assertEquals(15, this[ownReadonlyAccessor]);

    setterCalled = 0;
    super[ownSetter] = 42;
    assertEquals(1, setterCalled);
  }.toMethod(Derived.prototype);

  var d = new Derived();
  Object.defineProperty(d, 'ownReadOnly', { value : 42, writable : false });
  Object.defineProperty(d, 'ownSetter',
      { set : function() { setterCalled++; } });
  Object.defineProperty(d, 'ownReadonlyAccessor',
      { get : function() { return 15; }});
  d.mSloppy();
  d.mStrict();
}());


(function TestKeyedNumericSetterCreatingOwnProperties() {
  var ownReadOnly = 42;
  var ownReadonlyAccessor = 43;
  var ownSetter = 44;
  function Base() {}
  function Derived() {}
  Derived.prototype = { __proto__ : Base.prototype };
  var setterCalled;

  Derived.prototype.mSloppy = function() {
    assertEquals(42, this[ownReadOnly]);
    super[ownReadOnly] = 55;
    assertEquals(42, this[ownReadOnly]);

    assertEquals(15, this[ownReadonlyAccessor]);
    super[ownReadonlyAccessor] = 55;
    assertEquals(15, this[ownReadonlyAccessor]);

    setterCalled = 0;
    super[ownSetter] = 42;
    assertEquals(1, setterCalled);
  }.toMethod(Derived.prototype);

  Derived.prototype.mStrict = function() {
    'use strict';
    assertEquals(42, this[ownReadOnly]);
    var ex;
    try {
      super[ownReadOnly] = 55;
    } catch(e) { ex = e; }
    assertTrue(ex instanceof TypeError);
    assertEquals(42, this[ownReadOnly]);

    assertEquals(15, this[ownReadonlyAccessor]);
    ex = null;
    try {
      super[ownReadonlyAccessor] = 55;
    } catch(e) { ex = e; }
    assertTrue(ex instanceof TypeError);
    assertEquals(15, this[ownReadonlyAccessor]);

    setterCalled = 0;
    super[ownSetter] = 42;
    assertEquals(1, setterCalled);
  }.toMethod(Derived.prototype);

  var d = new Derived();
  Object.defineProperty(d, ownReadOnly, { value : 42, writable : false });
  Object.defineProperty(d, ownSetter,
      { set : function() { setterCalled++; } });
  Object.defineProperty(d, ownReadonlyAccessor,
      { get : function() { return 15; }});
  d.mSloppy();
  d.mStrict();
}());


(function TestSetterNoProtoWalk() {
  function Base() {}
  function Derived() {}
  var getCalled;
  var setCalled;
  Derived.prototype = {
    __proto__ : Base.prototype,
    get x() { getCalled++; return 42; },
    set x(v) { setCalled++; }
  };

  Derived.prototype.mSloppy = function() {
    setCalled = 0;
    getCalled = 0;
    assertEquals(42, this.x);
    assertEquals(1, getCalled);
    assertEquals(0, setCalled);

    getCalled = 0;
    setCalled = 0;
    this.x = 43;
    assertEquals(0, getCalled);
    assertEquals(1, setCalled);

    getCalled = 0;
    setCalled = 0;
    super.x = 15;
    assertEquals(0, setCalled);
    assertEquals(0, getCalled);

    assertEquals(15, this.x);
    assertEquals(0, getCalled);
    assertEquals(0, setCalled);

  }.toMethod(Derived.prototype);

  Derived.prototype.mStrict = function() {
    'use strict';
    setCalled = 0;
    getCalled = 0;
    assertEquals(42, this.x);
    assertEquals(1, getCalled);
    assertEquals(0, setCalled);

    getCalled = 0;
    setCalled = 0;
    this.x = 43;
    assertEquals(0, getCalled);
    assertEquals(1, setCalled);

    getCalled = 0;
    setCalled = 0;
    super.x = 15;
    assertEquals(0, setCalled);
    assertEquals(0, getCalled);

    assertEquals(15, this.x);
    assertEquals(0, getCalled);
    assertEquals(0, setCalled);

  }.toMethod(Derived.prototype);

  new Derived().mSloppy();
  new Derived().mStrict();
}());


(function TestKeyedSetterNoProtoWalk() {
  var x = 'x';
  function Base() {}
  function Derived() {}
  var getCalled;
  var setCalled;
  Derived.prototype = {
    __proto__ : Base.prototype,
    get x() { getCalled++; return 42; },
    set x(v) { setCalled++; }
  };

  Derived.prototype.mSloppy = function() {
    setCalled = 0;
    getCalled = 0;
    assertEquals(42, this[x]);
    assertEquals(1, getCalled);
    assertEquals(0, setCalled);

    getCalled = 0;
    setCalled = 0;
    this[x] = 43;
    assertEquals(0, getCalled);
    assertEquals(1, setCalled);

    getCalled = 0;
    setCalled = 0;
    super[x] = 15;
    assertEquals(0, setCalled);
    assertEquals(0, getCalled);

    assertEquals(15, this[x]);
    assertEquals(0, getCalled);
    assertEquals(0, setCalled);

  }.toMethod(Derived.prototype);

  Derived.prototype.mStrict = function() {
    'use strict';
    setCalled = 0;
    getCalled = 0;
    assertEquals(42, this[x]);
    assertEquals(1, getCalled);
    assertEquals(0, setCalled);

    getCalled = 0;
    setCalled = 0;
    this[x] = 43;
    assertEquals(0, getCalled);
    assertEquals(1, setCalled);

    getCalled = 0;
    setCalled = 0;
    super[x] = 15;
    assertEquals(0, setCalled);
    assertEquals(0, getCalled);

    assertEquals(15, this[x]);
    assertEquals(0, getCalled);
    assertEquals(0, setCalled);

  }.toMethod(Derived.prototype);

  new Derived().mSloppy();
  new Derived().mStrict();
}());


(function TestKeyedNumericSetterNoProtoWalk() {
  var x = 42;
  function Base() {}
  function Derived() {}
  var getCalled;
  var setCalled;
  Derived.prototype = {
    __proto__ : Base.prototype,
  };

  Object.defineProperty(Derived.prototype, x, {
    get: function() { getCalled++; return 42; },
    set: function(v) { setCalled++; }
  });

  Derived.prototype.mSloppy = function() {
    setCalled = 0;
    getCalled = 0;
    assertEquals(42, this[x]);
    assertEquals(1, getCalled);
    assertEquals(0, setCalled);

    getCalled = 0;
    setCalled = 0;
    this[x] = 43;
    assertEquals(0, getCalled);
    assertEquals(1, setCalled);

    getCalled = 0;
    setCalled = 0;
    super[x] = 15;
    assertEquals(0, setCalled);
    assertEquals(0, getCalled);

    assertEquals(15, this[x]);
    assertEquals(0, getCalled);
    assertEquals(0, setCalled);

  }.toMethod(Derived.prototype);

  Derived.prototype.mStrict = function() {
    'use strict';
    setCalled = 0;
    getCalled = 0;
    assertEquals(42, this[x]);
    assertEquals(1, getCalled);
    assertEquals(0, setCalled);

    getCalled = 0;
    setCalled = 0;
    this[x] = 43;
    assertEquals(0, getCalled);
    assertEquals(1, setCalled);

    getCalled = 0;
    setCalled = 0;
    super[x] = 15;
    assertEquals(0, setCalled);
    assertEquals(0, getCalled);

    assertEquals(15, this[x]);
    assertEquals(0, getCalled);
    assertEquals(0, setCalled);

  }.toMethod(Derived.prototype);

  new Derived().mSloppy();
  new Derived().mStrict();
}());


(function TestSetterDoesNotReconfigure() {
  function Base() {}
  function Derived() {}

  Derived.prototype.mStrict = function (){
    'use strict';
    super.nonEnumConfig = 5;
    var d1 = Object.getOwnPropertyDescriptor(this, 'nonEnumConfig');
    assertEquals(5, d1.value);
    assertTrue(d1.configurable);
    assertFalse(d1.enumerable);

    super.nonEnumNonConfig = 5;
    var d1 = Object.getOwnPropertyDescriptor(this, 'nonEnumNonConfig');
    assertEquals(5, d1.value);
    assertFalse(d1.configurable);
    assertFalse(d1.enumerable);
  }.toMethod(Derived.prototype);

  Derived.prototype.mSloppy = function (){
    super.nonEnumConfig = 42;
    var d1 = Object.getOwnPropertyDescriptor(this, 'nonEnumConfig');
    assertEquals(42, d1.value);
    assertTrue(d1.configurable);
    assertFalse(d1.enumerable);

    super.nonEnumNonConfig = 42;
    var d1 = Object.getOwnPropertyDescriptor(this, 'nonEnumNonConfig');
    assertEquals(42, d1.value);
    assertFalse(d1.configurable);
    assertFalse(d1.enumerable);
  }.toMethod(Derived.prototype);

  var d = new Derived();
  Object.defineProperty(d, 'nonEnumConfig',
      { value : 0, enumerable : false, configurable : true, writable : true });
  Object.defineProperty(d, 'nonEnumNonConfig',
      { value : 0, enumerable : false, configurable : false, writable : true });
  d.mStrict();
  d.mSloppy();
}());


(function TestKeyedSetterDoesNotReconfigure() {
  var nonEnumConfig = 'nonEnumConfig';
  var nonEnumNonConfig = 'nonEnumNonConfig';
  function Base() {}
  function Derived() {}

  Derived.prototype = { __proto__: Base.prototype };

  Derived.prototype.mStrict = function (){
    'use strict';
    super[nonEnumConfig] = 5;
    var d1 = Object.getOwnPropertyDescriptor(this, nonEnumConfig);
    assertEquals(5, d1.value);
    assertTrue(d1.configurable);
    assertFalse(d1.enumerable);

    super[nonEnumNonConfig] = 5;
    var d1 = Object.getOwnPropertyDescriptor(this, nonEnumNonConfig);
    assertEquals(5, d1.value);
    assertFalse(d1.configurable);
    assertFalse(d1.enumerable);
  }.toMethod(Derived.prototype);

  Derived.prototype.mSloppy = function (){
    super[nonEnumConfig] = 42;
    var d1 = Object.getOwnPropertyDescriptor(this, nonEnumConfig);
    assertEquals(42, d1.value);
    assertTrue(d1.configurable);
    assertFalse(d1.enumerable);

    super[nonEnumNonConfig] = 42;
    var d1 = Object.getOwnPropertyDescriptor(this, nonEnumNonConfig);
    assertEquals(42, d1.value);
    assertFalse(d1.configurable);
    assertFalse(d1.enumerable);
  }.toMethod(Derived.prototype);

  var d = new Derived();
  Object.defineProperty(d, nonEnumConfig,
      { value : 0, enumerable : false, configurable : true, writable : true });
  Object.defineProperty(d, nonEnumNonConfig,
      { value : 0, enumerable : false, configurable : false, writable : true });
  d.mStrict();
  d.mSloppy();
}());


(function TestKeyedNumericSetterDoesNotReconfigure() {
  var nonEnumConfig = 42;
  var nonEnumNonConfig = 43;
  function Base() {}
  function Derived() {}

  Derived.prototype = { __proto__: Base.prototype };

  Derived.prototype.mStrict = function (){
    'use strict';
    super[nonEnumConfig] = 5;
    var d1 = Object.getOwnPropertyDescriptor(this, nonEnumConfig);
    assertEquals(5, d1.value);
    assertTrue(d1.configurable);
    assertFalse(d1.enumerable);

    super[nonEnumNonConfig] = 5;
    var d1 = Object.getOwnPropertyDescriptor(this, nonEnumNonConfig);
    assertEquals(5, d1.value);
    assertFalse(d1.configurable);
    assertFalse(d1.enumerable);
  }.toMethod(Derived.prototype);

  Derived.prototype.mSloppy = function (){
    super[nonEnumConfig] = 42;
    var d1 = Object.getOwnPropertyDescriptor(this, nonEnumConfig);
    assertEquals(42, d1.value);
    assertTrue(d1.configurable);
    assertFalse(d1.enumerable);

    super[nonEnumNonConfig] = 42;
    var d1 = Object.getOwnPropertyDescriptor(this, nonEnumNonConfig);
    assertEquals(42, d1.value);
    assertFalse(d1.configurable);
    assertFalse(d1.enumerable);
  }.toMethod(Derived.prototype);

  var d = new Derived();
  Object.defineProperty(d, nonEnumConfig,
      { value : 0, enumerable : false, configurable : true, writable : true });
  Object.defineProperty(d, nonEnumNonConfig,
      { value : 0, enumerable : false, configurable : false, writable : true });
  d.mStrict();
  d.mSloppy();
}());


(function TestCountOperations() {
  function Base() {}
  Base.prototype = {
    constructor: Base,
    get x() {
      return this._x;
    },
    set x(v) {
      this._x = v;
    },
    _x: 1
  };

  function Derived() {}
  Derived.__proto__ = Base;
  Derived.prototype = {
    __proto__: Base.prototype,
    constructor: Derived,
    _x: 2
  };

  Derived.prototype.testCounts = function() {
    assertEquals(2, this._x);
    assertEquals(2, super.x);
    super.x++;
    assertEquals(3, super.x);
    ++super.x;
    assertEquals(4, super.x);
    assertEquals(4, super.x++);
    assertEquals(5, super.x);
    assertEquals(6, ++super.x);
    assertEquals(6, super.x);
    assertEquals(6, this._x);

    super.x--;
    assertEquals(5, super.x);
    --super.x;
    assertEquals(4, super.x);
    assertEquals(4, super.x--);
    assertEquals(3, super.x);
    assertEquals(2, --super.x);
    assertEquals(2, super.x);
    assertEquals(2, this._x);
  }.toMethod(Derived.prototype);
  new Derived().testCounts();
}());


(function TestKeyedCountOperations() {
  var x = 'x';
  function Base() {}
  Base.prototype = {
    constructor: Base,
    get x() {
      return this._x;
    },
    set x(v) {
      this._x = v;
    },
    _x: 1
  };

  function Derived() {}
  Derived.__proto__ = Base;
  Derived.prototype = {
    __proto__: Base.prototype,
    constructor: Derived,
    _x: 2
  };

  Derived.prototype.testCounts = function() {
    assertEquals(2, this._x);
    assertEquals(2, super[x]);
    super[x]++;
    assertEquals(3, super[x]);
    ++super[x];
    assertEquals(4, super[x]);
    assertEquals(4, super[x]++);
    assertEquals(5, super[x]);
    assertEquals(6, ++super[x]);
    assertEquals(6, super[x]);
    assertEquals(6, this._x);

    super[x]--;
    assertEquals(5, super[x]);
    --super[x];
    assertEquals(4, super[x]);
    assertEquals(4, super[x]--);
    assertEquals(3, super[x]);
    assertEquals(2, --super[x]);
    assertEquals(2, super[x]);
    assertEquals(2, this._x);
  }.toMethod(Derived.prototype);
  new Derived().testCounts();
}());


(function TestKeyedNumericCountOperations() {
  var x = 42;
  function Base() {}
  Base.prototype = {
    constructor: Base,
    _x: 1
  };

  Object.defineProperty(Base.prototype, x, {
    get: function() { return this._x; },
    set: function(v) { this._x = v;; }
  });

  function Derived() {}
  Derived.__proto__ = Base;
  Derived.prototype = {
    __proto__: Base.prototype,
    constructor: Derived,
    _x: 2
  };

  Derived.prototype.testCounts = function() {
    assertEquals(2, this._x);
    assertEquals(2, super[x]);
    super[x]++;
    assertEquals(3, super[x]);
    ++super[x];
    assertEquals(4, super[x]);
    assertEquals(4, super[x]++);
    assertEquals(5, super[x]);
    assertEquals(6, ++super[x]);
    assertEquals(6, super[x]);
    assertEquals(6, this._x);

    super[x]--;
    assertEquals(5, super[x]);
    --super[x];
    assertEquals(4, super[x]);
    assertEquals(4, super[x]--);
    assertEquals(3, super[x]);
    assertEquals(2, --super[x]);
    assertEquals(2, super[x]);
    assertEquals(2, this._x);
  }.toMethod(Derived.prototype);
  new Derived().testCounts();
}());


(function TestSetterSuperNonWritable() {
  function Base() {}
  Object.defineProperty(Base.prototype, 'x', { value : 27, writable: false });
  function Derived() {}

  Derived.prototype = { __proto__: Base.prototype, constructor: Derived };

  Derived.prototype.mSloppy = function() {
    assertEquals(27, super.x);
    assertEquals(27, this.x);
    super.x = 10;
    assertEquals(27, super.x);
    assertEquals(27, this.x);
  }.toMethod(Derived.prototype);
  Derived.prototype.mStrict = function() {
    'use strict';
    assertEquals(27, super.x);
    assertEquals(27, this.x);
    var ex = null;
    try { super.x = 10; } catch(e) { ex = e; }
    assertTrue(ex instanceof TypeError);
    assertEquals(27, super.x);
    assertEquals(27, this.x);
  }.toMethod(Derived.prototype);
  new Derived().mSloppy();
  new Derived().mStrict();
}());


(function TestSetterKeyedSuperNonWritable() {
  var x = 'xyz';
  function Base() {}
  Object.defineProperty(Base.prototype, x, { value : 27, writable: false });
  function Derived() {}

  Derived.prototype = { __proto__: Base.prototype, constructor: Derived };

  Derived.prototype.mSloppy = function() {
    assertEquals(27, super[x]);
    assertEquals(27, this[x]);
    super[x] = 10;
    assertEquals(27, super[x]);
    assertEquals(27, this[x]);
  }.toMethod(Derived.prototype);
  Derived.prototype.mStrict = function() {
    'use strict';
    assertEquals(27, super[x]);
    assertEquals(27, this[x]);
    var ex = null;
    try { super[x] = 10; } catch(e) { ex = e; }
    assertTrue(ex instanceof TypeError);
    assertEquals(27, super[x]);
    assertEquals(27, this[x]);
  }.toMethod(Derived.prototype);
  new Derived().mSloppy();
  new Derived().mStrict();
}());


(function TestSetterKeyedNumericSuperNonWritable() {
  var x = 42;
  function Base() {}
  Object.defineProperty(Base.prototype, x, { value : 27, writable: false });
  function Derived() {}

  Derived.prototype = { __proto__: Base.prototype, constructor: Derived };

  Derived.prototype.mSloppy = function() {
    assertEquals(27, super[x]);
    assertEquals(27, this[x]);
    super[x] = 10;
    assertEquals(27, super[x]);
    assertEquals(27, this[x]);
  }.toMethod(Derived.prototype);
  Derived.prototype.mStrict = function() {
    'use strict';
    assertEquals(27, super[x]);
    assertEquals(27, this[x]);
    var ex = null;
    try { super[x] = 10; } catch(e) { ex = e; }
    assertTrue(ex instanceof TypeError);
    assertEquals(27, super[x]);
    assertEquals(27, this[x]);
  }.toMethod(Derived.prototype);
  new Derived().mSloppy();
  new Derived().mStrict();
}());


function Subclass(base, constructor) {
  var homeObject = {
    __proto__: base.prototype,
    constructor: constructor
  };
  constructor.__proto__ = base;
  constructor.prototype = homeObject;
  // not doing toMethod: home object is not required for
  // super constructor calls.
  return constructor;
}

(function TestSuperCall() {
  var baseCalled = 0;
  var derivedCalled = 0;
  var derivedDerivedCalled = 0;

  function Base() {
    baseCalled++;
  }

  var Derived = Subclass(Base, function () {
    super();
    derivedCalled++;
  });

  assertEquals(Base, Base.prototype.constructor);
  assertEquals(Base.prototype, Derived.prototype.__proto__);

  baseCalled = 0;
  derivedCalled = 0;
  new Derived();
  assertEquals(1, baseCalled);
  assertEquals(1, derivedCalled);

  var DerivedDerived = Subclass(Derived, function () {
    super();
    derivedDerivedCalled++;
  });

  baseCalled = 0;
  derivedCalled = 0;
  derivedDerivedCalled = 0;
  new DerivedDerived();
  assertEquals(1, baseCalled);
  assertEquals(1, derivedCalled);
  assertEquals(1, derivedDerivedCalled);

  function Base2(v) {
    this.fromBase = v;
  }
  var Derived2 = Subclass(Base2, function (v1, v2) {
    super(v1);
    this.fromDerived = v2;
  });

  var d = new Derived2("base", "derived");
  assertEquals("base", d.fromBase);
  assertEquals("derived", d.fromDerived);

  function ImplicitSubclassOfFunction() {
    super();
    this.x = 123;
  }

  var o = new ImplicitSubclassOfFunction();
  assertEquals(123, o.x);

  var calls = 0;
  function G() {
    calls++;
  }
  function F() {
    super();
  }
  F.__proto__ = G;
  new F();
  assertEquals(1, calls);
  F.__proto__ = function() {};
  new F();
  assertEquals(1, calls);
}());


(function TestNewSuper() {
  var baseCalled = 0;
  var derivedCalled = 0;

  function Base() {
    baseCalled++;
    this.x = 15;
  }


  var Derived = Subclass(Base, function() {
    baseCalled = 0;
    var b = new super();
    assertEquals(1, baseCalled)
    assertEquals(Base.prototype, b.__proto__);
    assertEquals(15, b.x);
    assertEquals(undefined, this.x);
    derivedCalled++;
  });

  derivedCalled = 0;
  new Derived();
  assertEquals(1, derivedCalled);
}());


(function TestSuperCallErrorCases() {
  function T() {
    super();
  }
  T.__proto__ = null;
  // Spec says ReferenceError here, but for other IsCallable failures
  // we throw TypeError.
  // Filed https://bugs.ecmascript.org/show_bug.cgi?id=3282
  assertThrows(function() { new T(); }, TypeError);

  function T1() {
    var b = new super();
  }
  T1.__proto = null;
  assertThrows(function() { new T1(); }, TypeError);
}());
