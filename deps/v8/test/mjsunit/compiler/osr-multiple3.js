// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --use-osr

function f1(a,b,c) {
  var x = 0;
  var y = 0;
  var z = 0;
  for (var i = 0; i < 2; i++) {
    for (var j = 0; j < 2; j++) {
      while (a > 0) { x += 19; a--; }
      while (b > 0) { y += 23; b--; }
      while (c > 0) { z += 29; c--; }
    }
  }
  return x + y + z;
}

function f2(a,b,c) {
  var x = 0;
  var y = 0;
  var z = 0;
  for (var i = 0; i < 2; i++) {
    for (var j = 0; j < 2; j++) {
      while (a > 0) { x += 19; a--; }
      while (b > 0) { y += 23; b--; }
      while (c > 0) { z += 29; c--; }
    }
  }
  return x + y + z;
}


function f3(a,b,c) {
  var x = 0;
  var y = 0;
  var z = 0;
  for (var i = 0; i < 2; i++) {
    for (var j = 0; j < 2; j++) {
      while (a > 0) { x += 19; a--; }
      while (b > 0) { y += 23; b--; }
      while (c > 0) { z += 29; c--; }
    }
  }
  return x + y + z;
}

function check(f,a,b,c) {
  assertEquals(a * 19 + b * 23 + c * 29, f(a,b,c));
}

check(f1, 50000,     5,     6);
check(f2,     4, 50000,     6);
check(f3,    11,    12, 50000);
