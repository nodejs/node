// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Check that setScriptSource completes correctly when an exception is' +
    ' thrown.');

Protocol.Debugger.enable();

InspectorTest.runAsyncTestSuite([
  async function testIncorrectScriptId() {
    InspectorTest.logMessage(await Protocol.Debugger.setScriptSource(
        {scriptId: '-1', scriptSource: '0'}));
  },

  async function testSourceWithSyntaxError() {
    contextGroup.addScript('function foo() {}');
    const {params} = await Protocol.Debugger.onceScriptParsed();
    const msg = await Protocol.Debugger.setScriptSource({
      scriptId: params.scriptId,
      scriptSource: 'function foo() {\n  return a # b;\n}'
    });
    InspectorTest.logMessage(msg);
  }
]);
