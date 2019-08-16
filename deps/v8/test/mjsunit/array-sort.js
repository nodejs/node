// Copyright 2010 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Test array sort.

// Test counter-intuitive default number sorting.
function TestNumberSort() {
  var a = [ 200, 45, 7 ];

  // Default sort converts each element to string and orders
  // lexicographically.
  a.sort();
  assertArrayEquals([ 200, 45, 7 ], a);
  // Sort numbers by value using a compare functions.
  a.sort(function(x, y) { return x - y; });
  assertArrayEquals([ 7, 45, 200 ], a);

  // Default sort on negative numbers.
  a = [-12345,-123,-1234,-123456];
  a.sort();
  assertArrayEquals([-123,-1234,-12345,-123456], a);

  // Default sort on negative and non-negative numbers.
  a = [123456,0,-12345,-123,123,1234,-1234,0,12345,-123456];
  a.sort();
  assertArrayEquals([-123,-1234,-12345,-123456,0,0,123,1234,12345,123456], a);

  // Tricky case avoiding integer overflow in Runtime_SmiLexicographicCompare.
  a = [9, 1000000000].sort();
  assertArrayEquals([1000000000, 9], a);
  a = [1000000000, 1].sort();
  assertArrayEquals([1, 1000000000], a);
  a = [1000000000, 0].sort();
  assertArrayEquals([0, 1000000000], a);

  // One string is a prefix of the other.
  a = [1230, 123].sort();
  assertArrayEquals([123, 1230], a);
  a = [1231, 123].sort();
  assertArrayEquals([123, 1231], a);

  // Default sort on Smis and non-Smis.
  a = [1000000000, 10000000000, 1000000001, -1000000000, -10000000000, -1000000001];
  a.sort();
  assertArrayEquals([-1000000000, -10000000000, -1000000001, 1000000000, 10000000000, 1000000001], a);

  // Other cases are tested implicitly in TestSmiLexicographicCompare.
}

TestNumberSort();

// Test lexicographical string sorting.
function TestStringSort() {
  var a = [ "cc", "c", "aa", "a", "bb", "b", "ab", "ac" ];
  a.sort();
  assertArrayEquals([ "a", "aa", "ab", "ac", "b", "bb", "c", "cc" ], a);
}

TestStringSort();


// Test object sorting.  Calls toString on each element and sorts
// lexicographically.
function TestObjectSort() {
  var obj0 = { toString: function() { return "a"; } };
  var obj1 = { toString: function() { return "b"; } };
  var obj2 = { toString: function() { return "c"; } };
  var a = [ obj2, obj0, obj1 ];
  a.sort();
  assertArrayEquals([ obj0, obj1, obj2 ], a);
}

TestObjectSort();

// Test array sorting with holes in the array.
function TestArraySortingWithHoles() {
  var a = [];
  a[4] = "18";
  a[10] = "12";
  a.sort();
  assertEquals(11, a.length);
  assertEquals("12", a[0]);
  assertEquals("18", a[1]);
}

TestArraySortingWithHoles();

// Test array sorting with undefined elemeents in the array.
function TestArraySortingWithUndefined() {
  var a = [ 3, void 0, 2 ];
  a.sort();
  assertArrayEquals([ 2, 3, void 0 ], a);
}

TestArraySortingWithUndefined();

// Test that sorting using an unsound comparison function still gives a
// sane result, i.e. it terminates without error and retains the elements
// in the array.
function TestArraySortingWithUnsoundComparisonFunction() {
  var a = [ 3, void 0, 2 ];
  a.sort(function(x, y) { return 1; });
  a.sort();
  assertArrayEquals([ 2, 3, void 0 ], a);
}

TestArraySortingWithUnsoundComparisonFunction();


function TestSparseNonArraySorting(length) {
  assertTrue(length > 101);
  var obj = {length: length};
  obj[0] = 42;
  obj[10] = 37;
  obj[100] = undefined;
  obj[length - 1] = null;
  Array.prototype.sort.call(obj);
  assertEquals(length, obj.length, "objsort length unaffected");
  assertEquals(37, obj[0], "objsort smallest number");
  assertEquals(42, obj[1], "objsort largest number");
  assertEquals(null, obj[2], "objsort null");
  assertEquals(undefined, obj[3], "objsort undefined");
  assertTrue(3 in obj, "objsort undefined retained");
  assertFalse(4 in obj, "objsort non-existing retained");
}

