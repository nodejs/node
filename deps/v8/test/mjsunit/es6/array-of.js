// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Based on Mozilla Array.of() tests at http://dxr.mozilla.org/mozilla-central/source/js/src/jit-test/tests/collections

// Flags: --allow-natives-syntax


// Array.of makes real arrays.

function check(a) {
    assertEquals(Object.getPrototypeOf(a), Array.prototype);
    assertEquals(Array.isArray(a), true);
    a[9] = 9;
    assertEquals(a.length, 10);
}


check(Array.of());
check(Array.of(0));
check(Array.of(0, 1, 2));
var f = Array.of;
check(f());


// Array.of basics

var a = Array.of();

assertEquals(a.length, 0);
a = Array.of(undefined, null, 3.14, []);
assertEquals(a, [undefined, null, 3.14, []]);
a = [];
for (var i = 0; i < 1000; i++)
    a[i] = i;
assertEquals(Array.of.apply(null, a), a);


// Array.of does not leave holes

assertEquals(Array.of(undefined), [undefined]);
assertEquals(Array.of(undefined, undefined), [undefined, undefined]);
assertEquals(Array.of.apply(null, [,,undefined]), [undefined, undefined, undefined]);
assertEquals(Array.of.apply(null, Array(4)), [undefined, undefined, undefined, undefined]);


// Array.of can be transplanted to other classes.

var hits = 0;
function Bag() {
    hits++;
}
Bag.of = Array.of;

hits = 0;
var actual = Bag.of("zero", "one");
assertEquals(hits, 1);

hits = 0;
var expected = new Bag;
expected[0] = "zero";
expected[1] = "one";
expected.length = 2;
assertEquals(areSame(actual, expected), true);

hits = 0;
actual = Array.of.call(Bag, "zero", "one");
assertEquals(hits, 1);
assertEquals(areSame(actual, expected), true);

function areSame(object, array) {
    var result = object.length == array.length;
    for (var i = 0; i < object.length; i++) {
        result = result && object[i] == array[i];
    }
    return result;
}


// Array.of does not trigger prototype setters.
// (It defines elements rather than assigning to them.)

var status = "pass";
Object.defineProperty(Array.prototype, "0", {set: function(v) {status = "FAIL 1"}});
assertEquals(Array.of(1)[0], 1);
assertEquals(status, "pass");

Object.defineProperty(Bag.prototype, "0", {set: function(v) {status = "FAIL 2"}});
assertEquals(Bag.of(1)[0], 1);
assertEquals(status, "pass");


// Array.of passes the number of arguments to the constructor it calls.

var hits = 0;

function Herd(n) {
    assertEquals(arguments.length, 1);
    assertEquals(n, 5);
    hits++;
}

Herd.of = Array.of;
Herd.of("sheep", "cattle", "elephants", "whales", "seals");
assertEquals(hits, 1);


// Array.of calls a "length" setter if one is present.

var hits = 0;
var lastObj = null, lastVal = undefined;
function setter(v) {
    hits++;
    lastObj = this;
    lastVal = v;
}

// when the setter is on the new object
function Pack() {
    Object.defineProperty(this, "length", {set: setter});
}
Pack.of = Array.of;
var pack = Pack.of("wolves", "cards", "cigarettes", "lies");
assertEquals(lastObj, pack);
assertEquals(lastVal, 4);

// when the setter is on the new object's prototype
function Bevy() {}
Object.defineProperty(Bevy.prototype, "length", {set: setter});
Bevy.of = Array.of;
var bevy = Bevy.of("quail");
assertEquals(lastObj, bevy);
assertEquals(lastVal, 1);


// Array.of does a strict assignment to the new object's .length.
// The assignment is strict even if the code we're calling from is not strict.

function Empty() {}
Empty.of = Array.of;
Object.defineProperty(Empty.prototype, "length", {get: function() { return 0; }});

var nothing = new Empty;
nothing.length = 2;  // no exception; this is not a strict mode assignment

assertThrows(function() { Empty.of(); }, TypeError);


// Check superficial features of Array.of.

var desc = Object.getOwnPropertyDescriptor(Array, "of");

assertEquals(desc.configurable, true);
assertEquals(desc.enumerable, false);
assertEquals(desc.writable, true);
assertEquals(Array.of.length, 0);
assertThrows(function() { new Array.of() }, TypeError);  // not a constructor

// When the this-value passed in is not a constructor, the result is an array.
[
  undefined,
  null,
  false,
  "cow",
  NaN,
  67,
  Infinity,
  -Infinity,
  Math.cos, // builtin functions with no [[Construct]] slot
  Math.cos.bind(Math) // bound builtin functions with no [[Construct]] slot
].forEach(function(val) {
    assertEquals(Array.isArray(Array.of.call(val, val)), true);
});


(function testBoundConstructor() {
  var boundFn = (function() {}).bind(null);
  var instance = Array.of.call(boundFn, 1, 2, 3);
  assertEquals(instance.length, 3);
  assertEquals(instance instanceof boundFn, true);
  assertEquals(Array.isArray(instance), false);
})();

(function testDefinesOwnProperty() {
  // Assert that [[DefineOwnProperty]] is used in ArrayFrom, meaning a
  // setter isn't called, and a failed [[DefineOwnProperty]] will throw.
  var setterCalled = 0;
  function exotic() {
    Object.defineProperty(this,  '0', {
      get: function() { return 2; },
      set: function() { setterCalled++; }
    });
  }
  // Non-configurable properties can't be overwritten with DefineOwnProperty
  assertThrows(function () { Array.of.call(exotic, 1); }, TypeError);
  // The setter wasn't called
  assertEquals(0, setterCalled);

  // Check that array properties defined are writable, enumerable, configurable
  function ordinary() { }
  var x = Array.of.call(ordinary, 2);
  var xlength = Object.getOwnPropertyDescriptor(x, 'length');
  assertEquals(1, xlength.value);
  assertEquals(true, xlength.writable);
  assertEquals(true, xlength.enumerable);
  assertEquals(true, xlength.configurable);
  var x0 = Object.getOwnPropertyDescriptor(x, 0);
  assertEquals(2, x0.value);
  assertEquals(true, xlength.writable);
  assertEquals(true, xlength.enumerable);
  assertEquals(true, xlength.configurable);
})();

(function testElementsKind() {
  // Check that Array.of returns PACKED elements.
  var arr = Array.of(1, 2, 3);
  assertTrue(%HasFastPackedElements(arr));
  assertTrue(%HasSmiElements(arr));

  var arr = Array.of(1, 2.5, 3);
  assertTrue(%HasFastPackedElements(arr));
  assertTrue(%HasDoubleElements(arr));

  var arr = Array.of.call(Array, Array(65536));
  assertTrue(%HasFastPackedElements(arr));
  assertTrue(%HasObjectElements(arr));
})();
