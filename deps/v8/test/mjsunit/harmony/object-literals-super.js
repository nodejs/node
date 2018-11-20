// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-classes --allow-natives-syntax


(function TestHomeObject() {
  var object = {
    method() {
      return super.method();
    },
    get getter() {
      return super.getter;
    },
    set setter(v) {
      super.setter = v;
    },
    get accessor() {
      return super.accessor;
    },
    set accessor(v) {
      super.accessor = v;
    },

    methodNoSuper() {},
    get getterNoSuper() {},
    set setterNoSuper(v) {},
    get accessorNoSuper() {},
    set accessorNoSuper(v) {},
    propertyNoSuper: function() {},
    propertyWithParenNoSuper: (function() {}),
    propertyWithParensNoSuper: ((function() {}))
  };

  assertEquals(object, object.method[%HomeObjectSymbol()]);
  var desc = Object.getOwnPropertyDescriptor(object, 'getter');
  assertEquals(object, desc.get[%HomeObjectSymbol()]);
  desc = Object.getOwnPropertyDescriptor(object, 'setter');
  assertEquals(object, desc.set[%HomeObjectSymbol()]);
  desc = Object.getOwnPropertyDescriptor(object, 'accessor');
  assertEquals(object, desc.get[%HomeObjectSymbol()]);
  assertEquals(object, desc.set[%HomeObjectSymbol()]);

  assertEquals(undefined, object.methodNoSuper[%HomeObjectSymbol()]);
  desc = Object.getOwnPropertyDescriptor(object, 'getterNoSuper');
  assertEquals(undefined, desc.get[%HomeObjectSymbol()]);
  desc = Object.getOwnPropertyDescriptor(object, 'setterNoSuper');
  assertEquals(undefined, desc.set[%HomeObjectSymbol()]);
  desc = Object.getOwnPropertyDescriptor(object, 'accessorNoSuper');
  assertEquals(undefined, desc.get[%HomeObjectSymbol()]);
  assertEquals(undefined, desc.set[%HomeObjectSymbol()]);
  assertEquals(undefined, object.propertyNoSuper[%HomeObjectSymbol()]);
  assertEquals(undefined, object.propertyWithParenNoSuper[%HomeObjectSymbol()]);
  assertEquals(undefined,
               object.propertyWithParensNoSuper[%HomeObjectSymbol()]);
})();


(function TestMethod() {
  var object = {
    __proto__: {
      method(x) {
        return 'proto' + x;
      }
    },
    method(x) {
      return super.method(x);
    }
  };
  assertEquals('proto42', object.method(42));
})();


(function TestGetter() {
  var object = {
    __proto__: {
      _x: 42,
      get x() {
        return 'proto' + this._x;
      }
    },
    get x() {
      return super.x;
    }
  };
  assertEquals('proto42', object.x);
})();


(function TestSetter() {
  var object = {
    __proto__: {
      _x: 0,
      set x(v) {
        return this._x = v;
      }
    },
    set x(v) {
      super.x = v;
    }
  };
  assertEquals(1, object.x = 1);
  assertEquals(1, object._x);
  assertEquals(0, Object.getPrototypeOf(object)._x);
})();


(function TestOptimized() {
  // Object literals without any accessors get optimized.
  var object = {
    method() {
      return super.toString;
    }
  };
  assertEquals(Object.prototype.toString, object.method());
})();


(function TestConciseGenerator() {
  var o = {
    __proto__: {
      m() {
        return 42;
      }
    },
    *g() {
      yield super.m();
    },
  };

  assertEquals(42, o.g().next().value);
})();