TestSparseNonArraySorting(1000);
TestSparseNonArraySorting(5000);


function TestArrayLongerLength(length) {
  var x = new Array(4);
  x[0] = 42;
  x[2] = 37;
  x.length = length;
  Array.prototype.sort.call(x);
  assertEquals(length, x.length, "longlength length");
  assertEquals(37, x[0], "longlength first");
  assertEquals(42, x[1], "longlength second");
  assertFalse(2 in x,"longlength third");
}

TestArrayLongerLength(4);
TestArrayLongerLength(10);
TestArrayLongerLength(1000);
TestArrayLongerLength(5000);


function TestNonArrayLongerLength(length) {
  var x = {};
  x[0] = 42;
  x[2] = 37;
  x.length = length;
  Array.prototype.sort.call(x);
  assertEquals(length, x.length, "longlength length");
  assertEquals(37, x[0], "longlength first");
  assertEquals(42, x[1], "longlength second");
  assertFalse(2 in x,"longlength third");
}

TestNonArrayLongerLength(4);
TestNonArrayLongerLength(10);
TestNonArrayLongerLength(1000);
TestNonArrayLongerLength(5000);


function TestNonArrayWithAccessors() {
  // Regression test for issue 346, more info at URL
  // http://code.google.com/p/v8/issues/detail?id=346
  // Reported by nth10sd, test based on this report.
  var x = {};
  x[0] = 42;
  x.__defineGetter__("1", function(){return this.foo;});
  x.__defineSetter__("1", function(val){this.foo = val;});
  x[1] = 49
  x[3] = 37;
  x.length = 4;
  Array.prototype.sort.call(x);
  // Behavior of sort with accessors is undefined.  This accessor is
  // well-behaved (acts like a normal property), so it should work.
  assertEquals(4, x.length, "sortaccessors length");
  assertEquals(37, x[0], "sortaccessors first");
  assertEquals(42, x[1], "sortaccessors second");
  assertEquals(49, x[2], "sortaccessors third")
  assertFalse(3 in x, "sortaccessors fourth");
}

TestNonArrayWithAccessors();


function TestInheritedElementSort(depth) {
  var length = depth * 2 + 3;
  var obj = {length: length};
  obj[depth * 2 + 1] = 0;
  for (var i = 0; i < depth; i++) {
    var newObj = {};
    newObj.__proto__ = obj;
    obj[i] = undefined;
    obj[i + depth + 1] = depth - i;
    obj = newObj;
  }
  // expected (inherited) object: [undef1,...undefdepth,hole,1,...,depth,0,hole]

  Array.prototype.sort.call(obj, function(a,b) { return (b < a) - (a < b); });
  // expected result: [0,1,...,depth,undef1,...,undefdepth,hole]
  var name = "SortInherit("+depth+")-";

  assertEquals(length, obj.length, name+"length");
  for (var i = 0; i <= depth; i++) {
    assertTrue(obj.hasOwnProperty(i), name + "hasvalue" + i);
    assertEquals(i, obj[i], name + "value" + i);
  }
  for (var i = depth + 1; i < depth * 2 + 1; i++) {
    assertEquals(undefined, obj[i], name + "undefined" + i);
    assertTrue(obj.hasOwnProperty(i), name + "hasundefined" + i);
  }
  assertFalse(obj.hasOwnProperty(depth * 2 + 1), name + "hashole")
  assertFalse(obj.hasOwnProperty(depth * 2 + 2), name + "hashole");
}

TestInheritedElementSort(5);
TestInheritedElementSort(15);

