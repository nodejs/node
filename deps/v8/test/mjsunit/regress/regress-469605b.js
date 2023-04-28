// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function counter() {
  var i = 10000;
  return function() {
    if (i-- > 0) return i;
    throw "done";
  }
}


var f = (function() {
  "use asm";
  return function f(i, c1, c2) {
    i = i|0;
    do {
      if (i > 0) { while (0 ? this : this) { c1(); } }
      else c2();
    } while (true);
  }
})();

assertThrows(function() { f(0, counter(), counter()); });
assertThrows(function() { f(1, counter(), counter()); });
