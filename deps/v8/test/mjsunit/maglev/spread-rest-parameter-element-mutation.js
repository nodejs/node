// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --maglev-object-tracking

// A rest parameter is a JSArray whose backing store is an ArgumentsElements
// node. TryGetNonEscapingArgumentsOrArray accepts such an array unconditionally
// once it sees ArgumentsElements, so a preceding element store (which lands on
// the ArgumentsElements node and leaves the array itself non-escaping) does not
// prevent the spread from being lowered to CallForwardVarargs. That builtin
// copies the caller's physical stack frame and drops the store.
//
// Turbolev shares these paths, hence %OptimizeFunctionOnNextCall rather than
// %OptimizeMaglevOnNextCall: it reaches Maglev under the variants that pass
// --optimize-on-next-call-optimizes-to-maglev and Turbolev under --turbolev.

function join() {
  return Array.prototype.join.call(arguments, '|');
}

function spread(...r) {
  r[0] = 42;
  return join(...r);
}

function apply(...r) {
  r[0] = 42;
  return join.apply(null, r);
}

function Collect() {
  this.result = Array.prototype.join.call(arguments, '|');
}

function construct(...r) {
  r[0] = 42;
  return new Collect(...r).result;
}

function testOptimized(test) {
  %PrepareFunctionForOptimization(test);
  assertEquals('42|2|3', test(1, 2, 3));
  assertEquals('42|2|3', test(1, 2, 3));
  %OptimizeFunctionOnNextCall(test);
  assertEquals('42|2|3', test(1, 2, 3));
  assertOptimized(test);
}

// These stores are emitted directly while building the caller graph and must
// disable argument forwarding.
for (const test of [spread, apply, construct]) {
  testOptimized(test);
}

// Also cover a store emitted while eagerly inlining a callee.
function mutate(a) {
  a[0] = 42;
}

function spreadAfterInlinedMutation(...r) {
  mutate(r);
  return join(...r);
}

%PrepareFunctionForOptimization(mutate);
mutate([1, 2, 3]);
mutate([1, 2, 3]);
testOptimized(spreadAfterInlinedMutation);

// The following cases exercise the safety conditions that let the mutation
// mark remain local to stores already emitted by the graph builder.

// Passing the rest array to a non-inlined callee must make it escape, which
// prevents the following spread from using argument forwarding.
function mutateNeverInlined(a) {
  a[0] = 42;
}

%PrepareFunctionForOptimization(mutateNeverInlined);
mutateNeverInlined([0]);
%NeverOptimizeFunction(mutateNeverInlined);

function spreadAfterNonInlinedMutation(...r) {
  mutateNeverInlined(r);
  return join(...r);
}

testOptimized(spreadAfterNonInlinedMutation);

// The spread comes first in bytecode order, but on the second iteration it has
// to observe the store from the first. The rest allocation is outside the loop,
// so argument forwarding must be rejected inside the loop.
function spreadBeforeMutationInLoop(...r) {
  let result;
  for (let i = 0; i < 2; i++) {
    result = join(...r);
    r[0] = 42;
  }
  return result;
}

testOptimized(spreadBeforeMutationInLoop);
