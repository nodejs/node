// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --use-osr

function foo() {
  var sum = 0;
  for (var i = 0; i < 100000; i++) {
    sum += i;
  }
  return function() { return sum; }
}

assertEquals(4999950000, foo()());
assertEquals(4999950000, foo()());
assertEquals(4999950000, foo()());

function bar() {
  var sum = 0;
  var ret = 0;
  for (var i = 0; i < 90000; i++) {
    sum += i;
    if (i == 0) ret = function() { return sum; }
  }
  return ret;
}

assertEquals(4049955000, bar()());
assertEquals(4049955000, bar()());
assertEquals(4049955000, bar()());
