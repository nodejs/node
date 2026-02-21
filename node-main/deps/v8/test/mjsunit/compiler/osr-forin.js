// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --use-osr

function f(a) {
  var sum = 0;
  for (var j in a) {
    var i = a[j];
    var x = i + 2;
    var y = x + 5;
    var z = y + 3;
    sum += z;
  }
  return sum;
}

function test(a) {
  for (var i = 0; i < 10000; i++) {
    a[i] = (i * 999) % 77;
  }

  for (var i = 0; i < 3; i++) {
    console.log(f(a));
    assertEquals(480270, f(a));
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
