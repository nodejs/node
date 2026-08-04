// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function TestSuperNamedLoads() {
  function Base() { }
  function fBase() { }
  Base.prototype = {
    f() {
      return "Base " + this.toString();
    },
    x: 15,
    toString() {
      return "this is Base";
    }
  };

  function Derived() {
    this.derivedDataProperty = "xxx";
  }
  Derived.prototype = {
    __proto__: Base.prototype,
    toString() { return "this is Derived"; },
    x: 27,
    f() {
      assertEquals("Base this is Derived", super.f());
      var a = super.x;
      assertEquals(15, a);
      assertEquals(15, super.x);
      assertEquals(27, this.x);
      return "Derived";
    }
  };

  assertEquals("Base this is Base", new Base().f());
  assertEquals("Derived", new Derived().f());
}());


(function TestSuperKeyedLoads() {
  'use strict';

  var x = 'x';
  var derivedDataProperty = 'derivedDataProperty';
  var f = 'f';

  class Base {
    f() {
      return "Base " + this.toString();
    }
    toString() {
      return "this is Base";
    }
  }

  Base.prototype[x] = 15;

  function Derived() {
    this[derivedDataProperty] = "xxx";
  }
  Derived.prototype = {
    __proto__: Base.prototype,
    toString() { return "this is Derived"; },
    x: 27,
    f() {
      assertEquals("Base this is Derived", super[f]());
      var a = super[x];
      assertEquals(15, a);
      assertEquals(15, super[x]);
      assertEquals(27, this[x]);
      return "Derived";
    }
  };

  assertEquals("Base this is Base", new Base().f());
  assertEquals("Derived", new Derived().f());
}());


