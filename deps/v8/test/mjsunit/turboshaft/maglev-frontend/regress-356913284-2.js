// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan

let glob_nan = NaN;

function* foo(n) {
  // Creating a Phi so that ComputePredecessorPermutations gets called a first
  // time on a block with 2 predecessors.
  let v = n ? 3 : 7;

  // Running the function only once doesn't collect enough feedback for the next
  // line, which will thus lead to an uncondition deopt there.
  class C { [undefined] }

  // The following loop will still be emitted, despite the unconditional deopt
  // above, because there is a yield inside (that is, it looks like the body is
  // reachable despite the deopt above; it's not, but Maglev doesn't realize
  // it).
  let i = 0;
  for (i = n + 1; glob_nan;) {
    yield 42;
  }

  yield i + 17 + v;
}

%PrepareFunctionForOptimization(foo);
let gen = foo(3);
assertEquals(24, gen.next().value);
assertEquals(undefined, gen.next().value);

%OptimizeFunctionOnNextCall(foo);
gen = foo(3);
assertEquals(24, gen.next().value);
assertEquals(undefined, gen.next().value);
