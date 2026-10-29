// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax
// Flags: --maglev-non-eager-inlining --max-maglev-inlined-bytecode-size-small=0

var g1;
const g2 = { [Symbol.toPrimitive]() { return 42; } };
let countCalls = 0;

function set_alloc_time(timeout, inline) {
  if (countCalls++ < 20) {
    %SetAllocationTimeout(-1, timeout, inline);
  }
};

function foo(f, {
  input: input,
  check: check
}) {
  try {
    let x;
    try {
      x = {
        value: f(input),
        exception: false
      };
    } catch (e) {
      x = {
        value: e,
        exception: true
      };
    }
    set_alloc_time(177, false);
    check(x);
  } catch (e) {}
}

function bar(input,) {
  function check(o) {
    try {
      g1(`${o.value}`);
    } catch (e) {}
  }
  return {
    input: input,
    check: check
  };
}

function main(x, ...xs) {
  try {
    for (let i = 0; i < xs.length; ++i) {
      %PrepareFunctionForOptimization(x);
      foo(x, xs[i]);
      %OptimizeFunctionOnNextCall(x);
      for (let j = 0; j < xs.length; ++j) {
        foo(x, xs[j]);
      }
      %DeoptimizeFunction(x);
    }
  } catch (e) {}
}

try {
  main(x => x,  bar(1), bar(""), bar(8.4), bar(null, 0), bar(g2));
} catch (e) {}