(function TestSuperKeywordNonMethod() {
  'use strict';

  class C {
    f() {
      super.unknown();
    }
  }

  assertThrows(function() {
    new C().f();
  }, TypeError);
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
    _x: 'derived',
    testGetter() {
      return super.x;
    },
    testGetterStrict() {
      'use strict';
      return super.x;
    }
  };

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
    _x: 'derived',
    testGetter() {
      return super[x];
    },
    testGetterStrict() {
      'use strict';
      return super[x];
    },
    testGetterWithToString() {
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
    }
  };

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
    _x: 'derived',
    testGetter() {
      return super[x];
    },
    testGetterStrict() {
      'use strict';
      return super[x];
    },
    testGetterWithToString() {
      var toStringCalled;
      var o = {
        toString: function() {
          toStringCalled++;
          return '42';
        }
      };

      toStringCalled = 0;
      assertEquals('derived', super[o]);
      assertEquals(1, toStringCalled);

      var eToThrow = new Error();
      var oThrowsInToString = {
        toString: function() {
          throw eToThrow;
        }
      };

      var ex = null;
      try {
        super[oThrowsInToString];
      } catch(e) { ex = e }
      assertEquals(eToThrow, ex);

      var oReturnsNumericString = {
        toString: function() {
          return "42";
        }
      };

      assertEquals('derived', super[oReturnsNumericString]);
      assertEquals('derived', super[42]);
    }
  };

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
    _x: 'derived',
    testSetter() {
      assertEquals('foobar', super.x = 'foobar');
      assertEquals('foobarabc', super.x += 'abc');
    },
    testSetterStrict() {
      'use strict';
      assertEquals('foobar', super.x = 'foobar');
      assertEquals('foobarabc', super.x += 'abc');
    }
  };

  var d = new Derived();
  d.testSetter();
  assertEquals('base', Base.prototype._x);
  assertEquals('foobarabc', d._x);
  d._x = '';

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
    _x: 'derived',
    testSetter() {
      assertEquals('foobar', super[x] = 'foobar');
      assertEquals('foobarabc', super[x] += 'abc');
    },
    testSetterStrict() {
      'use strict';
      assertEquals('foobar', super[x] = 'foobar');
      assertEquals('foobarabc', super[x] += 'abc');
    },
    testSetterWithToString() {
      var toStringCalled;
      var o = {
        toString: function() {
          toStringCalled++;
          return x;
        }
      };

      toStringCalled = 0;
      super[o] = 'set';
      assertEquals(1, toStringCalled);
      assertEquals('set', this._x);

      var eToThrow = new Error();
      var oThrowsInToString = {
        toString: function() {
          throw eToThrow;
        }
      };

      var ex = null;
      try {
        super[oThrowsInToString] = 'xyz';
      } catch(e) { ex = e }
      assertEquals(eToThrow, ex);
      assertEquals('set', this._x);
    }
  };

  var d = new Derived();
  d.testSetter();
  assertEquals('base', Base.prototype._x);
  assertEquals('foobarabc', d._x);
  d._x = '';

  d.testSetterStrict();
  assertEquals('base', Base.prototype._x);
  assertEquals('foobarabc', d._x);

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
    _x: 'derived',
    testSetter() {
      assertEquals('foobar', super[x] = 'foobar');
      assertEquals('foobarabc', super[x] += 'abc');
    },
    testSetterStrict() {
      'use strict';
      assertEquals('foobar', super[x] = 'foobar');
      assertEquals('foobarabc', super[x] += 'abc');
    },
    testSetterWithToString() {
      var toStringCalled;
      var o = {
        toString: function() {
          toStringCalled++;
          return 'x';
        }
      };

      toStringCalled = 0;
      super[o] = 'set';
      assertEquals(1, toStringCalled);
      assertEquals('set', this._x);

      var eToThrow = new Error();
      var oThrowsInToString = {
        toString: function() {
          throw eToThrow;
        }
      };

      var ex = null;
      try {
        super[oThrowsInToString] = 'xyz';
      } catch(e) { ex = e }
      assertEquals(eToThrow, ex);
      assertEquals('set', this._x);

      var oReturnsNumericString = {
        toString: function() {
          return "1";
        }
      };

      assertEquals('abc', super[oReturnsNumericString] = 'abc');

      assertEquals('set', this._x);

      assertEquals(10,  super[1] = 10);
    }
  };

  var d = new Derived();
  d.testSetter();
  assertEquals('base', Base.prototype._x);
  assertEquals('foobarabc', d._x);
  d._x = '';
  d.testSetterStrict();
  assertEquals('base', Base.prototype._x);
  assertEquals('foobarabc', d._x);

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
    testSetter() {
      assertEquals('x from Base', super.x);
      super.x = 'data property';
      assertEquals('x from Base', super.x);
      assertEquals('data property', this.x);
    }
  };

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
    testSetter() {
      assertEquals('x from Base', super[x]);
      super[x] = 'data property';
      assertEquals('x from Base', super[x]);
      assertEquals('data property', this[x]);
    }
  };

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
    testSetter() {
      assertEquals('x from Base', super[x]);
      super[x] = 'data property';
      assertEquals('x from Base', super[x]);
      assertEquals('data property', this[x]);
    }
  };

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
    testSetter() {
      setCalled = 0;
      getCalled = 0;
      assertEquals('object', typeof this);
      assertInstanceof(this, Number)
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
    },
    testSetterStrict() {
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
      assertInstanceof(ex, TypeError);
    }
  }

  Derived.prototype.testSetter.call(42);
  Derived.prototype.testSetterStrict.call(42);

  function DerivedFromString() {}
  DerivedFromString.prototype = {
    __proto__: String.prototype,
    f() {
      'use strict';
      assertTrue(42 === this);
      assertEquals(String.prototype.toString, super.toString);
      var ex;
      try {
        super.toString();
      } catch(e) { ex = e; }

      assertInstanceof(ex, TypeError);
    }
  };

  DerivedFromString.prototype.f.call(42);
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
    testSetter() {
      setCalled = 0;
      getCalled = 0;
      assertEquals('object', typeof this);
      assertInstanceof(this, Number)
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
    },
    testSetterStrict() {
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
      assertInstanceof(ex,TypeError);
    }
  };

  Derived.prototype.testSetter.call(42);
  Derived.prototype.testSetterStrict.call(42);

  function DerivedFromString() {}
  DerivedFromString.prototype = {
    __proto__: String.prototype,
    f() {
      'use strict';
      assertTrue(42 === this);
      assertEquals(String.prototype.toString, super[toString]);
      var ex;
      try {
        super[toString]();
      } catch(e) { ex = e; }

      assertInstanceof(ex, TypeError);
    }
  };
  DerivedFromString.prototype.f.call(42);
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
    testSetter() {
      setCalled = 0;
      getCalled = 0;
      assertEquals('object', typeof this);
      assertInstanceof(this, Number)
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
    },
    testSetterStrict() {
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
      assertInstanceof(ex, TypeError);
    }
  };

  Derived.prototype.testSetter.call(42);
  Derived.prototype.testSetterStrict.call(42);
}());