function TestSparseInheritedElementSort(scale) {
  var length = scale * 10;
  var x = {length: length};
  var y = {};
  y.__proto__ = x;

  for (var i = 0; i < 5; i++) {
    x[i * 2 * scale] = 2 * (4 - i);
    y[(i * 2 + 1) * scale] = 2 * (4 - i) + 1;
  }

  var name = "SparseSortInherit(" + scale + ")-";

  Array.prototype.sort.call(y);

  assertEquals(length, y.length, name +"length");

  for (var i = 0; i < 10; i++) {
    assertTrue(y.hasOwnProperty(i), name + "hasvalue" + i);
    assertEquals(i, y[i], name + "value" + i);
  }
  for (var i = 10; i < length; i++) {
    assertFalse(y.hasOwnProperty(i), name + "noundef" + i);

    if (x.hasOwnProperty(i)) {
      assertTrue(0 == i % (2 * scale), name + "new_x" + i);
    }
  }
}

TestSparseInheritedElementSort(10);
TestSparseInheritedElementSort(100);
TestSparseInheritedElementSort(1000);

function TestSpecialCasesInheritedElementSort() {

  var x = {
    1:"d1",
    2:"c1",
    3:"b1",
    4: undefined,
    __proto__: {
      length: 10000,
      1: "e2",
      10: "a2",
      100: "b2",
      1000: "c2",
      2000: undefined,
      8000: "d2",
      12000: "XX",
      __proto__: {
        0: "e3",
        1: "d3",
        2: "c3",
        3: "b3",
        4: "f3",
        5: "a3",
        6: undefined,
      }
    }
  };
  Array.prototype.sort.call(x);

  var name = "SpecialInherit-";

  assertEquals(10000, x.length, name + "length");
  var sorted = ["a2", "a3", "b1", "b2", "c1", "c2", "d1", "d2", "e3",
                undefined, undefined, undefined];
  for (var i = 0; i < sorted.length; i++) {
    assertTrue(x.hasOwnProperty(i), name + "has" + i)
    assertEquals(sorted[i], x[i], name + i);
  }
  assertFalse(x.hasOwnProperty(sorted.length), name + "haspost");
  assertFalse(sorted.length in x, name + "haspost2");
  assertTrue(x.hasOwnProperty(10), name + "hasundefined10");
  assertEquals(undefined, x[10], name + "undefined10");
  assertFalse(x.hasOwnProperty(100), name + "hasno100");
  assertEquals("b2", x[100], "inherits100");
  assertFalse(x.hasOwnProperty(1000), name + "hasno1000");
  assertEquals("c2", x[1000], "inherits1000");
  assertFalse(x.hasOwnProperty(2000), name + "hasno2000");
  assertEquals(undefined, x[2000], "inherits2000");
  assertFalse(x.hasOwnProperty(8000), name + "hasno8000");
  assertEquals("d2", x[8000], "inherits8000");
  assertFalse(x.hasOwnProperty(12000), name + "has12000");
  assertEquals("XX", x[12000], name + "XX12000");
}

TestSpecialCasesInheritedElementSort();

// Test that sort calls compare function with global object as receiver,
// and with only elements of the array as arguments.
function o(v) {
  return {__proto__: o.prototype, val: v};
}
var arr = [o(1), o(2), o(4), o(8), o(16), o(32), o(64), o(128), o(256), o(-0)];
var global = this;
function cmpTest(a, b) {
  assertEquals(global, this);
  assertTrue(a instanceof o);
  assertTrue(b instanceof o);
  return a.val - b.val;
}
arr.sort(cmpTest);

function TestSortDoesNotDependOnObjectPrototypeHasOwnProperty() {
  Array.prototype.sort.call({
    __proto__: { hasOwnProperty: null, 0: 1 },
    length: 5
  });

  var arr = new Array(2);
  Object.defineProperty(arr, 0, { get: function() {}, set: function() {} });
  arr.hasOwnProperty = null;
  arr.sort();
}

TestSortDoesNotDependOnObjectPrototypeHasOwnProperty();

function TestSortDoesNotDependOnArrayPrototypePush() {
  // InsertionSort is used for arrays which length <= 22
  var arr = [];
  for (var i = 0; i < 22; i++) arr[i] = {};
  Array.prototype.push = function() {
    fail('Should not call push');
  };
  arr.sort();

  // Quicksort is used for arrays which length > 22
  // Arrays which length > 1000 guarantee GetThirdIndex is executed
  arr = [];
  for (var i = 0; i < 2000; ++i) arr[i] = {};
  arr.sort();
}

