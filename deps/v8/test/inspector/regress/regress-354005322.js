// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan
// Flags: --no-turboshaft-loop-unrolling --expose-gc

const {session, contextGroup, Protocol} =
    InspectorTest.start('Regression test for crbug.com/354005322.');

contextGroup.addScript(`
function foo(b) {
  %ScheduleBreak();

  // Allocating a first object.
  let o1 = { x : 42 };

  // Inserting a loop, in order to get a JSStackCheck.
  let v = 0;
  for (; v < 42; v = (v + 2) * 2) {}

  // Allocating a second object. This should not get allocation-folded with the
  // first allocation, because of the JSStackCheck's GC in the middle.
  let o2 = { x : o1, y : v };

  return o2;
}
%PrepareFunctionForOptimization(foo);
`);

(async function test() {
  await Protocol.Debugger.enable();

  Protocol.Debugger.onPaused(async () => {
    await Protocol.Runtime.evaluate({expression: 'gc()'});
    Protocol.Debugger.resume();
  });

  let {result: {result: r1}} = await Protocol.Runtime.evaluate({
    expression: 'foo()',
    returnByValue: true,
  });
  InspectorTest.logObject(r1.value);

  let {result: {result: r2}} = await Protocol.Runtime.evaluate({
    expression: 'foo()',
    returnByValue: true,
  });
  InspectorTest.logObject(r2.value);

  await Protocol.Runtime.evaluate({
    expression: '%OptimizeFunctionOnNextCall(foo)',
  });
  let {result: {result: r3}} = await Protocol.Runtime.evaluate({
    expression: 'foo()',
    returnByValue: true,
  });
  InspectorTest.logObject(r3.value);

  InspectorTest.completeTest();
})();