(function TestKeyedNumericSetterOnExotics() {
  function Base() {}
  function Derived() {}
  Derived.prototype = {
    __proto__: Base.prototype,
    callSetterOnArray() {
      super[42] = 1;
    },
    callStrictSetterOnString() {
      'use strict';
      assertEquals('string', typeof this);
      assertTrue('abcdef' === this);
      var ex = null;
      try {
        super[5] = 'q';
      } catch(e) { ex = e; }
      assertInstanceof(ex, TypeError);

      ex = null;
      try {
        super[1024] = 'q';
      } catch(e) { ex = e; }
      assertInstanceof(ex, TypeError);
    }
  };

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
  Derived.prototype = {
    __proto__: Base.prototype,
    mSloppy() {
      assertEquals(undefined, super.x);
      assertEquals(undefined, this.x);
      super.x = 10;
      assertEquals(10, this.x);
      assertEquals(undefined, super.x);
    },
    mStrict() {
      'use strict';
      assertEquals(undefined, super.x);
      assertEquals(undefined, this.x);
      super.x = 10;
      assertEquals(10, this.x);
      assertEquals(undefined, super.x);
    }
  };

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
  Derived.prototype = {
    __proto__: Base.prototype,
    mSloppy() {
      assertEquals(undefined, super[x]);
      assertEquals(undefined, this[x]);
      super[x] = 10;
      assertEquals(10, this[x]);
      assertEquals(undefined, super[x]);
    },
    mStrict() {
      'use strict';
      assertEquals(undefined, super[x]);
      assertEquals(undefined, this[x]);
      super[x] = 10;
      assertEquals(10, this[x]);
      assertEquals(undefined, super[x]);
    }
  };
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
  Derived.prototype = {
    __proto__: Base.prototype,
    mSloppy() {
      assertEquals(undefined, super[x]);
      assertEquals(undefined, this[x]);
      super[x] = 10;
      assertEquals(10, this[x]);
      assertEquals(undefined, super[x]);
    },
    mStrict() {
      'use strict';
      assertEquals(undefined, super[x]);
      assertEquals(undefined, this[x]);
      super[x] = 10;
      assertEquals(10, this[x]);
      assertEquals(undefined, super[x]);
    }
  };
  var d = new Derived();
  d.mSloppy();
  assertEquals(10, d[x]);
  var d1 = new Derived();
  d1.mStrict();
  assertEquals(10, d[x]);
}());


(function TestSetterCreatingOwnPropertiesReconfigurable() {
  function Base() {}
  function Derived() {}
  Derived.prototype = {
    __proto__: Base.prototype,
    mSloppy() {
      assertEquals(42, this.ownReadOnly);
      super.ownReadOnly = 55;
      assertSame(undefined, super.ownReadOnly);
      assertEquals(42, this.ownReadOnly);
      assertFalse(Base.prototype.hasOwnProperty('ownReadOnly'));

      assertEquals(15, this.ownReadonlyAccessor);
      super.ownReadonlyAccessor = 25;
      assertSame(undefined, super.ownReadonlyAccessor);
      assertEquals(15, this.ownReadonlyAccessor);
      assertFalse(Base.prototype.hasOwnProperty('ownReadonlyAccessor'));

      super.ownSetter = 35;
      assertSame(undefined, super.ownSetter);
      var descr = Object.getOwnPropertyDescriptor(this, 'ownSetter');
      assertTrue('set' in descr);
      assertFalse(Base.prototype.hasOwnProperty('ownSetter'));
    },
    mStrict() {
      'use strict';
      assertEquals(42, this.ownReadOnly);
      assertThrows(() => {super.ownReadOnly = 55}, TypeError);
      assertSame(undefined, super.ownReadOnly);
      assertEquals(42, this.ownReadOnly);
      assertFalse(Base.prototype.hasOwnProperty('ownReadOnly'));

      assertEquals(15, this.ownReadonlyAccessor);
      assertThrows(() => {super.ownReadonlyAccessor = 25}, TypeError);
      assertSame(undefined, super.ownReadonlyAccessor);
      assertEquals(15, this.ownReadonlyAccessor);
      assertFalse(Base.prototype.hasOwnProperty('ownReadonlyAccessor'));

      assertThrows(() => {super.ownSetter = 35}, TypeError);
      assertSame(undefined, super.ownSetter);
      var descr = Object.getOwnPropertyDescriptor(this, 'ownSetter');
      assertTrue('set' in descr);
      assertFalse(Base.prototype.hasOwnProperty('ownSetter'));
    },
  };

  var d = new Derived();
  Object.defineProperty(d, 'ownReadOnly', {
    value: 42,
    writable: false,
    configurable: true
  });
  Object.defineProperty(d, 'ownSetter', {
    set: function() { assertUnreachable(); },
    configurable: true
  });
  Object.defineProperty(d, 'ownReadonlyAccessor', {
    get: function() { return 15; },
    configurable: true
  });

  d.mSloppy();

  var d = new Derived();
  Object.defineProperty(d, 'ownReadOnly', {
    value: 42,
    writable: false,
    configurable: true
  });
  Object.defineProperty(d, 'ownSetter', {
    set: function() { assertUnreachable(); },
    configurable: true
  });
  Object.defineProperty(d, 'ownReadonlyAccessor', {
    get: function() { return 15; },
    configurable: true
  });
  d.mStrict();
}());


