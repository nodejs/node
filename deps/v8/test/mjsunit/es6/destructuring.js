// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function TestObjectLiteralPattern() {
  var { x : x, y : y, get, set } = { x : 1, y : 2, get: 3, set: 4 };
  assertEquals(1, x);
  assertEquals(2, y);
  assertEquals(3, get);
  assertEquals(4, set);

  var {z} = { z : 3 };
  assertEquals(3, z);


  var sum = 0;
  for (var {z} = { z : 3 }; z != 0; z--) {
    sum += z;
  }
  assertEquals(6, sum);


  var log = [];
  var o = {
    get x() {
      log.push("x");
      return 0;
    },
    get y() {
      log.push("y");
      return {
        get z() { log.push("z"); return 1; }
      }
    }
  };
  var { x : x0, y : { z : z1 }, x : x1 } = o;
  assertSame(0, x0);
  assertSame(1, z1);
  assertSame(0, x1);
  assertArrayEquals(["x", "y", "z", "x"], log);
}());


(function TestObjectLiteralPatternInitializers() {
  var { x : x, y : y = 2 } = { x : 1 };
  assertEquals(1, x);
  assertEquals(2, y);

  var {z = 3} = {};
  assertEquals(3, z);

  var sum = 0;
  for (var {z = 3} = {}; z != 0; z--) {
    sum += z;
  }
  assertEquals(6, sum);

  var log = [];
  var o = {
    get x() {
      log.push("x");
      return undefined;
    },
    get y() {
      log.push("y");
      return {
        get z() { log.push("z"); return undefined; }
      }
    }
  };
  var { x : x0 = 0, y : { z : z1 = 1}, x : x1 = 0} = o;
  assertSame(0, x0);
  assertSame(1, z1);
  assertSame(0, x1);
  assertArrayEquals(["x", "y", "z", "x"], log);
}());


(function TestObjectLiteralPatternLexicalInitializers() {
  'use strict';
  let { x : x, y : y = 2 } = { x : 1 };
  assertEquals(1, x);
  assertEquals(2, y);

  let {z = 3} = {};
  assertEquals(3, z);

  let log = [];
  let o = {
    get x() {
      log.push("x");
      return undefined;
    },
    get y() {
      log.push("y");
      return {
        get z() { log.push("z"); return undefined; }
      }
    }
  };

  let { x : x0 = 0, y : { z : z1 = 1 }, x : x1 = 5} = o;
  assertSame(0, x0);
  assertSame(1, z1);
  assertSame(5, x1);
  assertArrayEquals(["x", "y", "z", "x"], log);

  let sum = 0;
  for (let {x = 0, z = 3} = {}; z != 0; z--) {
    assertEquals(0, x);
    sum += z;
  }
  assertEquals(6, sum);
}());


(function TestObjectLiteralPatternLexical() {
  'use strict';
  let { x : x, y : y } = { x : 1, y : 2 };
  assertEquals(1, x);
  assertEquals(2, y);

  let {z} = { z : 3 };
  assertEquals(3, z);

  let log = [];
  let o = {
    get x() {
      log.push("x");
      return 0;
    },
    get y() {
      log.push("y");
      return {
        get z() { log.push("z"); return 1; }
      }
    }
  };
  let { x : x0, y : { z : z1 }, x : x1 } = o;
  assertSame(0, x0);
  assertSame(1, z1);
  assertSame(0, x1);
  assertArrayEquals(["x", "y", "z", "x"], log);

  let sum = 0;
  for (let {x, z} = { x : 0, z : 3 }; z != 0; z--) {
    assertEquals(0, x);
    sum += z;
  }
  assertEquals(6, sum);
}());


(function TestObjectLiteralPatternLexicalConst() {
  'use strict';
  const { x : x, y : y } = { x : 1, y : 2 };
  assertEquals(1, x);
  assertEquals(2, y);

  assertThrows(function() { x++; }, TypeError);
  assertThrows(function() { y++; }, TypeError);

  const {z} = { z : 3 };
  assertEquals(3, z);

  for (const {x, z} = { x : 0, z : 3 }; z != 3 || x != 0;) {
    assertTrue(false);
  }
}());


(function TestFailingMatchesSloppy() {
  var {x, y} = {};
  assertSame(undefined, x);
  assertSame(undefined, y);

  var { x : { z1 }, y2} = { x : {}, y2 : 42 }
  assertSame(undefined, z1);
  assertSame(42, y2);
}());


(function TestFailingMatchesStrict() {
  'use strict';
  var {x, y} = {};
  assertSame(undefined, x);
  assertSame(undefined, y);

  var { x : { z1 }, y2} = { x : {}, y2 : 42 }
  assertSame(undefined, z1);
  assertSame(42, y2);

  {
    let {x1,y1} = {};
    assertSame(undefined, x1);
    assertSame(undefined, y1);

    let { x : { z1 }, y2} = { x : {}, y2 : 42 }
    assertSame(undefined, z1);
    assertSame(42, y2);
  }
}());


