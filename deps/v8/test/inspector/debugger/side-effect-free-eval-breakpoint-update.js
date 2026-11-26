// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests setting and removing breakpoints during a side-effect-free ' +
    'evaluation.');

session.setupScriptMap();

contextGroup.addScript(`function spin() {
  while (true) {}
}

function G(early) {
  if (early) return 0;
  class C {
    get #a() { return 42; }
    read() { return this.#a; }
  }
  return new C().read();
}

function H(x) {
  const o = {v: x};
  o.v = x + 1;
  return o.v;
}

function R(x) {
  class D {
    get #b() { return x; }
    read() { return this.#b; }
  }
  return new D().read();
}

const holder = {
  value: 7,
  M(early) {
    const captureThis = () => this.value;
    if (early) return 0;
    {
      let blockLocal = captureThis();
      class E {
        get #c() { return blockLocal; }
        read() { return this.#c; }
      }
      return new E().read();
    }
  }
};

const globalHolder = {value: 0};
//# sourceURL=test.js`);

async function runDuringTerminatedSideEffectEval(callback) {
  // Arm the interrupt before queueing Runtime.evaluate so the interrupt
  // deterministically fires on entry to the side-effect-free evaluation.
  utils.interruptForMessages();
  const evalPromise = Protocol.Runtime.evaluate({
    expression: 'spin()',
    throwOnSideEffect: true,
    timeout: 60000,
  });
  const callbackResult = await callback();
  const terminatePromise = Protocol.Runtime.terminateExecution();
  await Protocol.Runtime.runIfWaitingForDebugger();
  const evalResult = await evalPromise;
  // Trigger CallCompletedCallback so terminateExecution resolves cleanly.
  await Protocol.Runtime.evaluate({expression: '0'});
  await terminatePromise;
  return {callbackResult, evalResult};
}

