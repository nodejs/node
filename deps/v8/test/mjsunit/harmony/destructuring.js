// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --harmony-destructuring --harmony-computed-property-names
// Flags: --harmony-arrow-functions

(function TestObjectLiteralPattern() {
  var { x : x, y : y } = { x : 1, y : 2 };
  assertEquals(1, x);
  assertEquals(2, y);

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


(function TestDuplicatesInParameters() {
  assertThrows("'use strict';function f(x,x){}", SyntaxError);
  assertThrows("'use strict';function f({x,x}){}", SyntaxError);
  assertThrows("'use strict';function f(x, {x}){}", SyntaxError);
  assertThrows("'use strict';var f = (x,x) => {};", SyntaxError);
  assertThrows("'use strict';var f = ({x,x}) => {};", SyntaxError);
  assertThrows("'use strict';var f = (x, {x}) => {};", SyntaxError);

  function ok(x) { var x; }; ok();
  assertThrows("function f({x}) { var x; }; f({});", SyntaxError);
  assertThrows("'use strict'; function f({x}) { let x = 0; }; f({});", SyntaxError);
}());


(function TestForInOfTDZ() {
  assertThrows("'use strict'; let x = {}; for (let [x, y] of {x});", ReferenceError);
  assertThrows("'use strict'; let x = {}; for (let [y, x] of {x});", ReferenceError);
  assertThrows("'use strict'; let x = {}; for (let [x, y] in {x});", ReferenceError);
  assertThrows("'use strict'; let x = {}; for (let [y, x] in {x});", ReferenceError);
}());
