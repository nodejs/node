// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function assertMethodDescriptor(object, name) {
    var descr = Object.getOwnPropertyDescriptor(object, name);
    assertTrue(descr.configurable);
    assertFalse(descr.enumerable);
    assertTrue(descr.writable);
    assertEquals('function', typeof descr.value);
    assertFalse('prototype' in descr.value);
    assertEquals(name, descr.value.name);
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