InspectorTest.runAsyncTestSuite([
  async function setUp() {
    await Protocol.Runtime.enable();
    await Protocol.Debugger.enable();
  },

  async function testUpdateBreakpointOnPreparedFunction() {
    // Set BP1 on `return new C().read();` so G has an instrumented
    // DebugBytecodeArray before the side-effect-free evaluation begins.
    const {result: {breakpointId: bpFinal}} =
        await Protocol.Debugger.setBreakpointByUrl({
          url: 'test.js',
          lineNumber: 10,
        });

    // Set BP2 on the `Return` position of `if (early) return 0;` from a V8
    // interrupt while `spin()` runs in side-effect-check mode, then terminate
    // the evaluation.
    const {callbackResult: {result: {breakpointId: bpEarly}}, evalResult} =
        await runDuringTerminatedSideEffectEval(
            () => Protocol.Debugger.setBreakpointByUrl({
              url: 'test.js',
              lineNumber: 5,
              columnNumber: 22,
            }));
    InspectorTest.log('spin error: ' + evalResult.error.message);

    const pauses = [];
    Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
      const top = callFrames[0];
      pauses.push(
          `${top.functionName}:${top.location.lineNumber}:${
              top.location.columnNumber}`);
      const setRet =
          await Protocol.Debugger.setReturnValue({newValue: {value: 99}});
      InspectorTest.log(
          'setReturnValue at pause: ' +
          (setRet.error ? setRet.error.message : 'ok'));
      await Protocol.Debugger.resume();
    });

    const {result: {result}} = await Protocol.Runtime.evaluate({
      expression: 'G(false)',
      returnByValue: true,
    });
    Protocol.Debugger.onPaused(null);

    InspectorTest.log('G(false) pauses: ' + JSON.stringify(pauses));
    InspectorTest.log('G(false) result: ' + result.value);

    await Protocol.Debugger.removeBreakpoint({breakpointId: bpEarly});
    await Protocol.Debugger.removeBreakpoint({breakpointId: bpFinal});
  },

  async function testSetBreakpointOnUnpreparedFunction() {
    const {callbackResult: {result: {breakpointId}}} =
        await runDuringTerminatedSideEffectEval(
            () => Protocol.Debugger.setBreakpointByUrl({
              url: 'test.js',
              lineNumber: 16,
            }));

    const pauses = [];
    Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
      const top = callFrames[0];
      pauses.push(`${top.functionName}:${top.location.lineNumber}`);
      await Protocol.Debugger.resume();
    });

    const {result: {result}} = await Protocol.Runtime.evaluate({
      expression: 'H(10)',
      returnByValue: true,
    });
    Protocol.Debugger.onPaused(null);

    InspectorTest.log('H(10) pauses: ' + JSON.stringify(pauses));
    InspectorTest.log('H(10) result: ' + result.value);

    await Protocol.Debugger.removeBreakpoint({breakpointId});
  },

  async function testRemoveBreakpointDuringSideEffectEval() {
    const {result: {breakpointId}} =
        await Protocol.Debugger.setBreakpointByUrl({
          url: 'test.js',
          lineNumber: 24,
        });

    await runDuringTerminatedSideEffectEval(
        () => Protocol.Debugger.removeBreakpoint({breakpointId}));

    let paused = false;
    Protocol.Debugger.onPaused(async () => {
      paused = true;
      await Protocol.Debugger.resume();
    });

    const {result: {result}} = await Protocol.Runtime.evaluate({
      expression: 'R(77)',
      returnByValue: true,
    });
    Protocol.Debugger.onPaused(null);

    InspectorTest.log('R(77) paused: ' + paused);
    InspectorTest.log('R(77) result: ' + result.value);
  },

  async function testConditionalBreakpointWithContextReceiver() {
    const {result: {breakpointId: bpEarly}} =
        await Protocol.Debugger.setBreakpointByUrl({
          url: 'test.js',
          lineNumber: 31,
          columnNumber: 24,
          condition: 'this.value === 999',
        });

    const {callbackResult: {result: {breakpointId: bpCond}}} =
        await runDuringTerminatedSideEffectEval(
            () => Protocol.Debugger.setBreakpointByUrl({
              url: 'test.js',
              lineNumber: 38,
              condition: 'this.value === 999',
            }));

    let paused = false;
    Protocol.Debugger.onPaused(async () => {
      paused = true;
      await Protocol.Debugger.resume();
    });

    const {result: {result}} = await Protocol.Runtime.evaluate({
      expression: 'holder.M(false)',
      returnByValue: true,
    });
    Protocol.Debugger.onPaused(null);

    InspectorTest.log('holder.M(false) paused: ' + paused);
    InspectorTest.log('holder.M(false) result: ' + result.value);

    await Protocol.Debugger.removeBreakpoint({breakpointId: bpEarly});
    await Protocol.Debugger.removeBreakpoint({breakpointId: bpCond});
  },

  async function testRemoveBreakpointPreservesSideEffectChecks() {
    // Pre-register a URL breakpoint on line 0 of `eval-mutate.js` so the
    // top-level eval script gets BreakInfo and a side-effect-patched
    // DebugBytecodeArray before entering execution. `globalHolder.value = 1`
    // compiles to `SetNamedProperty` (`kRequiresRuntimeChecks`), and the
    // interrupt fires at the script-entry StackCheck after `OnFunctionCall`
    // while the frame is already active on the stack.
    const {result: {breakpointId}} =
        await Protocol.Debugger.setBreakpointByUrl({
          url: 'eval-mutate.js',
          lineNumber: 0,
        });

    utils.interruptForMessages();
    const evalPromise = Protocol.Runtime.evaluate({
      expression: 'globalHolder.value = 1;\n//# sourceURL=eval-mutate.js',
      throwOnSideEffect: true,
    });
    await Protocol.Debugger.removeBreakpoint({breakpointId});
    await Protocol.Runtime.runIfWaitingForDebugger();
    const sideEffectResult = await evalPromise;
    InspectorTest.log(
        'eval-mutate side-effect check threw: ' +
        Boolean(sideEffectResult.result.exceptionDetails));

    const {result: {result: valueAfterSideEffectEval}} =
        await Protocol.Runtime.evaluate({
          expression: 'globalHolder.value',
          returnByValue: true,
        });
    InspectorTest.log(
        'globalHolder.value after side-effect eval: ' +
        valueAfterSideEffectEval.value);

    let paused = false;
    Protocol.Debugger.onPaused(async () => {
      paused = true;
      await Protocol.Debugger.resume();
    });
    const {result: {result: normalResult}} = await Protocol.Runtime.evaluate({
      expression: 'globalHolder.value = 1;\n//# sourceURL=eval-mutate.js',
      returnByValue: true,
    });
    Protocol.Debugger.onPaused(null);
    InspectorTest.log('eval-mutate normal paused: ' + paused);
    InspectorTest.log('eval-mutate normal result: ' + normalResult.value);
  },

  async function testDisableDebuggerDuringSideEffectEval() {
    await Protocol.Debugger.setBreakpointByUrl({
      url: 'test.js',
      lineNumber: 20,
    });

    await runDuringTerminatedSideEffectEval(() => Protocol.Debugger.disable());

    const {result: {result}} = await Protocol.Runtime.evaluate({
      expression: 'R(88)',
      returnByValue: true,
    });
    InspectorTest.log('R(88) after Debugger.disable result: ' + result.value);
  },
]);
