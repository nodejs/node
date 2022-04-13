// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
  InspectorTest.start('Breakpoint in liveedited script');

contextGroup.addScript(
`function foo() {
}
var f = foo;`);

const newSource = `function boo() {
}
function foo() {
}
f = foo;`;

(async function test() {
  session.setupScriptMap();
  Protocol.Debugger.enable();
  const {params: {scriptId}} = await Protocol.Debugger.onceScriptParsed();
  InspectorTest.log('Update script source');
  let {result} = await Protocol.Debugger.setScriptSource(
      {scriptId, scriptSource: newSource})
  InspectorTest.logMessage(result);
  InspectorTest.log('Set breakpoint');
  ({result} = await Protocol.Debugger.setBreakpoint({location:{
    scriptId,
    lineNumber: 3,
    columnNumber: 0
  }}));
  InspectorTest.logMessage(result);
  InspectorTest.log('Call function foo and dump stack');
  Protocol.Runtime.evaluate({expression: 'foo()'});
  const {params:{callFrames}} = await Protocol.Debugger.oncePaused();
  session.logCallFrames(callFrames);
  InspectorTest.completeTest();
})();