(function TestTDZInIntializers() {
  'use strict';
  {
    let {x, y = x} = {x : 42, y : 27};
    assertSame(42, x);
    assertSame(27, y);
  }

  {
    let {x, y = x + 1} = { x : 42 };
    assertSame(42, x);
    assertSame(43, y);
  }
  assertThrows(function() {
    let {x = y, y} = { y : 42 };
  }, ReferenceError);

  {
    let {x, y = eval("x+1")} = {x:42};
    assertEquals(42, x);
    assertEquals(43, y);
  }

  {
    let {x, y = () => eval("x+1")} = {x:42};
    assertEquals(42, x);
    assertEquals(43, y());
  }

  {
    let {x = function() {return y+1;}, y} = {y:42};
    assertEquals(43, x());
    assertEquals(42, y);
  }
  {
    let {x = function() {return eval("y+1");}, y} = {y:42};
    assertEquals(43, x());
    assertEquals(42, y);
  }
}());


(function TestSideEffectsInInitializers() {
  var callCount = 0;
  function f(v) { callCount++; return v; }

  callCount = 0;
  var { x = f(42) } = { x : 27 };
  assertSame(27, x);
  assertEquals(0, callCount);

  callCount = 0;
  var { x = f(42) } = {};
  assertSame(42, x);
  assertEquals(1, callCount);
}());


(function TestAssignmentExprInInitializers() {
  {
    let x, y;
    {
      let { x = y = 1 } = {};
      assertSame(x, 1);
      assertSame(y, 1);
    }
    assertSame(undefined, x);
    assertSame(1, y);
  }

  {
    let x, y;
    {
      let { x: x = y = 1 } = {};
      assertSame(1, x);
      assertSame(1, y);
    }
    assertSame(undefined, x);
    assertSame(1, y);
  }

  {
    let x, y;
    {
      let [ x = y = 1 ] = [];
      assertSame(1, x);
      assertSame(1, y);
    }
    assertSame(undefined, x);
    assertSame(1, y);
  }

  {
    let x, y;
    (function({ x = y = 1 }) {}({}));
    assertSame(undefined, x);
    assertSame(1, y);
  }

  {
    let x, y;
    (function({ x: x = y = 1 }) {}({}));
    assertSame(undefined, x);
    assertSame(1, y);
  }

  {
    let x, y;
    (function([ x = y = 1 ]) {}([]));
    assertSame(undefined, x);
    assertSame(1, y);
  }
}());


(function TestMultipleAccesses() {
  assertThrows(
    "'use strict';"+
    "const {x,x} = {x:1};",
    SyntaxError);

  assertThrows(
    "'use strict';"+
    "let {x,x} = {x:1};",
     SyntaxError);

  (function() {
    var {x,x = 2} = {x : 1};
    assertSame(1, x);
  }());

  assertThrows(function () {
    'use strict';
    let {x = (function() { x = 2; }())} = {};
  }, ReferenceError);

  (function() {
    'use strict';
    let {x = (function() { x = 2; }())} = {x:1};
    assertSame(1, x);
  }());
}());


(function TestComputedNames() {
  var x = 1;
  var {[x]:y} = {1:2};
  assertSame(2, y);

  (function(){
    'use strict';
    let {[x]:y} = {1:2};
    assertSame(2, y);
  }());

  var callCount = 0;
  function foo(v) { callCount++; return v; }

  (function() {
    callCount = 0;
    var {[foo("abc")]:x} = {abc:42};
    assertSame(42, x);
    assertEquals(1, callCount);
  }());

  (function() {
    'use strict';
    callCount = 0;
    let {[foo("abc")]:x} = {abc:42};
    assertSame(42, x);
    assertEquals(1, callCount);
  }());

  (function() {
    callCount = 0;
    var {[foo("abc")]:x} = {};
    assertSame(undefined, x);
    assertEquals(1, callCount);
  }());

  (function() {
    'use strict';
    callCount = 0;
    let {[foo("abc")]:x} = {};
    assertSame(undefined, x);
    assertEquals(1, callCount);
  }());

  for (val of [null, undefined]) {
    callCount = 0;
    assertThrows(function() {
      var {[foo()]:x} = val;
    }, TypeError);
    assertEquals(0, callCount);

    callCount = 0;
    assertThrows(function() {
      'use strict';
      let {[foo()]:x} = val;
    }, TypeError);
    assertEquals(0, callCount);
  }

  var log = [];
  var o = {
    get x() { log.push("get x"); return 1; },
    get y() { log.push("get y"); return 2; }
  }
  function f(v) { log.push("f " + v); return v; }

  (function() {
    log = [];
    var { [f('x')]:x, [f('y')]:y } = o;
    assertSame(1, x);
    assertSame(2, y);
    assertArrayEquals(["f x", "get x", "f y", "get y"], log);
  }());

  (function() {
    'use strict';
    log = [];
    let { [f('x')]:x, [f('y')]:y } = o;
    assertSame(1, x);
    assertSame(2, y);
    assertArrayEquals(["f x", "get x", "f y", "get y"], log);
  }());

  (function() {
    'use strict';
    log = [];
    const { [f('x')]:x, [f('y')]:y } = o;
    assertSame(1, x);
    assertSame(2, y);
    assertArrayEquals(["f x", "get x", "f y", "get y"], log);
  }());
}());


