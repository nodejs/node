// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan
// Flags: --no-maglev --no-stress-concurrent-inlining
// Flags: --invocation-count-for-turbofan=3000
// TODO(dmercadier): Investigate why loop peeling is breaking this test.
// Flags: --no-turbolev-non-eager-loop-peeling

// This test checks that loop interrupt stack checks are properly detected are
// processed.

// Note that interrupt requests are not handled at the same time in Maglev and
// Turbofan: in Maglev, they are not directly checked; execution instead relies
// on budget interrupts performing stack check interrupts, whereas in Turbofan,
// they are checked at every loop iteration.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Checks that loop interrupt stack checks are properly detected and ' +
    'processed.');

contextGroup.addScript(`
function g(x) {
  if (x == 0) {
    %ScheduleBreak();
  }
  return 7;
}

function loop_interrupt_check_f(x) {
  let r = 0;
  for (let i = 0; i < 10; i++) {
    if (i == 0) {
      // Schedules an interrupt if {x} is 0.
      r += g(x);
    } else {
      r += g(1);
    }
  }
  return r;
}

%PrepareFunctionForOptimization(loop_interrupt_check_f);
var r1 = loop_interrupt_check_f(1);
%OptimizeFunctionOnNextCall(loop_interrupt_check_f);
var r2 = loop_interrupt_check_f(1);
// V8OptimizationStatus.kOptimized is (1 << 3).
var opt = (%GetOptimizationStatus(loop_interrupt_check_f) & (1 << 3)) !== 0;

function checkUnoptimized() {
  return (%GetOptimizationStatus(loop_interrupt_check_f) & (1 << 3)) === 0;
}
`);

(async function test() {
  await Protocol.Debugger.enable();

  let trigger_count = 0;
  let called_from;
  Protocol.Debugger.onPaused(({params}) => {
    called_from = params.callFrames[0].functionName;
    trigger_count += 1;
    Protocol.Debugger.resume();
  });

  const {result: {result: {value: warmupResult}}} =
      await Protocol.Runtime.evaluate({
        expression: '({r1, r2, opt})',
        returnByValue: true,
      });
  InspectorTest.logObject(warmupResult);
  InspectorTest.log(`trigger_count before: ${trigger_count}`);

  const {result: {result: {value: r3}}} = await Protocol.Runtime.evaluate({
    expression: 'loop_interrupt_check_f(0)',
    returnByValue: true,
  });
  InspectorTest.log(`r3: ${r3}`);
  InspectorTest.log(`trigger_count after: ${trigger_count}`);
  InspectorTest.log(`called_from: ${called_from}`);

  // The function is expected to be deoptimized because the debugger pause
  // deoptimizes the topmost frame to prevent it from using stale
  // load-eliminated values in case the debugger executes JS that modifies heap
  // state.
  const {result: {result: {value: unoptimized}}} =
      await Protocol.Runtime.evaluate({
        expression: 'checkUnoptimized()',
        returnByValue: true,
      });
  InspectorTest.log(`unoptimized: ${unoptimized}`);

  InspectorTest.completeTest();
})();
