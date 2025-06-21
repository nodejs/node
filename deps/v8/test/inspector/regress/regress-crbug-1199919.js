// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} =
  InspectorTest.start('Regression test for crbug/1199919');

const source = `
async function defaultParameter(x = 1) {
  return x;
}

async function destructuringParameter({x}) {
  return x;
}
`;
const url = 'v8://test.js';

contextGroup.addScript(source, 0, 0, url);
session.setupScriptMap();

InspectorTest.runAsyncTestSuite([
  async function testDefaultParameter() {
    await Promise.all([Protocol.Runtime.enable(), Protocol.Debugger.enable()]);
    const {result: {breakpointId}} = await Protocol.Debugger.setBreakpointByUrl({lineNumber: 2, url});
    const evalPromise = Protocol.Runtime.evaluate({expression: 'defaultParameter()'});
    const {params: {callFrames}} = await Protocol.Debugger.oncePaused();
    session.logCallFrames(callFrames);
    await Protocol.Debugger.removeBreakpoint({breakpointId});
    await Promise.all([Protocol.Debugger.resume(), evalPromise]);
    await Promise.all([Protocol.Runtime.disable(), Protocol.Debugger.disable()]);
  },

  async function testDestructuringParameter() {
    await Promise.all([Protocol.Runtime.enable(), Protocol.Debugger.enable()]);
    const {result: {breakpointId}} = await Protocol.Debugger.setBreakpointByUrl({lineNumber: 6, url});
    const evalPromise = Protocol.Runtime.evaluate({expression: 'destructuringParameter({x: 5})'});
    const {params: {callFrames}} = await Protocol.Debugger.oncePaused();
    session.logCallFrames(callFrames);
    await Protocol.Debugger.removeBreakpoint({breakpointId});
    await Promise.all([Protocol.Debugger.resume(), evalPromise]);
    await Promise.all([Protocol.Runtime.disable(), Protocol.Debugger.disable()]);
  }
]);
