// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --stack-size=100

'use strict';

function makeCallChain(n) {
  let s = '(function f(){return f})';
  for (let i = 0; i < n; i++) s += '()';
  return s;
}

function test() {
  // The loop is here to ensure that the parser throw stack overflow on all
  // configurations. If it doesn't happen, update the numbers.
  for (let N = 300; N < 1500; N += 50) {
    const heavy = makeCallChain(N);

    const keyExpr = `(0 && (${heavy}), (function target(){ return 'k'; })())`;
    const src = `(class C { [${keyExpr}] = 1; })`;

    const C = eval(src);
    new C();
  }
}

assertThrows(test, RangeError, /Maximum call stack size exceeded/);
