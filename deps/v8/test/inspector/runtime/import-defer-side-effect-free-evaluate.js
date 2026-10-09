// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-defer-import-eval

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Checks that reading an export of a deferred module namespace in a ' +
    'side-effect-free evaluation (as DevTools does for eager evaluation and ' +
    'autocomplete) fails without starting the module evaluation, and that a ' +
    'later regular evaluation still evaluates the module.');

contextGroup.addModuleWithoutEvaluating(
    `
globalThis.consoleEvaluated = true;
export const foo = 1;
`,
    'console');

contextGroup.addModuleWithoutEvaluating(
    `
globalThis.pausedEvaluated = true;
export const foo = 2;
`,
    'paused');

contextGroup.addModule(
    `
import defer * as consoleNs from 'console';
import defer * as pausedNs from 'paused';
globalThis.consoleNs = consoleNs;
globalThis.pausedNs = pausedNs;
`,
    'main');

async function evaluateToString(expression) {
  const {result: {result}} =
      await Protocol.Runtime.evaluate({expression: `String(${expression})`});
  return result.value;
}

async function logModuleStatus(name) {
  const {result: {result}} = await Protocol.Runtime.evaluate(
      {expression: `globalThis.${name}Ns`, generatePreview: true});
  const status =
      result.preview.properties.find(p => p.name === '[[ModuleStatus]]');
  InspectorTest.log(
      `[[ModuleStatus]] of ${name}: ` + (status ? status.value : '<none>'));
  InspectorTest.log(
      `globalThis.${name}Evaluated: ` +
      await evaluateToString(`globalThis.${name}Evaluated`));
}

function logEvaluateResult(response) {
  const {result, exceptionDetails} = response.result;
  if (exceptionDetails) {
    InspectorTest.log('Threw: ' + exceptionDetails.exception.description);
  } else {
    InspectorTest.log('Result: ' + result.value);
  }
}

InspectorTest.runAsyncTestSuite([
  async function testRuntimeEvaluate() {
    await Protocol.Runtime.enable();
    await InspectorTest.waitForPendingTasks();
    await logModuleStatus('console');

    InspectorTest.log('\nSide-effect-free Runtime.evaluate of consoleNs.foo:');
    logEvaluateResult(await Protocol.Runtime.evaluate(
        {expression: 'consoleNs.foo', throwOnSideEffect: true}));
    await logModuleStatus('console');

    InspectorTest.log('\nRegular Runtime.evaluate of consoleNs.foo:');
    logEvaluateResult(
        await Protocol.Runtime.evaluate({expression: 'consoleNs.foo'}));
    await logModuleStatus('console');
  },

  async function testEvaluateOnCallFrame() {
    await Protocol.Debugger.enable();
    Protocol.Runtime.evaluate({expression: 'debugger;'});
    const {params: {callFrames: [{callFrameId}]}} =
        await Protocol.Debugger.oncePaused();

    InspectorTest.log(
        'Side-effect-free Debugger.evaluateOnCallFrame of pausedNs.foo:');
    logEvaluateResult(await Protocol.Debugger.evaluateOnCallFrame(
        {callFrameId, expression: 'pausedNs.foo', throwOnSideEffect: true}));

    InspectorTest.log('\nRegular Debugger.evaluateOnCallFrame of pausedNs.foo:');
    logEvaluateResult(await Protocol.Debugger.evaluateOnCallFrame(
        {callFrameId, expression: 'pausedNs.foo'}));

    await Protocol.Debugger.resume();
    await logModuleStatus('paused');
    await Protocol.Debugger.disable();
  },
]);
