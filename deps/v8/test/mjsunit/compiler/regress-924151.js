// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function g(code) {
  try {
    if (typeof code === 'function') {
      +Symbol();
    } else {
      eval();
    }
  } catch (e) {
    return;
  }
  dummy();
}

function f() {
  g(g);
}

try { g(); } catch(e) {; }

%PrepareFunctionForOptimization(f);
f();
%OptimizeFunctionOnNextCall(f);
f();
