// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function testIn(obj, key) {
  return key in obj;
}

function expectTrue(obj, key) {
  assertTrue(testIn(obj, key));
}

function expectFalse(obj, key) {
  assertFalse(testIn(obj, key));
}

var tests = {
  TestMonomorphicPackedSMIArray: function() {
    var a = [0, 1, 2];
    expectTrue(a, 0);
    expectTrue(a, 1);
    expectTrue(a, 2);
    expectFalse(a, 3);
  },

  TestMonomorphicPackedArrayPrototypeProperty: function()
  {
    var a = [0, 1, 2];

    expectTrue(a, 0);
    expectTrue(a, 1);
    expectFalse(a, 3);
    Array.prototype[3] = 3;
    expectTrue(a, 3);

    // ensure the prototype change doesn't affect later tests
    delete Array.prototype[3];
    assertFalse((3 in Array.prototype));
    expectFalse(a, 3);
  },

  TestMonomorphicPackedDoubleArray: function() {
    var a = [0.0, 1.1, 2.2];
    expectTrue(a, 0);
    expectTrue(a, 1);
    expectTrue(a, 2);
    expectFalse(a, 3);
  },

  TestMonomorphicPackedArray: function() {
    var a = ["A", "B", {}];
    expectTrue(a, 0);
    expectTrue(a, 1);
    expectTrue(a, 2);
    expectFalse(a, 3);
  },

  TestMonomorphicHoleyArray: function() {
    var a = [0, 1, 2];
    a[4] = 4;

    expectTrue(a, 0);
    expectTrue(a, 1);
    expectTrue(a, 2);
    expectFalse(a, 3);
    expectTrue(a, 4);
  },

  TestMonomorphicTypedArray: function() {
    var a = new Int32Array(3);
    expectTrue(a, 0);
    expectTrue(a, 1);
    expectTrue(a, 2);
    expectFalse(a, 3);
    expectFalse(a, 4);
  },

  TestPolymorphicPackedArrays: function() {
    var a = [0, 1, 2];
    var b = [0.0, 1.1, 2.2];
    var c = ["A", "B", {}];
    expectTrue(c, 0);
    expectTrue(b, 1);
    expectTrue(a, 2);
    expectTrue(c, 1);
    expectTrue(b, 2);
    expectTrue(a, 0);
    expectFalse(c, 3);
    expectFalse(b, 4);
    expectFalse(a, 5);
  },

  TestPolymorphicMixedArrays: function() {
    var a = new Array(3);     // holey SMI
    var b = [0.0,1.1,2.2];    // packed double
    var c = new Int8Array(3); // typed array

    expectFalse(a, 0);
    expectTrue(b, 1);
    expectTrue(c, 2);
    expectFalse(a, 1);
    expectTrue(b, 2);
    expectTrue(c, 0);
    expectFalse(a, 3);
    expectFalse(b, 4);
    expectFalse(c, 5);
  },

  TestMegamorphicArrays: function() {
    var a = [0,1,2,3]           // packed SMI
    var b = new Array(3);       // holey SMI
    var c = [0.0,1.1,2.2];      // packed double
    var d = ['a', 'b', 'c']     // packed
    var e = new Int8Array(3);   // typed array
    var f = new Uint8Array(3);  // typed array
    var g = new Int32Array(3);  // typed array

    expectTrue(a, 0);
    expectFalse(b, 1);
    expectTrue(c, 2);
    expectFalse(d, 3);
    expectFalse(e, 4);
    expectFalse(f, 5);
    expectFalse(g, 6);
    expectFalse(a, 5);
    expectFalse(b, 4);
    expectFalse(c, 3);
    expectTrue(d, 2);
    expectTrue(e, 1);
    expectTrue(f, 0);
    expectTrue(g, 0);
  },

  TestMonomorphicObject: function() {
    var a = { a: "A", b: "B" };

    expectTrue(a, 'a');
    expectTrue(a, 'a');
    expectTrue(a, 'a');
  },

  TestMonomorphicProxyHasPropertyNoTrap: function() {
    var a = new Proxy({a: 'A'}, {});

    expectTrue(a, 'a');
    expectTrue(a, 'a');
    expectTrue(a, 'a');
  },

  TestMonomorphicProxyNoPropertyNoTrap: function() {
    var a = new Proxy({}, {});

    expectFalse(a, 'a');
    expectFalse(a, 'a');
    expectFalse(a, 'a');
  },

  TestMonomorphicProxyHasPropertyHasTrap: function() {
    var a = new Proxy({a: 'A'}, { has: function() {return false;}});

    expectFalse(a, 'a');
    expectFalse(a, 'a');
    expectFalse(a, 'a');
  },

  TestMonomorphicProxyNoPropertyHasTrap: function() {
    var a = new Proxy({}, { has: function() { return true; }});

    expectTrue(a, 'a');
    expectTrue(a, 'a');
    expectTrue(a, 'a');
  },

  TestMonomorphicObjectPrototype: function() {
    var a = { b: "B" };

    expectFalse(a, 'a');
    expectFalse(a, 'a');
    expectFalse(a, 'a');
    Object.prototype.a = 'A';
    expectTrue(a, 'a');
    delete Object.prototype.a;
    assertFalse((a in Object.prototype));
    expectFalse(a, 'a');
  },

  TestPolymorphicObject: function() {
    var a = { a: "A" };
    var b = { a: "A", b: "B" };
    var c = { b: "B", c: "C" };

    expectTrue(a, 'a');
    expectTrue(a, 'a');
    expectTrue(b, 'a');
    expectFalse(c, 'a');
    expectTrue(a, 'a');
    expectTrue(b, 'a');
    expectFalse(c, 'a');
  },

  TestMegamorphicObject: function() {
    var a = { a: "A" };
    var b = { a: "A", b: "B" };
    var c = { b: "B", c: "C" };
    var d = { b: "A", a: "B" };
    var e = { e: "E", a: "A" };
    var f = { f: "F", b: "B", c: "C" };

    expectTrue(a, 'a');
    expectTrue(a, 'a');
    expectTrue(b, 'a');
    expectFalse(c, 'a');
    expectTrue(d, 'a');
    expectTrue(e, 'a');
    expectFalse(f, 'a');
    expectTrue(a, 'a');
    expectTrue(b, 'a');
    expectFalse(c, 'a');
    expectTrue(d, 'a');
    expectTrue(e, 'a');
    expectFalse(f, 'a');
  },

  TestPolymorphicKeys: function() {
    var a = { a: "A", b: "B" };

    expectTrue(a, 'a');
    expectTrue(a, 'b');
    expectFalse(a, 'c');
    expectTrue(a, 'a');
    expectTrue(a, 'b');
    expectFalse(a, 'c');
    expectTrue(a, 'a');
    expectTrue(a, 'b');
    expectFalse(a, 'c');
  },

  TestPolymorphicMixed: function() {
    var a = { a: "A" };
    var b = new Proxy({}, {});
    var c = new Int32Array(3);

    expectTrue(a, 'a');
    expectTrue(a, 'a');
    expectFalse(b, 'a');
    expectFalse(c, 'a');
    expectTrue(a, 'a');
    expectFalse(b, 'a');
    expectFalse(c, 'a');
  },
};

