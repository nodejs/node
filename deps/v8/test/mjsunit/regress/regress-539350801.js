// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const maxFeedback = 1125899906842623;  // 2^50 - 1

// Every operand stays inside the additive safe integer feedback range, but the
// running sum of the leading terms leaves the exactly representable range and
// gets rounded. The additions must not be truncated to Word32, otherwise the
// optimized code computes the exact sum where JavaScript rounds.
const params = [];
for (let i = 0; i < 20; i++) params.push('p' + i);
const body = 'return ((' + params.join(') + (') + ')) | 0;';

// Identical `new Function` sources share a SharedFunctionInfo, so the two
// copies need distinct bodies.
const reference = new Function(...params, body + '//ref');
%NeverOptimizeFunction(reference);
const optimized = new Function(...params, body + '//opt');

const x = maxFeedback - 4;
// Ten positive terms first: the running sum passes 2^53 and is rounded. The
// trailing negative terms then cancel it back down.
const args = [];
for (let i = 0; i < 10; i++) args.push(x);
for (let i = 0; i < 10; i++) args.push(-x);

// Warm up with pairwise cancelling operands, so the feedback is
// AdditiveSafeInteger and no intermediate leaves the representable range.
const warmup = [];
for (let i = 0; i < 20; i++) warmup.push(i % 2 ? -maxFeedback : maxFeedback);

%PrepareFunctionForOptimization(optimized);
for (let i = 0; i < 100; i++) optimized(...warmup);
%OptimizeFunctionOnNextCall(optimized);
optimized(...warmup);

assertEquals(reference(...args), optimized(...args));
