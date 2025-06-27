// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

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


(function TestSuperPropertyInEval() {
  var y = 3;
  var p  = {
    m() { return 1; },
    get x() { return 2; }
  };
  var o = {
    __proto__: p,
    evalM() {
      assertEquals(1, eval('super.m()'));
    },
    evalX() {
      assertEquals(2, eval('super.x'));
    },
    globalEval1() {
      assertThrows('super.x', SyntaxError);
      assertThrows('super.m()', SyntaxError);
    },
    globalEval2() {
      super.x;
      assertThrows('super.x', SyntaxError);
      assertThrows('super.m()', SyntaxError);
    }
  };
  o.evalM();
  o.evalX();
  o.globalEval1();
  o.globalEval2();
})();


(function TestSuperPropertyInArrow() {
  var y = 3;
  var p  = {
    m() { return 1; },
    get x() { return 2; }
  };
  var o = {
    __proto__: p,
    arrow() {
      assertSame(super.x, (() => super.x)());
      assertSame(super.m(), (() => super.m())());
      return (() => super.m())();
    }
  };
  assertSame(1, o.arrow());
})();
