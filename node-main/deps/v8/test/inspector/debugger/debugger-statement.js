// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    `Tests that pauses due to debugger statements and breakpoints report the hit breakpoint`);

InspectorTest.runAsyncTestSuite([async function testDebuggerStatement() {
  await Protocol.Debugger.enable();
  await Protocol.Runtime.enable();
  const {result: {scriptId}} = await Protocol.Runtime.compileScript(
      {expression: `debugger;`, sourceURL: 'foo.js', persistScript: true});
  await Protocol.Debugger.setBreakpointByUrl({
    lineNumber: 0,
    url: 'foo.js',
  });
  const runPromise = Protocol.Runtime.runScript({scriptId});
  let {params: {reason, hitBreakpoints}} = await Protocol.Debugger.oncePaused();
  InspectorTest.log(
      `Paused with reason ${reason} and hit breakpoints: ${hitBreakpoints}.`);
  await Protocol.Debugger.resume();
  await runPromise;

  await Protocol.Debugger.disable();
  await Protocol.Runtime.disable();
}]);