TestSortDoesNotDependOnArrayPrototypePush();

function TestSortDoesNotDependOnArrayPrototypeSort() {
  var arr = [];
  for (var i = 0; i < 2000; i++) arr[i] = {};
  var sortfn = Array.prototype.sort;
  Array.prototype.sort = function() {
    fail('Should not call sort');
  };
  sortfn.call(arr);
  // Restore for the next test
  Array.prototype.sort = sortfn;
}

TestSortDoesNotDependOnArrayPrototypeSort();

function TestSortToObject() {
  Number.prototype[0] = 5;
  Number.prototype[1] = 4;
  Number.prototype.length = 2;
  x = new Number(0);
  assertEquals(0, Number(Array.prototype.sort.call(x)));
  assertEquals(4, x[0]);
  assertEquals(5, x[1]);
  assertArrayEquals(["0", "1"], Object.getOwnPropertyNames(x));
  // The following would throw if ToObject weren't called.
  assertEquals(0, Number(Array.prototype.sort.call(0)));
}
TestSortToObject();

function TestSortOnProxy() {
  {
    var p = new Proxy([2,1,3], {});
    assertEquals([1,2,3], p.sort());
  }

  {
    function f() { return arguments };
    var a = f(2,1,3);
    a.__proto__ = new Proxy({}, {});
    assertEquals([1,2,3], [...(Array.prototype.sort.apply(a))]);
  }
}
TestSortOnProxy();

function TestSortOnNonExtensible() {
  {
    var arr = [1,,2];
    Object.preventExtensions(arr);
    assertThrows(() => arr.sort(), TypeError);
    assertEquals(arr, [1,,2]);
  }
  {
    var arr = [1,,undefined];
    Object.preventExtensions(arr);
    assertThrows(() => arr.sort(), TypeError);
    assertFalse(arr.hasOwnProperty(1));
    assertEquals(arr, [1,,undefined]);
  }
  {
    var arr = [1,undefined,2];
    Object.preventExtensions(arr);
    arr.sort();
    assertEquals(arr, [1,2,undefined]);
  }
}
TestSortOnNonExtensible();

function TestSortOnTypedArray() {
  var array = new Int8Array([10,9,8,7,6,5,4,3,2,1]);
  Object.defineProperty(array, "length", {value: 5});
  Array.prototype.sort.call(array);
  assertEquals(array, new Int8Array([10,6,7,8,9,5,4,3,2,1]));

  var array = new Int8Array([10,9,8,7,6,5,4,3,2,1]);
  Object.defineProperty(array, "length", {value: 15});
  Array.prototype.sort.call(array);
  assertEquals(array, new Int8Array([1,10,2,3,4,5,6,7,8,9]));
}
TestSortOnTypedArray();


// Test special prototypes
(function testSortSpecialPrototypes() {
  function test(proto, length, expected) {
    var result = {
       length: length,
       __proto__: proto,
     };
    Array.prototype.sort.call(result);
    assertEquals(expected.length, result.length, "result.length");
    for (var i = 0; i<expected.length; i++) {
      assertEquals(expected[i], result[i], "result["+i+"]");
    }
  }

  (function fast() {
    // Fast elements, non-empty
    test(arguments, 0, []);
    test(arguments, 1, [2]);
    test(arguments, 2, [1, 2]);
    test(arguments, 4, [1, 2, 3, 4]);
    delete arguments[0]
    // sort copies down the properties to the receiver, hence result[1]
    // is read on the arguments through the hole on the receiver.
    test(arguments, 2, [1, 1]);
    arguments[0] = undefined;
    test(arguments, 2, [1, undefined]);
  })(2, 1, 4, 3);

  (function fastSloppy(a) {
    // Fast sloppy
    test(arguments, 0, []);
    test(arguments, 1, [2]);
    test(arguments, 2, [1, 2]);
    delete arguments[0]
    test(arguments, 2, [1, 1]);
    arguments[0] = undefined;
    test(arguments, 2, [1, undefined]);
  })(2, 1);

  (function fastEmpty() {
    test(arguments, 0, []);
    test(arguments, 1, [undefined]);
    test(arguments, 2, [undefined, undefined]);
  })();

  (function stringWrapper() {
    // cannot redefine string wrapper properties
    assertThrows(() => test(new String('cba'), 3, []), TypeError);
  })();

  (function typedArrys() {
    test(new Int32Array(0), 0, []);
    test(new Int32Array(1), 1, [0]);
    var array = new Int32Array(3);
    array[0] = 2;
    array[1] = 1;
    array[2] = 3;
    test(array, 1, [2]);
    test(array, 2, [1, 2]);
    test(array, 3, [1, 2, 3]);
  })()

})();

