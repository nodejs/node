// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var typedArrayConstructors = [
  Uint8Array,
  Int8Array,
  Uint16Array,
  Int16Array,
  Uint32Array,
  Int32Array,
  Uint8ClampedArray,
  Float32Array,
  Float64Array
];

function clone(v) {
  // Shallow-copies arrays, returns everything else verbatim.
  if (v instanceof Array) {
    // Shallow-copy an array.
    var newArray = new Array(v.length);
    for (var i in v) {
      newArray[i] = v[i];
    }
    return newArray;
  }
  return v;
}


// Creates a callback function for reduce/reduceRight that tests the number
// of arguments and otherwise behaves as "func", but which also
// records all calls in an array on the function (as arrays of arguments
// followed by result).
function makeRecorder(func, testName) {
  var record = [];
  var f = function recorder(a, b, i, s) {
    assertEquals(4, arguments.length,
                 testName + "(number of arguments: " + arguments.length + ")");
    assertEquals("number", typeof(i), testName + "(index must be number)");
    assertEquals(s[i], b, testName + "(current argument is at index)");
    if (record.length > 0) {
      var prevRecord = record[record.length - 1];
      var prevResult = prevRecord[prevRecord.length - 1];
      assertEquals(prevResult, a,
                   testName + "(prev result -> current input)");
    }
    var args = [clone(a), clone(b), i, clone(s)];
    var result = func.apply(this, arguments);
    args.push(clone(result));
    record.push(args);
    return result;
  };
  f.record = record;
  return f;
}


function testReduce(type,
                    testName,
                    expectedResult,
                    expectedCalls,
                    array,
                    combine,
                    init) {
  var rec = makeRecorder(combine);
  var result;
  var performsCall;
  if (arguments.length > 6) {
    result = array[type](rec, init);
  } else {
    result = array[type](rec);
  }
  var calls = rec.record;
  assertEquals(expectedCalls.length, calls.length,
               testName + " (number of calls)");
  for (var i = 0; i < expectedCalls.length; i++) {
    assertEquals(expectedCalls[i], calls[i],
                 testName + " (call " + (i + 1) + ")");
  }
  assertEquals(expectedResult, result, testName + " (result)");
}


function sum(a, b) { return a + b; }
function prod(a, b) { return a * b; }
function dec(a, b, i, arr) { return a + b * Math.pow(10, arr.length - i - 1); }
function accumulate(acc, elem, i) { acc[i] = elem; return acc; }

