// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --use-osr

function foo(a) {
  var sum = 0;
  var inc = a ? 100 : 200;
  for (var i = 0; i < 100000; i++) {
    sum += inc;
  }
  return sum + inc;
}

function bar(a) {
  var sum = 0;
  var inc = a ? 100 : 200;
  var x = a ? 5 : 6;
  var y = a ? 7 : 8;
  for (var i = 0; i < 100000; i++) {
    sum += inc;
  }
  return sum ? x : y;
}

function baz(a) {
  var limit = a ? 100001 : 100002;
  var r = 1;
  var x = a ? 1 : 2;
  var y = a ? 3 : 4;
  for (var i = 0; i < limit; i++) {
    r = r * -1;
  }
  return r > 0 ? x == y : x != y;
}

function qux(a) {
  var limit = a ? 100001 : 100002;
  var r = 1;
  var x = a ? 1 : 2;
  var y = a ? 3 : 4;
  for (var i = 0; i < limit; i++) {
    r = r * -1;
  }
  var w = r > 0 ? x : y;
  var z = r > 0 ? y : x;
  return w === z;
}

function test(func, tv, fv) {
  assertEquals(tv, func(true));
  assertEquals(fv, func(false));
  assertEquals(tv, func(true));
  assertEquals(fv, func(false));
}

test(foo, 10000100, 20000200);
test(bar, 5, 6);
test(baz, true, false);
test(qux, false, false);
