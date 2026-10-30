// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests line numbers with \\r line endings.');

// Variable definitions for i0 through i3 are each terminated with \r.
contextGroup.addScript(
    'function f() {\n' +
        '  var i0 = 0;\r  var i1 = 1;\r  var i2 = 2;\r  var i3 = 3;\n' +
        '  var j0 = 0;\n' +
        '  var j1 = 1;\n' +
        '  var j2 = 2;\n' +
        '  var j3 = 3;\n' +
        '}',
    0, 0, 'test.js');

(async function test() {
  await Protocol.Debugger.enable();
  await Protocol.Debugger.setBreakpointByUrl({
    lineNumber: 3,
    columnNumber: 0,
    url: 'test.js',
  });

  Protocol.Debugger.onPaused(async message => {
    const location = message.params.callFrames[0].location;
    const {result: {scriptSource}} = await Protocol.Debugger.getScriptSource({
      scriptId: location.scriptId,
    });
    const lines = scriptSource.split(/\r\n|\r|\n/);
    InspectorTest.log(
        `Line ${location.lineNumber}: ${lines[location.lineNumber]}`);
    Protocol.Debugger.resume();
  });

  await Protocol.Runtime.evaluate({expression: 'f()'});
  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
