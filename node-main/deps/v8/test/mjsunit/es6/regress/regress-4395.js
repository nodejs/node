// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function testExpressionTypes() {
  "use strict";
  ((x, y = x) => assertEquals(42, y))(42);

  ((x, y = (x)) => assertEquals(42, y))(42);
  ((x, y = `${x}`) => assertEquals("42", y))(42);
  ((x, y = x = x + 1) => assertEquals(43, y))(42);
  ((x, y = x()) => assertEquals(42, y))(() => 42);
  ((x, y = new x()) => assertEquals(42, y.z))(function() { this.z = 42 });
  ((x, y = -x) => assertEquals(-42, y))(42);
  ((x, y = ++x) => assertEquals(43, y))(42);
  ((x, y = x === 42) => assertTrue(y))(42);
  ((x, y = (x == 42 ? x : 0)) => assertEquals(42, y))(42);

  ((x, y = function() { return x }) => assertEquals(42, y()))(42);
  ((x, y = () => x) => assertEquals(42, y()))(42);

  // Literals
  ((x, y = {z: x}) => assertEquals(42, y.z))(42);
  ((x, y = {[x]: x}) => assertEquals(42, y[42]))(42);
  ((x, y = [x]) => assertEquals(42, y[0]))(42);
  ((x, y = [...x]) => assertEquals(42, y[0]))([42]);

  ((x, y = class {
    static [x]() { return x }
  }) => assertEquals(42, y[42]()))(42);
  ((x, y = (new class {
    z() { return x }
  })) => assertEquals(42, y.z()))(42);

  ((x, y = (new class Y {
    static [x]() { return x }
    z() { return Y[42]() }
  })) => assertEquals(42, y.z()))(42);

  ((x, y = (new class {
    constructor() { this.z = x }
  })) => assertEquals(42, y.z))(42);
  ((x, y = (new class Y {
    constructor() { this.z = x }
  })) => assertEquals(42, y.z))(42);

  ((x, y = (new class extends x {
  })) => assertEquals(42, y.z()))(class { z() { return 42 } });

  // Defaults inside destructuring
  ((x, {y = x}) => assertEquals(42, y))(42, {});
  ((x, [y = x]) => assertEquals(42, y))(42, []);
})();


(function testMultiScopeCapture() {
  "use strict";
  var x = 1;
  {
    let y = 2;
    ((x, y, a = x, b = y) => {
      assertEquals(3, x);
      assertEquals(3, a);
      assertEquals(4, y);
      assertEquals(4, b);
    })(3, 4);
  }
})();


(function testSuper() {
  "use strict";
  class A {
    x() { return 42; }
  }

  class B extends A {
    y() {
      ((q = super.x()) => assertEquals(42, q))();
    }
  }

  new B().y();

  class C {
    constructor() { return { prop: 42 } }
  }

  class D extends C{
    constructor() {
      ((q = super()) => assertEquals(42, q.prop))();
    }
  }

  new D();
})();


(function testScopeFlags() {
  ((x, y = eval('x')) => assertEquals(42, y))(42);
  ((x, {y = eval('x')}) => assertEquals(42, y))(42, {});
})();
