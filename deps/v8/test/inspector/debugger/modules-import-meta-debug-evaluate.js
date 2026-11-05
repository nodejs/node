// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {contextGroup, Protocol} = InspectorTest.start(
    'Tests import.meta in lazy functions during module debug-evaluate.');

const moduleSource = `
let evaluateResult;
function runEval() {
  (() => eval('let h = () => import.meta; h()'))();
}
debugger;
try {
  runEval();
  globalThis.evalResult = 'did not throw';
} catch (e) {
  globalThis.evalResult = e.name;
}
`;

(async function test() {
  await Protocol.Debugger.enable();
  contextGroup.addModule(moduleSource, 'module1');
  const {params: {callFrames}} = await Protocol.Debugger.oncePaused();
  const callFrameId = callFrames[0].callFrameId;
  InspectorTest.logMessage(await Protocol.Debugger.evaluateOnCallFrame({
    callFrameId,
    expression: 'let f = () => import.meta; let g = () => () => import.meta; ' +
        'f() === import.meta && g()() === import.meta',
    returnByValue: true,
  }));
  await Protocol.Debugger.resume();
  InspectorTest.logMessage(await Protocol.Runtime.evaluate({
    expression: 'globalThis.evalResult',
  }));
  InspectorTest.completeTest();
})();
