// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

print('Check that setScriptSource completes correctly when an exception is thrown.');

Protocol.Debugger.enable();

InspectorTest.runTestSuite([
  function testIncorrectScriptId(next) {
    Protocol.Debugger.setScriptSource({ scriptId: '-1', scriptSource: '0' })
      .then(InspectorTest.logMessage)
      .then(next);
  },

  function testSourceWithSyntaxError(next) {
    Protocol.Debugger.onceScriptParsed()
      .then(message => Protocol.Debugger.setScriptSource({ scriptId: message.params.scriptId, scriptSource: 'a # b' }))
      .then(InspectorTest.logMessage)
      .then(next);
    InspectorTest.addScript('function foo() {}');
  }
]);
