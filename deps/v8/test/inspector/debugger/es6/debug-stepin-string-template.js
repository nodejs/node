// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests stepping into string template expressions.');

const source = `function u(x) {
  return x.toUpperCase();                         // d
}                                                 // e

var n = 3;

var o = {
  toString: function() {
    return "D";                                   // f
  }                                               // g
}

function testComplete() {}

debugger;                                         // a
var s = \`1 \${u("a")} 2 \${u("b")} 3 \${n} 4 \${o}\`;  // b
testComplete();                                   // c
`;

(async function test() {
  await Protocol.Debugger.enable();

  let log = [];
  const lines = source.split('\n');

  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    var entry = '';
    for (var i = 0; i < callFrames.length; i++) {
      let line = lines[callFrames[i].location.lineNumber];
      entry += line.substr(-1);
      entry += callFrames[i].location.columnNumber;
    }
    log.push(entry);
    InspectorTest.log(entry);
    if (entry !== 'c0') {
      await Protocol.Debugger.stepInto();
    } else {
      await Protocol.Debugger.resume();
    }
  });

  contextGroup.addScript(source, 0, 0, 'test.js');

  await InspectorTest.waitForPendingTasks();

  let {result: {result: sResult}} =
      await Protocol.Runtime.evaluate({expression: 's'});

  InspectorTest.log('s: ' + sResult.value);
  InspectorTest.log('log: ' + JSON.stringify(log));

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
