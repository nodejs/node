// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev-untagged-phis

function foo(a1) {
  let v2 = 4.65;
  for (let i4 = 0;
       i4 < 1;
       (() => {
         const v8 = [-205447721,578521958,1073741824,7,556293461,10,-38728,261406352];
         let v9 = [6,-1024,65535,2147483647,13];
         function f10() {
           v9 ??= v8;
         }
         i4++;
       })()) {
         v2 = a1[i4];
       }
  return [v2];
}

const arr = [,1.1];

%PrepareFunctionForOptimization(foo);
foo(arr);

%OptimizeMaglevOnNextCall(foo);
foo(arr);
