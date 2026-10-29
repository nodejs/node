// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-reuse-locals-blocklists

const {session, Protocol} = InspectorTest.start(
    `Test for Debugger.evaluateOnCallFrame and locals blocklist reuse`);

InspectorTest.runAsyncTestSuite([
  async function testContextChainNotInsideFunction() {
    await Protocol.Debugger.enable();
    await Protocol.Runtime.enable();

    let source = `
      class C {}
      try {
        let c = new C();
      } catch (e) {}
    `;
    const {result: {scriptId}} = await Protocol.Runtime.compileScript(
        {expression: source, sourceURL: 'notInside.js', persistScript: true});
    let {result: {breakpointId}} = await Protocol.Debugger.setBreakpointByUrl(
        {lineNumber: 3, url: 'notInside.js'});
    const runPromise = Protocol.Runtime.runScript({scriptId});
    let {params: {reason, callFrames: [{callFrameId}]}} =
        await Protocol.Debugger.oncePaused();

    const {result: {result}} = await Protocol.Debugger.evaluateOnCallFrame(
        {callFrameId, expression: 'C'});
    InspectorTest.logMessage(result);

    await Protocol.Debugger.resume();
    await runPromise;

    await Protocol.Debugger.disable();
    await Protocol.Runtime.disable();
  },

  async function testContextChainInsideFunction() {
    await Protocol.Debugger.enable();
    await Protocol.Runtime.enable();

    let source = `
      var someRandomStuff = 42;
      (function foo() {
        class C {}
        try {
          let c = new C();
        } catch (e) {}
      })();
    `;
    const {result: {scriptId}} = await Protocol.Runtime.compileScript(
        {expression: source, sourceURL: 'inside.js', persistScript: true});
    let {result: {breakpointId}} = await Protocol.Debugger.setBreakpointByUrl(
        {lineNumber: 5, url: 'inside.js'});
    const runPromise = Protocol.Runtime.runScript({scriptId});
    let {params: {reason, callFrames: [{callFrameId}]}} =
        await Protocol.Debugger.oncePaused();

    const {result: {result}} = await Protocol.Debugger.evaluateOnCallFrame(
        {callFrameId, expression: 'C'});
    InspectorTest.logMessage(result);

    await Protocol.Debugger.resume();
    await runPromise;

    await Protocol.Debugger.disable();
    await Protocol.Runtime.disable();
  }
]);
