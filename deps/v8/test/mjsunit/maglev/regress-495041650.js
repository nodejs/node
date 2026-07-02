// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbolev --allow-natives-syntax --max-turbolev-eager-inlined-bytecode-size=0

function pf() {}
%PrepareFunctionForOptimization(pf);
function tt() { throw "bla"; }
%PrepareFunctionForOptimization(tt);
async function f(f) {
  for (let i13 = 0; pf();) {
    await 1;
    tt();
  }
  const x = 1;
  function g() { return x };
  return g();
}

(async function() {
%PrepareFunctionForOptimization(f);
for (let i = 0; i < 9;i++) await f();
%OptimizeFunctionOnNextCall(f);
await f();
})();
