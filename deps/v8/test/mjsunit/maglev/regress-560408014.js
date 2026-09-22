// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev-non-eager-inlining
// Flags: --maglev-disable-builtin-reducers
// Flags: --max-maglev-eager-inlined-bytecode-size=0

// {f} is not inlined eagerly by the graph builder, so that the Maglev graph
// optimizer runs on {main}'s graph in order to inline it late. While walking
// that graph, the optimizer also reduces the {entries} calls into inlined
// allocations of array iterators.
function f() {}

function main() {
  f();
  // The array literals are inlined allocations created by the graph builder.
  // The iterator allocated for the second {entries} call must not be folded
  // into the allocation block of the first iterator, since the second array
  // literal allocates in between (and could thus trigger a GC, after which
  // the first block is not the most recent young allocation anymore).
  [].entries();
  [].entries();
}

%PrepareFunctionForOptimization(f);
%PrepareFunctionForOptimization(main);
main();
%OptimizeMaglevOnNextCall(main);
main();
