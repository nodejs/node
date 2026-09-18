// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let cases = [];
for (let i = 0; i < 70000; i++) {
  cases.push(`case ${i}:`);
}
let f = Function('x', `switch(x){${cases.join(' ')} default: 0;}`);

function h(cond) {
  if (cond) {
    f();
  }
}

function g(cond) {
  for (let i = 0; i < 10; i++) {
    h(cond);
  }
}

%PrepareFunctionForOptimization(g);
for (let i = 0; i < 20; i++) {
  g(true);
}
%OptimizeMaglevOnNextCall(g);
g(true);
