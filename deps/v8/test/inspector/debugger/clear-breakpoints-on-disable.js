// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
  InspectorTest.start('Tests that we clear breakpoints on agent disabled');

(async function test() {
session.setupScriptMap();
  Protocol.Runtime.evaluate({expression: 'function foo() {}'});
  Protocol.Debugger.enable();
  let {params:{scriptId}} = await Protocol.Debugger.onceScriptParsed();
  await Protocol.Debugger.setBreakpoint({
    location: {
      lineNumber: 0, columnNumber: 16, scriptId
    }
  });
  Protocol.Runtime.evaluate({expression: 'foo()'});
  var {params:{callFrames}} = await Protocol.Debugger.oncePaused();
  await session.logSourceLocation(callFrames[0].location);
  await Protocol.Debugger.disable();
  await Protocol.Debugger.enable();
  Protocol.Debugger.onPaused(InspectorTest.logMessage);
  Protocol.Runtime.evaluate({expression: 'foo();debugger;'});
  var {params:{callFrames}} = await Protocol.Debugger.oncePaused();
  await session.logSourceLocation(callFrames[0].location);
  InspectorTest.completeTest();
})()