(function TestExceptions() {
  for (var val of [null, undefined]) {
    assertThrows(function() { var {} = val; }, TypeError);
    assertThrows(function() { var {x} = val; }, TypeError);
    assertThrows(function() { var { x : {} } = { x : val }; }, TypeError);
    assertThrows(function() { 'use strict'; let {} = val; }, TypeError);
    assertThrows(function() { 'use strict'; let {x} = val; }, TypeError);
    assertThrows(function() { 'use strict'; let { x : {} } = { x : val }; },
                 TypeError);
  }
}());


(function TestArrayLiteral() {
  var [a, b, c] = [1, 2, 3];
  assertSame(1, a);
  assertSame(2, b);
  assertSame(3, c);
}());

(function TestIterators() {
  var log = [];
  function* f() {
    log.push("1");
    yield 1;
    log.push("2");
    yield 2;
    log.push("3");
    yield 3;
    log.push("done");
  };

  (function() {
    log = [];
    var [a, b, c] = f();
    assertSame(1, a);
    assertSame(2, b);
    assertSame(3, c);
    assertArrayEquals(["1", "2", "3"], log);
  }());

  (function() {
    log = [];
    var [a, b, c, d] = f();
    assertSame(1, a);
    assertSame(2, b);
    assertSame(3, c);
    assertSame(undefined, d);
    assertArrayEquals(["1", "2", "3", "done"], log);
  }());

  (function() {
    log = [];
    var [a, , c] = f();
    assertSame(1, a);
    assertSame(3, c);
    assertArrayEquals(["1", "2", "3"], log);
  }());

  (function() {
    log = [];
    var [a, , c, d] = f();
    assertSame(1, a);
    assertSame(3, c);
    assertSame(undefined, d);
    assertArrayEquals(["1", "2", "3", "done"], log);
  }());

  (function() {
    log = [];
    // last comma is not an elision.
    var [a, b,] = f();
    assertSame(1, a);
    assertSame(2, b);
    assertArrayEquals(["1", "2"], log);
  }());

  (function() {
    log = [];
    // last comma is not an elision, but the comma before the last is.
    var [a, b, ,] = f();
    assertSame(1, a);
    assertSame(2, b);
    assertArrayEquals(["1", "2", "3"], log);
  }());

  (function() {
    log = [];
    var [a, ...rest] = f();
    assertSame(1, a);
    assertArrayEquals([2,3], rest);
    assertArrayEquals(["1", "2", "3", "done"], log);
  }());

  (function() {
    log = [];
    var [a, b, c, ...rest] = f();
    assertSame(1, a);
    assertSame(2, b);
    assertSame(3, c);
    assertArrayEquals([], rest);
    assertArrayEquals(["1", "2", "3", "done"], log);
  }());

  (function() {
    log = [];
    var [a, b, c, d, ...rest] = f();
    assertSame(1, a);
    assertSame(2, b);
    assertSame(3, c);
    assertSame(undefined, d);
    assertArrayEquals([], rest);
    assertArrayEquals(["1", "2", "3", "done"], log);
  }());
}());


