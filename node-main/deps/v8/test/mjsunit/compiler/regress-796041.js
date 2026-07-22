// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

'use strict';

function f(abort, n, a, b) {
  if (abort) return;
  var x = a ? true : "" + a;
  if (!a) {
    var dead = n + 1 + 1;
    if(!b) {
      x = dead;
    }
    if (x) {
      x = false;
    }
    if (b) {
      x = false;
    }
  }
  return x + 1;
}
f(false, 5); f(false, 6); f(false, 7); f(false, 8);

function g(abort, a, b) {
  return f(abort, "abc", a, b);
}

%PrepareFunctionForOptimization(g);
g(true); g(true); g(true); g(true);

%OptimizeFunctionOnNextCall(g);
g(false);
