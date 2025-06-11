// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbolev-non-eager-inlining

function empty() { }

// bytecode length of non_eager_inlined() is not smaller than
// max_inlined_bytecode_size_small()
// ShouldEagerInlineCall() returns false
// non_eager_inlined() avoids greedy inlining
function non_eager_inlined(bool) {
  if (bool) { empty(); empty(); empty(); }
}

function target() {
  // there must be at least 1 inlined function in maglev graph to proceed
  // non-eager inlining empty() is greedy-inlined in maglev
  // graph building phase
  empty();

  let tmp;
  try {
    throw tmp = non_eager_inlined();
  } catch (e) {
    // insert phi node to exception handler block
    return tmp;
  }
}

%PrepareFunctionForOptimization(empty);
%PrepareFunctionForOptimization(non_eager_inlined);
%PrepareFunctionForOptimization(target);
target();
%OptimizeFunctionOnNextCall(target);
target();
