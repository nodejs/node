// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test array functions do not cause infinite loops when length is negative,
// max_value, etc.

// ArrayToString

var o = { length: Number.MIN_VALUE };
var result = Array.prototype.toString.call(o);
assertEquals("[object Object]", result);

// ArrayToLocaleString

var o = { length: Number.MIN_VALUE };
var result = Array.prototype.toLocaleString.call(o);
assertEquals("", result);

// ArrayJoin

var o = { length: Number.MIN_VALUE };
var result = Array.prototype.join.call(o);
assertEquals(0, result.length);

// ArrayPush

var o = { length: Number.MIN_VALUE };
Array.prototype.push.call(o, 1);
assertEquals(1, o.length);
assertEquals(1, o[0]);

var o = { length: Number.MAX_VALUE };
assertThrows(() => Array.prototype.push.call(o, 1), TypeError);

// ArrayPop

var o = { length: 0 };
Array.prototype.pop.call(o);
assertEquals(0, o.length);

var o = { length: Number.MIN_VALUE };
Array.prototype.pop.call(o);
assertEquals(0, o.length);

var o = { length: Number.MAX_VALUE };
Array.prototype.pop.call(o);
assertEquals(o.length, Number.MAX_SAFE_INTEGER - 1);

// ArrayReverse

var o = { 0: 'foo', length: Number.MIN_VALUE }
var result = Array.prototype.reverse.call(o);
assertEquals('object', typeof(result));
assertEquals(Number.MIN_VALUE, result.length);
assertEquals(Number.MIN_VALUE, o.length);

// ArrayShift

var o = { 0: "foo", length: Number.MIN_VALUE }
var result = Array.prototype.shift.call(o);
assertEquals(undefined, result);
assertEquals(0, o.length);

// ArrayUnshift

var o = { length: 0 };
Array.prototype.unshift.call(o);
assertEquals(0, o.length);

var o = { length: 0 };
Array.prototype.unshift.call(o, 'foo');
assertEquals('foo', o[0]);
assertEquals(1, o.length);

var o = { length: Number.MIN_VALUE };
Array.prototype.unshift.call(o);
assertEquals(0, o.length);

var o = { length: Number.MIN_VALUE };
Array.prototype.unshift.call(o, 'foo');
assertEquals('foo', o[0]);
assertEquals(1, o.length);

// ArraySplice

var o = { length: Number.MIN_VALUE };
Array.prototype.splice.call(o);
assertEquals(0, o.length);

var o = { length: Number.MIN_VALUE };
Array.prototype.splice.call(o, 0, 10, ['foo']);
assertEquals(['foo'], o[0]);
assertEquals(1, o.length);

var o = { length: Number.MIN_VALUE };
Array.prototype.splice.call(o, -1);
assertEquals(0, o.length);

var o = { length: Number.MAX_SAFE_INTEGER };
Array.prototype.splice.call(o, -1);
assertEquals(Number.MAX_SAFE_INTEGER - 1, o.length);

// ArraySlice

var o = { length: Number.MIN_VALUE };
var result = Array.prototype.slice.call(o);
assertEquals(0, result.length);

var o = { length: Number.MIN_VALUE };
var result = Array.prototype.slice.call(o, Number.MAX_VALUE);
assertEquals(0, result.length);

var o = { length: Number.MAX_VALUE };
var result = Array.prototype.slice.call(o, Number.MAX_VALUE - 1);
assertEquals(0, result.length);

// ArrayIndexOf

var o = { length: Number.MIN_VALUE };
var result = Array.prototype.indexOf.call(o);
assertEquals(-1, result);

var o = { length: Number.MAX_SAFE_INTEGER }
o[Number.MAX_SAFE_INTEGER - 1] = "foo"
var result = Array.prototype.indexOf.call(o,
    "foo", Number.MAX_SAFE_INTEGER - 2);
assertEquals(Number.MAX_SAFE_INTEGER - 1, result);

var o = { length: Number.MAX_SAFE_INTEGER };
o[Number.MAX_SAFE_INTEGER - 1] = "foo";
var result = Array.prototype.indexOf.call(o, "foo", -1);
assertEquals(Number.MAX_SAFE_INTEGER - 1, result);

// ArrayLastIndexOf

var o = { length: Number.MIN_VALUE };
var result = Array.prototype.lastIndexOf.call(o);
assertEquals(-1, result);

var o = { length: Number.MAX_SAFE_INTEGER }
o[Number.MAX_SAFE_INTEGER - 1] = "foo"
var result = Array.prototype.lastIndexOf.call(o,
    "foo", Number.MAX_SAFE_INTEGER);
assertEquals(Number.MAX_SAFE_INTEGER - 1, result);

var o = { length: Number.MAX_SAFE_INTEGER };
o[Number.MAX_SAFE_INTEGER - 1] = "foo";
var result = Array.prototype.lastIndexOf.call(o, "foo", -1);
assertEquals(Number.MAX_SAFE_INTEGER - 1, result);

// ArrayFilter

var func = function(v) { return v; }

var o = { length: Number.MIN_VALUE };
Array.prototype.filter.call(o, func);
assertEquals(Number.MIN_VALUE, o.length);

// ArrayForEach

var o = { length: Number.MIN_VALUE };
Array.prototype.forEach.call(o, func);
assertEquals(Number.MIN_VALUE, o.length);

// ArraySome

var o = { length: Number.MIN_VALUE };
Array.prototype.some.call(o, func);
assertEquals(Number.MIN_VALUE, o.length);

// ArrayEvery

var o = { length: Number.MIN_VALUE };
Array.prototype.every.call(o, func);
assertEquals(Number.MIN_VALUE, o.length);

// ArrayMap

var o = { length: Number.MIN_VALUE };
Array.prototype.map.call(o, func);
assertEquals(Number.MIN_VALUE, o.length);

// ArrayReduce

var o = { length: Number.MIN_VALUE };
Array.prototype.reduce.call(o, func, 0);
assertEquals(Number.MIN_VALUE, o.length);

// ArrayReduceRight

var o = { length: Number.MIN_VALUE };
Array.prototype.reduceRight.call(o, func, 0);
assertEquals(Number.MIN_VALUE, o.length);

// ArrayFill

var o = { length: Number.MIN_VALUE };
Array.prototype.fill(o, 0);
assertEquals(Number.MIN_VALUE, o.length);