for (test in tests) {
  %DeoptimizeFunction(testIn);
  %ClearFunctionFeedback(testIn);
  %PrepareFunctionForOptimization(testIn);
  tests[test]();
  %OptimizeFunctionOnNextCall(testIn);
  tests[test]();
}

// test function prototypes.
(function() {
  var o = function() {};

  var proto = function() {
    assertTrue("prototype" in o);
    o.prototype;
  };

  %PrepareFunctionForOptimization(proto);
  proto();
  proto();
  %OptimizeFunctionOnNextCall(proto);
  proto();
})();

// `in` is not allowed on string
(function() {
  function test() {
    0 in "string"
  };

  %PrepareFunctionForOptimization(test);
  assertThrows(test, TypeError);
  assertThrows(test, TypeError);
  %OptimizeFunctionOnNextCall(test);
  assertThrows(test, TypeError);
})();

// `in` is allowed on `this` even when `this` is a string
(function() {
  function test() {
    assertTrue("length" in this);
  };

  %PrepareFunctionForOptimization(test);
  test.call("");
  test.call("");
  %OptimizeFunctionOnNextCall(test);
  test.call("");
})();

(function() {
  var index = 0;
  function test(i) {
    return index in arguments;
  };

  %PrepareFunctionForOptimization(test);
  assertFalse(test())
  assertFalse(test())
  assertTrue(test(0));
  assertTrue(test(0,1));

  index = 2;
  assertFalse(test())
  assertFalse(test(0));
  assertFalse(test(0,1));
  assertTrue(test(0,1,2));

  %OptimizeFunctionOnNextCall(test);
  assertFalse(test(0,1));
  assertTrue(test(0,1,2));
})();