for (var constructor of typedArrayConstructors) {
  // ---- Test Reduce[Left]

  var simpleArray = new constructor([2,4,6])

  testReduce("reduce", "SimpleReduceSum", 12,
             [[0, 2, 0, simpleArray, 2],
              [2, 4, 1, simpleArray, 6],
              [6, 6, 2, simpleArray, 12]],
             simpleArray, sum, 0);

  testReduce("reduce", "SimpleReduceProd", 48,
             [[1, 2, 0, simpleArray, 2],
              [2, 4, 1, simpleArray, 8],
              [8, 6, 2, simpleArray, 48]],
             simpleArray, prod, 1);

  testReduce("reduce", "SimpleReduceDec", 246,
             [[0, 2, 0, simpleArray, 200],
              [200, 4, 1, simpleArray, 240],
              [240, 6, 2, simpleArray, 246]],
             simpleArray, dec, 0);

  testReduce("reduce", "SimpleReduceAccumulate", [2, 4, 6],
             [[[], 2, 0, simpleArray, [2]],
              [[2], 4, 1, simpleArray, [2, 4]],
              [[2,4], 6, 2, simpleArray, [2, 4, 6]]],
             simpleArray, accumulate, []);


  testReduce("reduce", "EmptyReduceSum", 0, [], new constructor([]), sum, 0);
  testReduce("reduce", "EmptyReduceProd", 1, [], new constructor([]), prod, 1);
  testReduce("reduce", "EmptyReduceDec", 0, [], new constructor([]), dec, 0);
  testReduce("reduce", "EmptyReduceAccumulate", [], [], new constructor([]), accumulate, []);

  testReduce("reduce", "EmptyReduceSumNoInit", 0, [], new constructor([0]), sum);
  testReduce("reduce", "EmptyReduceProdNoInit", 1, [], new constructor([1]), prod);
  testReduce("reduce", "EmptyReduceDecNoInit", 0, [], new constructor([0]), dec);

  // ---- Test ReduceRight

  testReduce("reduceRight", "SimpleReduceRightSum", 12,
             [[0, 6, 2, simpleArray, 6],
              [6, 4, 1, simpleArray, 10],
              [10, 2, 0, simpleArray, 12]],
             simpleArray, sum, 0);

  testReduce("reduceRight", "SimpleReduceRightProd", 48,
             [[1, 6, 2, simpleArray, 6],
              [6, 4, 1, simpleArray, 24],
              [24, 2, 0, simpleArray, 48]],
             simpleArray, prod, 1);

  testReduce("reduceRight", "SimpleReduceRightDec", 246,
             [[0, 6, 2, simpleArray, 6],
              [6, 4, 1, simpleArray, 46],
              [46, 2, 0, simpleArray, 246]],
             simpleArray, dec, 0);


  testReduce("reduceRight", "EmptyReduceRightSum", 0, [], new constructor([]), sum, 0);
  testReduce("reduceRight", "EmptyReduceRightProd", 1, [], new constructor([]), prod, 1);
  testReduce("reduceRight", "EmptyReduceRightDec", 0, [], new constructor([]), dec, 0);
  testReduce("reduceRight", "EmptyReduceRightAccumulate", [],
             [], new constructor([]), accumulate, []);

  testReduce("reduceRight", "EmptyReduceRightSumNoInit", 0, [], new constructor([0]), sum);
  testReduce("reduceRight", "EmptyReduceRightProdNoInit", 1, [], new constructor([1]), prod);
  testReduce("reduceRight", "EmptyReduceRightDecNoInit", 0, [], new constructor([0]), dec);

  // Ignore non-array properties:

  var arrayPlus = new constructor([1,2,3]);
  arrayPlus[-1] = NaN;
  arrayPlus["00"] = NaN;
  arrayPlus["02"] = NaN;
  arrayPlus["-0"] = NaN;
  arrayPlus.x = NaN;

  testReduce("reduce", "ArrayWithNonElementPropertiesReduce", 6,
             [[0, 1, 0, arrayPlus, 1],
              [1, 2, 1, arrayPlus, 3],
              [3, 3, 2, arrayPlus, 6],
             ], arrayPlus, sum, 0);

  testReduce("reduceRight", "ArrayWithNonElementPropertiesReduceRight", 6,
             [[0, 3, 2, arrayPlus, 3],
              [3, 2, 1, arrayPlus, 5],
              [5, 1, 0, arrayPlus, 6],
             ], arrayPlus, sum, 0);


  // Test error conditions:

  var exception = false;
  try {
    new constructor([1]).reduce("not a function");
  } catch (e) {
    exception = true;
    assertTrue(e instanceof TypeError,
               "reduce callback not a function not throwing TypeError");
    assertTrue(e.message.indexOf(" is not a function") >= 0,
               "reduce non function TypeError type");
  }
  assertTrue(exception);

  exception = false;
  try {
    new constructor([1]).reduceRight("not a function");
  } catch (e) {
    exception = true;
    assertTrue(e instanceof TypeError,
               "reduceRight callback not a function not throwing TypeError");
    assertTrue(e.message.indexOf(" is not a function") >= 0,
               "reduceRight non function TypeError type");
  }
  assertTrue(exception);

  exception = false;
  try {
    new constructor([]).reduce(sum);
  } catch (e) {
    exception = true;
    assertTrue(e instanceof TypeError,
               "reduce no initial value not throwing TypeError");
    assertEquals("Reduce of empty array with no initial value", e.message,
                 "reduce no initial TypeError type");
  }
  assertTrue(exception);

  exception = false;
  try {
    new constructor([]).reduceRight(sum);
  } catch (e) {
    exception = true;
    assertTrue(e instanceof TypeError,
               "reduceRight no initial value not throwing TypeError");
    assertEquals("Reduce of empty array with no initial value", e.message,
                 "reduceRight no initial TypeError type");
  }
  assertTrue(exception);

  // Reduce fails when called on non-TypedArrays
  assertThrows(function() {
    constructor.prototype.reduce.call([], function() {}, null);
  }, TypeError);
  assertThrows(function() {
    constructor.prototype.reduceRight.call([], function() {}, null);
  }, TypeError);

  // Shadowing length doesn't affect every, unlike Array.prototype.every
  var a = new constructor([1, 2]);
  Object.defineProperty(a, 'length', {value: 1});
  assertEquals(a.reduce(sum, 0), 3);
  assertEquals(Array.prototype.reduce.call(a, sum, 0), 1);
  assertEquals(a.reduceRight(sum, 0), 3);
  assertEquals(Array.prototype.reduceRight.call(a, sum, 0), 1);

  assertEquals(1, constructor.prototype.reduce.length);
  assertEquals(1, constructor.prototype.reduceRight.length);
}