(function TestIteratorsLexical() {
  'use strict';
  var log = [];
  function* f() {
    log.push("1");
    yield 1;
    log.push("2");
    yield 2;
    log.push("3");
    yield 3;
    log.push("done");
  };

  (function() {
    log = [];
    let [a, b, c] = f();
    assertSame(1, a);
    assertSame(2, b);
    assertSame(3, c);
    assertArrayEquals(["1", "2", "3"], log);
  }());

  (function() {
    log = [];
    let [a, b, c, d] = f();
    assertSame(1, a);
    assertSame(2, b);
    assertSame(3, c);
    assertSame(undefined, d);
    assertArrayEquals(["1", "2", "3", "done"], log);
  }());

  (function() {
    log = [];
    let [a, , c] = f();
    assertSame(1, a);
    assertSame(3, c);
    assertArrayEquals(["1", "2", "3"], log);
  }());

  (function() {
    log = [];
    let [a, , c, d] = f();
    assertSame(1, a);
    assertSame(3, c);
    assertSame(undefined, d);
    assertArrayEquals(["1", "2", "3", "done"], log);
  }());

  (function() {
    log = [];
    // last comma is not an elision.
    let [a, b,] = f();
    assertSame(1, a);
    assertSame(2, b);
    assertArrayEquals(["1", "2"], log);
  }());

  (function() {
    log = [];
    // last comma is not an elision, but the comma before the last is.
    let [a, b, ,] = f();
    assertSame(1, a);
    assertSame(2, b);
    assertArrayEquals(["1", "2", "3"], log);
  }());

  (function() {
    log = [];
    let [a, ...rest] = f();
    assertSame(1, a);
    assertArrayEquals([2,3], rest);
    assertArrayEquals(["1", "2", "3", "done"], log);
  }());

  (function() {
    log = [];
    let [a, b, c, ...rest] = f();
    assertSame(1, a);
    assertSame(2, b);
    assertSame(3, c);
    assertArrayEquals([], rest);
    assertArrayEquals(["1", "2", "3", "done"], log);
  }());

  (function() {
    log = [];
    let [a, b, c, d, ...rest] = f();
    assertSame(1, a);
    assertSame(2, b);
    assertSame(3, c);
    assertSame(undefined, d);
    assertArrayEquals([], rest);
    assertArrayEquals(["1", "2", "3", "done"], log);
  }());
}());

(function TestIteratorsRecursive() {
  var log = [];
  function* f() {
    log.push("1");
    yield {x : 1, y : 2};
    log.push("2");
    yield [42, 27, 30];
    log.push("3");
    yield "abc";
    log.push("done");
  };

  (function() {
    var [{x, y}, [a, b]] = f();
    assertSame(1, x);
    assertSame(2, y);
    assertSame(42, a);
    assertSame(27, b);
    assertArrayEquals(["1", "2"], log);
  }());

  (function() {
    'use strict';
    log = [];
    let [{x, y}, [a, b]] = f();
    assertSame(1, x);
    assertSame(2, y);
    assertSame(42, a);
    assertSame(27, b);
    assertArrayEquals(["1", "2"], log);
  }());
}());


(function TestForEachLexical() {
  'use strict';
  let a = [{x:1, y:-1}, {x:2,y:-2}, {x:3,y:-3}];
  let sumX = 0;
  let sumY = 0;
  let fs = [];
  for (let {x,y} of a) {
    sumX += x;
    sumY += y;
    fs.push({fx : function() { return x; }, fy : function() { return y }});
  }
  assertSame(6, sumX);
  assertSame(-6, sumY);
  assertSame(3, fs.length);
  for (let i = 0; i < fs.length; i++) {
    let {fx,fy} = fs[i];
    assertSame(i+1, fx());
    assertSame(-(i+1), fy());
  }

  var o = { __proto__:null, 'a1':1, 'b2':2 };
  let sx = '';
  let sy = '';
  for (let [x,y] in o) {
    sx += x;
    sy += y;
  }
  assertEquals('ab', sx);
  assertEquals('12', sy);
}());


(function TestForEachVars() {
  var a = [{x:1, y:-1}, {x:2,y:-2}, {x:3,y:-3}];
  var sumX = 0;
  var sumY = 0;
  var fs = [];
  for (var {x,y} of a) {
    sumX += x;
    sumY += y;
    fs.push({fx : function() { return x; }, fy : function() { return y }});
  }
  assertSame(6, sumX);
  assertSame(-6, sumY);
  assertSame(3, fs.length);
  for (var i = 0; i < fs.length; i++) {
    var {fx,fy} = fs[i];
    assertSame(3, fx());
    assertSame(-3, fy());
  }

  var o = { __proto__:null, 'a1':1, 'b2':2 };
  var sx = '';
  var sy = '';
  for (var [x,y] in o) {
    sx += x;
    sy += y;
  }
  assertEquals('ab', sx);
  assertEquals('12', sy);
}());


(function TestParameters() {
  function f({a, b}) { return a - b; }
  assertEquals(1, f({a : 6, b : 5}));

  function f1(c, {a, b}) { return c + a - b; }
  assertEquals(8, f1(7, {a : 6, b : 5}));

  function f2({c, d}, {a, b}) { return c - d + a - b; }
  assertEquals(7, f2({c : 7, d : 1}, {a : 6, b : 5}));

  function f3([{a, b}]) { return a - b; }
  assertEquals(1, f3([{a : 6, b : 5}]));

  var g = ({a, b}) => { return a - b; };
  assertEquals(1, g({a : 6, b : 5}));

  var g1 = (c, {a, b}) => { return c + a - b; };
  assertEquals(8, g1(7, {a : 6, b : 5}));

  var g2 = ({c, d}, {a, b}) => { return c - d + a - b; };
  assertEquals(7, g2({c : 7, d : 1}, {a : 6, b : 5}));

  var g3 = ([{a, b}]) => { return a - b; };
  assertEquals(1, g3([{a : 6, b : 5}]));
}());


