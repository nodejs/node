// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --use-osr

function f(a) {
  var sum = 0;
  for (var i of a) {
    var x = i + 2;
    var y = x + 5;
    var z = y + 3;
    sum += z;
  }
  return sum;
}

function wrap(array) {
  var iterable = {};
  var i = 0;
  function next() {
    return { done: i >= array.length, value: array[i++] };
  };
  iterable[Symbol.iterator] = function() { return { next:next }; };
  return iterable;
}

function test(a) {
  for (var i = 0; i < 10000; i++) {
    a[i] = (i * 999) % 77;
  }

  for (var i = 0; i < 3; i++) {
    assertEquals(480270, f(wrap(a)));
  }
}

var a = new Array(10000);
test(a);

// Non-extensible
var b =  Object.preventExtensions(a);
test(b);

// Sealed
var c =  Object.seal(a);
test(c);

// Frozen
var d =  Object.freeze(a);
test(d);
