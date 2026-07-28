// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc --maglev-non-eager-inlining
// Flags: --no-lazy-feedback-allocation

function F0() {
  for (const v5 of "kA") {
    for (let [v10, ...v11] of "p") {
      function f13() {
        try { (1).slice(); } catch (e) {}
        return f13;
      }
      f13().call();
      gc({execution: "async"}).finally(v11);
    }
    %OptimizeOsr();
  }
}
%PrepareFunctionForOptimization(F0);
F0();
