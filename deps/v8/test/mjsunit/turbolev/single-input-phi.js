// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev-future

function bar(x) {
  x+x+x+x+x+x+x+x+x+x+x+x+x; // preventing eager inlining
  return false;
}

function foo() {
  let phi;
  if (bar()) {
    phi = 42;
  } else {
    phi = 17;
  }
  phi += 2;
  %TurbofanStaticAssert(phi == 19);
}

%PrepareFunctionForOptimization(bar);
%PrepareFunctionForOptimization(foo);
foo();
foo();

%OptimizeFunctionOnNextCall(foo);
foo();
