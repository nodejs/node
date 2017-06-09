// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function h(g) {
  return g();
}

function f() {
  var g;
  for (var i = 0; i < 10; i++) {
    var y = i;
    if (i === 5) {
      g = function() {
        return y;
      };
      assertEquals(5, h(g));
      assertEquals(5, h(g));
      %OptimizeFunctionOnNextCall(h);
      assertEquals(5, h(g));
    }
  }
  return g;
}

var myg = f();

assertEquals(9, h(myg));
