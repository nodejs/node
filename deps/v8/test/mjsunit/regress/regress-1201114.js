// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turboprop --allow-natives-syntax --print-code

var a = {b: 1};
function nop() { return false; }
function __f_4(a) { return a; }
function __f_5(__v_2) {
  __f_4(__v_2.a);
  nop(__f_5)&a.b;
}
%PrepareFunctionForOptimization(__f_5);
__f_5(true);
%OptimizeFunctionOnNextCall(__f_5);
try {
  __f_5();
} catch {}
