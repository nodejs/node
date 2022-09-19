// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
  InspectorTest.start('Checks provisional breakpoints by hash in anonymous scripts');
session.setupScriptMap();

InspectorTest.runAsyncTestSuite([
  async function testNextScriptParsed() {
    await Protocol.Debugger.enable();
    // set breakpoint in anonymous script..
    Protocol.Runtime.evaluate({expression: 'function foo(){}'});
    let {params:{hash}} = await Protocol.Debugger.onceScriptParsed();
    let {result:{breakpointId}} = await Protocol.Debugger.setBreakpointByUrl({
      scriptHash: hash,
      lineNumber: 0,
      columnNumber: 15
    });
    // evaluate the same anonymous script again..
    Protocol.Runtime.evaluate({expression: 'function foo(){}'});
    // run function and check Debugger.paused event..
    let evaluation = Protocol.Runtime.evaluate({expression: 'foo()'});
    let result = await Promise.race([evaluation, Protocol.Debugger.oncePaused()]);
    if (result.method !== 'Debugger.paused') {
      InspectorTest.log('FAIL: breakpoint was ignored');
    } else {
      await session.logSourceLocation(result.params.callFrames[0].location);
    }
    // remove breakpoint and run again..
    await Protocol.Debugger.removeBreakpoint({breakpointId});
    evaluation = Protocol.Runtime.evaluate({expression: 'foo()'});
    result = await Promise.race([evaluation, Protocol.Debugger.oncePaused()]);
    if (result.method === 'Debugger.paused') {
      InspectorTest.log('FAIL: breakpoint was not removed');
    }
    await Protocol.Debugger.disable();
  },
  async function testPreviousScriptParsed() {
    await Protocol.Debugger.enable();
    // run script and store function to global list..
    await Protocol.Runtime.evaluate({expression: 'var list = list ? list.concat(foo) : [foo]; function foo(){}'});
    // run same script again..
    Protocol.Runtime.evaluate({expression: 'var list = list ? list.concat(foo) : [foo]; function foo(){}'});
    let {params:{hash}} = await Protocol.Debugger.onceScriptParsed();
    // set breakpoint by hash of latest script..
    let {result:{breakpointId}} = await Protocol.Debugger.setBreakpointByUrl({
      scriptHash: hash,
      lineNumber: 0,
      columnNumber: 49
    });
    // call each function in global list and wait for Debugger.paused events..
    let evaluation = Protocol.Runtime.evaluate({expression: 'list.forEach(x => x())'});
    let result = await Promise.race([evaluation, Protocol.Debugger.oncePaused()]);
    while (result.method === 'Debugger.paused') {
      await session.logSourceLocation(result.params.callFrames[0].location);
      Protocol.Debugger.resume();
      result = await Promise.race([evaluation, Protocol.Debugger.oncePaused()]);
    }
    // remove breakpoint and call functions again..
    await Protocol.Debugger.removeBreakpoint({breakpointId});
    evaluation = Protocol.Runtime.evaluate({expression: 'foo()'});
    result = await Promise.race([evaluation, Protocol.Debugger.oncePaused()]);
    if (result.method === 'Debugger.paused') {
      InspectorTest.log('FAIL: breakpoint was not removed');
    }
    await Protocol.Debugger.disable();
  }
]);
