// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const maxFeedback = 1125899906842623;  // 2^50 - 1

// Same root cause as regress-539350801-int32-operand.js, over a chain long
// enough that the cumulative bound is accumulated many times. The leading
// terms cancel so the value reaching the trailing run stays within the
// feedback range, and the trailing terms then leave the safe integer range.
let terms = ['s'];
for (let i = 0; i < 4400; i++) terms.push(i % 2 ? 'b' : 'a');
for (let i = 0; i < 900; i++) terms.push('c');
const source = `const s = x & 0x3ffffff; return (${terms.join('+')})|0;`;

// Identical `new Function` sources share a SharedFunctionInfo, so the two
// copies need distinct bodies.
const reference = new Function('x', 'a', 'b', 'c', source + '//ref');
%NeverOptimizeFunction(reference);
const chain = new Function('x', 'a', 'b', 'c', source + '//opt');

// The seed has to be zero during warmup: any larger value pushes the running
// sum past the feedback range, which degrades the feedback to Number and makes
// the test vacuous.
%PrepareFunctionForOptimization(chain);
chain(0, maxFeedback, -maxFeedback, 2 ** 40);
%OptimizeFunctionOnNextCall(chain);
chain(0, maxFeedback, -maxFeedback, 2 ** 40);

const c = maxFeedback - 4;
assertEquals(reference(0, maxFeedback, -maxFeedback, c),
             chain(0, maxFeedback, -maxFeedback, c));
