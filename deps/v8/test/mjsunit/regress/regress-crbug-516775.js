// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function arguments_with_length_getter(f) {
  arguments.__defineGetter__('length', f);
  return arguments;
}

var count = 0;
function increment_count_return() { count++; return "boom"; }
function increment_count_throw() { count++; throw "boom"; }

// Do not read the length of an arguments object on the prototype chain of
// an array.
var a1 = [];
%NormalizeElements(a1);
a1.__proto__ = arguments_with_length_getter(increment_count_return);
[].concat(a1);
assertEquals(0, count);

var a2 = [];
%NormalizeElements(a2);
a2.__proto__ = arguments_with_length_getter(increment_count_throw);
[].concat(a2);
assertEquals(0, count);

// Do read the length of an arguments object if spreadable.
var a3 = arguments_with_length_getter(increment_count_return);
a3[Symbol.isConcatSpreadable] = true;
[].concat(a3);
assertEquals(1, count);

var a4 = arguments_with_length_getter(increment_count_throw);
a4[Symbol.isConcatSpreadable] = true;
assertThrows(function() { [].concat(a4); });
assertEquals(2, count);

// Do read the length of an arguments object on the prototype chain of
// an object.
var a5 = {};
a5.__proto__ = arguments_with_length_getter(increment_count_return);
a5[Symbol.isConcatSpreadable] = true;
[].concat(a5);
assertEquals(3, count);

var a6 = {};
a6.__proto__ = arguments_with_length_getter(increment_count_throw);
a6[Symbol.isConcatSpreadable] = true;
assertThrows(function() { [].concat(a6); });
assertEquals(4, count);
