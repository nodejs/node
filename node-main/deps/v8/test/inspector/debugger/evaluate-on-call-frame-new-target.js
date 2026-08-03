// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} =
    InspectorTest.start(`Test that new.target can be inspected in Debugger.evaluateOnCallFrame`);

contextGroup.addScript(`
function C() {
  const fn = new.target;
  debugger;
}

function D() {
  const fn = eval('new.target');
  debugger;
}

function E() {
  debugger;
}

class A {
  constructor() {
    const fn = new.target;
    debugger;
  }
}

class B extends A {}

function F() {
  () => new.target;  // context-allocate.
  debugger;
}
`);

async function ensureNewTargetIsNotReportedInTheScopeChain(scopeChain) {
  for (const scope of scopeChain) {
    if (scope.type !== 'local') continue;
    const {result: {result: variables}} =
      await Protocol.Runtime.getProperties({ objectId: scope.object.objectId });
    const variable = variables.find(variable => variable.name === '.new.target');
    if (variable) {
      InspectorTest.logMessage(`FAIL: 'new.target' was also reported in the scopeChain on Debugger.paused`);
    }
  }
}

async function evaluateNewTargetOnPause(expression) {
  await Protocol.Debugger.enable();
  Protocol.Runtime.evaluate({ expression });

  const { params: { callFrames: [{ callFrameId, scopeChain }] } } = await Protocol.Debugger.oncePaused();
  await ensureNewTargetIsNotReportedInTheScopeChain(scopeChain);

  const { result: { result } } = await Protocol.Debugger.evaluateOnCallFrame({
    callFrameId,
    expression: 'new.target',
  });
  InspectorTest.logMessage(result);

  await Protocol.Debugger.resume();
  await Protocol.Debugger.disable();
}

InspectorTest.runAsyncTestSuite([
  async function withExplicitUsage() {
    await evaluateNewTargetOnPause('new C()');
  },
  async function withDirectEval() {
    await evaluateNewTargetOnPause('new D()');
  },
  async function withoutExplicitUsage() {
    await evaluateNewTargetOnPause('new E()');
  },
  async function withInheritence() {
    await evaluateNewTargetOnPause('new B()');
  },
  async function withContextAllocatedNewTarget() {
    await evaluateNewTargetOnPause('new F()');
  },
]);
