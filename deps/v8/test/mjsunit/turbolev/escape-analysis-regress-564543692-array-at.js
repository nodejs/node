// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbolev-escape-analysis
// Flags: --no-maglev-range-analysis --no-turbolev-untagged-phis

function bar() {}
%NeverOptimizeFunction(bar);

const offset = 0x1fffffff;

function foo() {
  let arr = [3.3, , 4.5];

  bar();

  return arr.at(offset);
}

%PrepareFunctionForOptimization(foo);
assertEquals(undefined, foo());

%OptimizeFunctionOnNextCall(foo);
assertEquals(undefined, foo());