(function TestExpressionsInParameters() {
  function f0(x = eval(0)) { return x }
  assertEquals(0, f0());
  function f1({a = eval(1)}) { return a }
  assertEquals(1, f1({}));
  function f2([x = eval(2)]) { return x }
  assertEquals(2, f2([]));
  function f3({[eval(7)]: x}) { return x }
  assertEquals(3, f3({7: 3}));
})();


(function TestParameterScoping() {
  var x = 1;

  function f1({a = x}) { var x = 2; return a; }
  assertEquals(1, f1({}));
  function f2({a = x}) { function x() {}; return a; }
  assertEquals(1, f2({}));
  (function() {
    'use strict';
    function f3({a = x}) { let x = 2; return a; }
    assertEquals(1, f3({}));
    function f4({a = x}) { const x = 2; return a; }
    assertEquals(1, f4({}));
    function f5({a = x}) { function x() {}; return a; }
    assertEquals(1, f5({}));
  })();
  function f6({a = eval("x")}) { var x; return a; }
  assertEquals(1, f6({}));
  (function() {
    'use strict';
    function f61({a = eval("x")}) { var x; return a; }
    assertEquals(1, f61({}));
  })();
  function f62({a = eval("'use strict'; x")}) { var x; return a; }
  assertEquals(1, f62({}));
  function f7({a = function() { return x }}) { var x; return a(); }
  assertEquals(1, f7({}));
  function f8({a = () => x}) { var x; return a(); }
  assertEquals(1, f8({}));
  function f9({a = () => eval("x")}) { var x; return a(); }
  assertEquals(1, f9({}));
  (function TestInitializedWithEvalArrowStrict() {
    'use strict';
    function f91({a = () => eval("x")}) { var x; return a(); }
    assertEquals(1, f91({}));
  })();
  function f92({a = () => { 'use strict'; return eval("x") }}) { var x; return a(); }
  assertEquals(1, f92({}));
  function f93({a = () => eval("'use strict'; x")}) { var x; return a(); }
  assertEquals(1, f93({}));

  var g1 = ({a = x}) => { var x = 2; return a; };
  assertEquals(1, g1({}));
  var g2 = ({a = x}) => { function x() {}; return a; };
  assertEquals(1, g2({}));
  (function() {
    'use strict';
    var g3 = ({a = x}) => { let x = 2; return a; };
    assertEquals(1, g3({}));
    var g4 = ({a = x}) => { const x = 2; return a; };
    assertEquals(1, g4({}));
    var g5 = ({a = x}) => { function x() {}; return a; };
    assertEquals(1, g5({}));
  })();
  var g6 = ({a = eval("x")}) => { var x; return a; };
  assertEquals(1, g6({}));
  (function() {
    'use strict';
    var g61 = ({a = eval("x")}) => { var x; return a; };
    assertEquals(1, g61({}));
  })();
  var g62 = ({a = eval("'use strict'; x")}) => { var x; return a; };
  assertEquals(1, g62({}));
  var g7 = ({a = function() { return x }}) => { var x; return a(); };
  assertEquals(1, g7({}));
  var g8 = ({a = () => x}) => { var x; return a(); };
  assertEquals(1, g8({}));
  var g9 = ({a = () => eval("x")}) => { var x; return a(); };
  assertEquals(1, g9({}));
  (function() {
    'use strict';
    var g91 = ({a = () => eval("x")}) => { var x; return a(); };
    assertEquals(1, g91({}));
    var g92 = ({a = () => { return eval("x") }}) => { var x; return a(); };
    assertEquals(1, g92({}));
  })();
  var g93 = ({a = () => eval("'use strict'; x")}) => { var x; return a(); };
  assertEquals(1, g93({}));

  var f11 = function f({x = f}) { var f; return x; }
  assertSame(f11, f11({}));
  var f12 = function f({x = f}) { function f() {}; return x; }
  assertSame(f12, f12({}));
  (function() {
    'use strict';
    var f13 = function f({x = f}) { let f; return x; }
    assertSame(f13, f13({}));
    var f14 = function f({x = f}) { const f = 0; return x; }
    assertSame(f14, f14({}));
    var f15 = function f({x = f}) { function f() {}; return x; }
    assertSame(f15, f15({}));
  })();
  var f16 = function f({f = 7, x = f}) { return x; }
  assertSame(7, f16({}));

  var y = 'a';
  function f20({[y]: x}) { var y = 'b'; return x; }
  assertEquals(1, f20({a: 1, b: 2}));
  function f21({[eval('y')]: x}) { var y = 'b'; return x; }
  assertEquals(1, f21({a: 1, b: 2}));
  var g20 = ({[y]: x}) => { var y = 'b'; return x; };
  assertEquals(1, g20({a: 1, b: 2}));
  var g21 = ({[eval('y')]: x}) => { var y = 'b'; return x; };
  assertEquals(1, g21({a: 1, b: 2}));
})();


