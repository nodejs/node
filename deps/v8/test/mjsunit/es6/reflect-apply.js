// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


(function testReflectApplyArity() {
  assertEquals(3, Reflect.apply.length);
})();


(function testReflectApplyNonConstructor() {
  assertThrows(function() {
    new Reflect.apply(function(){}, null, []);
  }, TypeError);
})();


(function testAppliedReceiverSloppy() {
  function returnThis() { return this; }
  var receiver = {};

  assertSame(this, Reflect.apply(returnThis, void 0, []));
  assertSame(this, Reflect.apply(returnThis, null, []));
  assertSame(this, Reflect.apply(returnThis, this, []));
  assertSame(receiver, Reflect.apply(returnThis, receiver, []));

  // Wrap JS values
  assertSame(String.prototype,
             Object.getPrototypeOf(Reflect.apply(returnThis, "str", [])));
  assertSame(Number.prototype,
             Object.getPrototypeOf(Reflect.apply(returnThis, 123, [])));
  assertSame(Boolean.prototype,
             Object.getPrototypeOf(Reflect.apply(returnThis, true, [])));
  assertSame(Symbol.prototype,
             Object.getPrototypeOf(
                Reflect.apply(returnThis, Symbol("test"), [])));
})();


(function testAppliedReceiverStrict() {
  function returnThis() { 'use strict'; return this; }
  var receiver = {};

  assertSame(void 0, Reflect.apply(returnThis, void 0, []));
  assertSame(this, Reflect.apply(returnThis, this, []));
  assertSame(receiver, Reflect.apply(returnThis, receiver, []));

  // Don't wrap value types
  var regexp = /123/;
  var symbol = Symbol("test");
  assertSame("str", Reflect.apply(returnThis, "str", []));
  assertSame(123, Reflect.apply(returnThis, 123, []));
  assertSame(true, Reflect.apply(returnThis, true, []));
  assertSame(regexp, Reflect.apply(returnThis, regexp, []));
  assertSame(symbol, Reflect.apply(returnThis, symbol, []));
})();


(function testAppliedArgumentsLength() {
  function returnLengthStrict() { 'use strict'; return arguments.length; }
  function returnLengthSloppy() { return arguments.length; }

  assertEquals(0, Reflect.apply(returnLengthStrict, this, []));
  assertEquals(0, Reflect.apply(returnLengthSloppy, this, []));
  assertEquals(0, Reflect.apply(returnLengthStrict, this, {}));
  assertEquals(0, Reflect.apply(returnLengthSloppy, this, {}));

  for (var i = 0; i < 256; ++i) {
    assertEquals(i, Reflect.apply(returnLengthStrict, this, new Array(i)));
    assertEquals(i, Reflect.apply(returnLengthSloppy, this, new Array(i)));
    assertEquals(i, Reflect.apply(returnLengthStrict, this, { length: i }));
    assertEquals(i, Reflect.apply(returnLengthSloppy, this, { length: i }));
  }
})();


(function testAppliedArgumentsLengthThrows() {
  function noopStrict() { 'use strict'; }
  function noopSloppy() { }
  function MyError() {}

  var argsList = {};
  Object.defineProperty(argsList, "length", {
    get: function() { throw new MyError(); }
  });

  assertThrows(function() {
    Reflect.apply(noopStrict, this, argsList);
  }, MyError);

  assertThrows(function() {
    Reflect.apply(noopSloppy, this, argsList);
  }, MyError);
})();


(function testAppliedArgumentsElementThrows() {
  function noopStrict() { 'use strict'; }
  function noopSloppy() { }
  function MyError() {}

  var argsList = { length: 1 };
  Object.defineProperty(argsList, "0", {
    get: function() { throw new MyError(); }
  });

  assertThrows(function() {
    Reflect.apply(noopStrict, this, argsList);
  }, MyError);

  assertThrows(function() {
    Reflect.apply(noopSloppy, this, argsList);
  }, MyError);
})();


(function testAppliedNonFunctionStrict() {
  'use strict';
  assertThrows(function() { Reflect.apply(void 0); }, TypeError);
  assertThrows(function() { Reflect.apply(null); }, TypeError);
  assertThrows(function() { Reflect.apply(123); }, TypeError);
  assertThrows(function() { Reflect.apply("str"); }, TypeError);
  assertThrows(function() { Reflect.apply(Symbol("x")); }, TypeError);
  assertThrows(function() { Reflect.apply(/123/); }, TypeError);
  assertThrows(function() { Reflect.apply(NaN); }, TypeError);
  assertThrows(function() { Reflect.apply({}); }, TypeError);
  assertThrows(function() { Reflect.apply([]); }, TypeError);
})();


