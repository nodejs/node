// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that Debugger.setVariableValue refuses TDZ writes');

const testScript = `
function testContextAllocated() {
  debugger;
  let x = 42;
  return () => x;
}

function testStackAllocated() {
  switch (1) {
    case 1:
      debugger;
      let y = 84;
      return y;
  }
}

function* testGenerator() {
  yield 1;
  const v = z;
  let z = 100;
  return v;
}

function* testGeneratorContext() {
  const r = () => z;
  yield 1;
  yield r();
  let z = 100;
  yield r();
}

function runGeneratorTest() {
  const g = testGenerator();
  g.next();
  const setRes = %SetGeneratorScopeVariableValue(g, 0, "z", 999);
  let readBeforeInitThrew = false;
  try {
    g.next();
  } catch (e) {
    readBeforeInitThrew = (e instanceof ReferenceError);
  }
  return {setRes, readBeforeInitThrew};
}

function runGeneratorContextTest() {
  const g = testGeneratorContext();
  g.next();
  const setRes = %SetGeneratorScopeVariableValue(g, 0, "z", 999);
  let readBeforeInitThrew = false;
  try {
    g.next();
  } catch (e) {
    readBeforeInitThrew = (e instanceof ReferenceError);
  }
  return {setRes, readBeforeInitThrew};
}
`;

contextGroup.addScript(testScript);

InspectorTest.runAsyncTestSuite([
  async function testContextAllocatedTdz() {
    await Protocol.Debugger.enable();
    await Protocol.Runtime.enable();

    const evalPromise = Protocol.Runtime.evaluate({
      expression: 'testContextAllocated()()',
    });
    const {params: {callFrames: [{callFrameId}]}} =
        await Protocol.Debugger.oncePaused();

    const setResult = await Protocol.Debugger.setVariableValue({
      scopeNumber: 0,
      variableName: 'x',
      newValue: {value: 999},
      callFrameId,
    });
    InspectorTest.log('setVariableValue on context TDZ result:');
    InspectorTest.logMessage(InspectorTest.trimErrorMessage(setResult));

    const evalBeforeInit = await Protocol.Debugger.evaluateOnCallFrame({
      expression: 'x',
      callFrameId,
    });
    InspectorTest.log(
        'evaluateOnCallFrame in TDZ threw exception: ' +
        (evalBeforeInit.result.exceptionDetails !== undefined));

    await Protocol.Debugger.resume();
    const finalResult = await evalPromise;
    InspectorTest.log(
        'Final return value after normal initialization: ' +
        finalResult.result.result.value);

    await Protocol.Debugger.disable();
    await Protocol.Runtime.disable();
  },

  async function testStackAllocatedTdz() {
    await Protocol.Debugger.enable();
    await Protocol.Runtime.enable();

    const evalPromise = Protocol.Runtime.evaluate({
      expression: 'testStackAllocated()',
    });
    const {params: {callFrames: [{callFrameId}]}} =
        await Protocol.Debugger.oncePaused();

    const setResult = await Protocol.Debugger.setVariableValue({
      scopeNumber: 0,
      variableName: 'y',
      newValue: {value: 999},
      callFrameId,
    });
    InspectorTest.log('setVariableValue on stack TDZ result:');
    InspectorTest.logMessage(InspectorTest.trimErrorMessage(setResult));

    await Protocol.Debugger.resume();
    const finalResult = await evalPromise;
    InspectorTest.log(
        'Final return value after normal initialization: ' +
        finalResult.result.result.value);

    await Protocol.Debugger.disable();
    await Protocol.Runtime.disable();
  },

  async function testScriptScopeTdz() {
    await Protocol.Debugger.enable();
    await Protocol.Runtime.enable();

    const scriptSource = `
      debugger;
      let scriptVar = 123;
    `;
    contextGroup.addScript(scriptSource, 0, 0, 'script-tdz.js');
    const {params: {callFrames: [{callFrameId, scopeChain}]}} =
        await Protocol.Debugger.oncePaused();

    const scopeNumber = scopeChain.findIndex(scope => scope.type === 'script');
    InspectorTest.log('Script scope index: ' + scopeNumber);

    const setResult = await Protocol.Debugger.setVariableValue({
      scopeNumber,
      variableName: 'scriptVar',
      newValue: {value: 999},
      callFrameId,
    });
    InspectorTest.log('setVariableValue on script TDZ result:');
    InspectorTest.logMessage(InspectorTest.trimErrorMessage(setResult));

    await Protocol.Debugger.resume();

    const evalRes = await Protocol.Runtime.evaluate({
      expression: 'scriptVar',
    });
    InspectorTest.log(
        'scriptVar value after normal initialization: ' +
        evalRes.result.result.value);

    await Protocol.Debugger.disable();
    await Protocol.Runtime.disable();
  },

  async function testModuleScopeTdz() {
    await Protocol.Debugger.enable();
    await Protocol.Runtime.enable();

    const moduleSource = `
      debugger;
      export let moduleExport = 456;
    `;
    contextGroup.addModule(moduleSource, 'module-tdz.js');
    const {params: {callFrames: [{callFrameId, scopeChain}]}} =
        await Protocol.Debugger.oncePaused();

    const scopeNumber = scopeChain.findIndex(scope => scope.type === 'module');
    InspectorTest.log('Module scope index: ' + scopeNumber);

    const setResult = await Protocol.Debugger.setVariableValue({
      scopeNumber,
      variableName: 'moduleExport',
      newValue: {value: 999},
      callFrameId,
    });
    InspectorTest.log('setVariableValue on module TDZ result:');
    InspectorTest.logMessage(InspectorTest.trimErrorMessage(setResult));

    await Protocol.Debugger.resume();

    await Protocol.Debugger.disable();
    await Protocol.Runtime.disable();
  },

  async function testGeneratorTdzRepro() {
    await Protocol.Runtime.enable();
    const evalRes = await Protocol.Runtime.evaluate({
      expression: 'runGeneratorTest()',
      returnByValue: true,
    });
    InspectorTest.log(
        'Generator register TDZ test result: ' +
        JSON.stringify(evalRes.result.result.value));

    const evalContextRes = await Protocol.Runtime.evaluate({
      expression: 'runGeneratorContextTest()',
      returnByValue: true,
    });
    InspectorTest.log(
        'Generator context TDZ test result: ' +
        JSON.stringify(evalContextRes.result.result.value));
    await Protocol.Runtime.disable();
  },
]);