(function TestParameterDestructuringTDZ() {
  function f1({a = x}, x) { return a }
  assertThrows(() => f1({}, 4), ReferenceError);
  assertEquals(4, f1({a: 4}, 5));
  function f2({a = eval("x")}, x) { return a }
  assertThrows(() => f2({}, 4), ReferenceError);
  assertEquals(4, f2({a: 4}, 5));
  (function() {
    'use strict';
    function f3({a = eval("x")}, x) { return a }
    assertThrows(() => f3({}, 4), ReferenceError);
    assertEquals(4, f3({a: 4}, 5));
  })();
  function f4({a = eval("'use strict'; x")}, x) { return a }
  assertThrows(() => f4({}, 4), ReferenceError);
  assertEquals(4, f4({a: 4}, 5));

  function f5({a = () => x}, x) { return a() }
  assertEquals(4, f5({a: () => 4}, 5));
  function f6({a = () => eval("x")}, x) { return a() }
  assertEquals(4, f6({a: () => 4}, 5));
  (function() {
    'use strict';
    function f7({a = () => eval("x")}, x) { return a() }
    assertEquals(4, f7({a: () => 4}, 5));
  })();
  function f8({a = () => eval("'use strict'; x")}, x) { return a() }
  assertEquals(4, f8({a: () => 4}, 5));

  function f11({a = b}, {b}) { return a }
  assertThrows(() => f11({}, {b: 4}), ReferenceError);
  assertEquals(4, f11({a: 4}, {b: 5}));
  function f12({a = eval("b")}, {b}) { return a }
  assertThrows(() => f12({}, {b: 4}), ReferenceError);
  assertEquals(4, f12({a: 4}, {b: 5}));
  (function() {
    'use strict';
    function f13({a = eval("b")}, {b}) { return a }
    assertThrows(() => f13({}, {b: 4}), ReferenceError);
    assertEquals(4, f13({a: 4}, {b: 5}));
  })();
  function f14({a = eval("'use strict'; b")}, {b}) { return a }
  assertThrows(() => f14({}, {b: 4}), ReferenceError);
  assertEquals(4, f14({a: 4}, {b: 5}));

  function f15({a = () => b}, {b}) { return a() }
  assertEquals(4, f15({a: () => 4}, {b: 5}));
  function f16({a = () => eval("b")}, {b}) { return a() }
  assertEquals(4, f16({a: () => 4}, {b: 5}));
  (function() {
    'use strict';
    function f17({a = () => eval("b")}, {b}) { return a() }
    assertEquals(4, f17({a: () => 4}, {b: 5}));
  })();
  function f18({a = () => eval("'use strict'; b")}, {b}) { return a() }
  assertEquals(4, f18({a: () => 4}, {b: 5}));

  // TODO(caitp): TDZ for rest parameters is not working yet.
  // function f30({x = a}, ...a) { return x[0] }
  // assertThrows(() => f30({}), ReferenceError);
  // assertEquals(4, f30({a: [4]}, 5));
  // function f31({x = eval("a")}, ...a) { return x[0] }
  // assertThrows(() => f31({}), ReferenceError);
  // assertEquals(4, f31({a: [4]}, 5));
  // function f32({x = eval("a")}, ...a) { 'use strict'; return x[0] }
  // assertThrows(() => f32({}), ReferenceError);
  // assertEquals(4, f32({a: [4]}, 5));
  // function f33({x = eval("'use strict'; a")}, ...a) { return x[0] }
  // assertThrows(() => f33({}), ReferenceError);
  // assertEquals(4, f33({a: [4]}, 5));

  function f34({x = function() { return a }}, ...a) { return x()[0] }
  assertEquals(4, f34({}, 4));
  function f35({x = () => a}, ...a) { return x()[0] }
  assertEquals(4, f35({}, 4));
  function f36({x = () => eval("a")}, ...a) { return x()[0] }
  assertEquals(4, f36({}, 4));
  (function() {
    'use strict';
    function f37({x = () => eval("a")}, ...a) { return x()[0] }
    assertEquals(4, f37({}, 4));
  })();
  function f38({x = () => { 'use strict'; return eval("a") }}, ...a) { return x()[0] }
  assertEquals(4, f38({}, 4));
  function f39({x = () => eval("'use strict'; a")}, ...a) { return x()[0] }
  assertEquals(4, f39({}, 4));

  // var g30 = ({x = a}, ...a) => {};
  // assertThrows(() => g30({}), ReferenceError);
  // var g31 = ({x = eval("a")}, ...a) => {};
  // assertThrows(() => g31({}), ReferenceError);
  // var g32 = ({x = eval("a")}, ...a) => { 'use strict'; };
  // assertThrows(() => g32({}), ReferenceError);
  // var g33 = ({x = eval("'use strict'; a")}, ...a) => {};
  // assertThrows(() => g33({}), ReferenceError);
  var g34 = ({x = function() { return a }}, ...a) => { return x()[0] };
  assertEquals(4, g34({}, 4));
  var g35 = ({x = () => a}, ...a) => { return x()[0] };
  assertEquals(4, g35({}, 4));
})();


