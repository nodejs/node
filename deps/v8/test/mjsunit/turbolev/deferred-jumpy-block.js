// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --no-turbofan --maglev-non-eager-inlining

// This test is probably a bit fragile, but currently, because of how deferring
// works, the last block of the graph (after the register allocator has deferred
// blocks) ends up being one that ends with a Jump (whose destination is a
// previous block that probably isn't deferred, or just happened to be earlier
// in the deferred list for some reason). This is an interesting edge case.

function leaf(obj) {
  try {
    // Generic call that can throw. Forces the try-catch structure to be preserved.
    obj.method();
  } catch(e) {
    // catch block
  }
  return 1; // after-try
}

function main(flag, str, obj) {
  if (flag) {
    leaf(obj);
    // Force a static deopt in Maglev.
    str.startsWith("h", "not-a-smi");
  }
  return 2;
}

%PrepareFunctionForOptimization(main);
%PrepareFunctionForOptimization(leaf);

let dummy_safe = { method() {} };
let dummy_throwing = { method() { throw "err"; } };

// Train with both to ensure the catch block is executed and preserved
main(true, "hello", dummy_safe);
main(true, "hello", dummy_throwing);
main(true, "hello", dummy_safe);

// Optimize. Maglev will inline leaf().
// The catch block is preserved because it was executed during training.
// The startsWith call at the end of the 'then' branch forces a static deopt.
%OptimizeMaglevOnNextCall(main);
main(true, "hello", dummy_safe);
