// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --no-additive-safe-int-feedback

// The truncation pass rewrites the addition chain into wrapping Int32Adds,
// dropping their overflow checks, so `s` no longer holds a genuine Int32: its
// JavaScript value is 5 * 2^31. It then truncates `s * 1000003` as well,
// because the multiplication was marked truncatable from the *static* Int32
// range of `s` rather than from the value it really holds. JavaScript computes
// a double product above 2^53 and rounds it, so the wrapping product differs.
//
// Neither the additive safe integer feedback nor the speculative truncation it
// enables is involved: the chain is built from Int32AddWithOverflow, and the
// multiplication is a plain Float64Multiply.
function makeChain(tag) {
  return new Function('a, b, c, d, e',
                      'var s = (a|0) + (b|0) + (c|0) + (d|0) + (e|0);' +
                      'var m = s * 1000003;' +
                      'return m | 0;  //' + tag);
}

// Identical `new Function` sources share a SharedFunctionInfo, so each copy
// needs a distinct body.
const reference = makeChain('ref');
%NeverOptimizeFunction(reference);
const chain = makeChain('opt');

// Warm up with heap numbers, so that `| 0` keeps accepting them, with sums
// that stay Smi sized, so the additions keep SignedSmall feedback and become
// Int32AddWithOverflow, and with a product that does not, so the
// multiplication gets Number feedback and becomes Float64Multiply.
%PrepareFunctionForOptimization(chain);
for (let i = 0; i < 100; i++) chain(400.5, 400.5, 400.5, 400.5, 400.5);
%OptimizeMaglevOnNextCall(chain);
chain(400.5, 400.5, 400.5, 400.5, 400.5);

const v = 2147483647.5;
assertEquals(reference(v, v, v, v, v), chain(v, v, v, v, v));