(function TestDuplicatesInParameters() {
  assertThrows("'use strict';function f(x,x){}", SyntaxError);
  assertThrows("'use strict';function f({x,x}){}", SyntaxError);
  assertThrows("'use strict';function f(x, {x}){}", SyntaxError);
  assertThrows("'use strict';var f = (x,x) => {};", SyntaxError);
  assertThrows("'use strict';var f = ({x,x}) => {};", SyntaxError);
  assertThrows("'use strict';var f = (x, {x}) => {};", SyntaxError);

  function ok1(x) { var x; return x; };
  assertEquals(1, ok1(1));
  function ok2(x) { 'use strict'; { let x = 2; return x; } };
  assertEquals(2, ok2(1));
}());


(function TestShadowingOfParameters() {
  function f1({x}) { var x = 2; return x }
  assertEquals(2, f1({x: 1}));
  function f2({x}) { { var x = 2; } return x; }
  assertEquals(2, f2({x: 1}));
  function f3({x}) { var y = x; var x = 2; return y; }
  assertEquals(1, f3({x: 1}));
  function f4({x}) { { var y = x; var x = 2; } return y; }
  assertEquals(1, f4({x: 1}));
  function f5({x}, g = () => x) { var x = 2; return g(); }
  assertEquals(1, f5({x: 1}));
  function f6({x}, g = () => x) { { var x = 2; } return g(); }
  assertEquals(1, f6({x: 1}));
  function f7({x}) { var g = () => x; var x = 2; return g(); }
  assertEquals(2, f7({x: 1}));
  function f8({x}) { { var g = () => x; var x = 2; } return g(); }
  assertEquals(2, f8({x: 1}));
  function f9({x}, g = () => eval("x")) { var x = 2; return g(); }
  assertEquals(1, f9({x: 1}));

  function f10({x}, y) { var y; return y }
  assertEquals(2, f10({x: 6}, 2));
  function f11({x}, y) { var z = y; var y = 2; return z; }
  assertEquals(1, f11({x: 6}, 1));
  function f12(y, g = () => y) { var y = 2; return g(); }
  assertEquals(1, f12(1));
  function f13({x}, y, [z], v) { var x, y, z; return x*y*z*v }
  assertEquals(210, f13({x: 2}, 3, [5], 7));

  function f20({x}) { function x() { return 2 }; return x(); }
  assertEquals(2, f20({x: 1}));
  // Annex B 3.3 function hoisting is blocked by the conflicting x declaration
  function f21({x}) { { function x() { return 2 } } return x; }
  assertEquals(1, f21({x: 1}));

  var g1 = ({x}) => { var x = 2; return x };
  assertEquals(2, g1({x: 1}));
  var g2 = ({x}) => { { var x = 2; } return x; };
  assertEquals(2, g2({x: 1}));
  var g3 = ({x}) => { var y = x; var x = 2; return y; };
  assertEquals(1, g3({x: 1}));
  var g4 = ({x}) => { { var y = x; var x = 2; } return y; };
  assertEquals(1, g4({x: 1}));
  var g5 = ({x}, g = () => x) => { var x = 2; return g(); };
  assertEquals(1, g5({x: 1}));
  var g6 = ({x}, g = () => x) => { { var x = 2; } return g(); };
  assertEquals(1, g6({x: 1}));
  var g7 = ({x}) => { var g = () => x; var x = 2; return g(); };
  assertEquals(2, g7({x: 1}));
  var g8 = ({x}) => { { var g = () => x; var x = 2; } return g(); };
  assertEquals(2, g8({x: 1}));
  var g9 = ({x}, g = () => eval("x")) => { var x = 2; return g(); };
  assertEquals(1, g9({x: 1}));

  var g10 = ({x}, y) => { var y; return y };
  assertEquals(2, g10({x: 6}, 2));
  var g11 = ({x}, y) => { var z = y; var y = 2; return z; };
  assertEquals(1, g11({x: 6}, 1));
  var g12 = (y, g = () => y) => { var y = 2; return g(); };
  assertEquals(1, g12(1));
  var g13 = ({x}, y, [z], v) => { var x, y, z; return x*y*z*v };
  assertEquals(210, g13({x: 2}, 3, [5], 7));

  var g20 = ({x}) => { function x() { return 2 }; return x(); }
  assertEquals(2, g20({x: 1}));
  var g21 = ({x}) => { { function x() { return 2 } } return x(); }
  assertThrows(() => g21({x: 1}), TypeError);

  // These errors are not recognized in lazy parsing; see mjsunit/bugs/bug-2728.js
  assertThrows("'use strict'; (function f(x) { let x = 0; })()", SyntaxError);
  assertThrows("'use strict'; (function f({x}) { let x = 0; })()", SyntaxError);
  assertThrows("'use strict'; (function f(x) { const x = 0; })()", SyntaxError);
  assertThrows("'use strict'; (function f({x}) { const x = 0; })()", SyntaxError);

  assertThrows("'use strict'; let g = (x) => { let x = 0; }", SyntaxError);
  assertThrows("'use strict'; let g = ({x}) => { let x = 0; }", SyntaxError);
  assertThrows("'use strict'; let g = (x) => { const x = 0; }", SyntaxError);
  assertThrows("'use strict'; let g = ({x}) => { const x = 0; }", SyntaxError);
}());


