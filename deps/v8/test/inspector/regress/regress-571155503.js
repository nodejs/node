// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const {session, contextGroup, Protocol} =
    InspectorTest.start('Regression test for crbug.com/571155503.');

contextGroup.addScript(`
function bar1() {
  %ScheduleBreak();
}
function foo1() {
  bar1();
}

function bar2() {
  %ScheduleBreak();
}
function foo2() {
  bar2();
}

let trigger3 = false;
function side_effect3(o) {
  o.x = 1;
}
%NeverOptimizeFunction(side_effect3);

function bar3(o, do_se) {
  if (do_se) side_effect3(o);
  if (trigger3) {
    debugger;
  }
}
%NeverOptimizeFunction(bar3);

function foo3(o, do_se) {
  bar3(o, do_se);
  return o.x;
}

function runCase3() {
  %PrepareFunctionForOptimization(foo3);
  let obj = {x: 0};
  for (let i = 0; i < 10; i++) foo3(obj, false);
  %OptimizeFunctionOnNextCall(foo3);
  foo3(obj, false);
  trigger3 = true;
  foo3(obj, false);
}

async function* gen4() {
  yield 1;
}
`, 0, 0, 'test.js');

InspectorTest.runAsyncTestSuite([
  // Case 1: Clearing the last breakpoint during an interrupt at a Return
  // bytecode after a one-shot step/debug break.
  async function testClearBreakPointDuringInterruptAtReturn() {
    await Protocol.Runtime.enable();
    await Protocol.Debugger.enable();

    const {result: {breakpointId: bp}} =
        await Protocol.Debugger.setBreakpointByUrl({
          lineNumber: 4,
          url: 'test.js',
        });

    const evalPromise = Protocol.Runtime.evaluate({expression: 'foo1()'});
    await Protocol.Debugger.oncePaused();
    await Protocol.Debugger.stepOut();

    await Protocol.Debugger.oncePaused();
    await Protocol.Debugger.removeBreakpoint({breakpointId: bp});
    await Protocol.Debugger.resume();
    await evalPromise;

    await Protocol.Debugger.disable();
    await Protocol.Runtime.disable();
  },

  // Case 2: Clearing a breakpoint and setting a new breakpoint in the caller
  // during an interrupt at a Return bytecode.
  async function testSetBreakPointDuringInterruptAtReturn() {
    await Protocol.Runtime.enable();
    await Protocol.Debugger.enable();

    const {result: {breakpointId: bp}} =
        await Protocol.Debugger.setBreakpointByUrl({
          lineNumber: 11,
          url: 'test.js',
        });

    const evalPromise = Protocol.Runtime.evaluate({expression: 'foo2()'});
    await Protocol.Debugger.oncePaused();
    await Protocol.Debugger.stepOut();

    await Protocol.Debugger.oncePaused();
    await Protocol.Debugger.removeBreakpoint({breakpointId: bp});
    const {result: {breakpointId: bp2}} =
        await Protocol.Debugger.setBreakpointByUrl({
          lineNumber: 11,
          url: 'test.js',
        });
    await Protocol.Debugger.resume();
    await evalPromise;

    await Protocol.Debugger.removeBreakpoint({breakpointId: bp2});
    await Protocol.Debugger.disable();
    await Protocol.Runtime.disable();
  },

  // Case 3: Lazy deoptimization of an active optimized frame after
  // Debug::PerformSideEffectCheck creates a DebugInfo with debug bytecode
  // (without BreakInfo), followed by setting and clearing a breakpoint.
  async function testLazyDeoptAfterSideEffectCheck() {
    await Protocol.Runtime.enable();
    await Protocol.Debugger.enable();

    const evalPromise = Protocol.Runtime.evaluate({expression: 'runCase3()'});
    const {params: {callFrames}} = await Protocol.Debugger.oncePaused();

    await Protocol.Debugger.evaluateOnCallFrame({
      callFrameId: callFrames[0].callFrameId,
      expression: 'foo3({x: 0}, true)',
      throwOnSideEffect: true,
    });
    await Protocol.Runtime.evaluate({
      expression: '%DeoptimizeFunction(foo3)',
    });
    const {result: {breakpointId: bp}} =
        await Protocol.Debugger.setBreakpointByUrl({
          lineNumber: 29,
          url: 'test.js',
        });
    await Protocol.Debugger.removeBreakpoint({breakpointId: bp});
    await Protocol.Debugger.resume();
    await evalPromise;

    await Protocol.Debugger.disable();
    await Protocol.Runtime.disable();
  },

  // Case 4: Async generator SuspendGenerator bytecode during side-effect check
  // evaluation followed by clearing breakpoints.
  async function testAsyncGeneratorSuspendDuringSideEffectCheck() {
    await Protocol.Runtime.enable();
    await Protocol.Debugger.enable();

    const {result: {breakpointId: bp}} =
        await Protocol.Debugger.setBreakpointByUrl({
          lineNumber: 44,
          url: 'test.js',
        });

    const evalPromise = Protocol.Runtime.evaluate({expression: 'gen4().next()'});
    const {params: {callFrames}} = await Protocol.Debugger.oncePaused();

    await Protocol.Debugger.evaluateOnCallFrame({
      callFrameId: callFrames[0].callFrameId,
      expression: 'gen4().next()',
      throwOnSideEffect: true,
    });
    await Protocol.Debugger.removeBreakpoint({breakpointId: bp});
    await Protocol.Debugger.resume();
    await evalPromise;

    await Protocol.Debugger.disable();
    await Protocol.Runtime.disable();
  },
]);