assertThrows(() => {
  Array.prototype.sort.call(undefined);
}, TypeError);

// This test ensures that RemoveArrayHoles does not shadow indices in the
// prototype chain. There are multiple code paths, we force both and check that
// they have the same behavior.
function TestPrototypeHoles() {
  function test(forceGenericFallback) {
    let proto2 = {
      7: 27,
    };

    let proto1 = {
      __proto__: proto2,
      8: 18,
      9: 19,
    };

    let xs = {
      __proto__: proto1,
      length: 10,
      7: 7,
      8: 8,
      9: 9,
    };

    if (forceGenericFallback) {
      Object.defineProperty(xs, "6", {
        get: () => this.foo,
        set: (val) => this.foo = val
      });
    }
    xs[6] = 6;

    Array.prototype.sort.call(xs, (a, b) => a - b);

    assertEquals(10, xs.length);
    assertEquals(6, xs[0]);
    assertEquals(7, xs[1]);
    assertEquals(8, xs[2]);
    assertEquals(9, xs[3]);

    // Index 7,8,9 will get the prototype values.
    assertFalse(xs.hasOwnProperty(7));
    assertEquals(27, xs[7]);

    assertFalse(xs.hasOwnProperty(8));
    assertEquals(18, xs[8]);

    assertFalse(xs.hasOwnProperty(9));
    assertEquals(19, xs[9]);
  }

  test(false);
  // Expect a TypeError when trying to delete the accessor.
  assertThrows(() => test(true), TypeError);
}
TestPrototypeHoles();

// The following test ensures that [[Delete]] is called and it throws.
function TestArrayWithAccessorThrowsOnDelete() {
  let array = [5, 4, 1, /*hole*/, /*hole*/];

  Object.defineProperty(array, '4', {
    get: () => array.foo,
    set: (val) => array.foo = val
  });
  assertThrows(() => array.sort((a, b) => a - b), TypeError);
}
TestArrayWithAccessorThrowsOnDelete();

// The following test ensures that elements on the prototype are also copied
// for JSArrays and not only JSObjects.
function TestArrayPrototypeHasElements() {
  let array = [1, 2, 3, 4, 5];
  for (let i = 0; i < array.length; i++) {
    delete array[i];
    Object.prototype[i] = 42;
  }

  let comparator_called = false;
  array.sort(function (a, b) {
    if (a === 42 || b === 42) {
      comparator_called = true;
    }
    return a - b;
  });

  assertTrue(comparator_called);
}
TestArrayPrototypeHasElements();

// The following Tests make sure that there is no crash when the element kind
// or the array length changes. Since comparison functions like this are not
// consistent, we do not have to make sure that the array is actually sorted
//
// The assertions for the element kinds are not there to ensure that a specific
// action causes a specific element kind change, but rather that we have most
// of the transitions covered.

function cmp_smaller(a, b) {
  if (a < b) return -1;
  if (b < a) return 1;
  return 0;
}

function create_cmpfn(transformfn) {
  let cmp_count = 0;
  return (a, b) => {
    ++cmp_count;
    if (cmp_count == 2) {
      transformfn();
    }

    return cmp_smaller(a, b);
  }
}

function HasPackedSmi(xs) {
  return %HasFastPackedElements(xs) && %HasSmiElements(xs);
}

function HasPackedDouble(xs) {
  return %HasFastPackedElements(xs) && %HasDoubleElements(xs);
}

function HasPackedObject(xs) {
  return %HasFastPackedElements(xs) && %HasObjectElements(xs);
}

