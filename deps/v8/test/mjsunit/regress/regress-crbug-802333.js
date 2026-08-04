// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function deferred_func() {
  class C {
    method1() {}
  }
}

let bound = (a => a).bind(this, 0);

function opt() {
  deferred_func.prototype;  // ReduceJSLoadNamed

  return bound();
};
%PrepareFunctionForOptimization(opt);
assertEquals(0, opt());
%OptimizeFunctionOnNextCall(opt);

assertEquals(0, opt());
