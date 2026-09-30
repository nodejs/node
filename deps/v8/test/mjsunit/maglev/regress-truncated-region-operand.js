// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --no-additive-safe-int-feedback

// Same shape as regress-truncated-region-bound.js, but the tagged use of `big`
// now comes *after* `sum`, so the backward propagation pass clears the
// truncation flag of `big` before it evaluates `sum`. `sum` then no longer
// walks into `big` to bound it, and has to bound it as a plain operand -- from
// its static range, which is just under 2^53. Charging it the additive safe
// integer feedback range instead would accept `sum`, leaving `small` truncated
// on its own and widened back with a ChangeInt32ToFloat64 that carries the
// wrapped value rather than the value it stands for.
//
// Neither the additive safe integer feedback nor the speculative truncation it
// enables is involved.
let escape;
function makeChain(tag) {
  return new Function('a, b',
                      'var x = (a|0) + 1;' +
                      'var y = (b|0) + 1;' +
                      'var small = x + (-4294967296);' +
                      'var big = y + 9007197107257343;' +
                      'var sum = small + big;' +
                      'escape = big;' +
                      'return sum | 0;  //' + tag);
}

// Identical `new Function` sources share a SharedFunctionInfo, so each copy
// needs a distinct body.
const reference = makeChain('ref');
%NeverOptimizeFunction(reference);
const chain = makeChain('opt');

// Warm up with heap numbers, so that `| 0` keeps accepting them, and with
// small values, so the additions stay in the Int32 range they are built from.
%PrepareFunctionForOptimization(chain);
chain(1.5, 1.5);
%OptimizeMaglevOnNextCall(chain);
chain(1.5, 1.5);

assertEquals(reference(2147483646, 2), chain(2147483646, 2));
assertEquals(2 + 1 + 9007197107257343, escape);
