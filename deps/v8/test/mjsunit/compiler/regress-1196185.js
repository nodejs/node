// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --disable-in-process-stack-traces
// Flags: --gc-interval=500 --stress-compaction

class X {}
function rando() {}

let named;
function foo(input) {
  var b;
  rando(), {
    blah: function () { b = b(); },
  };

  for (var i = 0; i < 10; i++) {
    var r = rando();
    var broom;

    try {
      input[r];
      named();
    } catch (e) {}

    try {
       broom = __v_859.exports.main;
    } catch (e) {}

    try {
      for (var j = 0; j < 10; j++) {
         broom();
      }
    } catch (e) {}
  }
}

function testfunc() {
  for (var i = 0; i < 10; i++) {
    %PrepareFunctionForOptimization(foo);
    %OptimizeFunctionOnNextCall(foo);
    foo();
  }
}
testfunc();
