// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const o1 = {a: 1};
const o2 = {b: 2};
Object.defineProperty(o2, 'a', {get() { return 1; }, enumerable: true});

globalThis.deleted_global = 1;

function* gen(do_load) {
  // The unconditional break leaves ForInStep dead.
  for (let k1 in o1) {
    // Prevents Maglev from peeling the outer for-in (only innermost loops are
    // peeled), so the index is a loop phi rather than constant 0 and does not
    // dominate the second for-in below.
    for (let j = 0; j < 1; j++) {}
    break;
  }
  // Deopts unconditionally once the global is deleted, so the ForInPrepare
  // below is dead and the second for-in is only reachable through resumption.
  deleted_global;
  for (let k2 in o2) {
    if (do_load) o2[k2];
    yield;
  }
}

%PrepareFunctionForOptimization(gen);
let g = gen(false);
g.next();
delete globalThis.deleted_global;
%OptimizeMaglevOnNextCall(gen);
g.next();
g.next();
