// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --no-additive-safe-int-feedback

// `big` adds a constant just under 2^53, so its exact value leaves the range
// where wrapping Int32 arithmetic still agrees with rounded Number semantics,
// and the truncation pass has to refuse it. Refusing it also has to refuse
// `sum`, which is computed from it: otherwise `small` is truncated on its own,
// and `sum`, now holding one Int32 and one Float64 operand, converts the Int32
// back with a ChangeInt32ToFloat64 -- widening the wrapped value rather than
// the value it stands for, with no check anywhere on that path.
//
// The tagged use of `big` is what made this reachable: it clears the
// truncation flag of `big` alone, leaving `sum` truncatable.
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
                      'escape = big;' +
                      'var sum = small + big;' +
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