(function TestSetterCreatingOwnPropertiesNonConfigurable() {
  function Base() {}
  function Derived() {}
  Derived.prototype = {
    __proto__: Base.prototype,
    mSloppy() {
      assertEquals(42, this.ownReadOnly);
      super.ownReadOnly = 55;
      assertEquals(42, this.ownReadOnly);
      var descr = Object.getOwnPropertyDescriptor(this, 'ownReadOnly');
      assertEquals(42, descr.value);
      assertFalse(descr.configurable);
      assertFalse(descr.enumerable);
      assertFalse(descr.writable);
      assertFalse(Base.prototype.hasOwnProperty('ownReadOnly'));

      assertEquals(15, this.ownReadonlyAccessor);
      super.ownReadonlyAccessor = 25;
      assertSame(undefined, super.ownReadonlyAccessor);
      assertEquals(15, this.ownReadonlyAccessor);
      var descr = Object.getOwnPropertyDescriptor(this, 'ownReadonlyAccessor');
      assertFalse(descr.configurable);
      assertFalse(descr.enumerable);
      assertFalse(Base.prototype.hasOwnProperty('ownReadonlyAccessor'));

      super.ownSetter = 35;
      var descr = Object.getOwnPropertyDescriptor(this, 'ownSetter');
      assertFalse(descr.configurable);
      assertFalse(descr.enumerable);
      assertFalse(Base.prototype.hasOwnProperty('ownSetter'));
    },
    mStrict() {
      'use strict';
      var ex;
      assertEquals(42, this.ownReadOnly);
      try {
        super.ownReadOnly = 55;
      } catch (e) {
        ex = e;
      }
      assertInstanceof(ex, TypeError);
      assertEquals(
          "Cannot assign to read only property 'ownReadOnly' of object '#<Base>'",
          ex.message);
      assertEquals(42, this.ownReadOnly);

      ex = null;
      assertEquals(15, this.ownReadonlyAccessor);
      try {
        super.ownReadonlyAccessor = 25;
      } catch (e) {
        ex = e;
      }
      assertInstanceof(ex, TypeError);
      assertEquals('Cannot redefine property: ownReadonlyAccessor', ex.message);
      assertEquals(15, this.ownReadonlyAccessor);

      ex = null;
      try {
        super.ownSetter = 35;
      } catch (e) {
        ex = e;
      }
      assertInstanceof(ex, TypeError);
      assertEquals('Cannot redefine property: ownSetter', ex.message);
    }
  };

  var d = new Derived();
  Object.defineProperty(d, 'ownReadOnly', { value : 42, writable : false });
  Object.defineProperty(d, 'ownSetter',
      { set : function() { assertUnreachable(); } });
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
    testIter() {
      setCalled = 0;
      getCalled = 0;
      for (super.x in [1,2,3]) {}
      assertEquals(0, getCalled);
      assertEquals(3, setCalled);
      assertEquals(["0", "1", "2"], this.x_);
    },
    testIterKeyed() {
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
    }
  };

  new Derived().testIter();

  var x = 'x';

  new Derived().testIterKeyed();
}());


