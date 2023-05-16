// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
  InspectorTest.start('Set breakpoint before function call.');

contextGroup.addScript(`
eval('function test1() {                \\n' +
     '  var a = 1;                      \\n' +
     '  return a;                       \\n' +
     '}                                 \\n' +
     '//# sourceURL=testScriptOne');

eval('function test2() {                \\n' +
     '  var b = 1;                      \\n' +
     '  return b;                       \\n' +
     '}                                 \\n' +
     '//# sourceURL=testScriptTwo');
`);

(async function test() {
  session.setupScriptMap();
  Protocol.Debugger.enable();
  Protocol.Runtime.evaluate({ expression: 'debugger; test1(); test2();' });
  await Protocol.Debugger.oncePaused();
  InspectorTest.log('Pause before function calls, set two breakpoints at:');
  await session.logSourceLocation((await Protocol.Debugger.setBreakpointByUrl({
    url: 'testScriptOne',
    lineNumber: 2
  })).result.locations[0]);
  await session.logSourceLocation((await Protocol.Debugger.setBreakpointByUrl({
    urlRegex: 'Scrip.Two',
    lineNumber: 2
  })).result.locations[0]);
  {
    InspectorTest.log('Resume..');
    Protocol.Debugger.resume();
    const {
      params:{
        callFrames:[{location}]
      }
    } = await Protocol.Debugger.oncePaused();
    InspectorTest.log('Paused at:');
    await session.logSourceLocation(location);
  }
  {
    InspectorTest.log('Resume..');
    Protocol.Debugger.resume();
    const {
      params:{
        callFrames:[{location}]
      }
    } = await Protocol.Debugger.oncePaused();
    InspectorTest.log('Paused at:');
    await session.logSourceLocation(location);
  }
  InspectorTest.completeTest();
})();
