// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --maglev-object-tracking
// Flags: --max-turbolev-eager-inlined-bytecode-size=0
// Flags: --trace-turbo-inlining --no-stress-concurrent-inlining
// Flags: --no-stress-maglev --no-optimize-on-next-call-optimizes-to-maglev

d8.file.execute('test/mjsunit/mjsunit.js');

// Disabling eager inlining makes Maglev build the caller before the mutation
// below is introduced by the later Maglev inlining phase. Passing the rest
// array to a callee keeps it escaping during graph building, so argument
// forwarding must not be selected before that phase runs.

function mutate(a) {
  a[0] = 42;
}

function join() {
  return Array.prototype.join.call(arguments, '|');
}

function spreadAfterLateInlinedMutation(...r) {
  mutate(r);
  return join(...r);
}

%PrepareFunctionForOptimization(mutate);
%PrepareFunctionForOptimization(spreadAfterLateInlinedMutation);

assertEquals('42|2|3', spreadAfterLateInlinedMutation(1, 2, 3));
assertEquals('42|2|3', spreadAfterLateInlinedMutation(1, 2, 3));

// CHECK-LABEL: TestLateInlinedMutation
print('TestLateInlinedMutation');
%OptimizeFunctionOnNextCall(spreadAfterLateInlinedMutation);
assertEquals('42|2|3', spreadAfterLateInlinedMutation(1, 2, 3));
// ANSI color escapes separate the decision from the function name.
// CHECK: INLINE{{.*}}mutate
assertOptimized(spreadAfterLateInlinedMutation);