function TestKeyedSetterCreatingOwnPropertiesReconfigurable(ownReadOnly,
    ownReadonlyAccessor, ownSetter) {
  function Base() {}
  function Derived() {}
  Derived.prototype = {
    __proto__: Base.prototype,
    mSloppy() {
      assertEquals(42, this[ownReadOnly]);
      super[ownReadOnly] = 55;
      assertSame(undefined, super[ownReadOnly]);
      assertEquals(42, this[ownReadOnly]);
      assertFalse(Base.prototype.hasOwnProperty(ownReadOnly));

      assertEquals(15, this[ownReadonlyAccessor]);
      super[ownReadonlyAccessor] = 25;
      assertSame(undefined, super[ownReadonlyAccessor]);
      assertEquals(15, this[ownReadonlyAccessor]);
      assertFalse(Base.prototype.hasOwnProperty(ownReadonlyAccessor));

      super[ownSetter] = 35;
      assertSame(undefined, super[ownSetter]);
      var descr = Object.getOwnPropertyDescriptor(this, ownSetter);
      assertTrue('set' in descr);
      assertFalse(Base.prototype.hasOwnProperty(ownSetter));
    },
    mStrict() {
      'use strict';
      assertEquals(42, this[ownReadOnly]);
      assertThrows(() => {super[ownReadOnly] = 55}, TypeError);
      assertSame(undefined, super[ownReadOnly]);
      assertEquals(42, this[ownReadOnly]);
      assertFalse(Base.prototype.hasOwnProperty(ownReadOnly));

      assertEquals(15, this[ownReadonlyAccessor]);
      assertThrows(() => {super[ownReadonlyAccessor] = 25}, TypeError);
      assertSame(undefined, super[ownReadonlyAccessor]);
      assertEquals(15, this[ownReadonlyAccessor]);
      assertFalse(Base.prototype.hasOwnProperty(ownReadonlyAccessor));

      assertThrows(() => {super[ownSetter] = 35}, TypeError);
      assertSame(undefined, super[ownSetter]);
      var descr = Object.getOwnPropertyDescriptor(this, ownSetter);
      assertTrue('set' in descr);
      assertFalse(Base.prototype.hasOwnProperty(ownSetter));
    },
  };

  var d = new Derived();
  Object.defineProperty(d, ownReadOnly, {
    value: 42,
    writable: false,
    configurable: true
  });
  Object.defineProperty(d, ownSetter, {
    set: function() { assertUnreachable(); },
    configurable: true
  });
  Object.defineProperty(d, ownReadonlyAccessor, {
    get: function() { return 15; },
    configurable: true
  });

  d.mSloppy();

  var d = new Derived();
  Object.defineProperty(d, ownReadOnly, {
    value: 42,
    writable: false,
    configurable: true
  });
  Object.defineProperty(d, ownSetter, {
    set: function() { assertUnreachable(); },
    configurable: true
  });
  Object.defineProperty(d, ownReadonlyAccessor, {
    get: function() { return 15; },
    configurable: true
  });
  d.mStrict();
}
TestKeyedSetterCreatingOwnPropertiesReconfigurable('ownReadOnly',
                                                   'ownReadonlyAccessor',
                                                   'ownSetter');
TestKeyedSetterCreatingOwnPropertiesReconfigurable(42, 43, 44);


function TestKeyedSetterCreatingOwnPropertiesNonConfigurable(
    ownReadOnly, ownReadonlyAccessor, ownSetter) {
  function Base() {}
  function Derived() {}
  Derived.prototype = {
    __proto__: Base.prototype,
    mSloppy() {
      assertEquals(42, this[ownReadOnly]);
      super[ownReadOnly] = 55;
      assertEquals(42, this[ownReadOnly]);
      var descr = Object.getOwnPropertyDescriptor(this, ownReadOnly);
      assertEquals(42, descr.value);
      assertFalse(descr.configurable);
      assertFalse(descr.enumerable);
      assertFalse(descr.writable);
      assertFalse(Base.prototype.hasOwnProperty(ownReadOnly));

      assertEquals(15, this[ownReadonlyAccessor]);
      super[ownReadonlyAccessor] = 25;
      assertSame(undefined, super[ownReadonlyAccessor]);
      assertEquals(15, this[ownReadonlyAccessor]);
      var descr = Object.getOwnPropertyDescriptor(this, ownReadonlyAccessor);
      assertFalse(descr.configurable);
      assertFalse(descr.enumerable);
      assertFalse(Base.prototype.hasOwnProperty(ownReadonlyAccessor));

      super[ownSetter] = 35;
      var descr = Object.getOwnPropertyDescriptor(this, ownSetter);
      assertFalse(descr.configurable);
      assertFalse(descr.enumerable);
      assertFalse(Base.prototype.hasOwnProperty(ownSetter));
    },
    mStrict() {
      'use strict';
      var ex;
      assertEquals(42, this[ownReadOnly]);
      try {
        super[ownReadOnly] = 55;
      } catch (e) {
        ex = e;
      }
      assertInstanceof(ex, TypeError);
      assertEquals(
          "Cannot assign to read only property '" + ownReadOnly +
              "' of object '#<Base>'",
          ex.message);
      assertEquals(42, this[ownReadOnly]);

      ex = null;
      assertEquals(15, this[ownReadonlyAccessor]);
      try {
        super[ownReadonlyAccessor] = 25;
      } catch (e) {
        ex = e;
      }
      assertInstanceof(ex, TypeError);
      assertEquals('Cannot redefine property: ' + ownReadonlyAccessor,
                   ex.message);
      assertEquals(15, this[ownReadonlyAccessor]);

      ex = null;
      try {
        super[ownSetter] = 35;
      } catch (e) {
        ex = e;
      }
      assertInstanceof(ex, TypeError);
      assertEquals('Cannot redefine property: ' + ownSetter, ex.message);
    }
  };

  var d = new Derived();
  Object.defineProperty(d, ownReadOnly, { value : 42, writable : false });
  Object.defineProperty(d, ownSetter,
      { set : function() { assertUnreachable(); } });
  Object.defineProperty(d, ownReadonlyAccessor,
      { get : function() { return 15; }});
  d.mSloppy();
  d.mStrict();
}
TestKeyedSetterCreatingOwnPropertiesNonConfigurable('ownReadOnly',
    'ownReadonlyAccessor', 'ownSetter');