(function() {
  function test(a) {
    arguments[3] = 1;
    return 2 in arguments;
  };

  %PrepareFunctionForOptimization(test);
  assertFalse(test(1));
  assertFalse(test(1));
  %OptimizeFunctionOnNextCall(test);
  assertFalse(test(1));
})();

(function() {
  function test(o, k) {
    try {
      k in o;
    } catch (e) {
      return false;
    }
    return true;
  }
  %PrepareFunctionForOptimization(test);

  var str = "string";
  // this will place slow_stub in the IC for strings.
  assertFalse(test(str, "length"));
  assertFalse(test(str, "length"));

  // this turns the cache polymorphic, and causes generats LoadElement
  // handlers for everything in the cache. This test ensures that
  // KeyedLoadIC::LoadElementHandler can handle seeing string maps.
  var ary = [0,1,2,3];
  assertTrue(test(ary, 1));
  assertTrue(test(ary, 1));

  assertFalse(test(str, 0));
  assertFalse(test(str, 0));
  %OptimizeFunctionOnNextCall(test);
  assertFalse(test(str, 0));
})();

(function() {
  function test(o, k) {
    try {
      k in o;
    } catch (e) {
      return false;
    }
    return true;
  }
  %PrepareFunctionForOptimization(test);

  var str = "string";
  assertFalse(test(str, "length"));
  assertFalse(test(str, "length"));
  %OptimizeFunctionOnNextCall(test);
  assertFalse(test(str, "length"));
})();

(function() {
  function test(o, k) {
    try {
      k in o;
    } catch (e) {
      return false;
    }
    return true;
  }
  %PrepareFunctionForOptimization(test);

  var str = "string";
  assertFalse(test(str, 0));
  assertFalse(test(str, 0));
  %OptimizeFunctionOnNextCall(test);
  assertFalse(test(str, 0));
})();

(function() {
  function test(o, k) {
    try {
      k in o;
    } catch (e) {
      return false;
    }
    return true;
  }
  %PrepareFunctionForOptimization(test);

  var ary = [0, 1, 2, '3'];
  function testArray(ary) {
    assertTrue(test(ary, 1));
    assertTrue(test(ary, 1));
  }
  testArray(ary);
  // Packed
  // Non-extensible
  var b =  Object.preventExtensions(ary);
  testArray(b);

  // Sealed
  var c =  Object.seal(ary);
  testArray(c);

  // Frozen
  var d =  Object.freeze(ary);
  testArray(d);

  // Holey
  var ary = [, 0, 1, 2, '3'];
  // Non-extensible
  var b =  Object.preventExtensions(ary);
  testArray(b);

  // Sealed
  var c =  Object.seal(ary);
  testArray(c);

  // Frozen
  var d =  Object.freeze(ary);
  testArray(d);

  var str = "string";
  assertFalse(test(str, 0));
  assertFalse(test(str, 0));
  %OptimizeFunctionOnNextCall(test);
  assertFalse(test(str, 0));
})();

const heap_constant_ary = [0,1,2,'3'];

function testHeapConstantArray(heap_constant_ary) {

  function test() {
    return 1 in heap_constant_ary;
  }
  %PrepareFunctionForOptimization(test);

  assertTrue(test());
  assertTrue(test());
  %OptimizeFunctionOnNextCall(test);
  assertTrue(test());

  heap_constant_ary[1] = 2;
  assertTrue(test());
  %PrepareFunctionForOptimization(test);
  %OptimizeFunctionOnNextCall(test);
  assertTrue(test());
}
testHeapConstantArray(heap_constant_ary);

// Packed
// Non-extensible
var b =  Object.preventExtensions(heap_constant_ary);
testHeapConstantArray(b);

// Sealed
var c =  Object.seal(heap_constant_ary);
testHeapConstantArray(c);

// Frozen
var d =  Object.freeze(heap_constant_ary);
testHeapConstantArray(d);

// Holey
const holey_heap_constant_ary = [,0,1,2,'3'];
// Non-extensible
var b =  Object.preventExtensions(holey_heap_constant_ary);
testHeapConstantArray(b);

// Sealed
var c =  Object.seal(holey_heap_constant_ary);
testHeapConstantArray(c);

// Frozen
var d =  Object.freeze(holey_heap_constant_ary);
testHeapConstantArray(d);
