// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests Debugger.setBreakpointOnFunctionCall.');

(async function test() {
  Protocol.Debugger.enable();
  {
    const {result: {result: {objectId}}} = await Protocol.Runtime.evaluate(
        {expression: 'function foo(a,b){ return a + b; }; foo'});
    InspectorTest.log('set breakpoint on function call');
    const {result: {breakpointId}} =
        await Protocol.Debugger.setBreakpointOnFunctionCall({objectId});
    InspectorTest.log('call function');
    Protocol.Runtime.evaluate({expression: 'foo(1,2)'});
    const {params: {hitBreakpoints}} = await Protocol.Debugger.oncePaused();
    InspectorTest.log('paused');
    InspectorTest.log(
        `hitBreakpoints contains id: ${hitBreakpoints[0] === breakpointId}`);
    await Protocol.Debugger.resume();
    InspectorTest.log('remove breakpoint');
    const result = await Protocol.Debugger.removeBreakpoint({breakpointId});
    InspectorTest.logMessage(result);
    InspectorTest.log('call function again');
    await Protocol.Runtime.evaluate({expression: 'foo(1,2)'});
    InspectorTest.log('evaluate finished without pause');
  }
  {
    const {result: {result: {objectId}}} = await Protocol.Runtime.evaluate(
        {expression: 'function foo(a,b){ return a + b; }; foo'});
    InspectorTest.log('set breakpoint on function call');
    const {result: {breakpointId}} =
        await Protocol.Debugger.setBreakpointOnFunctionCall({objectId});
    InspectorTest.log('set breakpoint on same function call');
    InspectorTest.logMessage(
        await Protocol.Debugger.setBreakpointOnFunctionCall({objectId}));
    await Protocol.Debugger.removeBreakpoint({breakpointId});
  }
  {
    const {result: {result: {objectId}}} =
        await Protocol.Runtime.evaluate({expression: 'Array.prototype.push'});
    InspectorTest.log('set breakpoint on function call with condition');
    const {result: {breakpointId}} =
        await Protocol.Debugger.setBreakpointOnFunctionCall(
            {objectId, condition: 'arguments[0] === 2'});
    InspectorTest.log('call function, condition is false');
    await Protocol.Runtime.evaluate({expression: '[].push(0)'});
    InspectorTest.log('evaluate finished without pause');
    InspectorTest.log('call function, condition is true');
    Protocol.Runtime.evaluate({expression: '[].push(2)'});
    const {params: {hitBreakpoints}} = await Protocol.Debugger.oncePaused();
    InspectorTest.log('paused');
    InspectorTest.log(
        `hitBreakpoints contains id: ${hitBreakpoints[0] === breakpointId}`);
    await Protocol.Debugger.resume();
    InspectorTest.log('remove breakpoint');
    const result = await Protocol.Debugger.removeBreakpoint({breakpointId});
    InspectorTest.logMessage(result);
    InspectorTest.log('call function again');
    Protocol.Runtime.evaluate({expression: '[].push(2)'});
    InspectorTest.log('evaluate finished without pause');
  }
  InspectorTest.completeTest();
})();