TestKeyedSetterCreatingOwnPropertiesNonConfigurable(42, 43, 44);


(function TestSetterNoProtoWalk() {
  function Base() {}
  function Derived() {}
  var getCalled;
  var setCalled;
  Derived.prototype = {
    __proto__: Base.prototype,
    get x() { getCalled++; return 42; },
    set x(v) { setCalled++; },
    mSloppy() {
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
    },
    mStrict() {
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
    }
  };

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
    __proto__: Base.prototype,
    get x() { getCalled++; return 42; },
    set x(v) { setCalled++; },
    mSloppy() {
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
    },
    mStrict() {
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
    }
  };

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
    __proto__: Base.prototype,
    mSloppy() {
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
    },
    mStrict() {
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
    }
  };

  Object.defineProperty(Derived.prototype, x, {
    get: function() { getCalled++; return 42; },
    set: function(v) { setCalled++; }
  });

  new Derived().mSloppy();
  new Derived().mStrict();
}());


(function TestSetterDoesNotReconfigure() {
  function Base() {}
  function Derived() {}
  Derived.prototype = {
    __proto__: Derived.prototype,
    mStrict(){
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
    },
    mSloppy(){
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
    }
  };

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

  Derived.prototype = {
    __proto__: Base.prototype,
    mStrict(){
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
    },
    mSloppy(){
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
    }
  };

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

  Derived.prototype = {
    __proto__: Base.prototype,
    mStrict(){
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
    },
    mSloppy(){
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
    }
  };

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
    _x: 2,
    testCounts() {
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
    }
  };
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
    _x: 2,
    testCounts() {
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
    }
  };
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
    _x: 2,
    testCounts() {
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
    }
  };
  new Derived().testCounts();
}());


(function TestSetterSuperNonWritable() {
  function Base() {}
  Object.defineProperty(Base.prototype, 'x', { value : 27, writable: false });
  function Derived() {}
  Derived.prototype = {
    __proto__: Base.prototype,
    constructor: Derived,
    mSloppy() {
      assertEquals(27, super.x);
      assertEquals(27, this.x);
      super.x = 10;
      assertEquals(27, super.x);
      assertEquals(27, this.x);
    },
    mStrict() {
      'use strict';
      assertEquals(27, super.x);
      assertEquals(27, this.x);
      var ex = null;
      try { super.x = 10; } catch(e) { ex = e; }
      assertInstanceof(ex, TypeError);
      assertEquals(27, super.x);
      assertEquals(27, this.x);
    }
  };
  new Derived().mSloppy();
  new Derived().mStrict();
}());


(function TestSetterKeyedSuperNonWritable() {
  var x = 'xyz';
  function Base() {}
  Object.defineProperty(Base.prototype, x, { value : 27, writable: false });
  function Derived() {}

  Derived.prototype = {
    __proto__: Base.prototype,
    constructor: Derived,
    mSloppy() {
      assertEquals(27, super[x]);
      assertEquals(27, this[x]);
      super[x] = 10;
      assertEquals(27, super[x]);
      assertEquals(27, this[x]);
    },
    mStrict() {
      'use strict';
      assertEquals(27, super[x]);
      assertEquals(27, this[x]);
      var ex = null;
      try { super[x] = 10; } catch(e) { ex = e; }
      assertInstanceof(ex, TypeError);
      assertEquals(27, super[x]);
      assertEquals(27, this[x]);
    }
  };
  new Derived().mSloppy();
  new Derived().mStrict();
}());