function HasHoleySmi(xs) {
  return %HasHoleyElements(xs) && %HasSmiElements(xs);
}

function HasHoleyDouble(xs) {
  return %HasHoleyElements(xs) && %HasDoubleElements(xs);
}

function HasHoleyObject(xs) {
  return %HasHoleyElements(xs) && %HasObjectElements(xs);
}

function TestSortCmpPackedSmiToPackedDouble() {
  let xs = [2,1,4];

  assertTrue(HasPackedSmi(xs));
  xs.sort(create_cmpfn(() => xs[0] += 0.1));
  assertTrue(HasPackedDouble(xs));
}
TestSortCmpPackedSmiToPackedDouble();

function TestSortCmpPackedDoubleToPackedElement() {
  let xs = [2.1, 1.2, 4.4];

  assertTrue(HasPackedDouble(xs));
  xs.sort(create_cmpfn(() => xs[0] = 'a'));
  assertTrue(HasPackedObject(xs));
}
TestSortCmpPackedDoubleToPackedElement();

function TestSortCmpPackedElementToDictionary() {
  let xs = ['a', 'b', 'c'];

  assertTrue(HasPackedObject(xs));
  xs.sort(create_cmpfn(() => xs[%MaxSmi()] = 'd'));
  assertTrue(%HasDictionaryElements(xs));
}
TestSortCmpPackedElementToDictionary();

function TestSortCmpHoleySmiToHoleyDouble() {
  let xs = [2, 1, 4];
  xs[5] = 42;

  assertTrue(HasHoleySmi(xs));
  xs.sort(create_cmpfn(() => xs[0] += 0.1));
  assertTrue(HasHoleyDouble(xs));
}
TestSortCmpHoleySmiToHoleyDouble();

function TestSortCmpHoleyDoubleToHoleyElement() {
  let xs = [2.1, 1.2, 4];
  xs[5] = 42;

  assertTrue(HasHoleyDouble(xs));
  xs.sort(create_cmpfn(() => xs[0] = 'a'));
  assertTrue(HasHoleyObject(xs));
}
TestSortCmpHoleyDoubleToHoleyElement();

function TestSortCmpHoleyElementToDictionary() {
  let xs = ['b', 'a', 'd'];
  xs[5] = '42';

  assertTrue(HasHoleyObject(xs));
  xs.sort(create_cmpfn(() => xs[%MaxSmi()] = 'e'));
  assertTrue(%HasDictionaryElements(xs));
}
TestSortCmpHoleyElementToDictionary();

function TestSortCmpPackedSmiToHoleySmi() {
  let xs = [2, 1, 4];

  assertTrue(HasPackedSmi(xs));
  xs.sort(create_cmpfn(() => xs[10] = 42));
  assertTrue(HasHoleySmi(xs));
}
TestSortCmpPackedSmiToHoleySmi();

function TestSortCmpPackedDoubleToHoleyDouble() {
  let xs = [2.1, 1.2, 4];

  assertTrue(HasPackedDouble(xs));
  xs.sort(create_cmpfn(() => xs[10] = 42));
  assertTrue(HasHoleyDouble(xs));
}
TestSortCmpPackedDoubleToHoleyDouble();

function TestSortCmpPackedObjectToHoleyObject() {
  let xs = ['b', 'a', 'd'];

  assertTrue(HasPackedObject(xs));
  xs.sort(create_cmpfn(() => xs[10] = '42'));
  assertTrue(HasHoleyObject(xs));
}
TestSortCmpPackedObjectToHoleyObject();

function TestSortCmpPackedChangesLength() {
  let xs = [2, 1, 4];

  assertTrue(HasPackedSmi(xs));
  xs.sort(create_cmpfn(() => xs.length *= 2));
  assertTrue(HasHoleySmi(xs));
}
TestSortCmpPackedChangesLength();

function TestSortCmpPackedSetLengthToZero() {
  let xs = [2, 1, 4, 3];

  assertTrue(HasPackedSmi(xs));
  xs.sort(create_cmpfn(() => xs.length = 0));
  assertTrue(HasPackedSmi(xs));
}
