// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --use-osr

function id(f) { return f; }

function foo() {
  var sum = 0;
  var r = id(foo);
  for (var i = 0; i < 100000; i++) {
    sum += i;
  }
  return foo == r;
}

assertEquals(true, foo());
assertEquals(true, foo());
assertEquals(true, foo());


function bar() {
  var sum = 0;
  for (var i = 0; i < 90000; i++) {
    sum += i;
  }
  return id(bar,sum);
}

assertEquals(bar, bar());
assertEquals(bar, bar());
assertEquals(bar, bar());