(function TestSetterKeyedNumericSuperNonWritable() {
  var x = 42;
  function Base() {}
  Object.defineProperty(Base.prototype, x, { value : 27, writable: false });
  function Derived() {}

  Derived.prototype = {
    __proto__: Base.prototype,
    constructor: Derived,
    mSloppy() {
      assertEquals(27, super[x]);
      assertEquals(27, this[x]);
      super[x] = 10;
      assertEquals(27, super[x]);
      assertEquals(27, this[x]);
    },
    mStrict() {
      'use strict';
      assertEquals(27, super[x]);
      assertEquals(27, this[x]);
      var ex = null;
      try { super[x] = 10; } catch(e) { ex = e; }
      assertInstanceof(ex, TypeError);
      assertEquals(27, super[x]);
      assertEquals(27, this[x]);
    }
  };
  new Derived().mSloppy();
  new Derived().mStrict();
}());


(function TestSuperCall() {
  'use strict';

  var baseCalled = 0;
  var derivedCalled = 0;
  var derivedDerivedCalled = 0;

  class Base {
    constructor() {
      baseCalled++;
    }
  }

  class Derived extends Base {
    constructor() {
      let r = super();
      assertEquals(this, r);
      derivedCalled++;
    }
  }

  assertEquals(Base, Base.prototype.constructor);
  assertEquals(Base.prototype, Derived.prototype.__proto__);

  baseCalled = 0;
  derivedCalled = 0;
  new Derived();
  assertEquals(1, baseCalled);
  assertEquals(1, derivedCalled);

  class DerivedDerived extends Derived {
    constructor() {
      let r = super();
      assertEquals(this, r);
      derivedDerivedCalled++;
    }
  }

  baseCalled = 0;
  derivedCalled = 0;
  derivedDerivedCalled = 0;
  new DerivedDerived();
  assertEquals(1, baseCalled);
  assertEquals(1, derivedCalled);
  assertEquals(1, derivedDerivedCalled);

  class Base2 {
    constructor(v) {
      this.fromBase = v;
    }
  }
  class Derived2 extends Base2 {
    constructor(v1, v2) {
      let r = super(v1);
      assertEquals(this, r);
      this.fromDerived = v2;
    }
  }

  var d = new Derived2("base", "derived");
  assertEquals("base", d.fromBase);
  assertEquals("derived", d.fromDerived);

  var calls = 0;
  class G {
    constructor() {
      calls++;
    }
  }

  class F extends Object {
    constructor() {
      super();
    }
  }
  F.__proto__ = G;
  new F();
  assertEquals(1, calls);
  F.__proto__ = function() {};
  new F();
  assertEquals(1, calls);
}());


(function TestExtendsObject() {
  'use strict';
  class F extends Object { }
  var f = new F(42);

  assertInstanceof(f, F);
  assertInstanceof(f, Object);
}());


(function TestSuperCallErrorCases() {
  'use strict';
  class T extends Object {
    constructor() {
      super();
    }
  }

  T.__proto__ = null;
  assertThrows(function() { new T(); }, TypeError);
}());


(function TestSuperPropertyInEval() {
  'use strict';
  let y = 3;
  class Base {
    m() { return 1; }
    get x() { return 2; }
  }
  class Derived extends Base {
    evalM() {
      assertEquals(1, eval('super.m()'));
    }
    evalX() {
      assertEquals(2, eval('super.x'));
    }
    globalEval1() {
      assertThrows('super.x', SyntaxError);
      assertThrows('super.m()', SyntaxError);
    }
    globalEval2() {
      super.x;
      assertThrows('super.x', SyntaxError);
      assertThrows('super.m()', SyntaxError);
    }
  }
  let d = new Derived();
  d.globalEval1();
  d.globalEval2();
  d.evalM();
  d.evalX();
})();


(function TestSuperPropertyInArrow() {
  'use strict';
  let y = 3;
  class Base {
    m() { return 1; }
    get x() { return 2; }
  }
  class Derived extends Base {
    arrow() {
      assertSame(super.x, (() => super.x)());
      assertSame(super.m(), (() => super.m())());
      return (() => super.m())();
    }
  }
  let d = new Derived();
  assertSame(1, d.arrow());
})();


(function TestSuperInOtherScopes() {
  var p = {x: 99};
  var o0 = {__proto__: p, f() { return eval("'use strict'; super.x") }};
  assertEquals(p.x, o0.f());
  var o1 = {__proto__: p, f() { with ({}) return super.x }};
  assertEquals(p.x, o1.f());
  var o2 = {__proto__: p, f({a}) { return super.x }};
  assertEquals(p.x, o2.f({}));
  var o3 = {__proto__: p, f(...a) { return super.x }};
  assertEquals(p.x, o3.f());
  var o4 = {__proto__: p, f() { 'use strict'; { let x; return super.x } }};
  assertEquals(p.x, o4.f());
})();


