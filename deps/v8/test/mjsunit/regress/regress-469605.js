// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function counter() {
  var i = 100;
  return function() {
    if (i-- > 0) return i;
    throw "done";
  }
}

var c1 = counter();
var c2 = counter();

var f = (function() {
  "use asm";
  return function f(i) {
    i = i|0;
    do {
      if (i > 0) c1();
      else c2();
    } while (true);
  }
})();

assertThrows(function() { f(0); });
assertThrows(function() { f(1); });

var c3 = counter();

var g = (function() {
  "use asm";
  return function g(i) {
    i = i + 1;
    do {
      i = c3(i);
    } while (true);
  }
})();

assertThrows(function() { g(0); });
assertThrows(function() { g(1); });
