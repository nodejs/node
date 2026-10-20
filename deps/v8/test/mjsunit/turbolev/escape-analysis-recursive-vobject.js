// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbolev-escape-analysis
// Flags: --maglev-verify-dominance

function RecObj() {
  this.a = this;
}

function baz() {}
%NeverOptimizeFunction(baz);

function foo() {
  // Creating a recursive object.
  const o = new RecObj();

  // Non-inlined function call, which will require a DeoptFrame, which will have
  // {o} as input.
  baz();

  // Keeping {o} alive so that it's in the DeoptFrame for the call above, but
  // making sure that it doesn't escape so that it can be elided.
  return o.e;
}

%PrepareFunctionForOptimization(foo);
%PrepareFunctionForOptimization(RecObj);
foo();
foo();

%OptimizeFunctionOnNextCall(foo);
foo();
