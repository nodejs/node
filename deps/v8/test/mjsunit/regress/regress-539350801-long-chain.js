// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const maxFeedback = 1125899906842623;  // 2^50 - 1

// Same root cause as regress-539350801.js, but over a chain long enough that
// the cumulative bound is accumulated many times. The leading terms cancel so
// the value reaching the trailing run stays within the feedback range, and the
// trailing terms then leave the safe integer range.
let terms = [];
for (let i = 0; i < 4400; i++) terms.push(i % 2 ? 'b' : 'a');
for (let i = 0; i < 900; i++) terms.push('c');
const source = `return (${terms.join('+')})|0;`;

// Identical `new Function` sources share a SharedFunctionInfo, so the two
// copies need distinct bodies.
const reference = new Function('a', 'b', 'c', source + '//ref');
%NeverOptimizeFunction(reference);
const chain = new Function('a', 'b', 'c', source + '//opt');

%PrepareFunctionForOptimization(chain);
for (let i = 0; i < 100; i++) chain(maxFeedback, -maxFeedback, 2 ** 40);
%OptimizeFunctionOnNextCall(chain);
chain(maxFeedback, -maxFeedback, 2 ** 40);

const c = maxFeedback - 4;
assertEquals(reference(maxFeedback, -maxFeedback, c),
             chain(maxFeedback, -maxFeedback, c));
