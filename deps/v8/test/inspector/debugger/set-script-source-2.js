// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests Debugger.setScriptSource');

session.setupScriptMap();

function foo() {
var x = 1;
debugger;
return x + 2;
}

function boo() {
debugger;
var x = 1;
return x + 2;
}

InspectorTest.runAsyncTestSuite([
  async function addLineAfter() {
    await Protocol.Debugger.enable();
    Protocol.Runtime.evaluate({expression: foo.toString()});
    let {params:{scriptId}} = await Protocol.Debugger.onceScriptParsed();
    Protocol.Runtime.evaluate({
      expression: 'setTimeout(foo, 0)//# sourceURL=test.js'});
    let {params:{callFrames}} = await Protocol.Debugger.oncePaused();
    await session.logSourceLocation(callFrames[0].location);
    await replaceInSource(scriptId, 'debugger;', 'debugger;\nvar x = 3;');

    Protocol.Debugger.resume();
    await Protocol.Debugger.oncePaused();
    await Protocol.Debugger.disable();
  },

  async function addLineBefore() {
    await Protocol.Debugger.enable();
    Protocol.Runtime.evaluate({expression: foo.toString()});
    let {params:{scriptId}} = await Protocol.Debugger.onceScriptParsed();
    Protocol.Runtime.evaluate({
      expression: 'setTimeout(foo, 0)//# sourceURL=test.js'});
    let {params:{callFrames}} = await Protocol.Debugger.oncePaused();
    await session.logSourceLocation(callFrames[0].location);
    await replaceInSource(scriptId, 'debugger;', 'var x = 3;\ndebugger;');

    Protocol.Debugger.resume();
    await Protocol.Debugger.oncePaused();
    await Protocol.Debugger.disable();
  },

  async function breakAtFirstLineAddLineAfter() {
    await Protocol.Debugger.enable();
    Protocol.Runtime.evaluate({expression: boo.toString()});
    let {params:{scriptId}} = await Protocol.Debugger.onceScriptParsed();
    Protocol.Runtime.evaluate({
      expression: 'setTimeout(boo, 0)//# sourceURL=test.js'});
    let {params:{callFrames}} = await Protocol.Debugger.oncePaused();
    await session.logSourceLocation(callFrames[0].location);
    await replaceInSource(scriptId, 'debugger;', 'debugger;\nvar x = 3;');

    await Protocol.Debugger.disable();
  },

  async function breakAtFirstLineAddLineBefore() {
    await Protocol.Debugger.enable();
    Protocol.Runtime.evaluate({expression: boo.toString()});
    let {params:{scriptId}} = await Protocol.Debugger.onceScriptParsed();
    Protocol.Runtime.evaluate({
      expression: 'setTimeout(boo, 0)//# sourceURL=test.js'});
    let {params:{callFrames}} = await Protocol.Debugger.oncePaused();
    await session.logSourceLocation(callFrames[0].location);
    await replaceInSource(scriptId, 'debugger;', 'var x = 3;\ndebugger;');

    await Protocol.Debugger.disable();
  }
]);

async function replaceInSource(scriptId, oldString, newString) {
  InspectorTest.log('---');
  let {result:{scriptSource}} =
      await Protocol.Debugger.getScriptSource({scriptId});
  let {result} = await Protocol.Debugger.setScriptSource({
    scriptId,
    scriptSource: scriptSource.replace(oldString, newString)
  });
  InspectorTest.log('Break location after LiveEdit:');
  await session.logSourceLocation(result.callFrames[0].location, true);
  InspectorTest.log('stackChanged: ' + result.stackChanged);
  if (result.stackChanged) {
    InspectorTest.log('Protocol.Debugger.stepInto');
    Protocol.Debugger.stepInto();
    var {params:{callFrames}} = await Protocol.Debugger.oncePaused();
    await session.logSourceLocation(callFrames[0].location);
  }
}
