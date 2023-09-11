// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
  'Tests V8InspectorClient::resourceNameToUrl.');

(async function test(){
  Protocol.Runtime.enable();
  await Protocol.Debugger.enable();
  contextGroup.addScript(`inspector.setResourceNamePrefix('prefix://')`);
  await Protocol.Debugger.onceScriptParsed();

  InspectorTest.log('Check script with url:');
  contextGroup.addScript('function foo(){}', 0, 0, 'url');
  InspectorTest.logMessage(await Protocol.Debugger.onceScriptParsed());

  InspectorTest.log('Check script with sourceURL comment:');
  contextGroup.addScript('function foo(){} //# sourceURL=foo.js', 0, 0, 'url');
  InspectorTest.logMessage(await Protocol.Debugger.onceScriptParsed());

  InspectorTest.log('Check script failed to parse:');
  contextGroup.addScript('function foo(){', 0, 0, 'url');
  InspectorTest.logMessage(await Protocol.Debugger.onceScriptFailedToParse());

  InspectorTest.log('Check script failed to parse with sourceURL comment:');
  contextGroup.addScript('function foo(){ //# sourceURL=foo.js', 0, 0, 'url');
  InspectorTest.logMessage(await Protocol.Debugger.onceScriptFailedToParse());

  InspectorTest.log('Test runtime stack trace:');
  contextGroup.addScript(`
    function foo() {
      console.log(42);
    }
    eval('foo(); //# sourceURL=boo.js');
  `, 0, 0, 'url');
  InspectorTest.logMessage(await Protocol.Runtime.onceConsoleAPICalled());

  InspectorTest.log('Test debugger stack trace:');
  contextGroup.addScript(`
    function foo() {
      debugger;
    }
    eval('foo(); //# sourceURL=boo.js');
  `, 0, 0, 'url');
  const {params:{callFrames}} = await Protocol.Debugger.oncePaused();
  InspectorTest.logMessage(callFrames.map(frame => frame.url));
  InspectorTest.completeTest();
})();
