// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
  InspectorTest.start('Set breakpoint before function call.');

contextGroup.addScript(`
function static() {
  console.log("> static");  // Break
}

function install() {
  eval("this.dynamic = function dynamic() { \\n" +
       "  console.log(\\"> dynamic\\");  // Break\\n" +
       "}\\n" +
       "//# sourceURL=dynamicScript");
}

install();
//# sourceURL=staticScript`);

(async function test() {
  session.setupScriptMap();
  Protocol.Debugger.enable();
  await session.logSourceLocation((await Protocol.Debugger.setBreakpointByUrl({
    url: 'dynamicScript',
    lineNumber: 1
  })).result.locations[0]);
  await session.logSourceLocation((await Protocol.Debugger.setBreakpointByUrl({
    url: 'staticScript',
    lineNumber: 2
  })).result.locations[0]);

  Protocol.Runtime.evaluate({ expression: 'dynamic(); static();' });
  {
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
