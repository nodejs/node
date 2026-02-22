// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --stack-size=100

function main(){
  function f0() {
    return f0;
  }
  class C1 extends f0 {
    constructor() {
      for (let i5 = 0, i6 = 10; i6; i6--) {
      }
      new C1();
    }
  }
  try { new C1(); } catch {}
}
main();
%PrepareFunctionForOptimization(main);
main();
main();
%OptimizeMaglevOnNextCall(main);
main();
