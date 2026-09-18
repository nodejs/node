// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --fuzzing --allow-natives-syntax

let get_p;
class C {
  get #m() { return "PRIVATE"; }
  [ (() => {
      get_p =o => o.#m;
      %PrepareFunctionForOptimization(get_p);
      try { get_p(); } catch(e) {}
      %OptimizeMaglevOnNextCall(get_p);
      try { get_p(); } catch(e) {}
  })() ] = 1;
}
let c = new C();
"Result: " + get_p(c);
