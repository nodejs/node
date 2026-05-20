// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var error = "error";
function counter(x) {
  return (function() { if (x-- == 0) throw error;});
}

// TODO(asm): This module is not valid asm.js.
function Module() {
  "use asm";

  function w0(f) {
    while (1) f();
    return 108;
  }

  function w1(f) {
    if (1) while (1) f();
    return 109;
  }

  function w2(f) {
    if (1) while (1) f();
    else while (1) f();
    return 110;
  }

  function w3(f) {
    if (0) while (1) f();
    return 111;
  }

  return { w0: w0, w1: w1, w2: w2, w3: w3 };
}

var m = Module();
assertThrowsEquals(function() { m.w0(counter(5)) }, error);
assertThrowsEquals(function() { m.w1(counter(5)) }, error);
assertThrowsEquals(function() { m.w2(counter(5)) }, error);
assertEquals(111, m.w3(counter(5)));
