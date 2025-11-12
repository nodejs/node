// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
  InspectorTest.start('Breakpoint can be set at line o = 1;');

contextGroup.addScript(
`var o = {
  f: function(x) {
    var a = x + 1;
    o = 1;
  }
}`);

(async function test() {
  session.setupScriptMap();
  Protocol.Debugger.enable();
  const {params: {scriptId}} = await Protocol.Debugger.onceScriptParsed();
  InspectorTest.log('Set breakpoint..');
  const {result} = await Protocol.Debugger.setBreakpoint({location:{
    scriptId,
    lineNumber: 3,
    columnNumber: 0
  }});
  await session.logSourceLocation(result.actualLocation);
  InspectorTest.completeTest();
})();
