// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan

function nop() {}
%NeverOptimizeFunction(nop);

function foo(o) {
  o.oh;
  nop();
  o.no;
}

%PrepareFunctionForOptimization(foo);
foo({});
foo(1);
%OptimizeFunctionOnNextCall(foo);
foo(2);