(function TestArgumentsForNonSimpleParameters() {
  function f1({}, x) { arguments[1] = 0; return x }
  assertEquals(6, f1({}, 6));
  function f2({}, x) { x = 2; return arguments[1] }
  assertEquals(7, f2({}, 7));
  function f3(x, {}) { arguments[0] = 0; return x }
  assertEquals(6, f3(6, {}));
  function f4(x, {}) { x = 2; return arguments[0] }
  assertEquals(7, f4(7, {}));
  function f5(x, ...a) { arguments[0] = 0; return x }
  assertEquals(6, f5(6, {}));
  function f6(x, ...a) { x = 2; return arguments[0] }
  assertEquals(6, f6(6, {}));
  function f7({a: x}) { x = 2; return arguments[0].a }
  assertEquals(5, f7({a: 5}));
  function f8(x, ...a) { a = []; return arguments[1] }
  assertEquals(6, f8(5, 6));
}());


(function TestForInOfTDZ() {
  assertThrows("'use strict'; let x = {}; for (let [x, y] of [x]);", ReferenceError);
  assertThrows("'use strict'; let x = {}; for (let [y, x] of [x]);", ReferenceError);
  assertThrows("'use strict'; let x = {}; for (let [x, y] in {x});", ReferenceError);
  assertThrows("'use strict'; let x = {}; for (let [y, x] in {x});", ReferenceError);
}());


(function TestFunctionLength() {
   assertEquals(1, (function({}) {}).length);
   assertEquals(1, (function([]) {}).length);
   assertEquals(1, (function({x}) {}).length);
   assertEquals(1, (function({}, ...a) {}).length);
   assertEquals(1, (function({x}, {y} = {}) {}).length);
   assertEquals(1, (function({x}, {y} = {}, ...a) {}).length);
   assertEquals(2, (function(x, {y}, {z} = {}) {}).length);
   assertEquals(2, (function({x}, {}, {z} = {}, ...a) {}).length);
   assertEquals(1, (function(x, {y} = {}, {z}) {}).length);
   assertEquals(1, (function({x}, {y} = {}, {z}, ...a) {}).length);
   assertEquals(1, (function(x, {y} = {}, {z}, {v} = {}) {}).length);
   assertEquals(1, (function({x}, {y} = {}, {z}, {v} = {}, ...a) {}).length);
})();


(function TestDirectiveThrows() {
  "use strict";

  assertThrows(function(){ eval("function({}){'use strict';}") }, SyntaxError);
  assertThrows(function(){ eval("({}) => {'use strict';}") }, SyntaxError);
  assertThrows(
    function(){ eval("(class{foo({}) {'use strict';}});") }, SyntaxError);

  assertThrows(
    function(){ eval("function(a, {}){'use strict';}") }, SyntaxError);
  assertThrows(function(){ eval("(a, {}) => {'use strict';}") }, SyntaxError);
  assertThrows(
    function(){ eval("(class{foo(a, {}) {'use strict';}});") }, SyntaxError);
})();


(function TestLegacyConstDestructuringInForLoop() {
  var result;
  for (const {foo} of [{foo: 1}]) { result = foo; }
  assertEquals(1, result);
})();


(function TestCatch() {
  "use strict";

  // For testing proper scoping.
  var foo = "hello", bar = "world", baz = 42;

  try {
    throw {foo: 1, bar: 2};
  } catch ({foo, bar, baz = 3}) {
    assertEquals(1, foo);
    assertEquals(2, bar);
    assertEquals(3, baz);
  }

  try {
    throw [1, 2, 3];
  } catch ([foo, ...bar]) {
    assertEquals(1, foo);
    assertEquals([2, 3], bar);
  }

  assertEquals("hello", foo);
  assertEquals("world", bar);
  assertEquals(42, baz);

  assertEquals(undefined, eval('try {throw {foo: 1, bar: 2}} catch({foo}) {}'));
  assertEquals(undefined, eval('try {throw [1, 2, 3]} catch([x]) {}'));
})();
