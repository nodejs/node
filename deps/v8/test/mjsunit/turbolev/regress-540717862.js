// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --print-turbolev-frontend
// Flags: --no-lazy-feedback-allocation

let cases = "";
for (let i = 0; i < 66000; i++) {
  cases += `case ${i}: `;
}
const g = Function(`switch(x){${cases} default:0;}`);

function f() {
  let r = 0;
  for (let i = 0; i < 3; i++) {
    try { r = g(); } catch (e) {}
  }
  return r;
}

%PrepareFunctionForOptimization(f);
f();
%OptimizeFunctionOnNextCall(f);
f();
