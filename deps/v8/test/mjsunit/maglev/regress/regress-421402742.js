// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

const f = () => {};

function nop() { }
function do_nop() { nop(); }

function foo() {
  eval();
  for (var i = 0; i <= 2; i++) {
    do_nop();
    if (f != null && typeof f == "object") Object.defineProperty();
    %OptimizeOsr();
  }
}

%PrepareFunctionForOptimization(nop);
%PrepareFunctionForOptimization(do_nop);
%PrepareFunctionForOptimization(foo);
foo();