(function testAppliedNonFunctionSloppy() {
  assertThrows(function() { Reflect.apply(void 0); }, TypeError);
  assertThrows(function() { Reflect.apply(null); }, TypeError);
  assertThrows(function() { Reflect.apply(123); }, TypeError);
  assertThrows(function() { Reflect.apply("str"); }, TypeError);
  assertThrows(function() { Reflect.apply(Symbol("x")); }, TypeError);
  assertThrows(function() { Reflect.apply(/123/); }, TypeError);
  assertThrows(function() { Reflect.apply(NaN); }, TypeError);
  assertThrows(function() { Reflect.apply({}); }, TypeError);
  assertThrows(function() { Reflect.apply([]); }, TypeError);
})();


(function testAppliedArgumentsNonList() {
  function noopStrict() { 'use strict'; }
  function noopSloppy() {}
  var R = void 0;
  assertThrows(function() { Reflect.apply(noopStrict, R, null); }, TypeError);
  assertThrows(function() { Reflect.apply(noopSloppy, R, null); }, TypeError);
  assertThrows(function() { Reflect.apply(noopStrict, R, 1); }, TypeError);
  assertThrows(function() { Reflect.apply(noopSloppy, R, 1); }, TypeError);
  assertThrows(function() { Reflect.apply(noopStrict, R, "BAD"); }, TypeError);
  assertThrows(function() { Reflect.apply(noopSloppy, R, "BAD"); }, TypeError);
  assertThrows(function() { Reflect.apply(noopStrict, R, true); }, TypeError);
  assertThrows(function() { Reflect.apply(noopSloppy, R, true); }, TypeError);
  var sym = Symbol("x");
  assertThrows(function() { Reflect.apply(noopStrict, R, sym); }, TypeError);
  assertThrows(function() { Reflect.apply(noopSloppy, R, sym); }, TypeError);
})();


(function testAppliedArgumentValue() {
  function returnFirstStrict(a) { 'use strict'; return a; }
  function returnFirstSloppy(a) { return a; }
  function returnLastStrict(a) {
    'use strict'; return arguments[arguments.length - 1]; }
  function returnLastSloppy(a) { return arguments[arguments.length - 1]; }
  function returnSumStrict() {
    'use strict';
    var sum = arguments[0];
    for (var i = 1; i < arguments.length; ++i) {
      sum += arguments[i];
    }
    return sum;
  }
  function returnSumSloppy() {
    var sum = arguments[0];
    for (var i = 1; i < arguments.length; ++i) {
      sum += arguments[i];
    }
    return sum;
  }

  assertEquals("OK!", Reflect.apply(returnFirstStrict, this, ["OK!"]));
  assertEquals("OK!", Reflect.apply(returnFirstSloppy, this, ["OK!"]));
  assertEquals("OK!", Reflect.apply(returnFirstStrict, this,
                                    { 0: "OK!", length: 1 }));
  assertEquals("OK!", Reflect.apply(returnFirstSloppy, this,
                                    { 0: "OK!", length: 1 }));
  assertEquals("OK!", Reflect.apply(returnLastStrict, this,
                                    [0, 1, 2, 3, 4, 5, 6, 7, 8, "OK!"]));
  assertEquals("OK!", Reflect.apply(returnLastSloppy, this,
                                    [0, 1, 2, 3, 4, 5, 6, 7, 8, "OK!"]));
  assertEquals("OK!", Reflect.apply(returnLastStrict, this,
                                    { 9: "OK!", length: 10 }));
  assertEquals("OK!", Reflect.apply(returnLastSloppy, this,
                                    { 9: "OK!", length: 10 }));
  assertEquals("TEST", Reflect.apply(returnSumStrict, this,
                                     ["T", "E", "S", "T"]));
  assertEquals("TEST!!", Reflect.apply(returnSumStrict, this,
                                       ["T", "E", "S", "T", "!", "!"]));
  assertEquals(10, Reflect.apply(returnSumStrict, this,
                                 { 0: 1, 1: 2, 2: 3, 3: 4, length: 4 }));
  assertEquals("TEST", Reflect.apply(returnSumSloppy, this,
                                     ["T", "E", "S", "T"]));
  assertEquals("TEST!!", Reflect.apply(returnSumSloppy, this,
                                       ["T", "E", "S", "T", "!", "!"]));
  assertEquals(10, Reflect.apply(returnSumSloppy, this,
                                 { 0: 1, 1: 2, 2: 3, 3: 4, length: 4 }));
})();