(function TestSuperCallInOtherScopes() {
  class C {constructor() { this.x = 99 }}
  class D0 extends C {constructor() { eval("'use strict'; super()") }}
  assertEquals(99, (new D0).x);
  class D2 extends C {constructor({a}) { super() }}
  assertEquals(99, (new D2({})).x);
  class D3 extends C {constructor(...a) { super() }}
  assertEquals(99, (new D3()).x);
  class D4 extends C {constructor() { { let x; super() } }}
  assertEquals(99, (new D4).x);
})();


(function TestSuperCallInEval() {
  'use strict';
  class Base {
    constructor(x) {
      this.x = x;
    }
  }
  class Derived extends Base {
    constructor(x) {
      let r = eval('super(x)');
      assertEquals(this, r);
    }
  }
  let d = new Derived(42);
  assertSame(42, d.x);
})();


(function TestSuperCallInArrow() {
  'use strict';
  class Base {
    constructor(x) {
      this.x = x;
    }
  }
  class Derived extends Base {
    constructor(x) {
      let r = (() => super(x))();
      assertEquals(this, r);
    }
  }
  let d = new Derived(42);
  assertSame(42, d.x);
})();


(function TestSuperCallEscapes() {
  'use strict';
  class Base {
    constructor(x) {
      this.x = x;
    }
  }

  let f;
  class Derived extends Base {
    constructor() {
      f = () => super(2);
    }
  }
  assertThrows(function() {
    new Derived();
  }, ReferenceError);

  let o = f();
  assertEquals(2, o.x);
  assertInstanceof(o, Derived);

  assertThrows(function() {
    f();
  }, ReferenceError);
})();


(function TestSuperCallInLoop() {
  'use strict';
  class Base {
    constructor(x) {
      this.x = x;
    }
  }
  class Derived extends Base {
    constructor(x, n) {
      for (var i = 0; i < n; ++i) {
        super(x);
      }
    }
  }

  let o = new Derived(23, 1);
  assertEquals(23, o.x);
  assertInstanceof(o, Derived);

  assertThrows("new Derived(42, 0)", ReferenceError);
  assertThrows("new Derived(65, 2)", ReferenceError);
})();


(function TestSuperCallReentrant() {
  'use strict';
  class Base {
    constructor(fun) {
      this.x = fun();
    }
  }
  class Derived extends Base {
    constructor(x) {
      let f = () => super(() => x)
      super(f);
    }
  }
  assertThrows("new Derived(23)", ReferenceError);
})();


(function TestSuperCallSpreadInEval() {
  'use strict';
  class Base {
    constructor(x) {
      this.x = x;
    }
  }
  class Derived extends Base {
    constructor(x) {
      let r = eval('super(...[x])');
      assertEquals(this, r);
    }
  }
  let d = new Derived(42);
  assertSame(42, d.x);
})();


(function TestSuperCallSpreadInArrow() {
  'use strict';
  class Base {
    constructor(x) {
      this.x = x;
    }
  }
  class Derived extends Base {
    constructor(x) {
      let r = (() => super(...[x]))();
      assertEquals(this, r);
    }
  }
  let d = new Derived(42);
  assertSame(42, d.x);
})();

(function TestNullSuperPropertyLoad() {
  var obj = {
    __proto__: null,
    named() { return super.x },
    keyed() { return super[5] }
  };
  assertThrows(obj.named, TypeError);
  assertThrows(obj.keyed, TypeError);
  class C extends null {
    named() { return super.x }
    keyed() { return super[5] }
  }
  assertThrows(C.prototype.named, TypeError);
  assertThrows(C.prototype.keyed, TypeError);
})();

(function TestNullSuperPropertyStore() {
  var obj = {
    __proto__: null,
    named() { super.x = 42 },
    keyed() { super[5] = 42 }
  };
  assertThrows(obj.named, TypeError);
  assertThrows(obj.keyed, TypeError);
  class C extends null {
    named() { super.x = 42 }
    keyed() { super[5] = 42 }
  }
  assertThrows(C.prototype.named, TypeError);
  assertThrows(C.prototype.keyed, TypeError);
})();

(function TestDeleteSuperPropertyEvaluationOrder() {
  var i = 0;
  class Base {}
  class Derived extends Base {
    test() {
      delete super[i++];
    }
  }
  assertThrows(Derived.prototype.test, ReferenceError);
  assertEquals(1, i);
})();
