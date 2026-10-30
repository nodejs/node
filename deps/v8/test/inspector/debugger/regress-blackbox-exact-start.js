// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Tests that a blackboxed range includes its exact start boundary');

const scriptSource = `debugger;\n//# sourceURL=test-script.js`;

Promise.all([
  Protocol.Debugger.enable(),
  Protocol.Runtime.enable()
]).then(() => {
    Protocol.Debugger.onPaused(message => {
      let callFrame = message.params.callFrames[0];
      InspectorTest.log(`Paused in ${callFrame.functionName || '<top level>'}`);
      Protocol.Debugger.resume();
    });

    return Protocol.Runtime.compileScript({
      expression: scriptSource,
      sourceURL: 'test-script.js',
      persistScript: true
    });
  })
  .then(response => {
    let scriptId = response.result.scriptId;
    InspectorTest.log(`Compiled script`);
    return Protocol.Debugger.setBlackboxedRanges({
      scriptId: scriptId,
      positions: [ { lineNumber: 0, columnNumber: 0 } ]
    }).then(() => scriptId);
  })
  .then(scriptId => {
    return Protocol.Runtime.runScript({ scriptId: scriptId });
  })
  .then(() => {
    InspectorTest.log('Test completed');
    InspectorTest.completeTest();
  })
  .catch(e => {
    InspectorTest.log(`Error: ${e}`);
    InspectorTest.completeTest();
  });
